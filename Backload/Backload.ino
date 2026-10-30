// Arduino IDE: Backload.ino im gleichnamigen Ordner Backload öffnen.
// ESP32S3 Dev Module (N16R8), ESP32-Core 2.0.13, Flash Size: 16MB,
// PSRAM: OPI PSRAM, Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS).
// Bibliotheken: ESP Async WebServer 3.6.0, Async TCP 3.3.2.
// RisalDash und AsyncWebConsole liegen mit Lizenzinformationen im Sketchordner.
#include <Adafruit_MAX31865.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include "WebSerialMonitor.h"
#include "SensorReading.h"
#include "PumpControl.h"
#include "AutomaticControl.h"
#include "TemperatureHistory.h"
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_arduino_version.h>
#include <esp_chip_info.h>
#include <Update.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include "GitHubRoots.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

// Zentrale Firmware-Version: bei Änderungen hier aktualisieren.
const char* FIRMWARE_VERSION = "1.11.2";

// Beim ersten Start ohne gespeicherte Betriebsart läuft die echte Steuerung.
// Der Testmodus wird über das Webinterface nach Bestätigung ein-/ausgeschaltet.
bool SIMULATION_MODE = false;
bool modeTransition = false;
unsigned long modeRestartAt = 0;
bool otaActive = false, otaFinished = false, otaVerificationPending = false;
unsigned long otaLastActivity = 0, otaStoppedAt = 0, otaBootCheckStarted = 0;
size_t otaExpectedSize = 0, otaReceived = 0;
size_t otaLastChunkOffset = 0, otaLastChunkLength = 0;
uint8_t otaLastChunkHash[32] = {};
String otaToken;
enum GithubUpdateState { GH_IDLE, GH_DOWNLOADING, GH_DOWNLOADED, GH_FAILED, GH_FINISHED };
std::atomic<int> githubUpdateState(GH_IDLE);
std::atomic<uint32_t> githubUpdateBytes(0);
std::atomic<bool> githubUpdateCancel(false);
bool githubUpdateActive = false;
String githubUpdateUrl, githubExpectedHash, githubUpdateToken;
char githubUpdateError[128] = {};

// Messprotokoll für USB und Live-Monitor; nach Änderungen neu kompilieren/hochladen.
// 0 = nur Sensorfehler, Erholung nach Fehlern und Relaisänderungen (Standard).
// 1 = zusätzlich alle Messwerte mit Datum/Uhrzeit und WLAN/IP, ohne Rohwert/CFG.
// 2 = vollständige Messausgabe einschließlich Rohwerten und CFG-Diagnose.
// Start- und WLAN-Meldungen sowie ausführliche Sensorfehler bleiben immer sichtbar.
const uint8_t SENSOR_LOG_LEVEL = 1;
static_assert(SENSOR_LOG_LEVEL <= 2, "SENSOR_LOG_LEVEL muss 0, 1 oder 2 sein.");

// Deutschland: automatische Umstellung zwischen MEZ und MESZ.
const char* TIME_ZONE = "CET-1CEST,M3.5.0/2,M10.5.0/3";
const char* NTP_SERVER_1 = "de.pool.ntp.org";
const char* NTP_SERVER_2 = "time.cloudflare.com";

// Mit diesem WLAN verbinden, dann http://192.168.4.1 öffnen.
// Passwort vor dem Einsatz ändern (mindestens 8 Zeichen).
const char* AP_SSID = nullptr;
const char* AP_PASSWORD = "Solar2026";

#define RREF      4300.0   // 430.0 für PT100, 4300.0 für PT1000
#define RNOMINAL  1000.0   // 100.0 für PT100, 1000.0 für PT1000
#define SPI_CLK   38
// N16R8: GPIO 35–37 sind intern mit dem Octal-PSRAM verbunden.
// Sensorleitungen umstecken: MISO von 37 auf 13, MOSI von 36 auf 11.
#define SPI_MISO  13
#define SPI_MOSI  11
#define LED_PIN   2

// Genau zwei Active-Low-Relais: LOW = Relais AN, HIGH = Relais AUS.
const uint8_t PUMP_ENABLE_PIN = 3;
const uint8_t PUMP_DIRECTION_PIN = 8;
// An die fertige Umpolschaltung anpassen: Förderrichtung bei Richtungsrelais AUS.
const PumpDirection DIRECTION_RELAY_OFF_DIRECTION = PumpDirection::CombiToBuffer;
PumpControl pump(DIRECTION_RELAY_OFF_DIRECTION);
bool manualControl = false;
bool manualPumpOn = false;
PumpDirection manualDirection = DIRECTION_RELAY_OFF_DIRECTION;
const bool AUTOMATIC_PUMP_CONFIGURED = true;
AutomaticControl automaticControl;
AutomaticThresholds automaticThresholds = {2.0f, 5.0f, 2.0f};
bool winterMode = false;       // Sommer: oben; Winter: Mitte.
bool allowCharging = false;   // Keine Einlagerung bei reinem Ölbetrieb.
bool autoSettingsValid = true;

// Active-Low-Relais: LOW = AN, HIGH = AUS.
// Einstellgrenzen je Heizkreis an die konkrete Anlage anpassen.
struct ControllerLimits {
  float minTarget;
  float maxTarget;
  float minHysteresis;
  float maxHysteresis;
};

// Kombispeicher oben und Mitte: Sollwert min/max, Hysterese min/max (jeweils K).
const ControllerLimits ROOM_HEATING_LIMITS = {
  0.0f, 100.0f, 0.1f, 20.0f
};

// Pufferspeicher: Sollwert min/max, Hysterese min/max (jeweils K).
const ControllerLimits HOT_WATER_LIMITS = {
  0.0f, 100.0f, 0.1f, 20.0f
};
const unsigned long MEASUREMENT_INTERVAL = 2000;
const size_t HISTORY_CAPACITY = 365UL * 24 * 60;
TemperatureHistory temperatureHistory;
unsigned long lastMeasurement = 0;
unsigned long lastHeartbeat = 0;
bool ledState = LOW;
bool storageReady = false;
// Der asynchrone öffentliche Server verbindet sich nur über Loopback mit
// dieser Steuerungs-API; deren Handler laufen weiterhin im Hauptzyklus.
WebServer server(IPAddress(127, 0, 0, 1), 8080);
Preferences preferences;
WiFiUDP roofUdp;
const uint16_t ROOF_HEARTBEAT_PORT = 4210;
const char ROOF_HEARTBEAT[] = "DACHSENSOR_1";
const unsigned long ROOF_TIMEOUT = 35000;
bool roofSeen = false;
unsigned long lastRoofHeartbeat = 0;

// Einfacher WLAN-Manager ohne zusätzliche Bibliothek.
// AP bleibt parallel zur Heimnetz-Verbindung verfügbar (2,4 GHz).
struct WifiCredentials {
  char ssid[33];
  char password[64];
};
WifiCredentials wifiCredentials = {};
enum WifiPhase { WIFI_UNCONFIGURED, WIFI_QUEUED, WIFI_STARTING,
                 WIFI_CONNECTING, WIFI_CONNECTED, WIFI_RETRY_WAIT };
WifiPhase wifiPhase = WIFI_UNCONFIGURED;
unsigned long wifiPhaseSince = 0;
const unsigned long WIFI_CONNECT_TIMEOUT = 20000;
const unsigned long WIFI_RETRY_INTERVAL = 30000;
bool wifiScanRequested = false;
String wifiChangeId; // Nur im RAM; bestätigt die zuletzt angenommene Änderung.


struct TemperatureController {
  uint8_t csPin;
  int8_t settingsSlot; // NVS: c0 oben, c1 Puffer, c2 Mitte; -1 ohne Sollwerte.
  float targetTemp;
  float hysteresis;
  const char* name;
  Adafruit_MAX31865* max;
  float currentTemp;
  uint8_t fault;
  bool valid;
  const ControllerLimits* limits;
  bool configValid;
  uint16_t rawRtd;
  const char* sensorError;
  Adafruit_SPIDevice* diagnosticSpi;
};

TemperatureController controllers[] = {
  {4, 0, 45.0, 1.5, "Kombispeicher oben", nullptr, NAN, 0, false, &ROOM_HEATING_LIMITS, false, 0, nullptr, nullptr},
  {5, 2, 45.0, 1.5, "Kombispeicher Mitte", nullptr, NAN, 0, false, &ROOM_HEATING_LIMITS, false, 0, nullptr, nullptr},
  {6, 1, 60.0, 2.0, "Pufferspeicher", nullptr, NAN, 0, false, &HOT_WATER_LIMITS, false, 0, nullptr, nullptr},
  {7, -1, 0, 0, "Kombispeicher unten", nullptr, NAN, 0, false, nullptr, false, 0, nullptr, nullptr},
  {15, -1, 0, 0, "Solarpanel", nullptr, NAN, 0, false, nullptr, false, 0, nullptr, nullptr}
};
const int numControllers = sizeof(controllers) / sizeof(controllers[0]);
// Nur im Testmodus genutzt; die Werte werden nicht dauerhaft gespeichert.
float simulationTemperatures[] = {50.0f, 45.0f, 55.0f, 40.0f, 70.0f};
bool simulationFaults[] = {false, false, false, false, false};
bool simulationVariation = true;
float simulationPhase = 0.0f;
static_assert(sizeof(simulationTemperatures) / sizeof(simulationTemperatures[0]) == sizeof(controllers) / sizeof(controllers[0]), "Testwerte und Sensoren müssen übereinstimmen.");
static_assert(sizeof(simulationFaults) / sizeof(simulationFaults[0]) == sizeof(controllers) / sizeof(controllers[0]), "Testfehler und Sensoren müssen übereinstimmen.");

