#include "RelayOta.h"
#include <mbedtls/base64.h>

namespace {
bool hexString(const String& text, size_t length) {
  if (text.length() != length) return false;
  for (size_t i = 0; i < length; ++i)
    if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
  return true;
}
bool number(const String& text, uint32_t& result) {
  if (!text.length() || text.length() > 10) return false;
  uint64_t n = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    n = n * 10 + text[i] - '0';
    if (n > UINT32_MAX) return false;
  }
  result = static_cast<uint32_t>(n); return true;
}
void hexDigest(const uint8_t bytes[32], char out[65]) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) { out[2*i] = hex[bytes[i] >> 4]; out[2*i+1] = hex[bytes[i] & 15]; }
  out[64] = 0;
}
}
void RelayOta::cancel() {
  if (Update.isRunning()) Update.abort();
  if (hashReady_) { mbedtls_sha256_free(&hash_); hashReady_ = false; }
  active_ = false; token_[0] = 0;
}
bool RelayOta::matches(WebServer& server) const {
  return active_ && server.arg("token") == token_;
}
void RelayOta::prepare(WebServer& server) {
  const String token = server.arg("token"), digest = server.arg("sha256"); uint32_t size = 0;
  if (server.args() != 3 || !hexString(token, 32) || !hexString(digest, 64) || !number(server.arg("size"), size)) {
    server.send(400, "text/plain", "Invalid update request"); return;
  }
  if (active_ && token == token_ && size == size_ && digest == expected_) {
    startedAt_ = millis(); server.send(200, "text/plain", "READY:" + String(targetAddress_)); return;
  }
  if (active_ || restartPending_) { server.send(409, "text/plain", "Update busy"); return; }
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (!next || size < 1024 || size > next->size) { server.send(400, "text/plain", "Image does not fit"); return; }
  if (!Update.begin(size, U_FLASH)) { lastError_ = WRITE_FAILED; server.send(500, "text/plain", "Update begin failed"); return; }
  mbedtls_sha256_init(&hash_); hashReady_ = true;
  if (mbedtls_sha256_starts_ret(&hash_, 0) != 0) { lastError_ = WRITE_FAILED; cancel(); server.send(500, "text/plain", "Hash start failed"); return; }
  token.toCharArray(token_, sizeof(token_)); digest.toCharArray(expected_, sizeof(expected_));
  size_ = size; targetAddress_ = next->address; received_ = lastLength_ = markerMatch_ = 0; markerSeen_ = false;
  startedAt_ = millis(); active_ = true;
  server.send(200, "text/plain", "READY:" + String(targetAddress_));
}
void RelayOta::chunk(WebServer& server) {
  if (!matches(server)) { server.send(409, "text/plain", "No matching update"); return; }
  uint32_t offset = 0;
  const String data = server.arg("data");
  static uint8_t decoded[2048]; size_t length = 0;
  if (server.args() != 3 || !number(server.arg("offset"), offset) || data.length() > 2732 || !data.length() ||
      mbedtls_base64_decode(decoded, sizeof(decoded), &length, reinterpret_cast<const uint8_t*>(data.c_str()), data.length()) != 0 || !length) {
    lastError_ = INVALID_DATA; cancel(); server.send(400, "text/plain", "Invalid chunk"); return;
  }
  uint8_t digest[32];
  if (mbedtls_sha256_ret(decoded, length, digest, 0) != 0) { lastError_ = WRITE_FAILED; cancel(); server.send(500, "text/plain", "Hash failed"); return; }
  if (lastLength_ && offset == lastOffset_ && length == lastLength_ && memcmp(digest, lastHash_, 32) == 0) {
    startedAt_ = millis(); server.send(200, "text/plain", "RECEIVED:" + String(received_)); return;
  }
  if (offset != received_ || length > size_ - received_ ||
      (!received_ && (length < 24 || decoded[0] != 0xe9 || decoded[12] != 0 || decoded[13] != 0))) {
    lastError_ = INVALID_DATA; cancel(); server.send(400, "text/plain", "Invalid offset or image target"); return;
  }
  for (size_t i = 0; i < length && !markerSeen_; ++i) {
    if (decoded[i] == static_cast<uint8_t>(RELAY_FIRMWARE_MARKER[markerMatch_])) {
      if (++markerMatch_ == sizeof(RELAY_FIRMWARE_MARKER) - 1) markerSeen_ = true;
    } else markerMatch_ = decoded[i] == static_cast<uint8_t>(RELAY_FIRMWARE_MARKER[0]) ? 1 : 0;
  }
  if (mbedtls_sha256_update_ret(&hash_, decoded, length) != 0 || Update.write(decoded, length) != length) {
    lastError_ = WRITE_FAILED; cancel(); server.send(500, "text/plain", "Write failed"); return;
  }
  lastOffset_ = offset; lastLength_ = length; memcpy(lastHash_, digest, 32); received_ += length; startedAt_ = millis();
  server.send(200, "text/plain", "RECEIVED:" + String(received_));
}
void RelayOta::finish(WebServer& server) {
  if (!matches(server) || server.args() != 1) { server.send(409, "text/plain", "No matching update"); return; }
  uint8_t digest[32]; char hex[65] = {};
  const bool goodHash = mbedtls_sha256_finish_ret(&hash_, digest) == 0;
  if (goodHash) hexDigest(digest, hex);
  if (received_ != size_ || !markerSeen_ || !goodHash || strcmp(hex, expected_) != 0 || !Update.end()) {
    lastError_ = INVALID_IMAGE; cancel(); server.send(400, "text/plain", "Incomplete or invalid firmware"); return;
  }
  if (hashReady_) { mbedtls_sha256_free(&hash_); hashReady_ = false; }
  active_ = false; restartPending_ = true; restartAt_ = millis();
  server.send(200, "text/plain", "RESTARTING");
}
void RelayOta::abort(WebServer& server) {
  if (!matches(server) || server.args() != 1) { server.send(409, "text/plain", "No matching update"); return; }
  lastError_ = ABORTED; cancel(); server.send(200, "text/plain", "ABORTED");
}
void RelayOta::info(WebServer& server, bool bootPending) {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!running) { server.send(503, "text/plain", "Partition unknown"); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", String(RELAY_FIRMWARE_MARKER) + ":" + String(running->address) + (bootPending ? ":PENDING" : ":READY"));
}
void RelayOta::loop() {
  const uint32_t now = millis();
  if (active_ && now - startedAt_ >= 45000) { lastError_ = TIMEOUT; cancel(); }
  if (restartPending_ && now - restartAt_ >= 1500) ESP.restart();
}
