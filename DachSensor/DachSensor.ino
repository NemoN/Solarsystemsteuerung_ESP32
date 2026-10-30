// Eigenständiger Sketch für einen ESP32 auf dem Dach (noch ohne Sensoren).
// Beim ersten Start mit "DachSensor-Einrichtung" verbinden und http://192.168.4.1 öffnen.
// Danach sendet der ESP32 im Haus-WLAN alle 10 Sekunden ein UDP-Lebenszeichen.
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>

const char* SETUP_AP_SSID = "DachSensor-Einrichtung";
const char* SETUP_AP_PASSWORD = "DachSensor2026"; // Vor dem Einsatz ändern (mindestens 8 Zeichen).
const uint16_t HEARTBEAT_PORT = 4210;
const char HEARTBEAT_MESSAGE[] = "DACHSENSOR_1";
const unsigned long HEARTBEAT_INTERVAL = 10000;
const unsigned long CONNECT_TIMEOUT = 20000;
const unsigned long RETRY_INTERVAL = 30000;

struct WifiCredentials {
  char ssid[33];
  char password[64];
};

WifiCredentials credentials = {};
Preferences preferences;
WebServer server(80);
WiFiUDP udp;
bool storageReady = false;
bool setupApActive = false;
bool connecting = false;
bool wasConnected = false;
bool reconnectQueued = false;
unsigned long reconnectAt = 0;
unsigned long connectStarted = 0;
unsigned long lastAttempt = 0;
unsigned long lastHeartbeat = 0;

