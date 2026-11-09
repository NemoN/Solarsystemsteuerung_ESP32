#include "RemoteRelayUpdate.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "GitHubRoots.h"
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

namespace {
constexpr size_t MAX_IMAGE = 0x140000;
constexpr char MARKER[] = "RBS16467-RELAY-FIRMWARE-OTA1";
constexpr char GITHUB_PREFIX[] = "https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/";
constexpr char GITHUB_SUFFIX[] = "/Relaiskarte.ino.bin";
bool validGithubSource(const String& url, const String& digest) {
  if (url.length() >= 256 || !url.startsWith(GITHUB_PREFIX) || !url.endsWith(GITHUB_SUFFIX) ||
      url.length() <= sizeof(GITHUB_PREFIX) + sizeof(GITHUB_SUFFIX) - 2 || digest.length() != 64) return false;
  const size_t first = sizeof(GITHUB_PREFIX) - 1, end = url.length() - (sizeof(GITHUB_SUFFIX) - 1);
  for (size_t i = first; i < end; ++i) {
    const char c = url[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')) return false;
  }
  for (size_t i = 0; i < digest.length(); ++i)
    if (!((digest[i] >= '0' && digest[i] <= '9') || (digest[i] >= 'a' && digest[i] <= 'f'))) return false;
  return true;
}
bool compatibleNetwork(const IPAddress& target) {
  if (WiFi.status() != WL_CONNECTED || target == WiFi.localIP()) return false;
  const IPAddress local = WiFi.localIP(), mask = WiFi.subnetMask();
  for (int i = 0; i < 4; ++i) if ((target[i] & mask[i]) != (local[i] & mask[i])) return false;
  return true;
}
String encodeForm(const String& text) {
  static const char hex[] = "0123456789ABCDEF";
  String out; out.reserve(text.length() + 64);
  for (size_t i = 0; i < text.length(); ++i) {
    const uint8_t c = text[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += char(c);
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}
void hexDigest(const uint8_t bytes[32], char out[65]) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) { out[2*i] = hex[bytes[i] >> 4]; out[2*i+1] = hex[bytes[i] & 15]; }
  out[64] = 0;
}
int exchange(const RemoteRelayConfig& cfg, const char* path, const String* body, String& response) {
  IPAddress target;
  if (!target.fromString(cfg.address) || !compatibleNetwork(target)) return -1;
  WiFiClient client; HTTPClient http; http.setConnectTimeout(4000); http.setTimeout(5000); http.setReuse(false);
  if (!http.begin(client, "http://" + String(cfg.address) + path)) return -1;
  http.addHeader("X-Relay-Key", cfg.key);
  if (body) http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const int code = body ? http.POST(*body) : http.GET();
  response = "";
  if (code == 200 && http.getSize() >= 0 && http.getSize() <= 128) response = http.getString();
  http.end(); return code;
}
bool requestWithRetry(const RemoteRelayConfig& cfg, const char* path, const String& body, const String& expected, int& lastCode) {
  for (int attempt = 0; attempt < 5; ++attempt) {
    String reply; const int code = exchange(cfg, path, &body, reply); lastCode = code;
    if (code == 200 && reply == expected) return true;
    if (code >= 400 && code < 500) return false;
    if (attempt < 4) vTaskDelay(pdMS_TO_TICKS(250 * (attempt + 1)));
  }
  return false;
}
}
bool RemoteRelayUpdate::begin() { lock_ = xSemaphoreCreateMutex(); return lock_ != nullptr; }
void RemoteRelayUpdate::releaseImage() { if (image_) { heap_caps_free(image_); image_ = nullptr; } }
bool RemoteRelayUpdate::active() {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  const bool value = state_ == RECEIVING || state_ == DOWNLOADING || state_ == SENDING || state_ == VERIFYING;
  xSemaphoreGive(lock_); return value;
}
bool RemoteRelayUpdate::prepare(size_t size, const RemoteRelaySnapshot& relay, char token[33]) {
  if (!lock_ || size < 1024 || size > MAX_IMAGE || !relay.online || relay.busy || !relay.config.address[0] || !relay.config.key[0]) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (state_ == RECEIVING || state_ == DOWNLOADING || state_ == SENDING || state_ == VERIFYING) { xSemaphoreGive(lock_); return false; }
  releaseImage();
  image_ = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!image_) { xSemaphoreGive(lock_); return false; }
  uint8_t random[16]; esp_fill_random(random, sizeof(random));
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < sizeof(random); ++i) { token_[2*i] = hex[random[i] >> 4]; token_[2*i+1] = hex[random[i] & 15]; }
  token_[32] = 0; memcpy(token, token_, 33);
  config_ = relay.config; size_ = size; received_ = sent_ = lastLength_ = 0;
  lastActivity_ = millis(); error_[0] = githubUrl_[0] = githubSha256_[0] = 0; cancelRequested_ = false; state_ = RECEIVING;
  xSemaphoreGive(lock_); return true;
}
bool RemoteRelayUpdate::prepareGithub(size_t size, const String& url, const String& sha256, const RemoteRelaySnapshot& relay, char token[33]) {
  if (!validGithubSource(url, sha256) || !prepare(size, relay, token)) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  url.toCharArray(githubUrl_, sizeof(githubUrl_)); sha256.toCharArray(githubSha256_, sizeof(githubSha256_));
  state_ = DOWNLOADING;
  if (xTaskCreate(taskEntry, "relay-github", 16384, this, 1, nullptr) != pdPASS) {
    state_ = FAILED; strlcpy(error_, "Download-Task konnte nicht gestartet werden.", sizeof(error_)); releaseImage(); xSemaphoreGive(lock_); return false;
  }
  xSemaphoreGive(lock_); return true;
}
bool RemoteRelayUpdate::chunk(const String& token, size_t offset, const String& base64) {
  if (!lock_ || base64.length() > 5464 || !base64.length()) return false;
  static uint8_t decoded[4096]; size_t length = 0; uint8_t digest[32];
  if (mbedtls_base64_decode(decoded, sizeof(decoded), &length, reinterpret_cast<const uint8_t*>(base64.c_str()), base64.length()) != 0 || !length || mbedtls_sha256_ret(decoded, length, digest, 0) != 0) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  bool ok = false;
  if (state_ == RECEIVING && token == token_) {
    if (lastLength_ && offset == lastOffset_ && length == lastLength_ && memcmp(digest, lastHash_, 32) == 0) ok = true;
    else if (offset == received_ && length <= size_ - received_) {
      memcpy(image_ + offset, decoded, length); lastOffset_ = offset; lastLength_ = length; memcpy(lastHash_, digest, 32); received_ += length; ok = true;
    }
    if (ok) lastActivity_ = millis();
  }
  xSemaphoreGive(lock_); return ok;
}
bool RemoteRelayUpdate::start(const String& token) {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (state_ != RECEIVING || token != token_ || received_ != size_) { xSemaphoreGive(lock_); return false; }
  state_ = SENDING;
  if (xTaskCreate(taskEntry, "relay-ota", 12288, this, 1, nullptr) != pdPASS) {
    state_ = FAILED; strlcpy(error_, "Update-Task konnte nicht gestartet werden.", sizeof(error_)); releaseImage(); xSemaphoreGive(lock_); return false;
  }
  xSemaphoreGive(lock_); return true;
}
bool RemoteRelayUpdate::cancel(const String& token) {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  bool ok = token == token_ && (state_ == RECEIVING || state_ == DOWNLOADING || state_ == SENDING);
  if (ok && state_ == RECEIVING) { releaseImage(); state_ = CANCELED; }
  else if (ok) cancelRequested_ = true;
  xSemaphoreGive(lock_); return ok;
}
bool RemoteRelayUpdate::status(const String& token, RemoteRelayUpdateStatus& result) {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (!token.length() || token != token_) { xSemaphoreGive(lock_); return false; }
  static const char* names[] = {"idle", "receiving", "downloading", "sending", "verifying", "done", "failed", "unknown", "canceled"};
  strlcpy(result.state, names[state_], sizeof(result.state)); strlcpy(result.error, error_, sizeof(result.error));
  result.size = size_; result.received = received_; result.sent = sent_;
  xSemaphoreGive(lock_); return true;
}
void RemoteRelayUpdate::loop() {
  if (!lock_) return;
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (state_ == RECEIVING && millis() - lastActivity_ >= 45000) { releaseImage(); state_ = FAILED; strlcpy(error_, "Upload zum Haupt-ESP unterbrochen.", sizeof(error_)); }
  xSemaphoreGive(lock_);
}
void RemoteRelayUpdate::finishTask(State state, const char* message) {
  xSemaphoreTake(lock_, portMAX_DELAY); releaseImage(); state_ = state; strlcpy(error_, message, sizeof(error_)); xSemaphoreGive(lock_);
}
bool RemoteRelayUpdate::downloadGithub() {
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    xSemaphoreTake(lock_, portMAX_DELAY); const bool canceled = cancelRequested_; received_ = 0; xSemaphoreGive(lock_);
    if (canceled) { finishTask(CANCELED, "Update abgebrochen."); return false; }
    WiFiClientSecure client; client.setCACert(GITHUB_ROOT_CERTS); client.setHandshakeTimeout(12);
    HTTPClient http; http.setConnectTimeout(12000); http.setTimeout(12000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); http.setRedirectLimit(4); http.setUserAgent("Backload-ESP32");
    const bool started = http.begin(client, githubUrl_);
    const int code = started ? http.GET() : -1;
    bool retry = code <= 0 || code == 408 || code == 429 || code >= 500;
    const char* error = nullptr;
    if (code != HTTP_CODE_OK) error = "GitHub-Download fehlgeschlagen.";
    else if (http.getSize() != int(size_)) error = "Die Dateigröße stimmt nicht mit dem Release überein.";
    size_t total = 0; uint32_t lastData = millis();
    WiFiClient* stream = error ? nullptr : http.getStreamPtr();
    while (!error && total < size_) {
      xSemaphoreTake(lock_, portMAX_DELAY); const bool stop = cancelRequested_; xSemaphoreGive(lock_);
      if (stop) { error = "Update abgebrochen."; break; }
      const int available = stream->available();
      if (available <= 0) {
        if (available < 0 || !http.connected() || millis() - lastData > 15000) { error = "GitHub-Download unterbrochen."; retry = true; break; }
        vTaskDelay(pdMS_TO_TICKS(10)); continue;
      }
      const size_t length = min(min(size_t(available), size_t(2048)), size_ - total);
      const int count = stream->read(image_ + total, length);
      if (count <= 0) {
        if (!http.connected() || millis() - lastData > 15000) { error = "GitHub-Download unterbrochen."; retry = true; break; }
        vTaskDelay(pdMS_TO_TICKS(10)); continue;
      }
      total += size_t(count); lastData = millis();
      xSemaphoreTake(lock_, portMAX_DELAY); received_ = total; xSemaphoreGive(lock_);
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    http.end();
    xSemaphoreTake(lock_, portMAX_DELAY); const bool stopped = cancelRequested_; xSemaphoreGive(lock_);
    if (stopped) { finishTask(CANCELED, "Update abgebrochen."); return false; }
    if (!error) {
      uint8_t digest[32]; char actual[65];
      if (mbedtls_sha256_ret(image_, size_, digest, 0) != 0) error = "SHA-256-Prüfung fehlgeschlagen.";
      else { hexDigest(digest, actual); if (strcmp(actual, githubSha256_) != 0) error = "SHA-256-Prüfsumme stimmt nicht mit GitHub überein."; }
    }
    if (!error) { xSemaphoreTake(lock_, portMAX_DELAY); state_ = SENDING; xSemaphoreGive(lock_); return true; }
    if (!retry || attempt == 2) { finishTask(FAILED, error); return false; }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  finishTask(FAILED, "GitHub-Download fehlgeschlagen."); return false;
}
void RemoteRelayUpdate::taskEntry(void* p) { static_cast<RemoteRelayUpdate*>(p)->task(); }
void RemoteRelayUpdate::task() {
  if (githubUrl_[0] && !downloadGithub()) { vTaskDelete(nullptr); return; }
  uint8_t digest[32]; char digestText[65];
  if (size_ < 24 || image_[0] != 0xe9 || image_[12] != 0 || image_[13] != 0 ||
      !memmem(image_, size_, MARKER, sizeof(MARKER)-1) || mbedtls_sha256_ret(image_, size_, digest, 0) != 0) {
    finishTask(FAILED, "Die Datei ist keine Firmware für diese Relaiskarte."); vTaskDelete(nullptr); return;
  }
  hexDigest(digest, digestText);
  String before;
  if (exchange(config_, "/api/ota/info", nullptr, before) != 200 || !before.startsWith(String(MARKER)+":") || !before.endsWith(":READY")) {
    finishTask(FAILED, "Relaiskarte unterstützt das Update nicht oder ist nicht bereit."); vTaskDelete(nullptr); return;
  }
  const String prep = "token=" + String(token_) + "&size=" + String(size_) + "&sha256=" + String(digestText);
  String ready; int code = -1;
  for (int attempt = 0; attempt < 3; ++attempt) {
    code = exchange(config_, "/api/ota/prepare", &prep, ready);
    if (code == 200 && ready.startsWith("READY:")) break;
    if (code >= 400 && code < 500) break;
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  if (code != 200 || !ready.startsWith("READY:") || ready.length() <= 6) {
    finishTask(FAILED, "Update konnte auf der Relaiskarte nicht vorbereitet werden."); vTaskDelete(nullptr); return;
  }
  const String target = ready.substring(6);
  for (size_t i = 0; i < target.length(); ++i) if (target[i] < '0' || target[i] > '9') {
    finishTask(FAILED, "Ungültige Zielpartition der Relaiskarte."); vTaskDelete(nullptr); return;
  }
  const String abortBody = "token=" + String(token_);
  for (size_t offset = 0; offset < size_; offset += 2048) {
    xSemaphoreTake(lock_, portMAX_DELAY); const bool canceled = cancelRequested_; xSemaphoreGive(lock_);
    if (canceled) { String reply; exchange(config_, "/api/ota/abort", &abortBody, reply); finishTask(CANCELED, "Update abgebrochen."); vTaskDelete(nullptr); return; }
    const size_t length = min(size_t(2048), size_ - offset);
    unsigned char encoded[2733]; size_t encodedLength = 0;
    if (mbedtls_base64_encode(encoded, sizeof(encoded), &encodedLength, image_ + offset, length) != 0) {
      String reply; exchange(config_, "/api/ota/abort", &abortBody, reply); finishTask(FAILED, "Datenblock konnte nicht vorbereitet werden."); vTaskDelete(nullptr); return;
    }
    const String body = "token=" + String(token_) + "&offset=" + String(offset) + "&data=" + encodeForm(String(reinterpret_cast<char*>(encoded)));
    int blockCode = -1;
    if (!requestWithRetry(config_, "/api/ota/chunk", body, "RECEIVED:" + String(offset + length), blockCode)) {
      String reply; exchange(config_, "/api/ota/abort", &abortBody, reply);
      char message[144];
      if (blockCode <= 0)
        snprintf(message, sizeof(message), "Übertragung bei %u von %u Bytes: Verbindungsfehler %d.", unsigned(offset), unsigned(size_), blockCode);
      else if (blockCode == 200)
        snprintf(message, sizeof(message), "Übertragung bei %u von %u Bytes: unerwartete Antwort.", unsigned(offset), unsigned(size_));
      else
        snprintf(message, sizeof(message), "Übertragung bei %u von %u Bytes: HTTP %d.", unsigned(offset), unsigned(size_), blockCode);
      finishTask(FAILED, message); vTaskDelete(nullptr); return;
    }
    xSemaphoreTake(lock_, portMAX_DELAY); sent_ = offset + length; xSemaphoreGive(lock_);
  }
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (cancelRequested_) { xSemaphoreGive(lock_); String reply; exchange(config_, "/api/ota/abort", &abortBody, reply); finishTask(CANCELED, "Update abgebrochen."); vTaskDelete(nullptr); return; }
  state_ = VERIFYING; xSemaphoreGive(lock_);
  // Bei verlorenem Abschluss-ACK keinesfalls erneut installieren: Neustart prüfen.
  String finishReply; const int finishCode = exchange(config_, "/api/ota/finish", &abortBody, finishReply);
  if (finishCode >= 400 && finishCode < 500) { finishTask(FAILED, "Relaiskarte hat die Firmware abgelehnt."); vTaskDelete(nullptr); return; }
  bool sawRestart = false;
  for (int i = 0; i < 30; ++i) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    String info;
    if (exchange(config_, "/api/ota/info", nullptr, info) != 200) { sawRestart = true; continue; }
    if (info.startsWith(String(MARKER) + ":") && info.endsWith(":PENDING")) sawRestart = true;
    if (info == String(MARKER) + ":" + target + ":READY") { finishTask(DONE, ""); vTaskDelete(nullptr); return; }
    if (sawRestart && info == before) { finishTask(FAILED, "Die Relaiskarte ist zur bisherigen Firmware zurückgekehrt."); vTaskDelete(nullptr); return; }
  }
  finishTask(UNKNOWN, "Neustart nicht bestätigt. Firmware und Relaiszustände prüfen."); vTaskDelete(nullptr);
}
