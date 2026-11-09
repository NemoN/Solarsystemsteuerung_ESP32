// WLAN-Relaiskarte RBS16467. HIGH schaltet laut Datenblatt das Relais ein.
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <mbedtls/base64.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include "RelayState.h"
#include "RelayOta.h"
#include "RelayPairAuth.h"
#include "RelayPairSecure.h"
#include "RelayFactoryReset.h"

constexpr uint8_t RELAY_PINS[] = {32, 33, 25, 26};
constexpr char RELAY_FIRMWARE_VERSION[] = "2.7.1";
constexpr uint32_t RETRY_MS = 30000;
struct Config { char ssid[33]; char password[64]; char key[33]; char apPassword[17]; };
Config config = {};
Preferences prefs;
WebServer server(80);
bool storeReady = false, apActive = false, connecting = false, wasConnected = false, reconnect = false;
bool mdnsActive = false, pairConnecting = false;
bool pairButtonReady = false, pairButtonHeld = false, pairWindow = false;
uint32_t pairButtonAt = 0, pairWindowAt = 0;
uint32_t lastMdnsAttempt = 0, lastPairScan = 0, lastPairClaim = 0;
char relayHostname[24] = {};
uint32_t connectAt = 0, lastAttempt = 0, reconnectAt = 0;
RelayState relayState;
RelayOta relayOta;
bool bootVerificationPending = false;
bool factoryResetPending = false;
uint32_t factoryResetAt = 0;
uint32_t bootVerificationStarted = 0;
// Neue OTA-Firmware erst nach der eigenen Startprüfung bestätigen.
extern "C" bool verifyRollbackLater() { return true; }

