#include "RelayProvisioning.h"
#include "RelayPairAuth.h"
#include <HTTPClient.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <ctype.h>

namespace {
bool sameNetwork(const IPAddress& a, const IPAddress& b, const IPAddress& mask) {
  for (int i = 0; i < 4; ++i) if ((a[i] & mask[i]) != (b[i] & mask[i])) return false;
  return true;
}
bool suffixMatches(const char* id, const char* suffix) {
  if (!relayPairHex(id, 12) || !suffix || strlen(suffix) != 4) return false;
  for (int i = 0; i < 4; ++i)
    if (toupper(static_cast<unsigned char>(id[i+8])) != suffix[i]) return false;
  return true;
}
String base64Text(const char* input) {
  unsigned char output[93] = {}; size_t length = 0;
  if (mbedtls_base64_encode(output, sizeof(output), &length, reinterpret_cast<const uint8_t*>(input), strlen(input)) != 0) return String();
  return String(reinterpret_cast<char*>(output));
}
}
bool RelayProvisioning::begin(RemoteRelay* boards, unsigned count, RelayDiscovery* discovery) {
  if (!boards || !count || !discovery) return false;
  lock_ = xSemaphoreCreateMutex(); if (!lock_) return false;
  boards_ = boards; count_ = count; discovery_ = discovery;
  server_.on("/hello", HTTP_GET, [this](AsyncWebServerRequest* request){ handleHello(request); });
  server_.on("/pair", HTTP_POST, [this](AsyncWebServerRequest* request){ handleClaim(request); });
  server_.onNotFound([](AsyncWebServerRequest* request){ request->send(404, "text/plain", "Not found"); });
  return true;
}
bool RelayProvisioning::start(unsigned board, const String& ssid, const char* homeSsid, const char* homePassword) {
  if (!lock_ || board >= count_ || WiFi.status() != WL_CONNECTED || !homeSsid || !homeSsid[0] || !homePassword ||
      strlen(homeSsid) > 32 || strlen(homePassword) > 63 || ssid.length() != 22 || !ssid.startsWith("Relaiserweiterung-") ||
      boards_[board].snapshot().config.address[0] || verifyState_.load() == 1) return false;
  char suffix[5] = {};
  for (int i = 0; i < 4; ++i) {
    const char c = ssid[18 + i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
    suffix[i] = c;
  }
  xSemaphoreTake(lock_, portMAX_DELAY);
  const bool inProgress = phase_ == WAITING || phase_ == CLAIMED || phase_ == SEARCHING || phase_ == VERIFYING;
  xSemaphoreGive(lock_); if (inProgress) return false;
  if ((WiFi.getMode() & WIFI_AP) && WiFi.softAPgetStationNum()) return false;
  const IPAddress candidates[] = {IPAddress(172,30,254,1), IPAddress(10,254,253,1), IPAddress(192,168,254,1)};
  IPAddress pairingIp; bool found = false;
  for (const auto& candidate : candidates) {
    if (!sameNetwork(candidate, WiFi.localIP(), WiFi.subnetMask()) && !sameNetwork(candidate, IPAddress(192,168,4,1), IPAddress(255,255,255,0))) { pairingIp = candidate; found = true; break; }
  }
  if (!found) return false;
  pairKey_.clear();
  if (!pairKey_.generate(pairPublic_)) { pairKey_.clear(); return false; }
  if (WiFi.getMode() & WIFI_AP) WiFi.enableAP(false);
  if (!WiFi.enableAP(true) || !WiFi.softAPConfig(pairingIp, pairingIp, IPAddress(255,255,255,0))) { WiFi.enableAP(false); pairKey_.clear(); return false; }
  const String pairSsid = "Backload-Pair-" + String(suffix);
  const String publicPassword = "relaypair" + String(suffix);
  if (!WiFi.softAP(pairSsid.c_str(), publicPassword.c_str(), WiFi.channel(), false, 1)) { WiFi.enableAP(false); pairKey_.clear(); return false; }
  xSemaphoreTake(lock_, portMAX_DELAY);
  board_ = board; strlcpy(expectedSuffix_, suffix, sizeof(expectedSuffix_));
  strlcpy(homeSsid_, homeSsid, sizeof(homeSsid_)); strlcpy(homePassword_, homePassword, sizeof(homePassword_));
  deviceId_[0] = deviceKey_[0] = 0; verifyState_.store(0); startedAt_ = millis();
  strlcpy(message_, "IO0 an der Relaiserweiterung eine Sekunde drücken.", sizeof(message_)); phase_ = WAITING;
  xSemaphoreGive(lock_);
  server_.begin(); return true;
}
void RelayProvisioning::handleHello(AsyncWebServerRequest* request) {
  if (request->client()->localIP() != WiFi.softAPIP() || request->params() != 0) { request->send(403, "text/plain", "Forbidden"); return; }
  String response;
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (phase_ == WAITING || phase_ == CLAIMED) response = pairPublic_;
  xSemaphoreGive(lock_);
  if (!response.length()) { request->send(403, "text/plain", "Forbidden"); return; }
  AsyncWebServerResponse* reply = request->beginResponse(200, "text/plain", response);
  reply->addHeader("Cache-Control", "no-store"); request->send(reply);
}
void RelayProvisioning::handleClaim(AsyncWebServerRequest* request) {
  if (request->client()->localIP() != WiFi.softAPIP() || !request->hasParam("id", true) ||
      !request->hasParam("pub", true) || !request->hasParam("proof", true) || request->params() != 3) {
    request->send(403, "text/plain", "Forbidden"); return;
  }
  const String id = request->getParam("id", true)->value(), pub = request->getParam("pub", true)->value(), proof = request->getParam("proof", true)->value();
  String response;
  uint8_t sessionKey[32] = {}; char key[33] = {}, envelope[441] = {};
  xSemaphoreTake(lock_, portMAX_DELAY);
  const String inAad = "relay-key:" + id, outAad = "relay-wifi:" + id;
  bool eligible = (phase_ == WAITING || phase_ == CLAIMED) && suffixMatches(id.c_str(), expectedSuffix_) &&
    pairKey_.shared(pub.c_str(), pairPublic_, true, sessionKey) &&
    relaySecureOpen(sessionKey, inAad.c_str(), proof.c_str(), key, sizeof(key)) && relayPairHex(key, 32) &&
    (phase_ == WAITING || (id == deviceId_ && !strcmp(key, deviceKey_)));
  if (eligible) {
    const String credentials = "PAIR1:" + base64Text(homeSsid_) + ":" + base64Text(homePassword_);
    eligible = relaySecureSeal(sessionKey, outAad.c_str(), credentials.c_str(), envelope, sizeof(envelope));
  }
  if (eligible) {
    if (phase_ == WAITING) {
      id.toCharArray(deviceId_, sizeof(deviceId_)); strlcpy(deviceKey_, key, sizeof(deviceKey_));
      claimedAt_ = millis(); phase_ = CLAIMED;
      strlcpy(message_, "Erweiterung übernimmt das Heim-WLAN.", sizeof(message_));
    }
    response = envelope;
  }
  memset(sessionKey, 0, sizeof(sessionKey)); memset(key, 0, sizeof(key)); memset(envelope, 0, sizeof(envelope));
  xSemaphoreGive(lock_);
  if (!eligible) { request->send(403, "text/plain", "Forbidden"); return; }
  AsyncWebServerResponse* reply = request->beginResponse(200, "text/plain", response);
  reply->addHeader("Cache-Control", "no-store"); request->send(reply);
}
void RelayProvisioning::stopPairAp() {
  server_.end(); WiFi.enableAP(false);
}
void RelayProvisioning::finish(Phase phase, const char* message) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  phase_ = phase; strlcpy(message_, message, sizeof(message_));
  pairKey_.clear(); memset(pairPublic_, 0, sizeof(pairPublic_)); memset(deviceKey_, 0, sizeof(deviceKey_));
  memset(homeSsid_, 0, sizeof(homeSsid_)); memset(homePassword_, 0, sizeof(homePassword_));
  xSemaphoreGive(lock_);
}
bool RelayProvisioning::cancel() {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  const Phase current = phase_;
  xSemaphoreGive(lock_);
  if (current != WAITING && current != CLAIMED && current != SEARCHING && current != VERIFYING) return false;
  if (current == WAITING || current == CLAIMED) stopPairAp();
  finish(FAILED, "Einrichtung abgebrochen. Die Erweiterung kann bereits im Heimnetz sein.");
  return true;
}
bool RelayProvisioning::active() {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY); const bool value = phase_ == WAITING || phase_ == CLAIMED || phase_ == SEARCHING || phase_ == VERIFYING; xSemaphoreGive(lock_); return value;
}
bool RelayProvisioning::apActive() {
  if (!lock_) return false;
  xSemaphoreTake(lock_, portMAX_DELAY); const bool value = phase_ == WAITING || phase_ == CLAIMED; xSemaphoreGive(lock_); return value;
}
RelayProvisionSnapshot RelayProvisioning::snapshot() {
  RelayProvisionSnapshot result = {};
  if (!lock_) return result;
  xSemaphoreTake(lock_, portMAX_DELAY);
  static const char* names[] = {"idle", "waiting", "claimed", "searching", "verifying", "done", "failed"};
  strlcpy(result.state, names[phase_], sizeof(result.state)); strlcpy(result.message, message_, sizeof(result.message)); result.board = board_;
  xSemaphoreGive(lock_); return result;
}
void RelayProvisioning::verifyTask(void* argument) {
  auto* self = static_cast<RelayProvisioning*>(argument);
  WiFiClient client; HTTPClient http; http.setConnectTimeout(1200); http.setTimeout(1500);
  String response; int code = -1;
  if (http.begin(client, "http://" + String(self->verifyAddress_) + "/api/identity")) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    code = http.POST("nonce=" + String(self->verifyNonce_));
    if (code == 200 && http.getSize() == 64) response = http.getString();
    http.end();
  }
  char expected[65] = {};
  const bool valid = code == 200 && relayPairProof(self->verifyKey_, self->verifyNonce_, expected) && relayPairEqual(response.c_str(), expected, 64);
  memset(self->verifyKey_, 0, sizeof(self->verifyKey_)); self->verifyState_.store(valid ? 2 : 3);
  vTaskDelete(nullptr);
}
void RelayProvisioning::loop() {
  if (!lock_) return;
  xSemaphoreTake(lock_, portMAX_DELAY);
  Phase phase = phase_; const uint32_t started = startedAt_, claimed = claimedAt_; char id[13]; strlcpy(id, deviceId_, sizeof(id));
  xSemaphoreGive(lock_);
  if (phase != WAITING && phase != CLAIMED && phase != SEARCHING && phase != VERIFYING) return;
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED || now - started >= 150000) {
    if (phase == WAITING || phase == CLAIMED) stopPairAp();
    finish(FAILED, "Einrichtung nicht abgeschlossen. WLAN und Erweiterung prüfen."); return;
  }
  if (phase == CLAIMED && now - claimed >= 12000) {
    stopPairAp();
    xSemaphoreTake(lock_, portMAX_DELAY); phase_ = SEARCHING; pairKey_.clear(); memset(pairPublic_, 0, sizeof(pairPublic_));
    memset(homePassword_, 0, sizeof(homePassword_)); strlcpy(message_, "Erweiterung wird im Heimnetz gesucht.", sizeof(message_)); xSemaphoreGive(lock_);
    lastSearch_ = now - 5000; phase = SEARCHING;
  }
  if (phase == SEARCHING) {
    const RelayDiscoverySnapshot found = discovery_->snapshot();
    const RelayDiscoveryEntry* match = nullptr; bool ambiguous = false;
    for (size_t i = 0; i < found.results.size(); ++i) if (!strcmp(found.results[i].id, id)) {
      if (match && strcmp(match->address, found.results[i].address)) ambiguous = true;
      match = &found.results[i];
    }
    if (ambiguous) { finish(FAILED, "Mehrere Adressen für dieselbe Erweiterung gefunden."); return; }
    if (match && !found.searching) {
      for (unsigned i = 0; i < count_; ++i) if (i != board_) {
        const RemoteRelaySnapshot other = boards_[i].snapshot();
        if (other.config.address[0] && (!strcmp(other.config.address, match->address) || relayPairEqual(other.config.key, deviceKey_, 32))) {
          finish(FAILED, "Die Erweiterung ist bereits eingerichtet."); return;
        }
      }
      uint8_t random[16]; esp_fill_random(random, sizeof(random)); static const char hex[] = "0123456789abcdef";
      for (size_t i = 0; i < sizeof(random); ++i) { verifyNonce_[2*i] = hex[random[i] >> 4]; verifyNonce_[2*i+1] = hex[random[i] & 15]; }
      verifyNonce_[32] = 0; strlcpy(verifyAddress_, match->address, sizeof(verifyAddress_)); strlcpy(verifyKey_, deviceKey_, sizeof(verifyKey_));
      verifyState_.store(1);
      if (xTaskCreate(verifyTask, "relay-identity", 6144, this, 1, nullptr) != pdPASS) { verifyState_.store(0); finish(FAILED, "Identitätsprüfung konnte nicht gestartet werden."); return; }
      xSemaphoreTake(lock_, portMAX_DELAY); phase_ = VERIFYING; strlcpy(message_, "Erweiterung wird überprüft.", sizeof(message_)); xSemaphoreGive(lock_);
    } else if (!found.searching && now - lastSearch_ >= 5000) { discovery_->search(); lastSearch_ = now; }
  } else if (phase == VERIFYING && verifyState_.load() != 1) {
    if (verifyState_.load() != 2) { verifyState_.store(0); finish(FAILED, "Identität der Relaiserweiterung konnte nicht bestätigt werden."); return; }
    const RemoteRelaySnapshot current = boards_[board_].snapshot();
    if (current.config.address[0]) { verifyState_.store(0); finish(FAILED, "Der gewählte Platz ist bereits belegt."); return; }
    String labels[4]; for (int i = 0; i < 4; ++i) labels[i] = current.config.labels[i];
    if (!boards_[board_].configure(verifyAddress_, deviceKey_, labels)) { verifyState_.store(0); finish(FAILED, "Erweiterung konnte nicht gespeichert werden."); return; }
    verifyState_.store(0); finish(DONE, "Relaiserweiterung eingerichtet und verbunden.");
  }
}