extern const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Schubis Solarsystemsteuerung</title>
<style>
:root{font-family:system-ui,sans-serif;color:#192f35;background:#f3f5f4;color-scheme:light}
*{box-sizing:border-box}body{margin:0}main{max-width:860px;margin:auto;padding:36px 22px}
h1{font-size:clamp(1.7rem,5vw,2.4rem);margin:0 0 12px}h2{font-size:1.25rem;margin:0}
p{line-height:1.6}header{margin-bottom:30px}.muted{color:#4e6268}
section{border-top:1px solid #b9c8c9;padding:26px 0} .heading,.readings{display:flex;gap:18px;justify-content:space-between;align-items:center;flex-wrap:wrap}
.readings{justify-content:flex-start;margin:20px 0;font-variant-numeric:tabular-nums}.temperature{font-size:1.7rem;font-weight:650}
.badge{padding:6px 12px;border-radius:6px;background:#e0e7e7;font-weight:600}.on{background:#c9e9d9;color:#145239}.error{color:#a32620}
form{display:grid;grid-template-columns:1fr 1fr auto;gap:16px;align-items:end}label{display:block;font-weight:600;font-size:.95rem}
input{min-width:0;display:block;width:100%;min-height:46px;margin-top:8px;padding:10px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;background:white;color:inherit}
button{min-height:46px;padding:10px 20px;border:0;border-radius:6px;background:#17636b;color:white;font:inherit;font-weight:600;cursor:pointer}
button:hover{background:#104d53}button:disabled{opacity:.55;cursor:wait}:focus-visible{outline:3px solid #bd650c;outline-offset:3px}::selection{background:#c9e9d9}
.stepper{display:grid;grid-template-columns:46px minmax(0,1fr) 46px;gap:6px;margin-top:8px}
.stepper input{margin:0;text-align:center;appearance:textfield;-moz-appearance:textfield}
.stepper input::-webkit-inner-spin-button,.stepper input::-webkit-outer-spin-button{-webkit-appearance:none;margin:0}
.stepper button{padding:0;font-size:1.35rem;line-height:1;touch-action:manipulation}
.relay-switch{display:inline-flex;align-items:center;gap:12px;background:white;color:#192f35;border:1px solid #8c9fa3;touch-action:manipulation}
.relay-switch:hover{background:#e0e7e7}.switch-track{display:block;width:50px;height:28px;padding:3px;border-radius:14px;background:#4e6268}
.switch-track::after{content:'';display:block;width:22px;height:22px;border-radius:50%;background:white}
.relay-switch[aria-checked="true"] .switch-track{background:#17636b}.relay-switch[aria-checked="true"] .switch-track::after{transform:translateX(22px)}
.pump-flow{display:block;width:100%;max-width:600px;margin:24px auto 8px;color:#17636b}.pump-flow rect{fill:none;stroke:#8c9fa3}.pump-flow text{fill:#192f35;font:24px system-ui,sans-serif}.pump-flow>path{fill:none;stroke:currentColor;stroke-width:3}.pump-flow.stopped{color:#4e6268}.pump-flow:not(.stopped)>path{stroke-dasharray:12 8;animation:flow 1s linear infinite}@keyframes flow{to{stroke-dashoffset:-20}}@media(prefers-reduced-motion:reduce){.pump-flow:not(.stopped)>path{animation:none}}
.pump-controls{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:20px;margin-top:24px}.pump-controls>div{min-width:0}.pump-controls .relay-switch{width:100%;text-align:left;min-height:58px}.pump-controls .switch-track{flex-shrink:0}.pump-controls p{margin:10px 0 0;font-size:.9rem}.relay-switch:active{background:#c9e9d9}
#pump-progress:empty,#pump-message:empty{display:none}
@media(max-width:760px){.pump-controls{grid-template-columns:1fr}.pump-controls .relay-switch{width:auto;min-width:240px}}
.message{min-height:1.6em;margin:12px 0 0}.rule{margin:16px 0 0;font-size:.9rem}footer{border-top:1px solid #b9c8c9;padding-top:18px;font-size:.9rem}
#wifi-form{grid-template-columns:repeat(2,minmax(0,1fr))}#wifi-form button{grid-column:1/-1;justify-self:start}#wifi-status,#wifi-address{overflow-wrap:anywhere}
#wifi-networks{display:block;width:100%;min-height:46px;margin:8px 0 16px;padding:10px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;background:white;color:inherit}#wifi-scan-message{margin:10px 0;min-height:1.6em}#wifi-network-label[hidden]{display:none}
#controllers form{grid-template-columns:minmax(0,1fr) auto}#auto-form{grid-template-columns:repeat(3,minmax(0,1fr))}#auto-form .full{grid-column:1/-1}#auto-form select{display:block;width:100%;min-height:46px;margin-top:8px;padding:10px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;background:white;color:inherit}#auto-form .check{display:flex;align-items:center;gap:10px;min-height:44px}#auto-form input[type=checkbox]{width:20px;min-height:20px;margin:0;accent-color:#17636b}#auto-form button{justify-self:start}
#sim-banner{padding:12px 16px;background:#fff0d7;border:1px solid #b87922;border-radius:6px}#sim-form{display:block}#sim-fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.sim-item{padding:0}.sim-check{display:flex;align-items:center;gap:8px;margin-top:10px;font-size:.9rem}.sim-check input{width:20px;min-height:20px;margin:0;accent-color:#17636b}#sim-form button{margin-top:20px}
@media(max-width:560px){#sim-fields{grid-template-columns:1fr}}
@media(max-width:560px){main{padding:26px 18px}form{grid-template-columns:1fr 1fr}#controllers form,#auto-form{grid-template-columns:1fr}button{grid-column:1/-1}.stepper button{grid-column:auto}.heading{align-items:flex-start}}
</style>
</head>
<body><main>
<script>document.body.classList.toggle('wifi-page',location.pathname==='/wlan');document.body.classList.toggle('test-page',location.pathname==='/testwerte');document.body.classList.toggle('history-page',location.pathname==='/verlauf');document.body.classList.toggle('update-page',location.pathname==='/update');document.body.classList.toggle('info-page',location.pathname==='/info');</script>
<nav class="page-nav" aria-label="Navigation"><a class="dashboard-link" href="/">← Zur Steuerung</a><a class="wifi-link" href="/wlan">WLAN einrichten</a><a class="history-link" href="/verlauf">Temperaturverlauf</a><a class="update-link" href="/update">Firmware aktualisieren</a><a class="info-link" href="/info">ESP-Info</a><a class="test-values-link" href="/testwerte?test=on" hidden>Testwerte bearbeiten</a><a id="console-link" href="/webserial" target="_blank" rel="noopener">Live-Monitor öffnen</a><a id="test-link" href="/?test=on" hidden>Testmodus starten</a><div class="nav-settings"><span id="firmware-version">Firmware-Version …</span><button id="theme-toggle" class="theme-toggle" type="button">Helles Design</button></div></nav>
<header class="dashboard-header"><h1>Solarsystemsteuerung</h1><div class="header-status"><p id="connection" role="status">Verbindung wird aufgebaut …</p><p id="device-clock" class="muted">ESP32-Zeit wird geladen …</p></div></header>
<div id="sim-banner" role="status" hidden><strong>Testmodus:</strong> Alle Temperaturen und Pumpenzustände sind simuliert. Der anfängliche Sechs-Stunden-Verlauf ist erzeugt. Die echten Pumpenrelais bleiben ausgeschaltet. <button id="test-stop" type="button">Testmodus beenden</button></div>
<p id="test-message" class="message" role="status"></p>
<section aria-labelledby="pump-heading">
<div class="heading"><h2 id="pump-heading">Pumpensteuerung</h2><span id="pump-badge" class="badge" role="status">Status wird geladen …</span></div>
<svg class="pump-flow stopped" viewBox="0 0 440 280" role="img" aria-labelledby="pump-flow-title" aria-describedby="pump-flow-desc">
<title id="pump-flow-title">Förderrichtung wird geladen</title>
<desc id="pump-flow-desc">Speichertemperaturen werden geladen.</desc>
<defs><marker id="flow-arrowhead" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0 0L8 4L0 8Z" fill="currentColor"/></marker></defs>
<g class="storage">
<text class="tank-name" x="96" y="24" text-anchor="middle">Kombispeicher</text>
<text class="tank-name" x="344" y="24" text-anchor="middle">Pufferspeicher</text>
<rect class="tank" x="14" y="42" width="164" height="218" rx="42"/>
<rect class="tank" x="262" y="42" width="164" height="218" rx="42"/>
<path class="tank-rim" d="M16 76 Q96 96 176 76 M264 76 Q344 96 424 76 M16 226 Q96 246 176 226 M264 226 Q344 246 424 226"/>
<text class="sensor-label" x="28" y="98">Oben</text><text class="sensor-value" id="diagram-temp-0" x="163" y="118" text-anchor="end">—</text>
<text class="sensor-label" x="28" y="148">Mitte</text><text class="sensor-value" id="diagram-temp-1" x="163" y="168" text-anchor="end">—</text>
<text class="sensor-label" x="28" y="198">Unten</text><text class="sensor-value" id="diagram-temp-3" x="163" y="218" text-anchor="end">—</text>
<text class="sensor-label" x="276" y="98">Oben</text><text class="sensor-value" id="diagram-temp-2" x="411" y="118" text-anchor="end">—</text>
</g>
<path id="pump-flow-arrow" d="M187 154H253" marker-end="url(#flow-arrowhead)"/>
</svg>
<p id="pump-direction-state" class="muted">Förderrichtung wird geladen …</p>
<div class="pump-controls">
<div><button id="manual-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="manual-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Manuelle Steuerung</span></button><p id="manual-help" class="muted">Handbetrieb übergeht Temperaturregelung und Sensorfehler.</p></div>
<div><button id="pump-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="pump-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Pumpe ein / aus</span></button><p id="pump-help" class="muted">Zuerst die manuelle Steuerung einschalten.</p></div>
<div><button id="direction-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="direction-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Pumprichtung</span></button><p id="direction-help" class="muted">Aus: Kombi → Puffer. Ein: Puffer → Kombi.</p></div>
</div>
<p id="pump-mode-detail" class="muted">Steuerungsmodus wird geladen …</p>
<p id="pump-progress" class="message" role="status"></p>
<p id="pump-message" class="message" role="status"></p>
<p class="muted">Richtungswechsel: Pumpe aus, zwei Sekunden warten, Richtung umschalten, danach bei Bedarf wieder einschalten. Die Pause gilt auch im Handbetrieb.</p>
<p class="muted">Die Zuordnung des Richtungsrelais ist angenommen und muss an der Anlage überprüft werden.</p>
</section>
<section aria-labelledby="auto-heading">
<h2 id="auto-heading">Automatische Regelung</h2>
<p class="muted">Sommer verwendet Kombispeicher oben, Winter Kombispeicher Mitte. Der Sollwert steht beim jeweiligen Sensor.</p>
<form id="auto-form">
<label class="full" for="auto-season">Betriebsart<select id="auto-season" name="winter"><option value="0">Sommer · Kombispeicher oben</option><option value="1">Winter · Kombispeicher Mitte</option></select></label>
<label class="check full"><input type="checkbox" name="charge">Einlagerung Kombi → Puffer freigeben</label>
<label>Sollwert-Startabstand (K)<input type="number" name="offset" min="0.1" max="20" step="0.1" required></label>
<label>Speicherabstand Start (K)<input type="number" name="start" min="0.1" max="30" step="0.1" required></label>
<label>Speicherabstand Stopp (K)<input type="number" name="stop" min="0" max="29.9" step="0.1" required></label>
<button class="full" type="submit">Regelung speichern</button>
</form>
<p class="muted">Einlagerung bei reinem Ölbetrieb ausgeschaltet lassen. Die Rückladung aus dem Puffer bleibt möglich. Der Stoppabstand muss kleiner als der Startabstand sein.</p>
<p id="auto-error" class="error" role="alert" hidden></p>
<p id="auto-message" class="message" role="status"></p>
</section>
<p id="test-page-status" role="status">Verbindung wird aufgebaut …</p>
<section id="info-section" aria-labelledby="info-heading">
<h2 id="info-heading">ESP-Informationen</h2><p class="muted">Daten des angeschlossenen ESP32. Speicherwerte werden alle 5 Sekunden aktualisiert.</p>
<p id="info-status" class="muted" role="status">Gerätedaten werden geladen …</p>
<div class="info-grid">
<div><h3>Chip</h3><dl><div><dt>Modell</dt><dd id="info-chip">—</dd></div><div><dt>Revision</dt><dd id="info-revision">—</dd></div><div><dt>CPU-Kerne</dt><dd id="info-cores">—</dd></div><div><dt>CPU-Takt</dt><dd id="info-cpu">—</dd></div><div><dt>Laufzeit</dt><dd id="info-uptime">—</dd></div></dl></div>
<div><h3>Firmware</h3><dl><div><dt>Version</dt><dd id="info-version">—</dd></div><div><dt>ESP32-Core</dt><dd id="info-core">—</dd></div><div><dt>ESP-IDF</dt><dd id="info-idf">—</dd></div><div><dt>Aktiver Firmware-Slot</dt><dd id="info-slot">—</dd></div><div><dt>Update-Slot</dt><dd id="info-update-slot">—</dd></div></dl></div>
<div><h3>Speicher</h3><dl><div><dt>Flash</dt><dd id="info-flash">—</dd></div><div><dt>Interner RAM nutzbar</dt><dd id="info-heap">—</dd></div><div><dt>Interner RAM frei</dt><dd id="info-heap-free">—</dd></div><div><dt>PSRAM nutzbar</dt><dd id="info-psram">—</dd></div><div><dt>PSRAM frei</dt><dd id="info-psram-free">—</dd></div></dl><p class="muted">Der Temperaturverlauf reserviert PSRAM; die nutzbare Größe kann unter der verbauten Größe liegen.</p></div>
</div></section>
<section id="update-section" aria-labelledby="update-heading">
<h2 id="update-heading">Firmware aktualisieren</h2>
<p class="muted">Während des Updates bleibt die Pumpe ausgeschaltet. Nach erfolgreichem Upload startet der ESP neu; der flüchtige Temperaturverlauf geht dabei verloren.</p>
<button id="release-check" type="button">GitHub-Releases suchen</button><p id="release-message" class="message" role="status">Noch nicht nach Updates gesucht.</p>
<form id="github-update-form"><label>Firmware-Version<select name="release" required disabled><option value="">Bitte zuerst Releases suchen</option></select></label><button type="submit" disabled>Ausgewählte Version installieren</button></form>
<progress id="update-progress" max="100" value="0" hidden></progress><p id="update-message" class="message" role="status"></p>
<details><summary>Firmware-Datei manuell hochladen</summary><p>Eine für dieses ESP32-S3-Board kompilierte Datei (.bin) auswählen.</p><form id="update-form"><label>Firmware-Datei<input name="firmware" type="file" accept=".bin,application/octet-stream" required></label><button type="submit">Datei hochladen</button></form></details>
</section>
<section id="history-section" aria-labelledby="history-heading">
<h2 id="history-heading">Temperaturverlauf</h2>
<p class="muted">Eine Messung pro Minute, bis zu einem Jahr. Der Verlauf liegt im flüchtigen PSRAM und wird beim Neustart gelöscht. Größere Zeiträume werden für die Anzeige zusammengefasst.</p>
<label class="history-range-label" for="history-range">Zeitraum<select id="history-range"><option value="24h">24 Stunden</option><option value="7d">7 Tage</option><option value="30d">30 Tage</option><option value="365d">365 Tage</option></select></label>
<div id="history-legend" class="history-legend" aria-label="Temperaturkurven"></div>
<p id="history-status" class="muted" role="status">Verlauf wird geladen …</p>
<div class="history-chart"><canvas id="history-chart" role="img" aria-label="Temperaturverlauf der ausgewählten Sensoren"></canvas></div>
<details><summary>Letzte dargestellte Werte</summary><p id="history-latest" class="muted">Noch keine Messwerte.</p></details>
</section>
<section id="simulation-section" aria-labelledby="simulation-heading" hidden>
<h2 id="simulation-heading">Testwerte für die Sensoren</h2>
<p class="muted">Temperaturen ändern oder einen Sensorfehler einschalten, dann Testwerte übernehmen. Bei automatischer Variation schwanken die Werte um die eingestellten Temperaturen. Ohne Variation bleiben sie fest. Die Einstellungen gelten bis zum Neustart.</p>
<form id="sim-form"><div id="sim-fields"></div><label class="sim-check"><input type="checkbox" name="variation"> Temperaturen automatisch variieren</label><button type="submit">Testwerte übernehmen</button></form>
<p id="sim-message" class="message" role="status"></p>
</section>
<div id="sensor-grid"><div id="controllers"></div>
<section id="roof-section" aria-labelledby="roof-heading">
<div class="heading"><h2 id="roof-heading">Dachsensor</h2><span id="roof-badge" class="badge" role="status">Status wird geladen …</span></div>
<p id="roof-detail" class="muted">Warte auf ein Lebenszeichen aus dem Heimnetz.</p>
</section></div>
<section id="wifi-section" aria-labelledby="wifi-heading">
<h2 id="wifi-heading">WLAN einrichten</h2>
<p class="muted">Mit einem 2,4-GHz-WLAN verbinden. Der eigene ESP32-WLAN-Zugang bleibt erreichbar.</p>
<p id="wifi-status" role="status">WLAN-Status wird geladen …</p>
<p id="wifi-address" class="muted"></p>
<button id="wifi-scan" type="button">WLANs suchen</button>
<p id="wifi-scan-message" class="muted" role="status"></p>
<label id="wifi-network-label" for="wifi-networks" hidden>Verfügbare WLANs
<select id="wifi-networks"><option value="">WLAN auswählen …</option></select></label>
<form id="wifi-form">
<label for="wifi-ssid">WLAN-Name (SSID)<input id="wifi-ssid" name="ssid" maxlength="32" autocomplete="off" required></label>
<label for="wifi-password">WLAN-Passwort<input id="wifi-password" name="password" type="password" maxlength="63" autocomplete="new-password" aria-describedby="wifi-help"></label>
<button type="submit">Speichern &amp; verbinden</button>
</form>
<p id="wifi-help" class="muted">Passwort bei jeder Änderung neu eingeben; nur bei offenen WLANs leer lassen. Zugangsdaten werden für den nächsten Start gespeichert.</p>
<p id="wifi-message" class="message" role="status"></p>
<button id="wifi-reset" type="button">Gespeichertes WLAN vergessen</button>
</section>
<footer class="muted">Messwerte werden alle 2 Sekunden aktualisiert. Gespeicherte Einstellungen bleiben nach einem Neustart erhalten. Im Handbetrieb steuerst du die Pumpe unabhängig von den Temperaturen. Nach einem Neustart ist der Handbetrieb ausgeschaltet; die Automatik kann bei passenden Temperaturen starten.</footer>
<noscript><p>Bitte JavaScript aktivieren, um Messwerte und Einstellungen anzuzeigen.</p></noscript>
<script>
const root=document.getElementById('controllers');
const connection=document.getElementById('connection');
const themeToggle=document.getElementById('theme-toggle');
function syncThemeToggle(){
  const light=document.documentElement.classList.contains('light');
  themeToggle.textContent=light?'Dunkles Design':'Helles Design';
  themeToggle.setAttribute('aria-label',light?'Zum dunklen Design wechseln':'Zum hellen Design wechseln');
}
themeToggle.addEventListener('click',()=>{
  const light=!document.documentElement.classList.contains('light');
  document.documentElement.classList.toggle('light',light);
  document.documentElement.classList.toggle('dark',!light);
  try{localStorage.setItem('rd-th',light?'light':'dark');}catch(error){}
  syncThemeToggle();
  if(document.body.classList.contains('history-page'))drawHistory();
});
syncThemeToggle();
const temperatureNumber=new Intl.NumberFormat('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1});
const setpointNumber=new Intl.NumberFormat('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1});
const inputNumber=value=>typeof value==='number'&&Number.isFinite(value)?value.toFixed(1):'';
const panels=new Map();
const wifiForm=document.getElementById('wifi-form');
const wifiMessage=document.getElementById('wifi-message');
const wifiReset=document.getElementById('wifi-reset');
const wifiScanButton=document.getElementById('wifi-scan');
const wifiScanMessage=document.getElementById('wifi-scan-message');
const wifiNetworks=document.getElementById('wifi-networks');
const wifiNetworkLabel=document.getElementById('wifi-network-label');
const roofBadge=document.getElementById('roof-badge');
const roofDetail=document.getElementById('roof-detail');
const manualSwitch=document.getElementById('manual-switch');
const pumpSwitch=document.getElementById('pump-switch');
const directionSwitch=document.getElementById('direction-switch');
const pumpBadge=document.getElementById('pump-badge');
const pumpMessage=document.getElementById('pump-message');
const autoForm=document.getElementById('auto-form');
const autoMessage=document.getElementById('auto-message');
const simForm=document.getElementById('sim-form');
const simMessage=document.getElementById('sim-message');
let simDirty=false,simBusy=false,simRevision=0,simulationEnabled=false;
let testParameterHandled=false,modeChanging=false;
function reflectTestModeInUrl(enabled){
  const url=new URL(location.href);
  if(enabled)url.searchParams.set('test','on');else url.searchParams.delete('test');
  history.replaceState(null,'',url.pathname+url.search+url.hash);
}
let autoDirty=false,autoBusy=false,autoRevision=0;
let pumpState=null,pumpBusy=false,pumpRevision=0;
const directionName=value=>value===0?'Kombispeicher → Pufferspeicher':'Pufferspeicher → Kombispeicher';
function updatePumpControls(){
  manualSwitch.disabled=pumpBusy||pumpState===null;
  pumpSwitch.disabled=directionSwitch.disabled=pumpBusy||pumpState===null||!pumpState.manual;
}
function updatePump(state){
  if(!state||typeof state.manual!=='boolean'||typeof state.on!=='boolean'||typeof state.requestedOn!=='boolean'||![0,1].includes(state.direction)||![0,1].includes(state.requestedDirection))throw new Error('Ungültiger Pumpenstatus');
  pumpState=state;
  manualSwitch.setAttribute('aria-checked',String(state.manual));
  pumpSwitch.setAttribute('aria-checked',String(state.requestedOn));
  directionSwitch.setAttribute('aria-checked',String(state.requestedDirection===1));
  pumpBadge.textContent='Pumpe: '+(state.on?'AN':'AUS')+(state.switching?' · Richtungswechsel':'');
  pumpBadge.className='badge'+(state.on?' on':'');
  document.getElementById('pump-direction-state').textContent='Förderrichtung: '+directionName(state.direction)+(state.direction!==state.requestedDirection?' · Angefordert: '+directionName(state.requestedDirection):'');
  document.getElementById('pump-flow-title').textContent='Förderrichtung: '+directionName(state.direction)+', Pumpe '+(state.on?'an':'aus');
  document.getElementById('pump-flow-arrow').setAttribute('d',state.direction===0?'M187 154H253':'M253 154H187');
  document.getElementById('pump-flow-arrow').style.visibility='visible';
  document.querySelector('.pump-flow').classList.toggle('stopped',!state.on);
  document.getElementById('pump-help').textContent=state.manual?'Pumpenauftrag: '+(state.requestedOn?'EIN':'AUS')+'. Der tatsächliche Zustand steht oben.':'Zuerst die manuelle Steuerung einschalten.';
  document.getElementById('pump-mode-detail').textContent=state.manual?'Handbetrieb aktiv. Temperaturregelung und Sensorfehler ändern die Pumpenbefehle nicht.':state.automaticConfigured?'Automatik aktiv; bei Fehler am gewählten Kombifühler oder Pufferfühler bleibt die Pumpe aus.':'Automatik gesperrt. Die Pumpe bleibt ausgeschaltet.';
  document.getElementById('pump-progress').textContent=state.phase==='stopping'?'Pumpe ist aus. Die Richtung wird nach zwei Sekunden Ausschaltpause gewechselt.':state.phase==='settling'?'Richtung umgeschaltet. Kurze Pause, bis das Relais geschaltet hat.':'';
  updatePumpControls();
}
function unknownPump(){
  pumpState=null;
  pumpBadge.textContent='Pumpe: Status unbekannt';pumpBadge.className='badge';
  document.getElementById('pump-direction-state').textContent='Keine aktuelle Förderrichtung. Verbindung zum ESP32 prüfen.';
  document.getElementById('pump-flow-title').textContent='Förderrichtung unbekannt';
  document.getElementById('pump-flow-arrow').style.visibility='hidden';
  document.getElementById('pump-help').textContent='Pumpenstatus unbekannt. Warte auf eine bestätigte Verbindung.';
  document.getElementById('pump-mode-detail').textContent='Steuerungsmodus unbekannt. Die Schalter werden nach erneuter Statusabfrage freigegeben.';
  document.getElementById('pump-progress').textContent='';
  document.querySelector('.pump-flow').classList.add('stopped');
  updatePumpControls();
}
function updateStorageDiagram(controllers){
  const names={0:'Kombispeicher oben',1:'Kombispeicher Mitte',2:'Pufferspeicher oben',3:'Kombispeicher unten'};
  const descriptions=[];
  for(const id of [0,1,3,2]){
    const sensor=Array.isArray(controllers)?controllers.find(c=>c.id===id):null;
    const valid=sensor&&sensor.valid===true&&typeof sensor.temperature==='number'&&Number.isFinite(sensor.temperature);
    const value=valid?temperatureNumber.format(sensor.temperature)+' °C':sensor?'Fehler':'—';
    const output=document.getElementById('diagram-temp-'+id);
    output.textContent=value;
    output.classList.toggle('fault',!!sensor&&!valid);
    descriptions.push(names[id]+': '+(valid?value:sensor?'Sensorfehler':'keine aktuellen Daten'));
  }
  document.getElementById('pump-flow-desc').textContent=descriptions.join('; ')+'.';
}
async function changePump(action){
  if(pumpBusy||pumpState===null||(action!=='manual'&&!pumpState.manual))return;
  const value=action==='manual'?!pumpState.manual:action==='power'?!pumpState.requestedOn:pumpState.requestedDirection===0;
  pumpBusy=true;pumpRevision++;updatePumpControls();
  pumpMessage.className='message';pumpMessage.textContent='Befehl wird übertragen …';
  try{
    const state=await request('/api/pump',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({action,value:value?'1':'0'})});
    updatePump(state);
    pumpMessage.textContent=action==='manual'?(state.manual?'Handbetrieb eingeschaltet. Pumpe ist zunächst AUS.':'Handbetrieb ausgeschaltet.'):'Pumpenbefehl übernommen.';
  }catch(error){
    unknownPump();
    pumpMessage.className='message error';pumpMessage.textContent='Schalten nicht bestätigt: '+error.message+' Der Status wird erneut abgefragt.';
  }finally{pumpBusy=false;updatePumpControls();}
}
manualSwitch.addEventListener('click',()=>changePump('manual'));
pumpSwitch.addEventListener('click',()=>changePump('power'));
directionSwitch.addEventListener('click',()=>changePump('direction'));
autoForm.addEventListener('input',()=>{autoDirty=true;autoMessage.textContent='Noch nicht gespeichert.';autoMessage.className='message';});
autoForm.addEventListener('change',()=>{autoDirty=true;autoMessage.textContent='Noch nicht gespeichert.';autoMessage.className='message';});
function updateAutomatic(state){
  if(!state)return;
  document.getElementById('auto-error').hidden=state.configValid;
  document.getElementById('auto-error').textContent=state.configValid?'':'Automatik gesperrt: Einstellungen prüfen und erneut speichern; gegebenenfalls den Gerätespeicher prüfen.';
  if(autoDirty||autoBusy||autoForm.contains(document.activeElement))return;
  autoForm.elements.winter.value=state.winter?'1':'0';
  autoForm.elements.charge.checked=state.allowCharging;
  autoForm.elements.offset.value=inputNumber(state.targetOffset);
  autoForm.elements.start.value=inputNumber(state.differenceStart);
  autoForm.elements.stop.value=inputNumber(state.differenceStop);
}
function updateSimulation(data){
  simulationEnabled=data.simulation===true;
  document.getElementById('sim-banner').hidden=!simulationEnabled;
  document.getElementById('test-link').hidden=simulationEnabled;
  document.querySelector('.test-values-link').hidden=!simulationEnabled;
  document.getElementById('simulation-section').hidden=!simulationEnabled;
  document.getElementById('test-page-status').textContent=simulationEnabled?'Testmodus aktiv · Die echten Pumpenrelais bleiben ausgeschaltet.':'Testmodus ist ausgeschaltet. Testwerte sind nur im Testmodus verfügbar.';
  document.querySelector('.dashboard-link').href=simulationEnabled?'/?test=on':'/';
  document.querySelector('.wifi-link').href=simulationEnabled?'/wlan?test=on':'/wlan';
  document.querySelector('.history-link').href=simulationEnabled?'/verlauf?test=on':'/verlauf';
  document.querySelector('.update-link').href=simulationEnabled?'/update?test=on':'/update';
  document.querySelector('.info-link').href=simulationEnabled?'/info?test=on':'/info';
  if(!testParameterHandled){
    testParameterHandled=true;
    const url=new URL(location.href),wanted=url.searchParams.get('test');
    if(wanted==='on'&&!simulationEnabled)changeTestMode(true);
    else if(simulationEnabled&&(wanted==='off'||(url.pathname==='/'&&wanted!=='on')))changeTestMode(false);
    else reflectTestModeInUrl(simulationEnabled);
  }
  if(!simulationEnabled)return;
  const fields=document.getElementById('sim-fields');
  if(!fields.children.length){
    for(const c of data.controllers){
      const item=document.createElement('div');item.className='sim-item';
      const label=document.createElement('label');label.textContent=c.name+' (°C)';
      const input=document.createElement('input');input.name='t'+c.id;input.type='number';input.min='-200';input.max='850';input.step='0.1';input.required=true;
      label.appendChild(input);item.appendChild(label);
      const faultLabel=document.createElement('label');faultLabel.className='sim-check';
      const fault=document.createElement('input');fault.type='checkbox';fault.name='f'+c.id;
      faultLabel.append(fault,document.createTextNode('Sensorfehler simulieren'));item.appendChild(faultLabel);fields.appendChild(item);
    }
  }
  if(simDirty||simBusy||simForm.contains(document.activeElement))return;
  simForm.elements.variation.checked=data.simulationVariation===true;
  for(const c of data.controllers){
    const value=data.simulationValues[c.id];
    simForm.elements['t'+c.id].value=inputNumber(value.temperature);
    simForm.elements['f'+c.id].checked=value.fault;
  }
}
async function changeTestMode(enable){
  if(modeChanging)return;
  const prompt=enable?'Testmodus starten? Die echte Pumpe wird ausgeschaltet und die Regelung während des Tests ausgesetzt. Der ESP32 startet neu.':'Testmodus beenden? Der ESP32 startet neu und die echte Automatik kann die Pumpe wieder einschalten.';
  if(!confirm(prompt)){reflectTestModeInUrl(simulationEnabled);return;}
  modeChanging=true;
  const message=document.getElementById('test-message');
  message.className='message';message.textContent='Betriebsart wird umgestellt. Die Pumpe bleibt während des Neustarts aus …';
  try{
    await request('/api/test-mode',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({enabled:enable?'1':'0'})});
    message.textContent='ESP32 startet neu. Falls die Verbindung nicht wiederkommt, mit dem ESP32-WLAN verbinden und die Seite erneut öffnen.';
    setTimeout(()=>location.assign(enable?'/?test=on':'/'),7000);
  }catch(error){modeChanging=false;reflectTestModeInUrl(simulationEnabled);message.className='message error';message.textContent='Umschalten nicht bestätigt: '+error.message+' Bitte den Status nach dem Neuladen prüfen.';}
}
document.getElementById('test-stop').addEventListener('click',()=>changeTestMode(false));
simForm.addEventListener('input',()=>{simDirty=true;simMessage.className='message';simMessage.textContent='Noch nicht übernommen.';});
simForm.addEventListener('change',()=>{simDirty=true;simMessage.className='message';simMessage.textContent='Noch nicht übernommen.';});
simForm.addEventListener('submit',async event=>{
  event.preventDefault();if(!simulationEnabled||simBusy)return;
  const values=new URLSearchParams();
  for(let i=0;i<5;i++){values.set('t'+i,simForm.elements['t'+i].value);values.set('f'+i,simForm.elements['f'+i].checked?'1':'0');}
  values.set('variation',simForm.elements.variation.checked?'1':'0');
  simBusy=true;simRevision++;pumpRevision++;simForm.querySelectorAll('input,button').forEach(e=>e.disabled=true);simMessage.className='message';simMessage.textContent='Wird übernommen …';
  try{await request('/api/simulation',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:values});simDirty=false;simMessage.textContent='Testwerte übernommen.';}
  catch(error){simMessage.className='message error';simMessage.textContent='Nicht übernommen: '+error.message;}
  finally{simBusy=false;simForm.querySelectorAll('input,button').forEach(e=>e.disabled=false);}
});
autoForm.addEventListener('submit',async event=>{
  event.preventDefault();if(autoBusy)return;
  if(Number(autoForm.elements.stop.value)>=Number(autoForm.elements.start.value)){
    autoMessage.className='message error';autoMessage.textContent='Der Stoppabstand muss kleiner als der Startabstand sein.';return;
  }
  const values=new URLSearchParams({winter:autoForm.elements.winter.value,charge:autoForm.elements.charge.checked?'1':'0',offset:autoForm.elements.offset.value,start:autoForm.elements.start.value,stop:autoForm.elements.stop.value});
  autoBusy=true;autoRevision++;pumpRevision++;autoForm.querySelectorAll('input,select,button').forEach(e=>e.disabled=true);autoMessage.textContent='Wird gespeichert …';autoMessage.className='message';
  try{
    await request('/api/automatic',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:values});
    autoDirty=false;autoMessage.textContent='Regelung gespeichert.';
  }catch(error){autoMessage.className='message error';autoMessage.textContent='Speichern nicht bestätigt: '+error.message;}
  finally{autoBusy=false;autoForm.querySelectorAll('input,select,button').forEach(e=>e.disabled=false);}
});
let wifiBusy=false,wifiDirty=false,wifiScanning=false,wifiRevision=0,wifiPending=null;
wifiForm.addEventListener('input',()=>{wifiDirty=true;wifiRevision++;});
wifiNetworks.addEventListener('change',()=>{
  if(!wifiNetworks.value)return;
  wifiForm.elements.ssid.value=wifiNetworks.value;
  wifiDirty=true;wifiRevision++;
  wifiForm.elements.password.focus();
});
wifiScanButton.addEventListener('click',async()=>{
  if(wifiScanning||wifiBusy)return;
  wifiScanning=true;wifiScanButton.disabled=true;wifiForm.querySelector('button').disabled=true;wifiReset.disabled=true;
  wifiNetworkLabel.hidden=true;wifiScanMessage.className='muted';wifiScanMessage.textContent='WLANs werden gesucht … Die Verbindung kann kurz unterbrochen werden.';
  try{
    await request('/api/wifi/scan',{method:'POST',headers:{'X-Requested-With':'HeatingUI'}});
    let result;
    for(let attempt=0;attempt<25;attempt++){
      await new Promise(resolve=>setTimeout(resolve,800));
      result=await request('/api/wifi/scan');
      if(result.state!=='scanning')break;
    }
    if(!result||result.state==='scanning')throw new Error('Die Suche dauert zu lange. Bitte erneut versuchen.');
    if(result.state!=='complete')throw new Error('WLAN-Suche fehlgeschlagen. Bitte erneut versuchen.');
    wifiNetworks.replaceChildren(new Option('WLAN auswählen …',''));
    const networks=new Map();
    for(const network of result.networks){
      const previous=networks.get(network.ssid);
      if(!previous||network.rssi>previous.rssi)networks.set(network.ssid,network);
    }
    for(const network of [...networks.values()].sort((a,b)=>b.rssi-a.rssi)){
      wifiNetworks.add(new Option(network.ssid+' · '+network.rssi+' dBm · '+(network.open?'offen':'geschützt'),network.ssid));
    }
    wifiNetworkLabel.hidden=networks.size===0;
    wifiScanMessage.textContent=networks.size?networks.size+' WLAN(s) gefunden. Auswählen oder den Namen unten eingeben.':'Keine sichtbaren WLANs gefunden. Erneut suchen oder Namen von Hand eingeben.';
  }catch(error){wifiScanMessage.className='error';wifiScanMessage.textContent='Suche nicht abgeschlossen: '+error.message;}
  finally{wifiScanning=false;wifiScanButton.disabled=false;wifiForm.querySelector('button').disabled=false;wifiReset.disabled=false;}
});
function updateWifi(wifi){
  const states={unconfigured:'Noch kein WLAN gespeichert.',queued:'WLAN-Wechsel wird vorbereitet …',starting:'Verbindung wird vorbereitet …',connecting:'Verbindungsaufbau mit '+wifi.ssid+' …',connected:'Verbunden mit '+wifi.ssid,retry:'Keine WLAN-Verbindung. Name, Passwort und Empfang prüfen. Automatischer Wiederholungsversuch nach 30 Sekunden.'};
  const status=document.getElementById('wifi-status');
  status.textContent=states[wifi.state]||'WLAN-Status unbekannt';
  status.className=wifi.state==='retry'?'error':'';
  document.getElementById('wifi-address').textContent=(wifi.ip?'Im Heimnetz: http://'+wifi.ip+' · ':'')+'ESP32-Zugang: http://'+wifi.apIp;
  if(wifiPending&&wifi.changeId===wifiPending.id){
    if(wifiPending.revision===wifiRevision){
      wifiForm.elements.password.value='';
      if(wifiPending.reset){wifiForm.elements.ssid.value='';wifiDirty=false;}
    }
    wifiMessage.className='message';
    if(wifiPending.reset&&wifi.state==='unconfigured'){
      wifiMessage.textContent='Gespeichertes WLAN wurde vergessen.';wifiPending=null;
    }else if(!wifiPending.reset&&wifi.state==='connected'){
      wifiMessage.textContent='WLAN gespeichert und verbunden. Heimnetz: http://'+wifi.ip;wifiPending=null;
    }else if(!wifiPending.reset&&wifi.state==='retry'){
      wifiMessage.className='message error';wifiMessage.textContent='WLAN gespeichert, aber Verbindung fehlgeschlagen. Name, Passwort und Empfang prüfen.';wifiPending=null;
    }else{
      wifiMessage.textContent=wifiPending.reset?'WLAN wird getrennt …':'WLAN gespeichert. Verbindung wird aufgebaut …';
    }
  }
  if(!wifiDirty&&!wifiForm.contains(document.activeElement))wifiForm.elements.ssid.value=wifi.ssid;
}
function updateRoof(roof,wifi){
  roofBadge.textContent=roof.online?'Verbunden':roof.seen?'Keine Verbindung':'Noch kein Lebenszeichen';
  roofBadge.className='badge'+(roof.online?' on':!roof.seen?' waiting':'');
  roofDetail.textContent=wifi.state!=='connected'
    ?'Der Haupt-ESP ist nicht mit dem Heimnetz verbunden.'
    :roof.seen?'Letztes Lebenszeichen vor '+roof.ageSeconds+' Sekunden.':'Der Dach-ESP muss im selben Heimnetz sein und Lebenszeichen senden.';
}
async function changeWifi(reset){
  if(wifiBusy||wifiScanning)return;
  if(reset&&!confirm('Gespeichertes WLAN vergessen? Über den ESP32-WLAN-Zugang kannst du es erneut einrichten.'))return;
  wifiBusy=true;wifiForm.querySelector('button').disabled=true;wifiReset.disabled=true;wifiScanButton.disabled=true;
  wifiMessage.className='message';wifiMessage.textContent='Wird gespeichert …';
  const id=Date.now().toString(36)+'-'+Math.random().toString(36).slice(2);
  wifiPending={id,reset,revision:wifiRevision};
  try{
    const body=reset?new URLSearchParams({changeId:id}):new URLSearchParams({ssid:wifiForm.elements.ssid.value,password:wifiForm.elements.password.value,changeId:id});
    await request(reset?'/api/wifi/reset':'/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body});
    if(wifiPending&&wifiPending.id===id){
      if(wifiPending.revision===wifiRevision)wifiForm.elements.password.value='';
      wifiMessage.textContent=reset?'WLAN wird getrennt …':'WLAN gespeichert. Verbindung wird aufgebaut …';
    }
  }catch(error){
    if(wifiPending&&wifiPending.id===id){
      if(error.status){wifiPending=null;wifiMessage.className='message error';wifiMessage.textContent='WLAN-Änderung abgelehnt: '+error.message;}
      else{wifiMessage.className='message';wifiMessage.textContent='Rückmeldung unterbrochen. Status wird erneut geprüft. Falls nötig, wieder mit dem ESP32-WLAN verbinden und http://192.168.4.1 öffnen.';}
    }
  }
  finally{wifiBusy=false;wifiForm.querySelector('button').disabled=false;wifiReset.disabled=false;wifiScanButton.disabled=false;}
}
wifiForm.addEventListener('submit',event=>{event.preventDefault();changeWifi(false);});
wifiReset.addEventListener('click',()=>changeWifi(true));
let saving=false;
async function request(url,options={}){
  const abort=new AbortController();
  const timer=setTimeout(()=>abort.abort(),6000);
  try{
    const response=await fetch(url,{...options,cache:'no-store',signal:abort.signal});
    if(!response.ok){const error=new Error(await response.text());error.status=response.status;throw error;}
    return await response.json();
  }finally{clearTimeout(timer);}
}
const updateForm=document.getElementById('update-form');
const githubForm=document.getElementById('github-update-form'),releaseMessage=document.getElementById('release-message');
let githubReleases=[],currentFirmwareVersion='';
function compareVersions(a,b){
  const parse=s=>/^v?(\d+)\.(\d+)\.(\d+)$/.exec(s||'');
  const x=parse(a),y=parse(b);if(!x||!y)return null;
  for(let i=1;i<=3;i++){const d=Number(x[i])-Number(y[i]);if(d)return Math.sign(d);}return 0;
}
document.getElementById('release-check').addEventListener('click',async event=>{
  const button=event.currentTarget,select=githubForm.elements.release;
  button.disabled=true;select.disabled=true;githubForm.querySelector('button').disabled=true;releaseMessage.className='message';releaseMessage.textContent='GitHub-Releases werden gesucht …';
  try{
    const device=await request('/api/status');currentFirmwareVersion=device.version||'';
    const response=await fetch('https://api.github.com/repos/NemoN/Solarsystemsteuerung_ESP32/releases?per_page=100',{cache:'no-store',headers:{Accept:'application/vnd.github+json'}});
    if(!response.ok)throw new Error('GitHub meldet HTTP '+response.status);
    const releases=await response.json();if(!Array.isArray(releases))throw new Error('Ungültige GitHub-Antwort');
    githubReleases=releases.filter(r=>!r.draft&&Array.isArray(r.assets)).map(r=>({tag:r.tag_name,name:r.name||r.tag_name,date:r.published_at||'',prerelease:r.prerelease,asset:r.assets.find(a=>a.name==='Backload.ino.bin'&&typeof a.digest==='string'&&/^sha256:[0-9a-f]{64}$/.test(a.digest)&&a.size>=1024&&a.size<=3145728)})).filter(r=>r.asset&&/^[A-Za-z0-9._-]+$/.test(r.tag)).sort((a,b)=>b.date.localeCompare(a.date));
    select.replaceChildren(new Option(githubReleases.length?'Version auswählen …':'Keine passenden Releases',''));
    githubReleases.forEach((r,i)=>select.add(new Option(r.name+(r.prerelease?' · Vorabversion':'')+(r.tag.replace(/^v/,'')===currentFirmwareVersion?' · installiert':''),String(i))));
    select.disabled=!githubReleases.length;githubForm.querySelector('button').disabled=!githubReleases.length;
    if(!githubReleases.length)releaseMessage.textContent='Keine Releases mit passender Firmware und SHA-256-Prüfsumme gefunden.';
    else{
      const latest=githubReleases.find(r=>!r.prerelease)||githubReleases[0],comparison=compareVersions(latest.tag,currentFirmwareVersion);
      const availability=comparison===null?'':comparison>0?'Neue Version verfügbar. ':comparison===0?'Firmware ist aktuell. ':'Installierte Version ist neuer. ';
      releaseMessage.textContent=availability+'Neueste Version: '+latest.tag+' · Installiert: '+(currentFirmwareVersion||'unbekannt')+'.'+(githubReleases.length>1?' Ältere Versionen sind ebenfalls auswählbar.':'');
    }
  }catch(error){githubReleases=[];select.replaceChildren(new Option('Suche fehlgeschlagen',''));releaseMessage.className='message error';releaseMessage.textContent='GitHub nicht erreichbar: '+error.message+'. Internetverbindung dieses Geräts prüfen.';}
  finally{button.disabled=false;}
});
if(document.body.classList.contains('update-page'))document.getElementById('release-check').click();
githubForm.addEventListener('submit',async event=>{
  event.preventDefault();const selected=githubForm.elements.release.value;
  const release=selected===''?null:githubReleases[Number(selected)];if(!release)return;
  if(!confirm('Version '+release.tag+' installieren? Die Pumpe wird ausgeschaltet und der ESP startet danach neu.'))return;
  const button=githubForm.querySelector('button'),progress=document.getElementById('update-progress'),message=document.getElementById('update-message');
  button.disabled=true;progress.hidden=false;progress.value=0;message.className='message';message.textContent='GitHub-Download wird gestartet …';
  const post=(path,values)=>request('/api/update/'+path,{method:'POST',headers:{'X-Requested-With':'HeatingUI','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values)});
  try{
    const start=await post('github',{url:release.asset.browser_download_url,size:String(release.asset.size),sha256:release.asset.digest.slice(7)});
    for(;;){
      await new Promise(resolve=>setTimeout(resolve,1000));
      const state=await post('github-status',{token:start.token});
      progress.value=Math.round(state.received*100/state.size);
      if(state.state==='failed')throw new Error(state.error||'Download fehlgeschlagen');
      if(state.state==='finished'){message.textContent='Update erfolgreich. Der ESP startet neu; bitte die Seite anschließend neu laden.';break;}
      message.textContent='Firmware wird von GitHub geladen: '+progress.value+' %';
    }
  }catch(error){message.className='message error';message.textContent='Update nicht abgeschlossen: '+(error.message||'Verbindung unterbrochen')+'. Bitte Status prüfen und erneut versuchen.';}
  finally{button.disabled=false;}
});
updateForm.addEventListener('submit',async event=>{
  event.preventDefault();
  const file=updateForm.elements.firmware.files[0];
  if(!file)return;
  if(!confirm('Firmware aktualisieren? Die Pumpe wird ausgeschaltet und der ESP startet danach neu.'))return;
  const button=updateForm.querySelector('button'),progress=document.getElementById('update-progress'),message=document.getElementById('update-message');
  button.disabled=true;progress.hidden=false;progress.value=0;message.className='message';message.textContent='Update wird vorbereitet …';
  const post=(path,values)=>request('/api/update/'+path,{method:'POST',headers:{'X-Requested-With':'HeatingUI','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values)});
  let token='';
  try{
    const start=await post('prepare',{size:String(file.size)});token=start.token;
    for(let offset=0;offset<file.size;offset+=4096){
      const bytes=new Uint8Array(await file.slice(offset,offset+4096).arrayBuffer());
      const data=btoa(Array.from(bytes,byte=>String.fromCharCode(byte)).join(''));
      let sent=false;
      for(let attempt=0;attempt<3&&!sent;attempt++){
        try{await post('chunk',{token,offset:String(offset),data});sent=true;}
        catch(error){if(attempt===2||(error.status&&error.status<500))throw error;await new Promise(resolve=>setTimeout(resolve,400));}
      }
      progress.value=Math.round((offset+bytes.length)*100/file.size);message.textContent='Firmware wird übertragen: '+progress.value+' %';
    }
    await post('finish',{token});token='';
    message.textContent='Update erfolgreich. Der ESP startet neu; bitte die Seite anschließend neu laden.';
  }catch(error){
    if(token){try{await post('abort',{token});}catch(ignored){}}
    message.className='message error';message.textContent='Update nicht abgeschlossen: '+(error.message||'Verbindung unterbrochen')+'. Bitte Status prüfen und erneut versuchen.';
  }finally{button.disabled=false;}
});
function createPanel(c){
  const section=document.createElement('section');
  section.dataset.sensorId=String(c.id);
  if(!c.hasSettings||c.id===2){
    section.innerHTML='<h2></h2><div class="readings"><output class="temperature">—</output></div>';
    section.querySelector('h2').textContent=c.name;
    const sparkline=addTemperatureSparkline(section,c);
    root.appendChild(section);
    return {section,sparkline,form:null,isDirty:()=>false};
  }
  const limits=c.limits;
  section.innerHTML=`<div class="heading"><h2></h2><span class="badge">Sensor: —</span></div>
    <div class="readings"><output class="temperature">—</output></div>
    <form><div><label for="target-${c.id}">Sollwert (°C)</label><div class="stepper"><button type="button" data-field="target" data-direction="-1">−</button><input id="target-${c.id}" name="target" type="number" inputmode="decimal" step="0.1" required><button type="button" data-field="target" data-direction="1">+</button></div></div>
    <input name="hysteresis" type="hidden">
    <button type="submit">Speichern</button></form><p class="message" role="status"></p><p class="config-error error" role="alert" hidden></p><p class="rule muted"></p>`;
  section.querySelector('h2').textContent=c.name;
  const form=section.querySelector('form');
  form.elements.target.min=limits.minTarget;form.elements.target.max=limits.maxTarget;
  form.elements.hysteresis.min=limits.minHysteresis;form.elements.hysteresis.max=limits.maxHysteresis;
  form.elements.target.value=inputNumber(c.target);form.elements.hysteresis.value=inputNumber(c.hysteresis);
  form.elements.target.dataset.savedValue=c.target;
  form.elements.hysteresis.dataset.savedValue=c.hysteresis;
  form.querySelectorAll('.stepper button').forEach(button=>{
    const field=button.dataset.field;
    const label=field==='target'?'Sollwert':'Hysterese';
    button.setAttribute('aria-label',c.name+': '+label+(button.dataset.direction==='-1'?' senken':' erhöhen'));
  });
  const message=section.querySelector('.message');
  let dirty=false;
  form.addEventListener('input',()=>{dirty=true;message.className='message';message.textContent='Noch nicht gespeichert.';});
  form.addEventListener('focusout',event=>{
    if(!event.target.matches('input[type="number"]')||event.target.value==='')return;
    const value=Number(event.target.value);
    if(Number.isFinite(value))event.target.value=inputNumber(value);
  });
  form.addEventListener('click',event=>{
    const button=event.target.closest('.stepper button');
    if(!button||button.disabled)return;
    const input=form.elements[button.dataset.field];
    const fallback=Number(input.dataset.savedValue);
    const current=input.value===''?fallback:Number(input.value);
    const minimum=Number(input.min),maximum=Number(input.max);
    const next=Math.min(maximum,Math.max(minimum,Math.round((current+Number(button.dataset.direction)*0.1)*10)/10));
    if(!Number.isFinite(next)||input.value===inputNumber(next))return;
    input.value=inputNumber(next);
    input.dispatchEvent(new Event('input',{bubbles:true}));
  });
  form.addEventListener('submit',async event=>{
    event.preventDefault();if(saving||section.dataset.configValid!=='true')return;
    saving=true;
    root.querySelectorAll('button').forEach(b=>b.disabled=true);
    const target=form.elements.target.value,hysteresis=form.elements.hysteresis.value;
    message.className='message';message.textContent='Wird gespeichert …';
    try{
      await request('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({id:c.id,target,hysteresis})});
      dirty=form.elements.target.value!==target||form.elements.hysteresis.value!==hysteresis;
      message.textContent=dirty?'Gespeichert. Weitere Änderungen sind noch offen.':'Einstellungen gespeichert.';
    }catch(error){message.className='message error';message.textContent='Speichern nicht bestätigt: '+error.message+' Werte prüfen und erneut speichern.';}
    finally{saving=false;root.querySelectorAll('button').forEach(b=>b.disabled=b.closest('section').dataset.configValid!=='true');}
  });
  const sparkline=addTemperatureSparkline(section,c);
  root.appendChild(section);
  return {section,sparkline,form,isDirty:()=>dirty};
}
let sparkData=null,sparkMessage='Verlauf wird geladen …',sparkLoading=false;
function addTemperatureSparkline(section,c){
  const chart=document.createElement('div');chart.className='sparkline';
  chart.innerHTML='<svg viewBox="0 0 360 62" preserveAspectRatio="none" role="img"><path></path><g></g></svg><div class="sparkline-info"><span>Letzte 6 Stunden · 1 Min.</span><span class="sparkline-range"></span></div><div class="sparkline-status"></div>';
  section.querySelector('.readings').after(chart);
  drawSparkline(chart,c.id,c.name);
  return chart;
}
function showTemperature(p,c){
  const valid=c&&c.valid===true&&typeof c.temperature==='number'&&Number.isFinite(c.temperature);
  const output=p.section.querySelector('.temperature');
  const detail=c?'Sensorfehler'+(c.sensorError?': '+c.sensorError:'')+(c.fault?' (0x'+c.fault.toString(16).toUpperCase()+')':''):'—';
  output.textContent=valid?temperatureNumber.format(c.temperature)+' °C':detail;
  output.className='temperature'+(c&&!valid?' error':'');
}
function drawSparkline(chart,id,name){
  const svg=chart.querySelector('svg'),path=svg.querySelector('path'),dots=svg.querySelector('g');
  const status=chart.querySelector('.sparkline-status'),range=chart.querySelector('.sparkline-range');
  const points=sparkData&&Array.isArray(sparkData.points)?sparkData.points:[];
  const valid=points.filter(p=>typeof p[id+1]==='number'&&Number.isFinite(p[id+1]));
  path.setAttribute('d','');dots.replaceChildren();range.textContent='';
  if(!valid.length){
    svg.setAttribute('hidden','');status.textContent=sparkMessage||'Keine gültigen Werte im Zeitraum.';
    svg.setAttribute('aria-label',name+': '+status.textContent);return;
  }
  svg.removeAttribute('hidden');status.textContent='';
  const values=valid.map(p=>p[id+1]),min=Math.min(...values),max=Math.max(...values);
  const pad=Math.max(.5,(max-min)*.1),low=min-pad,high=max+pad;
  const end=points[points.length-1][0],start=end-359*60;
  const x=t=>Math.max(3,Math.min(357,3+(t-start)/(359*60)*354));
  const y=v=>4+(high-v)/(high-low)*54;
  let d='',active=false;
  for(const p of points){const v=p[id+1];if(typeof v!=='number'||!Number.isFinite(v)){active=false;continue;}d+=(active?' L':' M')+x(p[0]).toFixed(1)+' '+y(v).toFixed(1);active=true;}
  path.setAttribute('d',d);
  for(let i=0;i<points.length;i++){
    const p=points[i],v=p[id+1],prev=i>0&&typeof points[i-1][id+1]==='number',next=i+1<points.length&&typeof points[i+1][id+1]==='number';
    if(typeof v==='number'&&Number.isFinite(v)&&!prev&&!next){const dot=document.createElementNS('http://www.w3.org/2000/svg','circle');dot.setAttribute('cx',x(p[0]));dot.setAttribute('cy',y(v));dot.setAttribute('r','2.5');dots.appendChild(dot);}
  }
  range.textContent=temperatureNumber.format(min)+'–'+temperatureNumber.format(max)+' °C';
  svg.setAttribute('aria-label',name+': Verlauf der letzten sechs Stunden, Minimum '+temperatureNumber.format(min)+', Maximum '+temperatureNumber.format(max)+' Grad Celsius.');
}
async function loadSparklines(){
  if(sparkLoading)return;sparkLoading=true;
  try{
    sparkData=await request('/api/history?range=6h');
    sparkMessage=!sparkData.available?'Verlauf nicht verfügbar: PSRAM fehlt.':!sparkData.samples?'Noch keine Daten. Erfassung beginnt nach Zeitsynchronisierung.':'';
  }catch(error){sparkData=null;sparkMessage='Verlauf nicht erreichbar. Neuer Versuch erfolgt automatisch.';}
  finally{sparkLoading=false;panels.forEach((p,id)=>drawSparkline(p.sparkline,id,p.section.querySelector('h2').textContent));}
}
async function refresh(){
  const revision=pumpRevision;
  const autoRequestRevision=autoRevision;
  const simRequestRevision=simRevision;
  try{
    if(saving||wifiBusy||wifiScanning)return;
    const data=await request('/api/status');
    if(simRequestRevision===simRevision)updateSimulation(data);
    updateWifi(data.wifi);
    updateRoof(data.roof,data.wifi);
    if(autoRequestRevision===autoRevision)updateAutomatic(data.automatic);
    if(!pumpBusy&&revision===pumpRevision)updatePump(data.pump);
    updateStorageDiagram(data.controllers);
    const clock=document.getElementById('device-clock');
    clock.textContent=data.clock.available
      ?'ESP32-Zeit: '+data.clock.local+' '+data.clock.zone+(data.wifi.ip?'':' · läuft ohne WLAN weiter')
      :'ESP32-Zeit: noch nicht synchronisiert. WLAN mit Internetzugang erforderlich.';
    document.getElementById('firmware-version').textContent='Firmware-Version '+data.version;
    currentFirmwareVersion=data.version;
    for(const c of data.controllers){
      if(!panels.has(c.id))panels.set(c.id,createPanel(c));
      const p=panels.get(c.id);
      showTemperature(p,c);
      if(!p.form)continue;
      p.section.dataset.configValid=String(c.configValid);
      const configError=p.section.querySelector('.config-error');
      configError.hidden=c.configValid;
      configError.textContent=c.configValid?'':'Konfigurationsfehler: Grenzen oder Standardwerte im Sketch prüfen.';
      p.form.querySelectorAll('input,button').forEach(e=>e.disabled=!c.configValid||(e.tagName==='BUTTON'&&saving));
      const badge=p.section.querySelector('.badge');badge.textContent=c.valid?'Sensor: OK':'Sensorfehler';badge.className='badge'+(c.valid?' on':'');
      p.section.querySelector('.rule').textContent=data.pump.manual?'Handbetrieb: Sollwerte werden ignoriert.':(data.automatic.winter?c.id===1:c.id===0)?'Dieser Sollwert wird für die Automatik verwendet.':'Dieser Sollwert gilt für die andere Betriebsart.';
      p.form.elements.target.dataset.savedValue=c.target;
      p.form.elements.hysteresis.dataset.savedValue=c.hysteresis;
      if(!p.isDirty()&&!p.form.contains(document.activeElement)&&!saving){p.form.elements.target.value=inputNumber(c.target);p.form.elements.hysteresis.value=inputNumber(c.hysteresis);}
    }
    connection.className='muted';connection.textContent='Verbunden · Messwerte werden alle 2 Sekunden aktualisiert';
  }catch(error){
    if(!pumpBusy&&revision===pumpRevision)unknownPump();
    updateStorageDiagram(null);
    document.getElementById('test-page-status').textContent='Keine Verbindung zum ESP32. Testmodus und Werte sind nicht aktuell.';
    document.getElementById('device-clock').textContent='ESP32-Zeit: keine aktuellen Daten.';
    document.getElementById('wifi-status').textContent='WLAN-Status nicht aktuell: Verbindung zum ESP32 unterbrochen.';
    roofBadge.textContent='Status unbekannt';roofBadge.className='badge';
    roofDetail.textContent='Keine aktuellen Daten vom Haupt-ESP.';
    connection.className='error';connection.textContent='Keine Verbindung. Anzeigen sind nicht aktuell. Neuer Versuch erfolgt automatisch.';
    panels.forEach(p=>{showTemperature(p,null);const badge=p.section.querySelector('.badge');if(badge){badge.textContent='Sensor: unbekannt';badge.className='badge';}});
  }finally{setTimeout(refresh,pumpState&&pumpState.switching?250:2000);}
}
const historyNames=['Kombispeicher oben','Kombispeicher Mitte','Pufferspeicher oben','Kombispeicher unten','Solarpanel'];
const historyColorsDark=['#63dac6','#ffd27a','#8bbcff','#c6a5ff','#ff9d85'];
const historyColorsLight=['#006b5d','#8a4d00','#145d9d','#6738a8','#a73517'];
let historyData=null,historyRequest=0;
const historyRange=document.getElementById('history-range'),historyLegend=document.getElementById('history-legend');
function historyColors(){return document.documentElement.classList.contains('light')?historyColorsLight:historyColorsDark;}
function selectedHistory(){return [...historyLegend.querySelectorAll('input')].map(input=>input.checked);}
function drawHistory(){
  if(!document.body.classList.contains('history-page'))return;
  const canvas=document.getElementById('history-chart'),box=canvas.getBoundingClientRect();
  if(!box.width||!box.height)return;
  const ratio=Math.min(window.devicePixelRatio||1,2),width=box.width,height=box.height;
  canvas.width=Math.round(width*ratio);canvas.height=Math.round(height*ratio);
  const ctx=canvas.getContext('2d');ctx.setTransform(ratio,0,0,ratio,0,0);ctx.clearRect(0,0,width,height);
  const colors=historyColors(),selected=selectedHistory(),points=historyData&&historyData.points||[];
  historyLegend.querySelectorAll('label').forEach((label,i)=>label.style.setProperty('--curve',colors[i]));
  let values=[];for(const p of points)for(let i=0;i<5;i++)if(selected[i]&&typeof p[i+1]==='number'&&Number.isFinite(p[i+1]))values.push(p[i+1]);
  if(!values.length){canvas.setAttribute('aria-label','Keine darstellbaren Messwerte für die ausgewählten Sensoren.');return;}
  let min=Math.min(...values),max=Math.max(...values),padding=Math.max(1,(max-min)*0.08);min=Math.floor(min-padding);max=Math.ceil(max+padding);
  const left=48,right=14,top=14,bottom=35,w=Math.max(1,width-left-right),h=Math.max(1,height-top-bottom);
  const textColor=getComputedStyle(document.getElementById('history-status')).color;
  ctx.font='12px system-ui, sans-serif';ctx.fillStyle=textColor;ctx.strokeStyle=textColor;ctx.globalAlpha=.18;
  for(let i=0;i<=4;i++){let y=top+h*i/4;ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(left+w,y);ctx.stroke();}
  ctx.globalAlpha=1;ctx.textAlign='right';ctx.textBaseline='middle';for(let i=0;i<=4;i++){let y=top+h*i/4;ctx.fillText((max-(max-min)*i/4).toFixed(1).replace('.',','),left-7,y);}
  const start=points[0][0],end=points[points.length-1][0],duration=Math.max(1,end-start);
  const x=t=>left+(t-start)/duration*w,y=v=>top+(max-v)/(max-min)*h;
  for(let i=0;i<5;i++){if(!selected[i])continue;ctx.strokeStyle=colors[i];ctx.fillStyle=colors[i];ctx.lineWidth=2;let active=false;ctx.beginPath();
    for(const point of points){const value=point[i+1];if(typeof value!=='number'||!Number.isFinite(value)){active=false;continue;}const px=points.length===1?left+w/2:x(point[0]),py=y(value);if(active)ctx.lineTo(px,py);else ctx.moveTo(px,py);active=true;}
    ctx.stroke();
    for(let j=0;j<points.length;j++){
      const value=points[j][i+1],previous=j>0&&typeof points[j-1][i+1]==='number',next=j+1<points.length&&typeof points[j+1][i+1]==='number';
      if(typeof value==='number'&&Number.isFinite(value)&&!previous&&!next){ctx.beginPath();ctx.arc(points.length===1?left+w/2:x(points[j][0]),y(value),3,0,Math.PI*2);ctx.fill();}
    }
  }
  const longRange=end-start>86400,small=width<400;
  const timeOptions=longRange?{day:'2-digit',month:'2-digit',year:'2-digit',...(small?{}:{hour:'2-digit',minute:'2-digit'})}:{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'};
  const timeFormat=new Intl.DateTimeFormat('de-DE',{timeZone:'Europe/Berlin',...timeOptions});
  ctx.fillStyle=textColor;ctx.textBaseline='top';ctx.textAlign='left';ctx.fillText(timeFormat.format(new Date(start*1000)),left,top+h+10,w/2-4);
  if(end>start){ctx.textAlign='right';ctx.fillText(timeFormat.format(new Date(end*1000)),left+w,top+h+10,w/2-4);}
  canvas.setAttribute('aria-label','Temperaturverlauf in Grad Celsius; '+points.length+' dargestellte Zeitpunkte.');
}
async function loadHistory(){
  if(!document.body.classList.contains('history-page'))return;
  const revision=++historyRequest,status=document.getElementById('history-status');
  try{
    const data=await request('/api/history?range='+encodeURIComponent(historyRange.value));
    if(revision!==historyRequest)return;
    historyData=data;
    status.className='muted';
    status.textContent=!data.available?'Kein ausreichender PSRAM verfügbar. Die Steuerung läuft weiter.':!data.samples?'Noch keine Daten. Nach Zeitsynchronisierung beginnt die Erfassung.':new Intl.NumberFormat('de-DE').format(data.samples)+' Minuten gespeichert · '+data.points.length+' Punkte dargestellt.';
    const latest=data.points[data.points.length-1];
    document.getElementById('history-latest').textContent=latest?historyNames.map((name,i)=>name+': '+(typeof latest[i+1]==='number'?temperatureNumber.format(latest[i+1])+' °C':'kein gültiger Wert')).join(' · '):'Noch keine Messwerte.';
    drawHistory();
  }catch(error){if(revision!==historyRequest)return;historyData=null;status.className='error';status.textContent='Verlauf nicht erreichbar. Verbindung prüfen und erneut versuchen.';document.getElementById('history-latest').textContent='Keine aktuellen Daten.';drawHistory();}
}
let infoBusy=false;
async function loadDeviceInfo(){
  if(infoBusy)return;infoBusy=true;
  const status=document.getElementById('info-status');
  const set=(id,value)=>document.getElementById('info-'+id).textContent=value;
  const size=value=>typeof value==='number'&&Number.isFinite(value)?new Intl.NumberFormat('de-DE',{maximumFractionDigits:2}).format(value>=1048576?value/1048576:value/1024)+(value>=1048576?' MiB':' KiB'):'—';
  try{
    const d=await request('/api/device-info');
    set('chip',d.chip);set('revision',d.revision);set('cores',String(d.cores));set('cpu',d.cpuMHz+' MHz');
    const seconds=d.uptimeSeconds,days=Math.floor(seconds/86400),hours=Math.floor(seconds%86400/3600),minutes=Math.floor(seconds%3600/60);
    set('uptime',(days?days+' Tage · ':'')+hours+' Std. '+minutes+' Min.');
    set('version',d.firmware);set('core',d.arduinoCore);set('idf',d.idf);
    set('slot',d.runningSlot?d.runningSlot.label+' · '+size(d.runningSlot.size):'Nicht verfügbar');
    set('update-slot',d.updateSlot?d.updateSlot.label+' · '+size(d.updateSlot.size):'Nicht verfügbar');
    set('flash',size(d.flashBytes));set('heap',size(d.heapBytes));set('heap-free',size(d.heapFreeBytes));
    set('psram',d.psramBytes?size(d.psramBytes):'Nicht verfügbar');set('psram-free',d.psramBytes?size(d.psramFreeBytes):'Nicht verfügbar');
    status.className='muted';status.textContent='Verbunden · Aktualisierung alle 5 Sekunden';
  }catch(error){
    document.querySelectorAll('#info-section dd').forEach(e=>e.textContent='—');
    status.className='error';status.textContent='Gerätedaten nicht erreichbar. Neuer Versuch erfolgt automatisch.';
  }finally{infoBusy=false;}
}
if(document.body.classList.contains('history-page')){
  historyNames.forEach((name,i)=>{const label=document.createElement('label'),input=document.createElement('input'),line=document.createElement('i');input.type='checkbox';input.checked=true;input.addEventListener('change',drawHistory);line.setAttribute('aria-hidden','true');label.append(input,line,document.createTextNode(name));historyLegend.appendChild(label);});
  historyRange.addEventListener('change',loadHistory);window.addEventListener('resize',drawHistory);loadHistory();setInterval(loadHistory,60000);
}
if(document.body.classList.contains('info-page')){loadDeviceInfo();setInterval(loadDeviceInfo,5000);}
if(!document.body.classList.contains('history-page')&&!document.body.classList.contains('wifi-page')&&!document.body.classList.contains('test-page')&&!document.body.classList.contains('update-page')&&!document.body.classList.contains('info-page')){loadSparklines();setInterval(loadSparklines,60000);}
refresh();
</script></main></body></html>)HTML";

bool validLimits(const ControllerLimits& limits) {
  return isfinite(limits.minTarget) && isfinite(limits.maxTarget) &&
    isfinite(limits.minHysteresis) && isfinite(limits.maxHysteresis) &&
    limits.minTarget <= limits.maxTarget &&
    limits.minHysteresis > 0 && limits.minHysteresis <= limits.maxHysteresis;
}

bool validSettings(const ControllerLimits& limits, float target, float hysteresis) {
  return validLimits(limits) && isfinite(target) && isfinite(hysteresis) &&
    target >= limits.minTarget && target <= limits.maxTarget &&
    hysteresis >= limits.minHysteresis && hysteresis <= limits.maxHysteresis;
}

// Nicht-endliche Konfigurationswerte dürfen kein ungültiges JSON erzeugen.
String jsonNumber(float value) {
  return isfinite(value) ? String(value, 2) : String("null");
}

// Alle Zeichen prüfen: String::toFloat() würde z.B. "abc" als 0 akzeptieren.
bool parseNumber(String value, float& result) {
  value.trim();
  if (value.length() == 0 || value.length() > 16) return false;
  char* end = nullptr;
  result = strtof(value.c_str(), &end);
  return end != value.c_str() && *end == '\0' && isfinite(result);
}

const char* pumpDirectionName(PumpDirection direction) {
  return direction == PumpDirection::CombiToBuffer
    ? "Kombispeicher -> Pufferspeicher" : "Pufferspeicher -> Kombispeicher";
}

void servicePump() {
  pump.update(millis());
  // Immer zuerst ausschalten; die Richtung wird ausschließlich bei AUS verändert.
  if (!SIMULATION_MODE) {
    if (!pump.on()) digitalWrite(PUMP_ENABLE_PIN, HIGH);
    digitalWrite(PUMP_DIRECTION_PIN, pump.direction() == DIRECTION_RELAY_OFF_DIRECTION ? HIGH : LOW);
    if (pump.on()) digitalWrite(PUMP_ENABLE_PIN, LOW);
  }

  static bool lastOn = false;
  static PumpDirection lastDirection = DIRECTION_RELAY_OFF_DIRECTION;
  static PumpPhase lastPhase = PumpPhase::Ready;
  if (lastOn != pump.on() || lastDirection != pump.direction() || lastPhase != pump.phase()) {
    printMeasurementHeader();
    FirmwareLog.printf("%sPumpe: %s | Richtung: %s | Steuerung: %s",
      SIMULATION_MODE ? "[SIMULATION] " : "", pump.on() ? "AN" : "AUS", pumpDirectionName(pump.direction()), manualControl ? "Manuell" : "Automatik");
    if (pump.phase() == PumpPhase::Stopping) FirmwareLog.print(" | Richtungswechsel: 2 Sekunden Ausschaltpause");
    if (pump.phase() == PumpPhase::Settling) FirmwareLog.print(" | Richtung geschaltet, Relais schwingt aus");
    FirmwareLog.println();
    FirmwareLog.println();
    lastOn = pump.on();
    lastDirection = pump.direction();
    lastPhase = pump.phase();
  }
}

void updateAutomaticPump() {
  if (modeTransition || otaActive || otaVerificationPending) return;
  if (manualControl) { automaticControl.reset(); return; }
  TemperatureController& combi = controllers[winterMode ? 1 : 0];
  TemperatureController& buffer = controllers[2];
  const TransferDemand demand = automaticControl.update(AUTOMATIC_PUMP_CONFIGURED && storageReady && autoSettingsValid,
    allowCharging, combi.valid && combi.configValid, combi.currentTemp,
    buffer.valid, buffer.currentTemp, combi.targetTemp, automaticThresholds);
  PumpDirection direction = pump.direction();
  if (demand == TransferDemand::CombiToBuffer) direction = PumpDirection::CombiToBuffer;
  if (demand == TransferDemand::BufferToCombi) direction = PumpDirection::BufferToCombi;
  pump.command(demand != TransferDemand::Off, direction, millis());
  servicePump();
}

void initializeTemperatureSensor(TemperatureController& c) {
  c.max->begin(MAX31865_2WIRE);
  // begin() erhält das Filterbit. 60 Hz ausdrücklich setzen: readRTD() wartet
  // 65 ms; bei 60 Hz sind maximal 55 ms, bei 50 Hz bis zu 66 ms erforderlich.
  c.max->enable50Hz(false);
}

void printMeasurementHeader() {
  if (SIMULATION_MODE) FirmwareLog.println("[SIMULATION] Testwerte; keine Relaisausgänge");
  const time_t now = time(nullptr);
  struct tm localTime;
  // Die vorhandene ESP32-Uhr lesen; auf NTP niemals die Regelung warten lassen.
  if (now >= 1704067200 && localtime_r(&now, &localTime) != nullptr) {
    char formatted[24];
    strftime(formatted, sizeof(formatted), "%d.%m.%Y %H:%M:%S", &localTime);
    FirmwareLog.printf("%s %s\n", formatted, localTime.tm_isdst > 0 ? "MESZ" : "MEZ");
  } else {
    FirmwareLog.printf("Datum/Zeit: noch nicht synchronisiert | Laufzeit: %lu s\n",
      millis() / 1000UL);
  }

  const bool apActive = (WiFi.getMode() & WIFI_AP) != 0;
  if (WiFi.status() == WL_CONNECTED) {
    FirmwareLog.printf("IP: %s | WLAN: %s", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str());
    if (apActive) {
      FirmwareLog.printf(" | ESP32-Zugang: %s", WiFi.softAPIP().toString().c_str());
    }
    FirmwareLog.println();
  } else if (apActive) {
    FirmwareLog.printf("IP: %s | WLAN: %s (ESP32-Zugang) | Heimnetz: nicht verbunden\n",
      WiFi.softAPIP().toString().c_str(), AP_SSID);
  } else {
    FirmwareLog.println("IP: - | WLAN: noch nicht verbunden");
  }
}

// Sanfte, pro Neustart unterschiedlich phasenverschobene Testkurven. Die
// eingegebenen Temperaturen bleiben die Bezugswerte und jederzeit veränderbar.
float simulatedTemperature(size_t sensor, float minutes) {
  const float phase = simulationPhase + float(sensor) * 1.17f;
  const float offset = 1.2f * (sinf(minutes * 0.04f + phase) - sinf(phase)) +
    0.5f * (sinf(minutes * 0.11f + phase * 1.7f) - sinf(phase * 1.7f));
  return constrain(simulationTemperatures[sensor] + offset, -200.0f, 850.0f);
}

void measureTemperatures() {
  bool logBlockStarted = false;
  for (int i = 0; i < numControllers; i++) {
    TemperatureController& c = controllers[i];
    const bool previouslyFailed = c.sensorError != nullptr;
    SensorReading reading = {};
    if (SIMULATION_MODE) {
      reading.temperature = simulationFaults[i] ? NAN : simulationVariation
        ? simulatedTemperature(i, millis() / 60000.0f) : simulationTemperatures[i];
      reading.error = simulationFaults[i] ? "Simulierter Sensorfehler" : nullptr;
    } else {
      reading = readTemperatureSensor(*c.max, *c.diagnosticSpi, RNOMINAL, RREF);
    }
    c.rawRtd = reading.rawRtd;
    c.currentTemp = reading.temperature;
    c.fault = reading.fault;
    c.sensorError = reading.error;
    c.valid = reading.valid();

    // Nur die Ausgabe filtern: Messung und Relaisregelung laufen in jeder Stufe.
    if (SENSOR_LOG_LEVEL > 0 || !c.valid || previouslyFailed) {
      if (!logBlockStarted) {
        printMeasurementHeader();
        logBlockStarted = true;
      }
      if (!c.valid) {
        if (SIMULATION_MODE) {
          FirmwareLog.printf("FEHLER bei %s: %s\n", c.name, c.sensorError);
        } else {
          FirmwareLog.printf("FEHLER bei %s: %s | Rohwert: %u | Code: 0x%X | Grenzen: 0x%04X/0x%04X | CFG vor/Bias/nachADC: 0x%02X/0x%02X/0x%02X%s. Neuinitialisierung.\n",
            c.name, c.sensorError, unsigned(c.rawRtd), unsigned(c.fault),
            unsigned(reading.lowerThreshold), unsigned(reading.upperThreshold),
            unsigned(reading.configBefore), unsigned(reading.configWithBias), unsigned(reading.configAfterConversion),
            manualControl ? " | Handbetrieb aktiv" :
            (i == (winterMode ? 1 : 0) || i == 2) ? " | Automatik: Pumpe AUS" : " | nur Anzeige");
        }
      } else {
        if (i < 2) {
          FirmwareLog.printf("%s: %.1f°C | Soll: %.1f°C | Sollwert-Startabstand: %.1f K",
            c.name, c.currentTemp, c.targetTemp, automaticThresholds.targetOffset);
        } else {
          FirmwareLog.printf("%s: %.1f°C", c.name, c.currentTemp);
        }
        if (previouslyFailed) FirmwareLog.print(" | Sensor wieder verfügbar");
        if (SENSOR_LOG_LEVEL >= 2 && !SIMULATION_MODE) {
          FirmwareLog.printf(" | Rohwert: %u | CFG vor/Bias/nachADC: 0x%02X/0x%02X/0x%02X",
            unsigned(c.rawRtd), unsigned(reading.configBefore), unsigned(reading.configWithBias), unsigned(reading.configAfterConversion));
        }
        FirmwareLog.println();
      }
    }
    if (!c.valid && !SIMULATION_MODE) {
      // Nur der Wandler wird neu aufgesetzt; Handbetrieb bleibt unbeeinflusst.
      // Die nächste reguläre Messung prüft erneut; keine zusätzliche Warteschleife.
      initializeTemperatureSensor(c);
    }
  }
  // Nur tatsächlich ausgegebene Blöcke mit einer Leerzeile abschließen.
  if (logBlockStarted) FirmwareLog.println();
  updateAutomaticPump();
  const time_t now = time(nullptr);
  if (temperatureHistory.available() && now >= 1704067200) {
    // Ausschließlich im Testmodus einen erkennbaren Beispielverlauf erzeugen.
    // Die aktuelle Minute kommt anschließend aus derselben Messung wie die UI.
    const uint32_t nowMinute = uint32_t(now / 60);
    if (SIMULATION_MODE && temperatureHistory.size() == 0 && nowMinute >= 359) {
      const float uptimeMinutes = millis() / 60000.0f;
      for (uint32_t age = 359; age > 0; --age) {
        TemperatureHistory::Sample demo;
        for (size_t i = 0; i < TemperatureHistory::SENSOR_COUNT; ++i) {
          demo.values[i] = simulationFaults[i] ? TemperatureHistory::MISSING :
            int16_t(lroundf(simulatedTemperature(i, uptimeMinutes - float(age)) * 10.0f));
        }
        temperatureHistory.record(nowMinute - age, demo);
        if ((age & 63U) == 0) { servicePump(); yield(); }
      }
      FirmwareLog.println("[SIMULATION] Sechs Stunden Beispielverlauf erzeugt.");
    }
    TemperatureHistory::Sample sample;
    for (size_t i = 0; i < TemperatureHistory::SENSOR_COUNT; ++i) {
      const TemperatureController& c = controllers[i];
      sample.values[i] = c.valid && isfinite(c.currentTemp) && c.currentTemp >= -200.0f && c.currentTemp <= 850.0f
        ? int16_t(lroundf(c.currentTemp * 10.0f)) : TemperatureHistory::MISSING;
    }
    temperatureHistory.record(nowMinute, sample);
  }
}

// SSID ist frei wählbar: Anführungszeichen und Steuerzeichen JSON-sicher ausgeben.
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

bool validWifiCredentials(const String& ssid, const String& password) {
  // WLAN-Namen dürfen Leerzeichen enthalten, deshalb kein trim().
  return ssid.length() >= 1 && ssid.length() <= 32 &&
    strlen(ssid.c_str()) == ssid.length() &&
    strlen(password.c_str()) == password.length() &&
    (password.length() == 0 || (password.length() >= 8 && password.length() <= 63));
}

void queueWifiChange() {
  wifiPhase = WIFI_QUEUED;
  wifiPhaseSince = millis();
}

void loadWifiCredentials() {
  WifiCredentials saved = {};
  if (storageReady && preferences.getBytesLength("wifi") == sizeof(saved) &&
      preferences.getBytes("wifi", &saved, sizeof(saved)) == sizeof(saved) &&
      memchr(saved.ssid, '\0', sizeof(saved.ssid)) != nullptr &&
      memchr(saved.password, '\0', sizeof(saved.password)) != nullptr &&
      validWifiCredentials(String(saved.ssid), String(saved.password))) {
    wifiCredentials = saved;
    queueWifiChange();
  }
}

// Keine Warteschleife: Die Temperaturregelung läuft beim Verbinden weiter.
void processWifi() {
  unsigned long elapsed = millis() - wifiPhaseSince;
  switch (wifiPhase) {
    case WIFI_QUEUED:
      // HTTP-Antwort vor dem Wechsel der bisherigen WLAN-Verbindung senden.
      if (elapsed < 500) return;
      WiFi.disconnect(false, true); // AP bleibt an; alte STA-Konfiguration löschen.
      wifiPhaseSince = millis();
      wifiPhase = wifiCredentials.ssid[0] ? WIFI_STARTING : WIFI_UNCONFIGURED;
      break;
    case WIFI_STARTING:
      if (elapsed < 200) return;
      WiFi.begin(wifiCredentials.ssid, wifiCredentials.password);
      wifiPhase = WIFI_CONNECTING;
      wifiPhaseSince = millis();
      break;
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiPhase = WIFI_CONNECTED;
        // Startet die NTP-Synchronisierung im Hintergrund, ohne auf sie zu warten.
        // Nach WLAN-Wiederverbindung erneut anstoßen; SNTP synchronisiert regelmäßig.
        configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
        FirmwareLog.print("Heimnetz-Webinterface: http://"); FirmwareLog.println(WiFi.localIP());
      } else if (elapsed >= WIFI_CONNECT_TIMEOUT) {
        WiFi.disconnect(false, false);
        wifiPhase = WIFI_RETRY_WAIT;
        wifiPhaseSince = millis();
        FirmwareLog.println("WLAN-Verbindung fehlgeschlagen. ESP32-Zugang bleibt erreichbar.");
      }
      break;
    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        wifiPhase = WIFI_RETRY_WAIT;
        wifiPhaseSince = millis();
      }
      break;
    case WIFI_RETRY_WAIT:
      // Den nächsten Verbindungsversuch erst nach der laufenden Suche starten.
      if (elapsed >= WIFI_RETRY_INTERVAL &&
          !(wifiScanRequested && WiFi.scanComplete() == WIFI_SCAN_RUNNING)) queueWifiChange();
      break;
    case WIFI_UNCONFIGURED:
      break;
  }
}

String wifiStatusJson() {
  const char* states[] = {"unconfigured", "queued", "starting", "connecting", "connected", "retry"};
  bool connected = wifiPhase == WIFI_CONNECTED && WiFi.status() == WL_CONNECTED;
  return "{\"state\":" + jsonString(states[wifiPhase]) +
    ",\"ssid\":" + jsonString(wifiCredentials.ssid) +
    ",\"changeId\":" + jsonString(wifiChangeId) +
    ",\"ip\":" + jsonString(connected ? WiFi.localIP().toString() : String("")) +
    ",\"apIp\":" + jsonString(WiFi.softAPIP().toString()) + "}";
}

void startWifiScan() {
  if (modeTransition) { server.send(409, "text/plain", "Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden.");
    return;
  }
  if (wifiPhase == WIFI_QUEUED || wifiPhase == WIFI_STARTING || wifiPhase == WIFI_CONNECTING) {
    server.send(409, "text/plain; charset=utf-8", "WLAN-Verbindung wird aufgebaut. Danach erneut suchen.");
    return;
  }
  if (wifiScanRequested && WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    server.send(409, "text/plain; charset=utf-8", "Eine WLAN-Suche läuft bereits.");
    return;
  }
  // Asynchron: Die Messungen und der Pumpen-Zustandsautomat laufen weiter.
  int16_t result = WiFi.scanNetworks(true, false, false, 200);
  wifiScanRequested = result == WIFI_SCAN_RUNNING;
  server.sendHeader("Cache-Control", "no-store");
  if (!wifiScanRequested) {
    server.send(503, "text/plain; charset=utf-8", "WLAN-Suche konnte nicht gestartet werden. Bitte erneut versuchen.");
    return;
  }
  server.send(202, "application/json", "{\"state\":\"scanning\"}");
}

void sendWifiScan() {
  server.sendHeader("Cache-Control", "no-store");
  if (!wifiScanRequested) {
    server.send(200, "application/json", "{\"state\":\"idle\"}");
    return;
  }
  const int16_t count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", "{\"state\":\"scanning\"}");
    return;
  }
  if (count < 0) {
    server.send(200, "application/json", "{\"state\":\"failed\"}");
    return;
  }
  String json = "{\"state\":\"complete\",\"networks\":[";
  bool first = true;
  // MAX 40 Ergebnisse begrenzt Speicherbedarf und Antwortgröße.
  for (int i = 0; i < count && i < 40; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    if (!first) json += ',';
    first = false;
    json += "{\"ssid\":" + jsonString(ssid) + ",\"rssi\":" + String(WiFi.RSSI(i)) +
      ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  json += "]}";
  server.send(200, "application/json; charset=utf-8", json);
}

bool allowWifiChange() {
  if (modeTransition) { server.send(409, "text/plain", "Neustart läuft."); return false; }
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden.");
    return false;
  }
  if (!storageReady) {
    server.send(503, "text/plain; charset=utf-8", "Speicher nicht verfügbar. WLAN wurde nicht geändert.");
    return false;
  }
  if (wifiScanRequested && WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    server.send(409, "text/plain; charset=utf-8", "Bitte das Ende der WLAN-Suche abwarten.");
    return false;
  }
  String id = server.arg("changeId");
  if (id.length() > 48) {
    server.send(400, "text/plain; charset=utf-8", "Ungültige Änderungskennung.");
    return false;
  }
  for (size_t i = 0; i < id.length(); i++) {
    const char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
      server.send(400, "text/plain; charset=utf-8", "Ungültige Änderungskennung.");
      return false;
    }
  }
  return true;
}

void saveWifi() {
  if (!allowWifiChange()) return;
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  if (!server.hasArg("password") || !validWifiCredentials(ssid, password)) {
    server.send(400, "text/plain; charset=utf-8", "WLAN-Name: 1–32 Bytes. Passwort: 8–63 Bytes oder leer für ein offenes WLAN.");
    return;
  }
  WifiCredentials next = {};
  ssid.toCharArray(next.ssid, sizeof(next.ssid));
  password.toCharArray(next.password, sizeof(next.password));
  // Beide Werte zusammen speichern; Flash nur bei geänderten Daten beschreiben.
  if (memcmp(&next, &wifiCredentials, sizeof(next)) != 0 &&
      preferences.putBytes("wifi", &next, sizeof(next)) != sizeof(next)) {
    server.send(500, "text/plain; charset=utf-8", "Speicherfehler. WLAN wurde nicht geändert.");
    return;
  }
  wifiCredentials = next;
  wifiChangeId = server.arg("changeId");
  queueWifiChange();
  server.sendHeader("Cache-Control", "no-store");
  server.send(202, "application/json", "{\"saved\":true}");
}

void resetWifi() {
  if (!allowWifiChange()) return;
  // Nur WLAN löschen. Sollwerte und Hysteresen bleiben erhalten.
  if (preferences.isKey("wifi") && !preferences.remove("wifi")) {
    server.send(500, "text/plain; charset=utf-8", "Gespeichertes WLAN konnte nicht gelöscht werden.");
    return;
  }
  memset(&wifiCredentials, 0, sizeof(wifiCredentials));
  wifiChangeId = server.arg("changeId");
  queueWifiChange();
  server.sendHeader("Cache-Control", "no-store");
  server.send(202, "application/json", "{\"forgotten\":true}");
}

String clockStatusJson() {
  const time_t now = time(nullptr);
  struct tm localTime;
  // Nach Kaltstart beginnt die Uhr bei 1970. Keine blockierende getLocalTime()-Abfrage.
  if (now < 1704067200 || localtime_r(&now, &localTime) == nullptr) {
    return "{\"available\":false,\"local\":null,\"zone\":null}";
  }
  char formatted[24];
  strftime(formatted, sizeof(formatted), "%d.%m.%Y %H:%M:%S", &localTime);
  return "{\"available\":true,\"local\":" + jsonString(formatted) +
    ",\"zone\":" + jsonString(localTime.tm_isdst > 0 ? "MESZ" : "MEZ") + "}";
}

// UDP-Lebenszeichen aus demselben Heimnetz empfangen. Es steuert keine Relais.
void processRoofHeartbeat() {
  int packetSize;
  while ((packetSize = roofUdp.parsePacket()) > 0) {
    char packet[sizeof(ROOF_HEARTBEAT)] = {};
    IPAddress sender = roofUdp.remoteIP();
    int length = roofUdp.read(packet, sizeof(packet));
    while (roofUdp.available()) roofUdp.read();
    if (WiFi.status() != WL_CONNECTED ||
        packetSize != sizeof(ROOF_HEARTBEAT) - 1 ||
        length != sizeof(ROOF_HEARTBEAT) - 1 ||
        memcmp(packet, ROOF_HEARTBEAT, sizeof(ROOF_HEARTBEAT) - 1) != 0) continue;
    IPAddress local = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    bool sameSubnet = true;
    for (int i = 0; i < 4; i++) {
      if ((sender[i] & mask[i]) != (local[i] & mask[i])) sameSubnet = false;
    }
    if (sameSubnet) {
      roofSeen = true;
      lastRoofHeartbeat = millis();
    }
  }
}

String pumpStatusJson() {
  const char* phase = pump.phase() == PumpPhase::Stopping ? "stopping"
    : pump.phase() == PumpPhase::Settling ? "settling" : "ready";
  return "{\"manual\":" + String(manualControl ? "true" : "false") +
    ",\"on\":" + String(pump.on() ? "true" : "false") +
    ",\"requestedOn\":" + String(pump.requestedOn() ? "true" : "false") +
    ",\"direction\":" + String(pump.direction() == PumpDirection::BufferToCombi ? 1 : 0) +
    ",\"requestedDirection\":" + String(pump.requestedDirection() == PumpDirection::BufferToCombi ? 1 : 0) +
    ",\"phase\":\"" + phase + "\",\"switching\":" + String(pump.switching() ? "true" : "false") +
    ",\"remainingMs\":" + String(pump.remainingMs(millis())) +
    ",\"automaticConfigured\":" + String(AUTOMATIC_PUMP_CONFIGURED && storageReady && autoSettingsValid ? "true" : "false") + "}";
}

String automaticStatusJson() {
  return "{\"winter\":" + String(winterMode ? "true" : "false") +
    ",\"allowCharging\":" + String(allowCharging ? "true" : "false") +
    ",\"configValid\":" + String(storageReady && autoSettingsValid ? "true" : "false") +
    ",\"targetOffset\":" + jsonNumber(automaticThresholds.targetOffset) +
    ",\"differenceStart\":" + jsonNumber(automaticThresholds.differenceStart) +
    ",\"differenceStop\":" + jsonNumber(automaticThresholds.differenceStop) + "}";
}

// The browser receives at most 400 averaged points; every minute remains in
// PSRAM. Any invalid minute makes its sensor's display bucket a gap.
void sendTemperatureHistory() {
  const String range = server.arg("range");
  uint32_t minutes = range == "6h" ? 360 : range == "24h" || !range.length() ? 1440 :
    range == "7d" ? 10080 : range == "30d" ? 43200 : range == "365d" ? 525600 : 0;
  if (!minutes) { server.send(400, "text/plain; charset=utf-8", "Ungültiger Zeitraum."); return; }
  String json;
  if (!json.reserve(24000)) { server.send(503, "text/plain", "Nicht genügend Speicher."); return; }
  json = "{\"available\":" + String(temperatureHistory.available() ? "true" : "false") +
    ",\"samples\":" + String(unsigned(temperatureHistory.size())) +
    ",\"capacity\":" + String(unsigned(temperatureHistory.capacity())) +
    ",\"psramBytes\":" + String(ESP.getPsramSize()) + ",\"points\":[";
  const time_t now = time(nullptr);
  if (temperatureHistory.size() && now >= 1704067200) {
    const uint32_t nowMinute = uint32_t(now / 60);
    const uint32_t first = temperatureHistory.firstMinute(), last = temperatureHistory.lastMinute();
    const uint32_t requestedFirst = nowMinute >= minutes - 1 ? nowMinute - minutes + 1 : 0;
    const uint32_t from = first > requestedFirst ? first : requestedFirst;
    const uint32_t to = last < nowMinute ? last : nowMinute;
    if (from <= to) {
      const uint32_t total = to - from + 1, step = (total + 399) / 400;
      bool firstPoint = true;
      for (uint32_t offset = 0; offset < total; offset += step) {
        const uint32_t n = total - offset < step ? total - offset : step;
        int32_t sum[TemperatureHistory::SENSOR_COUNT] = {};
        bool invalid[TemperatureHistory::SENSOR_COUNT] = {};
        for (uint32_t j = 0; j < n; ++j) {
          const auto* entry = temperatureHistory.at(size_t(from - first + offset + j));
          for (size_t sensor = 0; sensor < TemperatureHistory::SENSOR_COUNT; ++sensor) {
            const int16_t value = entry->values[sensor];
            if (value == TemperatureHistory::MISSING) invalid[sensor] = true;
            else sum[sensor] += value;
          }
          if ((j & 1023U) == 0) { servicePump(); yield(); }
        }
        if (!firstPoint) json += ',';
        firstPoint = false;
        json += '['; json += String((from + offset + n / 2) * 60UL);
        for (size_t sensor = 0; sensor < TemperatureHistory::SENSOR_COUNT; ++sensor) {
          json += ',';
          json += invalid[sensor] ? String("null") : String(float(sum[sensor]) / float(n) / 10.0f, 1);
        }
        json += ']';
      }
    }
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

void sendDeviceInfo() {
  esp_chip_info_t chip;
  esp_chip_info(&chip);
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  const String core = String(ESP_ARDUINO_VERSION_MAJOR) + "." + ESP_ARDUINO_VERSION_MINOR + "." + ESP_ARDUINO_VERSION_PATCH;
  const String revision = String(chip.full_revision / 100) + "." + String(chip.full_revision % 100);
  const auto slotJson = [](const esp_partition_t* p) -> String {
    return p ? "{\"label\":" + jsonString(p->label) + ",\"size\":" + String(unsigned(p->size)) + "}" : "null";
  };
  String json;
  json.reserve(700);
  json = "{\"firmware\":" + jsonString(FIRMWARE_VERSION) + ",\"chip\":" + jsonString(ESP.getChipModel()) +
    ",\"revision\":" + jsonString(revision) + ",\"cores\":" + String(chip.cores) +
    ",\"cpuMHz\":" + String(ESP.getCpuFreqMHz()) + ",\"arduinoCore\":" + jsonString(core) +
    ",\"idf\":" + jsonString(ESP.getSdkVersion()) + ",\"uptimeSeconds\":" + String((unsigned long)(esp_timer_get_time() / 1000000ULL)) +
    ",\"flashBytes\":" + String(ESP.getFlashChipSize()) + ",\"heapBytes\":" + String(ESP.getHeapSize()) +
    ",\"heapFreeBytes\":" + String(ESP.getFreeHeap()) + ",\"psramBytes\":" + String(ESP.getPsramSize()) +
    ",\"psramFreeBytes\":" + String(ESP.getFreePsram()) + ",\"runningSlot\":" + slotJson(running) + ",\"updateSlot\":" + slotJson(next) + "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

void sendStatus() {
  String json;
  json.reserve(1400);
  json = "{\"version\":\"" + String(FIRMWARE_VERSION) + "\",\"simulation\":" + String(SIMULATION_MODE ? "true" : "false") + ",\"controllers\":[";
  for (int i = 0; i < numControllers; i++) {
    TemperatureController& c = controllers[i];
    if (i) json += ',';
    // Namen sind feste Zeichenketten im Sketch, keine Benutzereingaben.
    json += "{\"id\":" + String(i) + ",\"name\":\"" + String(c.name) +
      "\",\"temperature\":" + (c.valid ? String(c.currentTemp, 1) : String("null")) +
      ",\"valid\":" + String(c.valid ? "true" : "false") +
      ",\"fault\":" + String(c.fault) +
      ",\"rawRtd\":" + String(c.rawRtd) +
      ",\"sensorError\":" + (c.sensorError ? jsonString(c.sensorError) : String("null")) +
      ",\"hasRelay\":false,\"hasSettings\":" + String(c.settingsSlot >= 0 ? "true" : "false");
    if (c.settingsSlot >= 0) {
      json += ",\"target\":" + jsonNumber(c.targetTemp) + ",\"hysteresis\":" + jsonNumber(c.hysteresis) +
        ",\"configValid\":" + String(c.configValid ? "true" : "false") +
        ",\"limits\":{\"minTarget\":" + jsonNumber(c.limits->minTarget) +
        ",\"maxTarget\":" + jsonNumber(c.limits->maxTarget) +
        ",\"minHysteresis\":" + jsonNumber(c.limits->minHysteresis) +
        ",\"maxHysteresis\":" + jsonNumber(c.limits->maxHysteresis) + "}}";
    } else json += "}";
  }
  if (SIMULATION_MODE) {
    json += "],\"simulationVariation\":" + String(simulationVariation ? "true" : "false") + ",\"simulationValues\":[";
    for (int i = 0; i < numControllers; i++) {
      if (i) json += ',';
      json += "{\"temperature\":" + jsonNumber(simulationTemperatures[i]) +
        ",\"fault\":" + String(simulationFaults[i] ? "true" : "false") + "}";
    }
  }
  bool roofOnline = roofSeen && WiFi.status() == WL_CONNECTED && millis() - lastRoofHeartbeat < ROOF_TIMEOUT;
  json += "],\"pump\":" + pumpStatusJson() + ",\"automatic\":" + automaticStatusJson() +
    ",\"wifi\":" + wifiStatusJson() + ",\"clock\":" + clockStatusJson() +
    ",\"roof\":{\"seen\":" + String(roofSeen ? "true" : "false") +
    ",\"online\":" + String(roofOnline ? "true" : "false") +
    ",\"ageSeconds\":" + (roofSeen ? String((millis() - lastRoofHeartbeat) / 1000) : String("null")) + "}}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void saveSettings() {
  if (otaActive || otaVerificationPending) { server.send(409, "text/plain", "Firmware-Update läuft."); return; }
  if (modeTransition) { server.send(409, "text/plain", "Neustart läuft."); return; }
  // Verhindert Änderungen durch fremde Webseiten/Formulare (kein CORS erlaubt).
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden.");
    return;
  }
  int index = -1;
  for (int i = 0; i < numControllers; i++) {
    if (server.arg("id") == String(i)) index = i;
  }
  float target, hysteresis;
  if (index < 0 || controllers[index].settingsSlot < 0 || !parseNumber(server.arg("target"), target) ||
      !parseNumber(server.arg("hysteresis"), hysteresis) || !validSettings(*controllers[index].limits, target, hysteresis)) {
    server.send(400, "text/plain; charset=utf-8", "Ungültige Werte oder Einstellgrenzen überschritten.");
    return;
  }
  TemperatureController& c = controllers[index];
  if (!c.configValid) {
    server.send(409, "text/plain; charset=utf-8", "Konfigurationsfehler. Grenzen und Standardwerte im Sketch prüfen.");
    return;
  }
  if (target != c.targetTemp || hysteresis != c.hysteresis) {
    // Ein NVS-Blob pro Heizkreis: Sollwert und Hysterese gemeinsam speichern.
    float values[2] = {target, hysteresis};
    String key = "c" + String(int(c.settingsSlot));
    if (!storageReady || preferences.putBytes(key.c_str(), values, sizeof(values)) != sizeof(values)) {
      server.send(500, "text/plain; charset=utf-8", "Speicherfehler. Einstellungen wurden nicht übernommen.");
      return;
    }
    c.targetTemp = target;
    c.hysteresis = hysteresis;
    // Aktuelle Messung vor der Anwendung geänderter Schaltgrenzen.
    measureTemperatures();
    lastMeasurement = millis();
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"saved\":true}");
}

void saveAutomaticSettings() {
  if (otaActive || otaVerificationPending) { server.send(409, "text/plain", "Firmware-Update läuft."); return; }
  if (modeTransition) { server.send(409, "text/plain", "Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden.");
    return;
  }
  const String winter = server.arg("winter"), charge = server.arg("charge");
  AutomaticThresholds next;
  if (server.args() != 5 || (winter != "0" && winter != "1") || (charge != "0" && charge != "1") ||
      !parseNumber(server.arg("offset"), next.targetOffset) ||
      !parseNumber(server.arg("start"), next.differenceStart) ||
      !parseNumber(server.arg("stop"), next.differenceStop) || !AutomaticControl::validThresholds(next)) {
    server.send(400, "text/plain; charset=utf-8", "Ungültige Schaltabstände. Stopp muss kleiner als Start sein.");
    return;
  }
  const float saved[5] = {next.targetOffset, next.differenceStart, next.differenceStop,
    winter == "1" ? 1.0f : 0.0f, charge == "1" ? 1.0f : 0.0f};
  if (!storageReady || preferences.putBytes("auto1", saved, sizeof(saved)) != sizeof(saved)) {
    server.send(500, "text/plain; charset=utf-8", "Speicherfehler. Regelung wurde nicht übernommen.");
    return;
  }
  if (!manualControl) {
    pump.command(false, pump.direction(), millis());
    servicePump();
  }
  automaticControl.reset();
  automaticThresholds = next;
  autoSettingsValid = true;
  winterMode = winter == "1";
  allowCharging = charge == "1";
  measureTemperatures();
  lastMeasurement = millis();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", automaticStatusJson());
}

void saveSimulationValues() {
  if (modeTransition) { server.send(409, "text/plain", "Neustart läuft."); return; }
  if (!SIMULATION_MODE) {
    server.send(404, "text/plain; charset=utf-8", "Nicht gefunden.");
    return;
  }
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden.");
    return;
  }
  float temperatures[sizeof(controllers) / sizeof(controllers[0])];
  bool faults[sizeof(controllers) / sizeof(controllers[0])];
  if (server.args() != numControllers * 2 &&
      !(server.args() == numControllers * 2 + 1 && server.hasArg("variation"))) {
    server.send(400, "text/plain; charset=utf-8", "Bitte alle fünf Sensorwerte angeben.");
    return;
  }
  for (int i = 0; i < numControllers; i++) {
    const String tempKey = "t" + String(i), faultKey = "f" + String(i);
    const String fault = server.arg(faultKey);
    if (!server.hasArg(tempKey) || !server.hasArg(faultKey) ||
        !parseNumber(server.arg(tempKey), temperatures[i]) ||
        temperatures[i] < -200.0f || temperatures[i] > 850.0f || (fault != "0" && fault != "1")) {
      server.send(400, "text/plain; charset=utf-8", "Ungültiger Testwert oder Sensorfehlerstatus.");
      return;
    }
    faults[i] = fault == "1";
  }
  if (server.hasArg("variation") && server.arg("variation") != "0" && server.arg("variation") != "1") {
    server.send(400, "text/plain; charset=utf-8", "Ungültiger Variationsstatus.");
    return;
  }
  // Alte Clients, die nur Sensorwerte senden, erhalten weiterhin feste Werte.
  simulationVariation = server.hasArg("variation") && server.arg("variation") == "1";
  for (int i = 0; i < numControllers; i++) {
    simulationTemperatures[i] = temperatures[i];
    simulationFaults[i] = faults[i];
  }
  FirmwareLog.println("[SIMULATION] Neue Testwerte übernommen.");
  measureTemperatures();
  lastMeasurement = millis();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"saved\":true}");
}

void savePumpControl() {
  if (otaActive || otaVerificationPending) { server.send(409, "text/plain", "Firmware-Update läuft; Pumpe bleibt aus."); return; }
  if (modeTransition) { server.send(409, "text/plain", "Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden.");
    return;
  }
  const String action = server.arg("action");
  const String value = server.arg("value");
  if (server.args() != 2 || (value != "0" && value != "1") ||
      (action != "manual" && action != "power" && action != "direction")) {
    server.send(400, "text/plain; charset=utf-8", "Ungültiger Pumpenbefehl. Bitte die Seite neu laden.");
    return;
  }
  const bool enabled = value == "1";
  if (action == "manual") {
    if (manualControl != enabled) {
      manualControl = enabled;
      automaticControl.reset();
      manualPumpOn = false;
      manualDirection = pump.direction();
      pump.command(false, manualDirection, millis());
      servicePump();
      FirmwareLog.printf("Manuelle Steuerung: %s | Pumpe zunächst AUS\n\n", manualControl ? "AN" : "AUS");
      if (!manualControl) {
        measureTemperatures();
        lastMeasurement = millis();
      }
    }
  } else {
    if (!manualControl) {
      server.send(409, "text/plain; charset=utf-8", "Zuerst Manuelle Steuerung einschalten.");
      return;
    }
    if (action == "power") manualPumpOn = enabled;
    if (action == "direction") {
      manualDirection = enabled ? PumpDirection::BufferToCombi : PumpDirection::CombiToBuffer;
    }
    pump.command(manualPumpOn, manualDirection, millis());
    servicePump();
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", pumpStatusJson());
}

void setTestMode() {
  if (otaActive || otaVerificationPending) { server.send(409, "text/plain", "Firmware-Update läuft."); return; }
  if (modeTransition) { server.send(409, "text/plain", "Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden."); return;
  }
  const String enabled = server.arg("enabled");
  if (server.args() != 1 || (enabled != "0" && enabled != "1")) {
    server.send(400, "text/plain; charset=utf-8", "Ungültige Betriebsart."); return;
  }
  const bool next = enabled == "1";
  if (next == SIMULATION_MODE) { server.send(200, "application/json", "{\"changed\":false}"); return; }
  if (!storageReady) { server.send(503, "text/plain; charset=utf-8", "Speicher nicht verfügbar."); return; }

  // Bei einem Modus ohne WLAN-Einstellungen den aktuellen Zugang übernehmen.
  Preferences other;
  if (!other.begin(next ? "heating-test" : "heating", false)) {
    server.send(500, "text/plain; charset=utf-8", "Zielspeicher nicht verfügbar."); return;
  }
  bool wifiOk = true;
  if (!other.isKey("wifi") && wifiCredentials.ssid[0])
    wifiOk = other.putBytes("wifi", &wifiCredentials, sizeof(wifiCredentials)) == sizeof(wifiCredentials);
  other.end();
  if (!wifiOk) { server.send(500, "text/plain; charset=utf-8", "WLAN konnte nicht übernommen werden."); return; }

  modeTransition = true;
  pump.command(false, pump.direction(), millis());
  servicePump();
  Preferences mode;
  if (!mode.begin("backload-mode", false)) {
    modeTransition = false; server.send(500, "text/plain; charset=utf-8", "Betriebsart konnte nicht gespeichert werden."); return;
  }
  bool saved = mode.putBool("test", next) == 1;
  mode.end();
  if (!saved) { modeTransition = false; server.send(500, "text/plain; charset=utf-8", "Betriebsart konnte nicht gespeichert werden."); return; }
  // Die Richtung darf erst nach mindestens zwei Sekunden Pumpenstillstand
  // beim Neustart auf ihre Grundstellung zurückfallen.
  modeRestartAt = millis();
  FirmwareLog.printf("Betriebsart: %s angefordert. Pumpe AUS, Neustart nach 2 Sekunden.\n\n", next ? "Test" : "Anlage");
  server.send(202, "application/json", "{\"restarting\":true}");
}

bool otaNumber(const String& value, uint32_t& number) {
  if (!value.length() || value.length() > 10) return false;
  uint64_t n = 0;
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
    n = n * 10 + (value[i] - '0');
    if (n > UINT32_MAX) return false;
  }
  number = uint32_t(n); return true;
}

void cancelOta() {
  if (Update.isRunning()) Update.abort();
  otaActive = false; otaFinished = false; otaToken = "";
  automaticControl.reset();
  FirmwareLog.println("Firmware-Update abgebrochen. Handbetrieb bleibt ausgeschaltet.\n");
}

bool checkOtaRequest() {
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return false; }
  if (!otaActive || otaFinished || githubUpdateActive || server.arg("token") != otaToken) { server.send(409, "text/plain", "Kein passendes Update aktiv."); return false; }
  otaLastActivity = millis();
  return true;
}

void prepareOta() {
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return; }
  if (otaActive || otaVerificationPending || modeTransition) { server.send(409, "text/plain", "Steuerung ist beschäftigt."); return; }
  uint32_t size = 0;
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (server.args() != 1 || !otaNumber(server.arg("size"), size) || size < 1024 || !next || size > next->size) {
    server.send(400, "text/plain; charset=utf-8", "Ungültige Dateigröße oder keine passende OTA-Partition."); return;
  }
  otaActive = true; otaFinished = false;
  manualControl = false; manualPumpOn = false; automaticControl.reset();
  pump.command(false, pump.direction(), millis()); servicePump();
  otaStoppedAt = otaLastActivity = millis();
  if (!Update.begin(size, U_FLASH)) { cancelOta(); server.send(500, "text/plain", "Update konnte nicht gestartet werden."); return; }
  otaExpectedSize = size; otaReceived = 0; otaLastChunkLength = 0;
  char token[17]; snprintf(token, sizeof(token), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  otaToken = token;
  FirmwareLog.println("Firmware-Update vorbereitet. Pumpe AUS.\n");
  server.send(200, "application/json", "{\"token\":\"" + otaToken + "\"}");
}

void writeOtaChunk() {
  if (!checkOtaRequest()) return;
  uint32_t offset = 0;
  const String encoded = server.arg("data");
  static uint8_t decoded[4096]; size_t length = 0;
  if (server.args() != 3 || !otaNumber(server.arg("offset"), offset) ||
      !encoded.length() || encoded.length() > 5464 ||
      mbedtls_base64_decode(decoded, sizeof(decoded), &length, reinterpret_cast<const unsigned char*>(encoded.c_str()), encoded.length()) != 0 ||
      !length) {
    cancelOta(); server.send(400, "text/plain; charset=utf-8", "Ungültiger Datenblock. Update abgebrochen."); return;
  }
  uint8_t hash[32];
  if (mbedtls_sha256_ret(decoded, length, hash, 0) != 0) {
    cancelOta(); server.send(500, "text/plain", "Datenblock konnte nicht geprüft werden."); return;
  }
  // Wenn nur die Bestätigung verloren ging, darf exakt derselbe Block erneut
  // gesendet werden. Ein abweichender oder älterer Block bricht das Update ab.
  if (otaLastChunkLength && offset == otaLastChunkOffset && length == otaLastChunkLength && memcmp(hash, otaLastChunkHash, sizeof(hash)) == 0) {
    server.send(200, "application/json", "{\"received\":" + String(unsigned(otaReceived)) + "}"); return;
  }
  if (offset != otaReceived || length > otaExpectedSize - otaReceived) {
    cancelOta(); server.send(400, "text/plain; charset=utf-8", "Datenblöcke stimmen nicht überein. Update abgebrochen."); return;
  }
  if (Update.write(decoded, length) != length) {
    cancelOta(); server.send(500, "text/plain; charset=utf-8", "Schreibfehler. Update abgebrochen."); return;
  }
  otaLastChunkOffset = offset; otaLastChunkLength = length; memcpy(otaLastChunkHash, hash, sizeof(hash));
  otaReceived += length;
  server.send(200, "application/json", "{\"received\":" + String(unsigned(otaReceived)) + "}");
}

void finishOta() {
  if (!checkOtaRequest()) return;
  if (server.args() != 1 || otaReceived != otaExpectedSize || !Update.end()) {
    cancelOta(); server.send(400, "text/plain; charset=utf-8", "Firmware unvollständig oder ungültig. Update abgebrochen."); return;
  }
  otaFinished = true; otaLastActivity = millis();
  FirmwareLog.println("Firmware übertragen. Pumpe AUS, Neustart folgt.\n");
  server.send(200, "application/json", "{\"restarting\":true}");
}

void abortOta() {
  if (!checkOtaRequest()) return;
  if (server.args() != 1) { server.send(400, "text/plain", "Ungültige Anfrage."); return; }
  cancelOta(); server.send(200, "application/json", "{\"aborted\":true}");
}

// Läuft auf einem eigenen Task: ein langsames HTTPS oder DNS darf die
// Hauptschleife und die Pumpen-Sicherheitslogik nicht blockieren.
void githubDownloadTask(void*) {
  const char* error = nullptr;
  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CERTS);
  client.setHandshakeTimeout(12);
  HTTPClient http;
  http.setConnectTimeout(12000); http.setTimeout(12000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); http.setRedirectLimit(4);
  http.setUserAgent("Backload-ESP32");
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  mbedtls_sha256_starts_ret(&hash, 0);
  if (!http.begin(client, githubUpdateUrl)) error = "HTTPS-Verbindung konnte nicht vorbereitet werden.";
  int code = error ? 0 : http.GET();
  if (!error && code != HTTP_CODE_OK) error = "GitHub-Download nicht erreichbar oder TLS-Prüfung fehlgeschlagen.";
  if (!error && http.getSize() != int(otaExpectedSize)) error = "Dateigröße stimmt nicht mit dem Release überein.";
  uint32_t total = 0, lastData = millis();
  uint8_t buffer[2048];
  WiFiClient* stream = error ? nullptr : http.getStreamPtr();
  while (!error && total < otaExpectedSize) {
    if (githubUpdateCancel.load()) { error = "Update wurde abgebrochen."; break; }
    size_t available = stream->available();
    if (!available) {
      if (!http.connected() || millis() - lastData > 15000) { error = "Verbindung während des Downloads unterbrochen."; break; }
      delay(10); continue;
    }
    size_t length = available < sizeof(buffer) ? available : sizeof(buffer);
    if (length > otaExpectedSize - total) length = otaExpectedSize - total;
    int read = stream->read(buffer, length);
    if (read <= 0) { delay(10); continue; }
    if (Update.write(buffer, size_t(read)) != size_t(read) || mbedtls_sha256_update_ret(&hash, buffer, size_t(read)) != 0) {
      error = "Firmware konnte nicht geschrieben oder geprüft werden."; break;
    }
    total += uint32_t(read); githubUpdateBytes.store(total); lastData = millis();
    delay(1);
  }
  uint8_t digest[32];
  if (!error && mbedtls_sha256_finish_ret(&hash, digest) != 0) error = "SHA-256-Prüfung fehlgeschlagen.";
  if (!error) {
    char actual[65];
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(actual + i * 2, 3, "%02x", digest[i]);
    if (githubExpectedHash != actual) error = "SHA-256-Prüfsumme stimmt nicht mit GitHub überein.";
  }
  mbedtls_sha256_free(&hash); http.end();
  if (error) { snprintf(githubUpdateError, sizeof(githubUpdateError), "%s", error); githubUpdateState.store(GH_FAILED); }
  else githubUpdateState.store(GH_DOWNLOADED);
  vTaskDelete(nullptr);
}

void startGithubOta() {
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return; }
  if (otaActive || otaVerificationPending || modeTransition) { server.send(409, "text/plain", "Steuerung ist beschäftigt."); return; }
  const String url = server.arg("url"), digest = server.arg("sha256");
  const String prefix = "https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/", suffix = "/Backload.ino.bin";
  uint32_t size = 0; const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (server.args() != 3 || !url.startsWith(prefix) || !url.endsWith(suffix) || url.length() <= prefix.length() + suffix.length() ||
      !otaNumber(server.arg("size"), size) || size < 1024 || !next || size > next->size || digest.length() != 64) {
    server.send(400, "text/plain; charset=utf-8", "Ungültiges Release oder keine passende OTA-Partition."); return;
  }
  const String tag = url.substring(prefix.length(), url.length() - suffix.length());
  for (size_t i = 0; i < tag.length(); ++i) {
    const char c = tag[i];
    if (!isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') { server.send(400, "text/plain", "Ungültiger Release-Name."); return; }
  }
  for (size_t i = 0; i < digest.length(); ++i)
    if (!((digest[i] >= '0' && digest[i] <= '9') || (digest[i] >= 'a' && digest[i] <= 'f'))) { server.send(400, "text/plain", "Ungültige SHA-256-Prüfsumme."); return; }
  if (WiFi.status() != WL_CONNECTED) { server.send(503, "text/plain; charset=utf-8", "ESP32 ist nicht mit dem Heimnetz verbunden."); return; }
  otaActive = true; otaFinished = false;
  manualControl = false; manualPumpOn = false; automaticControl.reset();
  pump.command(false, pump.direction(), millis()); servicePump();
  otaStoppedAt = otaLastActivity = millis();
  if (!Update.begin(size, U_FLASH)) { cancelOta(); server.send(500, "text/plain", "Update konnte nicht gestartet werden."); return; }
  otaExpectedSize = size; otaReceived = 0;
  char token[17]; snprintf(token, sizeof(token), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  otaToken = token; githubUpdateToken = token; githubUpdateUrl = url; githubExpectedHash = digest;
  githubUpdateBytes.store(0); githubUpdateCancel.store(false); githubUpdateError[0] = '\0';
  githubUpdateActive = true; githubUpdateState.store(GH_DOWNLOADING);
  if (xTaskCreate(githubDownloadTask, "github-update", 16384, nullptr, 1, nullptr) != pdPASS) {
    githubUpdateActive = false; githubUpdateState.store(GH_FAILED); snprintf(githubUpdateError, sizeof(githubUpdateError), "Download-Task konnte nicht gestartet werden.");
    cancelOta(); server.send(503, "text/plain", "Download konnte nicht gestartet werden."); return;
  }
  FirmwareLog.println("GitHub-Update gestartet. Pumpe AUS.\n");
  server.send(200, "application/json", "{\"token\":\"" + githubUpdateToken + "\"}");
}

void githubOtaStatus() {
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return; }
  if (!githubUpdateToken.length() || server.args() != 1 || server.arg("token") != githubUpdateToken) { server.send(404, "text/plain", "Update nicht gefunden."); return; }
  const int state = githubUpdateState.load();
  const char* label = state == GH_FINISHED ? "finished" : state == GH_FAILED ? "failed" : "downloading";
  server.send(200, "application/json", "{\"state\":\"" + String(label) + "\",\"received\":" + String(githubUpdateBytes.load()) +
    ",\"size\":" + String(unsigned(otaExpectedSize)) + ",\"error\":" + jsonString(state == GH_FAILED ? githubUpdateError : "") + "}");
}

void setup() {
  // Auch im Testbetrieb die echten Active-Low-Ausgänge auf AUS halten.
  digitalWrite(PUMP_ENABLE_PIN, HIGH);
  pinMode(PUMP_ENABLE_PIN, OUTPUT);
  digitalWrite(PUMP_DIRECTION_PIN, HIGH);
  pinMode(PUMP_DIRECTION_PIN, OUTPUT);
  esp_ota_img_states_t imageState;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &imageState) == ESP_OK && imageState == ESP_OTA_IMG_PENDING_VERIFY)
    otaVerificationPending = true;
  Preferences mode;
  if (mode.begin("backload-mode", true)) {
    SIMULATION_MODE = mode.getBool("test", SIMULATION_MODE);
    mode.end();
  }
  AP_SSID = SIMULATION_MODE ? "ESP32-Backload-Test" : "ESP32-Solarsteuerung";
  if (!SIMULATION_MODE) {
    for (int i = 0; i < numControllers; i++) {
      digitalWrite(controllers[i].csPin, HIGH);
      pinMode(controllers[i].csPin, OUTPUT);
    }
    // Software-SPI im Ruhezustand halten, auch während der Startpause (SPI_MODE1).
    digitalWrite(SPI_CLK, LOW);
    pinMode(SPI_CLK, OUTPUT);
    digitalWrite(SPI_MOSI, HIGH);
    pinMode(SPI_MOSI, OUTPUT);
    pinMode(SPI_MISO, INPUT);
    pinMode(LED_PIN, OUTPUT);
  }
  // Zeitzone auch für eine nach Soft-Reset noch gültige interne Uhr setzen.
  setenv("TZ", TIME_ZONE, 1);
  tzset();
  Serial.begin(115200);
  delay(1000);  // Sensorversorgung vor der Initialisierung stabilisieren lassen
  if (SIMULATION_MODE) simulationPhase = float(esp_random() % 6283U) / 1000.0f;
  FirmwareLog.printf("Heizungssteuerung – Firmware %s\n", FIRMWARE_VERSION);
  auto* historyBuffer = static_cast<TemperatureHistory::Sample*>(
    heap_caps_malloc(HISTORY_CAPACITY * sizeof(TemperatureHistory::Sample), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  temperatureHistory.attach(historyBuffer, HISTORY_CAPACITY, [](){ servicePump(); yield(); });
  if (temperatureHistory.available()) FirmwareLog.printf("Temperaturverlauf: %u Minuten im PSRAM reserviert.\n", unsigned(HISTORY_CAPACITY));
  else FirmwareLog.println("Temperaturverlauf nicht verfügbar: PSRAM fehlt oder ist zu klein. Regelung läuft weiter.");
  if (SIMULATION_MODE) FirmwareLog.println("[SIMULATION] Aktiv: simulierte Sensoren und Pumpe; reale Relais bleiben AUS.");
  storageReady = preferences.begin(SIMULATION_MODE ? "heating-test" : "heating", false);
  if (!storageReady) FirmwareLog.println("NVS nicht verfügbar: Speichern nicht möglich.");
  float autoSaved[5];
  if (storageReady && preferences.isKey("auto1")) {
    autoSettingsValid = false;
    if (preferences.getBytesLength("auto1") == sizeof(autoSaved) &&
        preferences.getBytes("auto1", autoSaved, sizeof(autoSaved)) == sizeof(autoSaved)) {
      AutomaticThresholds candidate = {autoSaved[0], autoSaved[1], autoSaved[2]};
      if (AutomaticControl::validThresholds(candidate) && (autoSaved[3] == 0 || autoSaved[3] == 1) &&
          (autoSaved[4] == 0 || autoSaved[4] == 1)) {
        automaticThresholds = candidate;
        winterMode = autoSaved[3] == 1;
        allowCharging = autoSaved[4] == 1;
        autoSettingsValid = true;
      }
    }
    if (!autoSettingsValid) FirmwareLog.println("Ungültige Automatikdaten: Pumpe gesperrt; Einstellungen erneut speichern.");
  }
  for (int i = 0; i < numControllers; i++) {
    TemperatureController& c = controllers[i];
    if (c.settingsSlot >= 0) {
      // Feste Speicherplätze: c0 oben, c1 Puffer, c2 Mitte.
      c.configValid = validSettings(*c.limits, c.targetTemp, c.hysteresis);
      if (!c.configValid) {
        FirmwareLog.printf("KONFIGURATIONSFEHLER bei %s: Automatik bleibt gesperrt.\n", c.name);
      }
      String key = "c" + String(int(c.settingsSlot));
      float values[2];
      if (c.configValid && storageReady && preferences.getBytesLength(key.c_str()) == sizeof(values) &&
          preferences.getBytes(key.c_str(), values, sizeof(values)) == sizeof(values) &&
          validSettings(*c.limits, values[0], values[1])) {
        c.targetTemp = values[0];
        c.hysteresis = values[1];
      }
    }
    // Vier Pin-Argumente wählen Software-SPI, wie im ursprünglichen Sketch.
    if (!SIMULATION_MODE) {
      c.max = new Adafruit_MAX31865(c.csPin, SPI_MOSI, SPI_MISO, SPI_CLK);
      // Separater Registerzugriff auf denselben Bus, da die Bibliothek keinen
      // öffentlichen Zugriff auf das Konfigurationsregister bietet. Nur loop/setup lesen.
      c.diagnosticSpi = new Adafruit_SPIDevice(c.csPin, SPI_CLK, SPI_MISO, SPI_MOSI,
        1000000, SPI_BITORDER_MSBFIRST, SPI_MODE1);
      c.diagnosticSpi->begin();
      initializeTemperatureSensor(c);
    }
  }
  measureTemperatures();
  lastMeasurement = millis();
  WiFi.persistent(false); // Zugangsdaten ausschließlich im eigenen NVS-Blob verwalten.
  WiFi.setHostname(SIMULATION_MODE ? "backload-test" : "esp32-heizung");
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false); // Wiederholungen werden in processWifi() gesteuert.
  WiFi.disconnect(false, true);
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    FirmwareLog.print("Webinterface: http://"); FirmwareLog.println(WiFi.softAPIP());
  } else {
    FirmwareLog.println("WLAN konnte nicht gestartet werden. Temperaturregelung läuft weiter.");
  }
  if (!roofUdp.begin(ROOF_HEARTBEAT_PORT)) {
    FirmwareLog.println("UDP-Empfang für Dachsensor konnte nicht gestartet werden.");
  }
  loadWifiCredentials();
  const char* headers[] = {"X-Requested-With"};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/history", HTTP_GET, sendTemperatureHistory);
  server.on("/api/device-info", HTTP_GET, sendDeviceInfo);
  server.on("/api/settings", HTTP_POST, saveSettings);
  server.on("/api/pump", HTTP_POST, savePumpControl);
  server.on("/api/automatic", HTTP_POST, saveAutomaticSettings);
  server.on("/api/test-mode", HTTP_POST, setTestMode);
  server.on("/api/update/prepare", HTTP_POST, prepareOta);
  server.on("/api/update/chunk", HTTP_POST, writeOtaChunk);
  server.on("/api/update/finish", HTTP_POST, finishOta);
  server.on("/api/update/abort", HTTP_POST, abortOta);
  server.on("/api/update/github", HTTP_POST, startGithubOta);
  server.on("/api/update/github-status", HTTP_POST, githubOtaStatus);
  if (SIMULATION_MODE) server.on("/api/simulation", HTTP_POST, saveSimulationValues);
  server.on("/api/wifi", HTTP_POST, saveWifi);
  server.on("/api/wifi/reset", HTTP_POST, resetWifi);
  server.on("/api/wifi/scan", HTTP_POST, startWifiScan);
  server.on("/api/wifi/scan", HTTP_GET, sendWifiScan);
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
  beginWebSerialMonitor();
}

void loop() {
  servicePump();
  unsigned long now = millis();
  if (otaActive && otaFinished && now - otaLastActivity >= 2500 && now - otaStoppedAt >= 2500) ESP.restart();
  if (otaActive && !otaFinished && !githubUpdateActive && now - otaLastActivity >= 30000) cancelOta();
  if (githubUpdateActive && (githubUpdateState.load() == GH_DOWNLOADED || githubUpdateState.load() == GH_FAILED)) {
    const bool downloaded = githubUpdateState.load() == GH_DOWNLOADED;
    githubUpdateActive = false;
    if (downloaded && Update.end()) {
      otaFinished = true; otaLastActivity = millis(); githubUpdateState.store(GH_FINISHED);
      FirmwareLog.println("GitHub-Firmware geprüft. Pumpe AUS, Neustart folgt.\n");
    } else {
      if (downloaded) snprintf(githubUpdateError, sizeof(githubUpdateError), "Firmware-Image ist ungültig.");
      githubUpdateState.store(GH_FAILED); cancelOta();
    }
  }
  if (otaVerificationPending && now - otaBootCheckStarted >= 10000) {
    if (storageReady && (WiFi.getMode() & WIFI_AP)) {
      if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        otaVerificationPending = false;
        FirmwareLog.println("Neue Firmware gestartet und bestätigt.\n");
        measureTemperatures(); lastMeasurement = millis();
      }
    } else {
      FirmwareLog.println("Firmware-Startprüfung fehlgeschlagen; Rückkehr zur vorherigen Version.\n");
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
  }
  if (modeTransition && now - modeRestartAt >= 2500) ESP.restart();
  if (now - lastHeartbeat >= 500) {
    lastHeartbeat = now;
    ledState = !ledState;
    if (!SIMULATION_MODE) digitalWrite(LED_PIN, ledState);
  }
  if (now - lastMeasurement >= MEASUREMENT_INTERVAL) {
    measureTemperatures();
    lastMeasurement = millis();
  }
  // Keine zweisekündige Pause: HTTP-Anfragen zwischen Messungen bearbeiten.
  server.handleClient();
  processWifi();
  processRoofHeartbeat();
  processWebSerialMonitor();
  servicePump();
  delay(1);
}