const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Relaiskarte einrichten</title><style>body{font:16px system-ui;max-width:35rem;margin:2rem auto;padding:0 1rem;color:#19353a}label{display:block;margin:1rem 0}input{display:block;box-sizing:border-box;width:100%;padding:.7rem;font:inherit}button{padding:.8rem;font:inherit}</style><h1>Relaiskarte einrichten</h1><p>Das 2,4-GHz-WLAN eintragen. Den Verbindungsschlüssel und die IP-Adresse zeigt der serielle Monitor mit 115200 Baud.</p><form method="post" action="/setup"><label>WLAN-Name<input name="ssid" maxlength="32" required></label><label>WLAN-Passwort<input name="password" type="password" maxlength="63"></label><button>Speichern und verbinden</button></form></html>)HTML";

bool validWifi(const String& ssid, const String& password) {
  return ssid.length() && ssid.length() <= 32 && strlen(ssid.c_str()) == ssid.length() &&
    strlen(password.c_str()) == password.length() &&
    (password.isEmpty() || (password.length() >= 8 && password.length() <= 63));
}
void randomHex(char* output, size_t characters) {
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < characters; i++) output[i] = digits[esp_random() & 15];
  output[characters] = '\0';
}
void setAllOff() {
  for (uint8_t pin : RELAY_PINS) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
  relayState.restore(0);
}
void startAp() {
  if (apActive || !storeReady) return;
  WiFi.mode(WIFI_AP_STA);
  char name[32]; snprintf(name, sizeof(name), "Relaiserweiterung-%04X", unsigned(ESP.getEfuseMac() & 0xffff));
  apActive = WiFi.softAP(name, config.apPassword);
  if (apActive) { Serial.printf("Einrichtungs-WLAN: %s\nPasswort: %s\nAdresse: http://", name, config.apPassword); Serial.println(WiFi.softAPIP()); }
}
void connectWifi() {
  if (!config.ssid[0]) { startAp(); return; }
  WiFi.begin(config.ssid, config.password); connecting = true; connectAt = lastAttempt = millis();
}
bool authorized() {
  const String key = server.header("X-Relay-Key");
  if (!storeReady || key.length() != 32) return false;
  unsigned char diff = 0;
  for (size_t i = 0; i < 32; ++i) diff |= static_cast<unsigned char>(key[i] ^ config.key[i]);
  return diff == 0;
}
void sendState() {
  if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", "RLY1:" + String(relayState.mask()));
}
void changeRelay() {
  if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; }
  if (relayOta.active() || bootVerificationPending || factoryResetPending) { server.send(409, "text/plain", "Update busy"); return; }
  const String id = server.arg("id"), value = server.arg("value");
  if (id.length() != 1 || id[0] < '1' || id[0] > '4' || (value != "0" && value != "1")) {
    server.send(400, "text/plain", "Invalid command"); return;
  }
  const uint8_t index = id[0] - '1';
  const bool on = value == "1";
  const bool changed = relayState.set(index, on,
    [](uint8_t next) { return prefs.putUChar("relaymask", next) == 1 && prefs.getUChar("relaymask", 255) == next; },
    [](unsigned channel, bool enabled) { digitalWrite(RELAY_PINS[channel], enabled ? HIGH : LOW); });
  if (!changed) { server.send(503, "text/plain", "Relay state could not be saved"); return; }
  sendState();
}
bool importPairedWifi(const String& reply) {
  if (!reply.startsWith("PAIR1:") || reply.length() > 160) return false;
  const int separator = reply.indexOf(':', 6);
  if (separator < 0) return false;
  const String encodedSsid = reply.substring(6, separator), encodedPassword = reply.substring(separator + 1);
  uint8_t ssid[33] = {}, password[64] = {}; size_t ssidLength = 0, passwordLength = 0;
  if (mbedtls_base64_decode(ssid, 32, &ssidLength, reinterpret_cast<const uint8_t*>(encodedSsid.c_str()), encodedSsid.length()) != 0 ||
      mbedtls_base64_decode(password, 63, &passwordLength, reinterpret_cast<const uint8_t*>(encodedPassword.c_str()), encodedPassword.length()) != 0 ||
      !ssidLength || memchr(ssid, 0, ssidLength) || memchr(password, 0, passwordLength)) return false;
  ssid[ssidLength] = 0; password[passwordLength] = 0;
  if (!validWifi(String(reinterpret_cast<char*>(ssid)), String(reinterpret_cast<char*>(password)))) return false;
  Config next = config; memcpy(next.ssid, ssid, ssidLength + 1); memcpy(next.password, password, passwordLength + 1);
  if (prefs.putBytes("config", &next, sizeof(next)) != sizeof(next)) return false;
  config = next; return true;
}
void claimPairing() {
  WiFiClient client; HTTPClient http; http.setConnectTimeout(2500); http.setTimeout(4000);
  const String base = "http://" + WiFi.gatewayIP().toString() + ":8088";
  if (!http.begin(client, base + "/hello")) { Serial.println("Kopplung: Verbindung zum Haupt-ESP konnte nicht vorbereitet werden."); return; }
  const int helloCode = http.GET();
  String mainPublic;
  if (helloCode == 200 && http.getSize() == 130) mainPublic = http.getString();
  http.end();
  if (mainPublic.length() != 130) { Serial.printf("Kopplung: Schlüsselaustausch nicht erreichbar (HTTP %d).\n", helloCode); return; }
  RelayPairKey ownKey; char ownPublic[131] = {}, proof[121] = {}, reply[193] = {};
  uint8_t sessionKey[32] = {};
  const String id = relayHostname + 7, inAad = "relay-key:" + id, outAad = "relay-wifi:" + id;
  if (!ownKey.generate(ownPublic) || !ownKey.shared(mainPublic.c_str(), ownPublic, false, sessionKey) ||
      !relaySecureSeal(sessionKey, inAad.c_str(), config.key, proof, sizeof(proof))) { Serial.println("Kopplung: Schlüsselaustausch fehlgeschlagen."); memset(sessionKey, 0, sizeof(sessionKey)); return; }
  if (!http.begin(client, base + "/pair")) { Serial.println("Kopplung: Anfrage konnte nicht vorbereitet werden."); memset(sessionKey, 0, sizeof(sessionKey)); return; }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const int code = http.POST("id=" + id + "&pub=" + String(ownPublic) + "&proof=" + String(proof));
  String encrypted;
  if (code == 200 && http.getSize() >= 56 && http.getSize() <= 440) encrypted = http.getString();
  http.end();
  const bool valid = code == 200 && relaySecureOpen(sessionKey, outAad.c_str(), encrypted.c_str(), reply, sizeof(reply));
  memset(sessionKey, 0, sizeof(sessionKey)); memset(proof, 0, sizeof(proof));
  if (!valid || !importPairedWifi(String(reply))) { Serial.printf("Kopplung: Anfrage oder WLAN-Übernahme fehlgeschlagen (HTTP %d).\n", code); memset(reply, 0, sizeof(reply)); return; }
  memset(reply, 0, sizeof(reply)); pairWindow = false;
  Serial.println("Heim-WLAN übernommen. Verbindung wird aufgebaut.");
  pairConnecting = false; connecting = false; WiFi.disconnect(false, true); connectWifi();
}
void searchPairingAp(uint32_t now) {
  if (!pairWindow || config.ssid[0] || pairConnecting || connecting) return;
  if (now - lastPairScan < 8000) return;
  lastPairScan = now;
  char wanted[32]; snprintf(wanted, sizeof(wanted), "Backload-Pair-%04X", unsigned(ESP.getEfuseMac() & 0xffff));
  char publicPassword[18]; snprintf(publicPassword, sizeof(publicPassword), "relaypair%04X", unsigned(ESP.getEfuseMac() & 0xffff));
  Serial.println("Kopplung: Verbindung zum Haupt-ESP wird aufgebaut.");
  WiFi.begin(wanted, publicPassword); pairConnecting = connecting = true; connectAt = now;
}
void updatePairButton(uint32_t now) {
  if (config.ssid[0] || !storeReady) return;
  if (digitalRead(0) == HIGH) { pairButtonReady = true; pairButtonHeld = false; }
  else if (pairButtonReady && !pairButtonHeld) { pairButtonHeld = true; pairButtonAt = now; }
  else if (pairButtonReady && pairButtonHeld && now - pairButtonAt >= 1000) {
    pairButtonReady = false; pairWindow = true; pairWindowAt = now; lastPairScan = now - 8000;
    Serial.println("Kopplung für zwei Minuten freigegeben.");
  }
  if (pairWindow && now - pairWindowAt >= 120000) {
    pairWindow = false;
    if (pairConnecting) { pairConnecting = connecting = false; WiFi.disconnect(false, false); }
    Serial.println("Kopplungszeit abgelaufen.");
  }
}
void setup() {
  setAllOff();
  pinMode(0, INPUT_PULLUP);
  Serial.begin(115200);
  esp_ota_img_states_t imageState;
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running && esp_ota_get_state_partition(running, &imageState) == ESP_OK && imageState == ESP_OTA_IMG_PENDING_VERIFY) {
    bootVerificationPending = true; bootVerificationStarted = millis();
  }
  storeReady = prefs.begin("relay-board", false);
  if (storeReady && prefs.getBytesLength("config") == sizeof(config)) prefs.getBytes("config", &config, sizeof(config));
  config.ssid[32] = config.password[63] = config.key[32] = config.apPassword[16] = '\0';
  if (storeReady && (strlen(config.key) != 32 || strlen(config.apPassword) != 16)) {
    randomHex(config.key, 32); randomHex(config.apPassword, 16);
    if (prefs.putBytes("config", &config, sizeof(config)) != sizeof(config)) storeReady = false;
  }
  if (storeReady && prefs.isKey("relaymask")) {
    const uint8_t saved = prefs.getUChar("relaymask", 255);
    if (saved <= 15) {
      relayState.restore(saved);
      for (unsigned i = 0; i < 4; ++i) digitalWrite(RELAY_PINS[i], (relayState.mask() & (1U << i)) ? HIGH : LOW);
    } else { storeReady = false; Serial.println("Gespeicherter Relaiszustand ungültig; Relais bleiben AUS."); }
  }
  if (!storeReady) Serial.println("Speicherfehler: Relais bleiben AUS.");
  else Serial.printf("Verbindungsschlüssel für Haupt-ESP: %s\n", config.key);
  snprintf(relayHostname, sizeof(relayHostname), "relais-%012llx", (unsigned long long)(ESP.getEfuseMac() & 0xffffffffffffULL));
  WiFi.persistent(false); WiFi.setHostname(relayHostname); WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.setAutoReconnect(false);
  const char* headers[] = {"X-Relay-Key"}; server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, []() {
    if (!apActive || server.client().localIP() != WiFi.softAPIP()) { server.send(404, "text/plain", "Not found"); return; }
    server.send_P(200, "text/html; charset=utf-8", SETUP_PAGE);
  });
  server.on("/setup", HTTP_POST, []() {
    if (!apActive || server.client().localIP() != WiFi.softAPIP()) { server.send(404, "text/plain", "Not found"); return; }
    const String ssid = server.arg("ssid"), password = server.arg("password");
    if (!storeReady || !server.hasArg("password") || !validWifi(ssid, password)) { server.send(400, "text/plain", "WLAN-Daten oder Speicher ungültig."); return; }
    Config next = config; ssid.toCharArray(next.ssid, sizeof(next.ssid)); password.toCharArray(next.password, sizeof(next.password));
    if (prefs.putBytes("config", &next, sizeof(next)) != sizeof(next)) { server.send(500, "text/plain", "Speichern fehlgeschlagen."); return; }
    config = next; server.send(200, "text/plain; charset=utf-8", "Gespeichert. Die IP-Adresse steht im seriellen Monitor.");
    reconnect = true; reconnectAt = millis();
  });
  server.on("/api/identity", HTTP_POST, []() {
    if (!storeReady || pairConnecting || WiFi.status() != WL_CONNECTED || server.client().localIP() != WiFi.localIP() || server.args() != 1) { server.send(403, "text/plain", "Forbidden"); return; }
    const String nonce = server.arg("nonce"); char proof[65];
    if (!relayPairProof(config.key, nonce.c_str(), proof)) { server.send(400, "text/plain", "Invalid challenge"); return; }
    server.send(200, "text/plain", proof);
  });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/version", HTTP_GET, []() {
    if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; }
    if (server.args()) { server.send(400, "text/plain", "Invalid request"); return; }
    server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/plain", String("RLYVER1:") + RELAY_FIRMWARE_VERSION);
  });
  server.on("/api/relay", HTTP_POST, changeRelay);
  server.on("/api/factory-reset", HTTP_POST, []() {
    if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; }
    if (server.args() || relayOta.active() || bootVerificationPending || factoryResetPending) { server.send(409, "text/plain", "Busy"); return; }
    if (!relayFactoryReset(prefs, [](){ setAllOff(); })) { server.send(500, "text/plain", "Reset storage failed"); return; }
    config = {}; factoryResetPending = true; factoryResetAt = millis();
    server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/plain", "RESET1:OK");
  });
  server.on("/api/ota/info", HTTP_GET, []() { if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; } relayOta.info(server, bootVerificationPending); });
  server.on("/api/ota/prepare", HTTP_POST, []() { if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; } if (bootVerificationPending) { server.send(409, "text/plain", "Boot check pending"); return; } relayOta.prepare(server); });
  server.on("/api/ota/chunk", HTTP_POST, []() { if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; } relayOta.chunk(server); });
  server.on("/api/ota/finish", HTTP_POST, []() { if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; } relayOta.finish(server); });
  server.on("/api/ota/abort", HTTP_POST, []() { if (!authorized()) { server.send(403, "text/plain", "Forbidden"); return; } relayOta.abort(server); });
  server.onNotFound([](){ server.send(404, "text/plain", "Not found"); });
  server.begin();
  if (storeReady) connectWifi();
}
void loop() {
  const uint32_t now = millis(); server.handleClient(); relayOta.loop(); updatePairButton(now);
  if (factoryResetPending) { if (now - factoryResetAt >= 500) ESP.restart(); delay(1); return; }
  if (bootVerificationPending && now - bootVerificationStarted >= 10000) {
    if (!storeReady || esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
      Serial.println("Startprüfung fehlgeschlagen; Firmware-Rückfall wird ausgelöst.");
      ESP.restart();
    }
    bootVerificationPending = false;
    Serial.println("Firmware-Startprüfung erfolgreich.");
  }
  if (reconnect && now - reconnectAt >= 500) { reconnect = false; WiFi.disconnect(false, true); connecting = wasConnected = false; connectWifi(); }
  if (WiFi.status() == WL_CONNECTED && pairConnecting) {
    if (now - lastPairClaim >= 1200) { lastPairClaim = now; claimPairing(); }
    if (pairConnecting && now - connectAt >= 20000) { Serial.println("Kopplung: Verbindung wurde nicht abgeschlossen."); pairConnecting = connecting = false; WiFi.disconnect(false, false); lastPairScan = now; }
    delay(1); return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      wasConnected = true; connecting = false; lastMdnsAttempt = now - RETRY_MS;
      Serial.print("Relaiskarte im WLAN: "); Serial.println(WiFi.localIP());
      if (apActive) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); apActive = false; }
    }
    if (!mdnsActive && now - lastMdnsAttempt >= RETRY_MS) {
      lastMdnsAttempt = now;
      if (MDNS.begin(relayHostname)) {
        char name[32]; snprintf(name, sizeof(name), "Relaiserweiterung-%04X", unsigned(ESP.getEfuseMac() & 0xffff));
        MDNS.setInstanceName(name);
        mdnsActive = MDNS.addService("backload-relay", "tcp", 80);
      }
      if (!mdnsActive) MDNS.end();
      if (!mdnsActive) Serial.println("Relaiserweiterung: mDNS-Anmeldung fehlgeschlagen; neuer Versuch folgt.");
    }
  } else {
    if (wasConnected) { wasConnected = false; connecting = false; lastAttempt = now - RETRY_MS; if (mdnsActive) { MDNS.end(); mdnsActive = false; } }
    if (connecting && now - connectAt >= 20000) { if (pairConnecting) Serial.println("Kopplung: WLAN-Verbindung fehlgeschlagen."); connecting = pairConnecting = false; WiFi.disconnect(false, false); lastAttempt = lastPairScan = now; startAp(); }
    if (config.ssid[0] && !connecting && now - lastAttempt >= RETRY_MS) connectWifi();
    if (!config.ssid[0]) searchPairingAp(now);
  }
  delay(1);
}