const char SETUP_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Dachsensor einrichten</title>
<style>
:root{font-family:system-ui,sans-serif;color:#192f35;background:#f3f5f4;color-scheme:light}
*{box-sizing:border-box}body{margin:0}main{max-width:540px;margin:auto;padding:32px 20px}
h1{font-size:clamp(1.7rem,5vw,2.2rem);margin:0 0 12px}p{line-height:1.6;color:#4e6268}
form{border-top:1px solid #b9c8c9;padding-top:24px}label{display:block;font-weight:600;margin:18px 0}
input{display:block;width:100%;min-height:46px;margin-top:8px;padding:10px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;background:white;color:inherit}
button{min-height:46px;padding:10px 20px;border:0;border-radius:6px;background:#17636b;color:white;font:inherit;font-weight:600;cursor:pointer}
button:hover{background:#104d53}:focus-visible{outline:3px solid #bd650c;outline-offset:3px}
</style></head><body><main>
<h1>Dachsensor einrichten</h1>
<p>Verbinde diesen ESP32 mit dem 2,4-GHz-Haus-WLAN. Er sendet danach regelmäßig ein Lebenszeichen an den Haupt-ESP im selben Netzwerk.</p>
<p id="status">WLAN-Status wird geladen …</p>
<form method="post" action="/save">
<label>WLAN-Name (SSID)<input name="ssid" maxlength="32" required></label>
<label>WLAN-Passwort<input name="password" type="password" maxlength="63" autocomplete="new-password"></label>
<button type="submit">Speichern und verbinden</button>
</form>
<p>Bei einem Verbindungsfehler bleibt der Einrichtungszugang erreichbar.</p>
<script>
async function refresh(){try{const r=await fetch('/api/status',{cache:'no-store'});const d=await r.json();
document.getElementById('status').textContent=d.connected?'Verbunden mit '+d.ssid+' · http://'+d.ip:
d.configured?'Verbindung zu '+d.ssid+' wird versucht.':'Noch kein Haus-WLAN eingerichtet.';
}catch(e){document.getElementById('status').textContent='Status nicht erreichbar.';}}
refresh();setInterval(refresh,3000);
</script></main></body></html>)HTML";

bool validCredentials(const String& ssid, const String& password) {
  return ssid.length() >= 1 && ssid.length() <= 32 &&
    strlen(ssid.c_str()) == ssid.length() &&
    strlen(password.c_str()) == password.length() &&
    (password.length() == 0 || (password.length() >= 8 && password.length() <= 63));
}

String jsonString(const String& value) {
  String result = "\"";
  for (size_t i = 0; i < value.length(); i++) {
    unsigned char ch = value[i];
    if (ch == '"' || ch == '\\') { result += '\\'; result += char(ch); }
    else if (ch < 0x20) {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
      result += escaped;
    } else result += char(ch);
  }
  return result + '"';
}

void startSetupAp() {
  if (setupApActive) return;
  WiFi.mode(WIFI_AP_STA);
  setupApActive = WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);
  if (setupApActive) {
    Serial.print("Einrichtung: http://");
    Serial.println(WiFi.softAPIP());
  } else Serial.println("Einrichtungs-WLAN konnte nicht gestartet werden.");
}

void startConnection() {
  if (!credentials.ssid[0]) return;
  WiFi.begin(credentials.ssid, credentials.password);
  connecting = true;
  connectStarted = millis();
  lastAttempt = connectStarted;
  Serial.print("Verbinde mit Haus-WLAN: ");
  Serial.println(credentials.ssid);
}

void saveCredentials() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  if (!server.hasArg("password") || !validCredentials(ssid, password)) {
    server.send(400, "text/plain; charset=utf-8", "WLAN-Name: 1–32 Bytes. Passwort: 8–63 Bytes oder leer für ein offenes WLAN.");
    return;
  }
  if (!storageReady) {
    server.send(503, "text/plain; charset=utf-8", "Speicher nicht verfügbar.");
    return;
  }
  WifiCredentials next = {};
  ssid.toCharArray(next.ssid, sizeof(next.ssid));
  password.toCharArray(next.password, sizeof(next.password));
  if (preferences.putBytes("wifi", &next, sizeof(next)) != sizeof(next)) {
    server.send(500, "text/plain; charset=utf-8", "WLAN konnte nicht gespeichert werden.");
    return;
  }
  credentials = next;
  server.send(200, "text/html; charset=utf-8", "<!doctype html><html lang=de><meta charset=utf-8><title>Gespeichert</title><p>WLAN gespeichert. Der Dachsensor verbindet sich jetzt mit dem Haus-WLAN. Bei einem Fehler bleibt das Einrichtungs-WLAN erreichbar.</p>");
  reconnectQueued = true;
  reconnectAt = millis();
}

void sendHeartbeat() {
  IPAddress address = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress broadcast;
  for (int i = 0; i < 4; i++) broadcast[i] = address[i] | ~mask[i];
  if (udp.beginPacket(broadcast, HEARTBEAT_PORT)) {
    udp.write((const uint8_t*)HEARTBEAT_MESSAGE, sizeof(HEARTBEAT_MESSAGE) - 1);
    if (udp.endPacket()) Serial.println("Lebenszeichen gesendet.");
    else Serial.println("Lebenszeichen konnte nicht gesendet werden.");
  }
}

void setup() {
  Serial.begin(115200);
  storageReady = preferences.begin("roof", false);
  if (!storageReady) Serial.println("Speicher nicht verfügbar: WLAN-Einstellungen können nicht gespeichert werden.");
  WifiCredentials saved = {};
  if (storageReady && preferences.getBytesLength("wifi") == sizeof(saved) &&
      preferences.getBytes("wifi", &saved, sizeof(saved)) == sizeof(saved) &&
      memchr(saved.ssid, '\0', sizeof(saved.ssid)) &&
      memchr(saved.password, '\0', sizeof(saved.password)) &&
      validCredentials(String(saved.ssid), String(saved.password))) credentials = saved;

  WiFi.persistent(false);
  WiFi.setHostname("dachsensor");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (credentials.ssid[0]) startConnection();
  else startSetupAp();

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
  });
  server.on("/api/status", HTTP_GET, []() {
    bool connected = WiFi.status() == WL_CONNECTED;
    String json = "{\"configured\":" + String(credentials.ssid[0] ? "true" : "false") +
      ",\"connected\":" + String(connected ? "true" : "false") +
      ",\"ssid\":" + jsonString(credentials.ssid) +
      ",\"ip\":" + jsonString(connected ? WiFi.localIP().toString() : String("")) + "}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
  });
  server.on("/save", HTTP_POST, saveCredentials);
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
}

void loop() {
  unsigned long now = millis();
  server.handleClient();

  if (reconnectQueued && now - reconnectAt >= 1000) {
    reconnectQueued = false;
    WiFi.disconnect(false, true);
    wasConnected = false;
    connecting = false;
    udp.stop();
    startSetupAp();
    startConnection();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      wasConnected = true;
      connecting = false;
      lastHeartbeat = now - HEARTBEAT_INTERVAL;
      Serial.print("Haus-WLAN verbunden. IP: ");
      Serial.println(WiFi.localIP());
      if (setupApActive) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        setupApActive = false;
      }
      udp.begin(0);
    }
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      lastHeartbeat = now;
      sendHeartbeat();
    }
  } else {
    if (wasConnected) {
      wasConnected = false;
      connecting = false;
      lastAttempt = now - RETRY_INTERVAL;
      udp.stop();
      Serial.println("Haus-WLAN getrennt.");
    }
    if (connecting && now - connectStarted >= CONNECT_TIMEOUT) {
      connecting = false;
      WiFi.disconnect(false, false);
      lastAttempt = now;
      startSetupAp();
      Serial.println("Haus-WLAN nicht erreichbar. Neuer Versuch in 30 Sekunden.");
    }
    if (credentials.ssid[0] && !connecting && now - lastAttempt >= RETRY_INTERVAL) startConnection();
  }
  delay(1);
}
