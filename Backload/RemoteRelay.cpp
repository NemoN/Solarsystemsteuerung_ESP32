#include "RemoteRelay.h"
#include "RemoteRelayProtocol.h"
#include "RelayResponse.h"
#include <WiFi.h>
#include <HTTPClient.h>

namespace {
bool validKey(const String& key) {
  if (key.length() != 32) return false;
  for (size_t i = 0; i < key.length(); ++i)
    if (!((key[i] >= '0' && key[i] <= '9') || (key[i] >= 'a' && key[i] <= 'f'))) return false;
  return true;
}
bool validAddress(const String& text) {
  if (text.isEmpty()) return true;
  IPAddress ip;
  return text.length() <= 15 && ip.fromString(text) && ip.toString() == text &&
    ip[0] != 0 && ip[0] != 127 && ip[0] < 224 && ip[3] != 0 && ip[3] != 255;
}
bool validLabel(const String& text) {
  if (!text.length() || text.length() > 48 || strlen(text.c_str()) != text.length()) return false;
  for (size_t i = 0; i < text.length(); ++i) if (static_cast<uint8_t>(text[i]) < 32) return false;
  return true;
}
bool compatibleNetwork(IPAddress target) {
  if (WiFi.status() != WL_CONNECTED || target == WiFi.localIP()) return false;
  const IPAddress local = WiFi.localIP(), mask = WiFi.subnetMask();
  for (int i = 0; i < 4; ++i) if ((target[i] & mask[i]) != (local[i] & mask[i])) return false;
  return true;
}
bool readResponse(HTTPClient& http, char* out, size_t capacity) {
  const int size = http.getSize();
  WiFiClient* stream = http.getStreamPtr();
  return size > 0 && stream && readRelayResponse(*stream, size_t(size), out, capacity,
    []() { return millis(); }, []() { vTaskDelay(pdMS_TO_TICKS(10)); });
}
bool exchange(const RemoteRelayConfig& cfg, unsigned id, bool value, uint8_t& mask) {
  IPAddress ip;
  if (!ip.fromString(cfg.address) || !compatibleNetwork(ip)) return false;
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(1500); http.setTimeout(1500); http.setReuse(false);
  String url = "http://" + String(cfg.address) + (id ? "/api/relay" : "/api/state");
  if (!http.begin(client, url)) return false;
  http.addHeader("X-Relay-Key", cfg.key);
  int code;
  if (id) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    code = http.POST("id=" + String(id) + "&value=" + String(value ? 1 : 0));
  } else code = http.GET();
  char body[8] = {};
  const int size = http.getSize();
  const bool complete = code == 200 && size >= 6 && size <= 7 && readResponse(http, body, sizeof(body));
  http.end();
  return complete && parseRemoteRelayState(body, size_t(size), mask);
}
bool exchangeVersion(const RemoteRelayConfig& cfg, char version[32]) {
  IPAddress ip;
  if (!ip.fromString(cfg.address) || !compatibleNetwork(ip)) return false;
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(1500); http.setTimeout(1500); http.setReuse(false);
  if (!http.begin(client, "http://" + String(cfg.address) + "/api/version")) return false;
  http.addHeader("X-Relay-Key", cfg.key);
  const int code = http.GET();
  char body[40] = {};
  const int size = http.getSize();
  const bool complete = code == 200 && size > 8 && size < 40 && readResponse(http, body, sizeof(body));
  http.end(); return complete && parseRemoteRelayVersion(body, size_t(size), version);
}
bool exchangeDiagnostics(const RemoteRelayConfig& cfg, RelayDiagnostics& diagnostics) {
  IPAddress ip;
  if (!ip.fromString(cfg.address) || !compatibleNetwork(ip)) return false;
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(1500); http.setTimeout(1500); http.setReuse(false);
  if (!http.begin(client, "http://" + String(cfg.address) + "/api/diagnostics")) return false;
  http.addHeader("X-Relay-Key", cfg.key);
  const int code = http.GET(), size = http.getSize();
  char body[101] = {};
  const bool complete = code == 200 && size > 0 && size <= 100 && readResponse(http, body, sizeof(body));
  http.end(); return complete && parseRelayDiagnostics(body, size_t(size), diagnostics);
}
// A lost reply can mean that the reboot has already been scheduled: never retry it.
RemoteRelay::RebootState rebootCard(const RemoteRelayConfig& cfg) {
  IPAddress ip;
  if (!ip.fromString(cfg.address) || !compatibleNetwork(ip)) return RemoteRelay::REBOOT_FAILED;
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(1500); http.setTimeout(2500); http.setReuse(false);
  if (!http.begin(client, "http://" + String(cfg.address) + "/api/reboot")) return RemoteRelay::REBOOT_FAILED;
  http.addHeader("X-Relay-Key", cfg.key);
  const int code = http.POST("");
  char body[11] = {};
  const bool complete = code == 200 && http.getSize() == 10 && readResponse(http, body, sizeof(body));
  http.end();
  if (complete && !strcmp(body, "REBOOT1:OK")) return RemoteRelay::REBOOT_SENT;
  if (code == 404) return RemoteRelay::REBOOT_UNSUPPORTED;
  return code < 0 || code == 200 || code >= 500 ? RemoteRelay::REBOOT_UNCERTAIN : RemoteRelay::REBOOT_FAILED;
}
// One request only: a lost reply may mean that the card has already reset.
RemoteRelay::ForgetState resetCard(const RemoteRelayConfig& cfg) {
  IPAddress ip;
  if (!ip.fromString(cfg.address) || !compatibleNetwork(ip)) return RemoteRelay::FORGET_FAILED;
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(1500); http.setTimeout(2500); http.setReuse(false);
  if (!http.begin(client, "http://" + String(cfg.address) + "/api/factory-reset")) return RemoteRelay::FORGET_FAILED;
  http.addHeader("X-Relay-Key", cfg.key);
  const int code = http.POST("");
  char body[10] = {};
  const bool complete = code == 200 && http.getSize() == 9 && readResponse(http, body, sizeof(body));
  http.end();
  if (complete && !strcmp(body, "RESET1:OK")) return RemoteRelay::FORGET_DONE;
  return code < 0 || code == 200 || code >= 500 ? RemoteRelay::FORGET_UNCERTAIN : RemoteRelay::FORGET_FAILED;
}
}
bool RemoteRelay::begin(const char* storageKey) {
  if (!storageKey || !storageKey[0] || strlen(storageKey) >= sizeof(storageKey_)) return false;
  strlcpy(storageKey_, storageKey, sizeof(storageKey_));
  lock_ = xSemaphoreCreateMutex();
  if (!lock_) return false;
  for (int i = 0; i < 4; ++i) snprintf(config_.labels[i], sizeof(config_.labels[i]), "Relais %d", i + 1);
  storageReady_ = preferences_.begin("remote-relay", false);
  RemoteRelayConfig saved = {};
  if (storageReady_ && preferences_.getBytesLength(storageKey_) == sizeof(saved) &&
      preferences_.getBytes(storageKey_, &saved, sizeof(saved)) == sizeof(saved) &&
      memchr(saved.address, 0, sizeof(saved.address)) && memchr(saved.key, 0, sizeof(saved.key)) &&
      validAddress(saved.address) && (!saved.address[0] || validKey(saved.key))) {
    bool ok = true;
    for (int i = 0; i < 4; ++i) if (!memchr(saved.labels[i], 0, sizeof(saved.labels[i])) || !validLabel(saved.labels[i])) ok = false;
    if (ok) config_ = saved;
  }
  return xTaskCreate(taskEntry, "remote-relay", 8192, this, 1, nullptr) == pdPASS;
}
RemoteRelaySnapshot RemoteRelay::snapshot() {
  RemoteRelaySnapshot result = {};
  if (!lock_) return result;
  xSemaphoreTake(lock_, portMAX_DELAY);
  result.config = config_; result.online = online_ && millis() - lastContact_ <= 6500 && WiFi.status() == WL_CONNECTED; result.busy = busy_; result.mask = mask_; result.forgetState = forgetState_; result.rebootState = rebootState_; if (result.online) { strlcpy(result.version, version_, sizeof(result.version)); result.diagnosticsAvailable = diagnosticsAvailable_; result.diagnostics = diagnostics_; }
  xSemaphoreGive(lock_); return result;
}
bool RemoteRelay::configure(const String& address, const String& key, const String labels[4]) {
  if (!lock_ || !storageReady_ || !validAddress(address)) return false;
  RemoteRelayConfig next = snapshot().config;
  const String useKey = key.isEmpty() ? String(next.key) : key;
  // Einen gespeicherten Schlüssel nie automatisch an eine andere Adresse senden.
  if (!address.isEmpty() && address != String(next.address) && key.isEmpty()) return false;
  if ((!address.isEmpty() && !validKey(useKey)) || (!key.isEmpty() && !validKey(key))) return false;
  for (int i = 0; i < 4; ++i) if (!validLabel(labels[i])) return false;
  address.toCharArray(next.address, sizeof(next.address));
  useKey.toCharArray(next.key, sizeof(next.key));
  for (int i = 0; i < 4; ++i) labels[i].toCharArray(next.labels[i], sizeof(next.labels[i]));
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (busy_) { xSemaphoreGive(lock_); return false; }
  if (preferences_.putBytes(storageKey_, &next, sizeof(next)) != sizeof(next)) { xSemaphoreGive(lock_); return false; }
  const bool sameConnection = !strcmp(config_.address, next.address) && !strcmp(config_.key, next.key);
  config_ = next; if (!sameConnection) { online_ = false; version_[0] = 0; versionChecked_ = false; diagnosticsAvailable_ = diagnosticsChecked_ = false; } forgetState_ = FORGET_IDLE; rebootState_ = REBOOT_IDLE;
  xSemaphoreGive(lock_); return true;
}
bool RemoteRelay::forget() {
  if (!lock_ || !storageReady_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  const bool accepted = config_.address[0] && online_ && millis() - lastContact_ <= 6500 && WiFi.status() == WL_CONNECTED && !busy_;
  if (accepted) { pendingId_ = 255; busy_ = true; forgetState_ = FORGET_PENDING; }
  xSemaphoreGive(lock_); return accepted;
}
bool RemoteRelay::reboot() {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  const bool accepted = config_.address[0] && online_ && millis() - lastContact_ <= 6500 && WiFi.status() == WL_CONNECTED && !busy_;
  if (accepted) { pendingId_ = 254; busy_ = true; rebootState_ = REBOOT_PENDING; }
  xSemaphoreGive(lock_); return accepted;
}
bool RemoteRelay::clearAssociation() {
  if (!lock_ || !storageReady_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (busy_ || !config_.address[0]) { xSemaphoreGive(lock_); return false; }
  RemoteRelayConfig cleared = {};
  for (int i = 0; i < 4; ++i) snprintf(cleared.labels[i], sizeof(cleared.labels[i]), "Relais %d", i + 1);
  const bool saved = preferences_.putBytes(storageKey_, &cleared, sizeof(cleared)) == sizeof(cleared);
  if (saved) { config_ = cleared; online_ = false; mask_ = 0; version_[0] = 0; versionChecked_ = false; diagnosticsAvailable_ = diagnosticsChecked_ = false; forgetState_ = FORGET_IDLE; rebootState_ = REBOOT_IDLE; }
  xSemaphoreGive(lock_); return saved;
}
bool RemoteRelay::command(unsigned id, bool on) {
  if (!lock_ || id < 1 || id > 4) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  bool accepted = online_ && millis() - lastContact_ <= 6500 && WiFi.status() == WL_CONNECTED && !busy_;
  if (accepted) { pendingId_ = id; pendingValue_ = on; busy_ = true; }
  xSemaphoreGive(lock_); return accepted;
}
void RemoteRelay::taskEntry(void* arg) { static_cast<RemoteRelay*>(arg)->task(); }
void RemoteRelay::task() {
  uint32_t lastPoll = 0;
  for (;;) {
    RemoteRelayConfig cfg;
    unsigned id; bool value;
    xSemaphoreTake(lock_, portMAX_DELAY);
    cfg = config_; id = pendingId_; value = pendingValue_; pendingId_ = 0;
    xSemaphoreGive(lock_);
    if (id == 254) {
      const RebootState result = rebootCard(cfg);
      xSemaphoreTake(lock_, portMAX_DELAY);
      rebootState_ = result; rebootSentAt_ = millis();
      if (result == REBOOT_SENT || result == REBOOT_UNCERTAIN) { online_ = false; version_[0] = 0; versionChecked_ = false; diagnosticsAvailable_ = diagnosticsChecked_ = false; }
      busy_ = false;
      xSemaphoreGive(lock_); lastPoll = millis();
    } else if (id == 255) {
      const ForgetState result = resetCard(cfg);
      xSemaphoreTake(lock_, portMAX_DELAY);
      if (result == FORGET_DONE) {
        RemoteRelayConfig cleared = {};
        for (int i = 0; i < 4; ++i) snprintf(cleared.labels[i], sizeof(cleared.labels[i]), "Relais %d", i + 1);
        if (preferences_.putBytes(storageKey_, &cleared, sizeof(cleared)) == sizeof(cleared)) { config_ = cleared; online_ = false; mask_ = 0; version_[0] = 0; versionChecked_ = false; diagnosticsAvailable_ = diagnosticsChecked_ = false; forgetState_ = FORGET_DONE; }
        else { online_ = false; forgetState_ = FORGET_SAVE_FAILED; }
      } else forgetState_ = result;
      busy_ = false;
      xSemaphoreGive(lock_); lastPoll = millis();
    } else if (id || millis() - lastPoll >= 2000) {
      uint8_t nextMask = 0;
      const bool ok = cfg.address[0] && cfg.key[0] && exchange(cfg, id, value, nextMask);
      bool checkVersion = false, checkDiagnostics = false;
      xSemaphoreTake(lock_, portMAX_DELAY);
      // Konfiguration kann sich während einer Abfrage ändern.
      if (memcmp(&cfg, &config_, sizeof(cfg)) == 0) {
        if (ok) {
          online_ = true; mask_ = nextMask; lastContact_ = millis();
          if (rebootState_ == REBOOT_SENT) rebootState_ = REBOOT_ONLINE;
          if (!id && (!versionChecked_ || millis() - lastVersionCheck_ >= 10000)) { versionChecked_ = true; lastVersionCheck_ = millis(); checkVersion = true; }
          if (!id && !checkVersion && (!diagnosticsChecked_ || millis() - lastDiagnosticsCheck_ >= 30000)) { diagnosticsChecked_ = true; lastDiagnosticsCheck_ = millis(); checkDiagnostics = true; }
        } else if (id || millis() - lastContact_ > 6500) {
          if (rebootState_ == REBOOT_SENT && uint32_t(millis() - rebootSentAt_) >= 30000) rebootState_ = REBOOT_UNCERTAIN;
          // Einzelne fehlgeschlagene Statusabfragen überbrücken, aber einen
          // nicht bestätigten Schaltauftrag sofort als unbekannt melden.
          online_ = false; version_[0] = 0; versionChecked_ = false; diagnosticsAvailable_ = diagnosticsChecked_ = false;
        }
      }
      if (id) busy_ = false;
      xSemaphoreGive(lock_); lastPoll = millis();
      if (checkVersion) {
        char nextVersion[32] = {}; exchangeVersion(cfg, nextVersion);
        xSemaphoreTake(lock_, portMAX_DELAY);
        if (online_ && !strcmp(cfg.address, config_.address) && !strcmp(cfg.key, config_.key)) strlcpy(version_, nextVersion, sizeof(version_));
        xSemaphoreGive(lock_);
      }
      if (checkDiagnostics) {
        RelayDiagnostics nextDiagnostics = {};
        const bool valid = exchangeDiagnostics(cfg, nextDiagnostics);
        xSemaphoreTake(lock_, portMAX_DELAY);
        if (online_ && !strcmp(cfg.address, config_.address) && !strcmp(cfg.key, config_.key)) { diagnosticsAvailable_ = valid; if (valid) diagnostics_ = nextDiagnostics; }
        xSemaphoreGive(lock_);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
