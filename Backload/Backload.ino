// Arduino IDE: Backload.ino im gleichnamigen Ordner Backload öffnen.
// ESP32S3 Dev Module (N16R8), ESP32-Core 2.0.13, Flash Size: 16MB,
// PSRAM: OPI PSRAM, Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS).
// Die zusätzlich benötigten Bibliotheken liegen mit Lizenzinformationen im Sketchordner src.
#include "src/Adafruit_MAX31865/Adafruit_MAX31865.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Preferences.h>
#include "WebSerialMonitor.h"
#include "SensorReading.h"
#include "PumpControl.h"
#include "PumpRuntime.h"
#include "RemoteRelay.h"
#include "RelayDiscovery.h"
#include "RelayProvisioning.h"
#include "RemoteRelayUpdate.h"
#include "AutomaticControl.h"
#include "SolarControl.h"
#include "TemperatureHistory.h"
#include "RecoveryApPolicy.h"
#include "Hostname.h"
#include "HistoryServerUrl.h"
#include "StatusLed.h"
#include <esp32-hal-rgb-led.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_arduino_version.h>
#include <esp_chip_info.h>
#include <esp_system.h>
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
const char* FIRMWARE_VERSION = "3.0.0";
// Der AP wird nur für Einrichtung und Wiederherstellung benötigt.
constexpr bool AP_RECOVERY_ONLY = true;

// Beim ersten Start ohne gespeicherte Betriebsart läuft die echte Steuerung.
// Der Testmodus wird über das Webinterface nach Bestätigung ein-/ausgeschaltet.
bool SIMULATION_MODE = false;
bool modeTransition = false;
unsigned long modeRestartAt = 0;
bool otaActive = false, otaFinished = false, otaVerificationPending = false;
unsigned long otaLastActivity = 0, otaStoppedAt = 0, otaBootCheckStarted = 0;
size_t otaExpectedSize = 0, otaReceived = 0;
size_t otaLastChunkOffset = 0, otaLastChunkLength = 0;
unsigned otaLoggedPercent = 0;
uint8_t otaLastChunkHash[32] = {};
String otaToken;
enum GithubUpdateState { GH_IDLE, GH_DOWNLOADING, GH_DOWNLOADED, GH_FAILED, GH_FINISHED };
std::atomic<int> githubUpdateState(GH_IDLE);
std::atomic<uint32_t> githubUpdateBytes(0);
constexpr unsigned GITHUB_DOWNLOAD_ATTEMPTS = 3;
std::atomic<unsigned> githubUpdateAttempt(0);
std::atomic<bool> githubUpdateCancel(false);
bool githubUpdateActive = false;
String githubUpdateUrl, githubExpectedHash, githubUpdateToken;
char githubUpdateError[256] = {};
// Jeder Eintrag wird vom Download-Task genau einmal vor dem Veröffentlichen
// der nächsten Versuchsnummer geschrieben und dann nur noch gelesen.
char githubRetryErrors[GITHUB_DOWNLOAD_ATTEMPTS - 1][256] = {};

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
// Integrierte adressierbare RGB-LED; GPIO 48 am Testgerät bestätigt.
constexpr uint8_t RGB_STATUS_PIN = 48;

// Vier Active-Low-Relais: LOW = Relais AN, HIGH = Relais AUS.
const uint8_t PUMP_ENABLE_PIN = 3;
const uint8_t PUMP_DIRECTION_PIN = 8;
const uint8_t SOLAR_PUMP_PIN = 18; // Relais 3: Solarpumpe, Active Low.
const uint8_t OIL_BURNER_PIN = 17; // Relais 4: manuelle Ölbrennerfreigabe.
static_assert(RGB_STATUS_PIN != SPI_CLK && RGB_STATUS_PIN != SPI_MOSI && RGB_STATUS_PIN != SPI_MISO &&
  RGB_STATUS_PIN != LED_PIN && RGB_STATUS_PIN != PUMP_ENABLE_PIN && RGB_STATUS_PIN != PUMP_DIRECTION_PIN &&
  RGB_STATUS_PIN != SOLAR_PUMP_PIN && RGB_STATUS_PIN != OIL_BURNER_PIN, "RGB-LED benötigt einen eigenen GPIO.");
// Vorgesehene Belegung; an die tatsächliche Verdrahtung des Relaisboards anpassen.
static_assert(PUMP_ENABLE_PIN != PUMP_DIRECTION_PIN && PUMP_ENABLE_PIN != SOLAR_PUMP_PIN &&
  PUMP_ENABLE_PIN != OIL_BURNER_PIN && PUMP_DIRECTION_PIN != SOLAR_PUMP_PIN &&
  PUMP_DIRECTION_PIN != OIL_BURNER_PIN && SOLAR_PUMP_PIN != OIL_BURNER_PIN,
  "Relais benötigen unterschiedliche GPIOs.");
// An die fertige Umpolschaltung anpassen: Förderrichtung bei Richtungsrelais AUS.
const PumpDirection DIRECTION_RELAY_OFF_DIRECTION = PumpDirection::CombiToBuffer;
PumpControl pump(DIRECTION_RELAY_OFF_DIRECTION);
bool manualControl = false;
bool manualPumpOn = false;
PumpDirection manualDirection = DIRECTION_RELAY_OFF_DIRECTION;
const bool AUTOMATIC_PUMP_CONFIGURED = true;
AutomaticControl automaticControl;
AutomaticThresholds automaticThresholds = {8.0f};
SolarControl solarControl;
SolarPanelGrace solarPanelGrace;
bool solarGraceActive = false;
SensorFaultDelay solarPanelFaultDisplay;
float solarStartDifference = SolarControl::DEFAULT_START;
bool solarSettingsValid = true;
bool oilBurnerEnabled = false; // Sommer = AUS, Winter = AN; keine Flammenrückmeldung.
bool heatingModeValid = false;
bool autoSettingsValid = true;

// Active-Low-Relais: LOW = AN, HIGH = AUS.
// Einstellgrenzen je Heizkreis an die konkrete Anlage anpassen.
struct ControllerLimits {
  float minTarget;
  float maxTarget;
  float minHysteresis;
  float maxHysteresis;
};

// Der gemeinsame Einlagerungssollwert liegt aus Kompatibilitätsgründen in c0.
// Er gilt für den durch Sommer/Winter gewählten Kombifühler. Das c0-Format enthält
// weiterhin einen ungenutzten per-Sensor-Hysteresewert zur NVS-Kompatibilität.
// Die wirksame Startdifferenz ist automaticThresholds.differenceStart (5–10 K).
const ControllerLimits ROOM_HEATING_LIMITS = {
  AutomaticControl::MIN_CHARGE_TARGET, AutomaticControl::MAX_CHARGE_TARGET, 0.1f, 20.0f
};

const unsigned long MEASUREMENT_INTERVAL = 2000;
const size_t HISTORY_CAPACITY = 365UL * 24 * 60;
TemperatureHistory temperatureHistory;
unsigned long lastMeasurement = 0;
uint32_t lastSolarMeasurement = 0;
bool solarMeasurementReady = false;
constexpr uint32_t SOLAR_MAX_MEASUREMENT_AGE = 60000;
unsigned long lastHeartbeat = 0;
bool ledState = LOW;
StatusLed statusLed;
bool statusLedPinSafe = false;
bool storageReady = false;
// Der asynchrone öffentliche Server verbindet sich nur über Loopback mit
// dieser Steuerungs-API; deren Handler laufen weiterhin im Hauptzyklus.
WebServer server(IPAddress(127, 0, 0, 1), 8080);
Preferences preferences;
Preferences hostnamePreferences;
Preferences runtimePreferences;
PumpRuntime pumpRuntime;
constexpr unsigned REMOTE_RELAY_COUNT = 4;
RemoteRelay remoteRelays[REMOTE_RELAY_COUNT];
bool remoteRelayCardReady[REMOTE_RELAY_COUNT] = {};
RemoteRelayUpdate remoteRelayUpdate;
RelayDiscovery relayDiscovery;
RelayProvisioning relayProvisioning;
bool remoteRelayReady = false, remoteRelayUpdateReady = false, relayDiscoveryReady = false, relayProvisioningReady = false;
bool runtimeReady = false, runtimeStorageHealthy = false;
bool runtimeSolarOn = false, runtimePumpOn = false;
PumpDirection runtimePumpDirection = PumpDirection::CombiToBuffer;
uint32_t runtimeLastUpdate = 0, runtimeLastSave = 0;
bool hostnameStorageReady = false, hostnameUsesDefault = true, mdnsReady = false;
String configuredHostname, activeHostname;
String historyServerUrl;
// Dachsensor vorläufig deaktiviert; die Implementierung bleibt für später erhalten.
constexpr bool ROOF_SENSOR_ENABLED = false;
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
RecoveryApPolicy recoveryApPolicy(30000);


struct TemperatureController {
  uint8_t csPin;
  int8_t settingsSlot; // Nur c0 oben aktiv. Alte c1/c2 bleiben unbenutzt im NVS.
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
  {4, 0, 68.0, 1.5, "Kombispeicher oben", nullptr, NAN, 0, false, &ROOM_HEATING_LIMITS, false, 0, nullptr, nullptr},
  {5, -1, 0, 0, "Kombispeicher Mitte", nullptr, NAN, 0, false, nullptr, false, 0, nullptr, nullptr},
  {7, -1, 0, 0, "Pufferspeicher oben", nullptr, NAN, 0, false, nullptr, false, 0, nullptr, nullptr},
  {6, -1, 0, 0, "Kombispeicher unten", nullptr, NAN, 0, false, nullptr, false, 0, nullptr, nullptr},
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

#include "DashboardPage.h"

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

bool chargingAllowed() {
  return heatingModeValid && !oilBurnerEnabled;
}

bool oilBurnerRelayOn() {
  return heatingModeValid && oilBurnerEnabled && !SIMULATION_MODE &&
    !modeTransition && !otaActive && !otaVerificationPending;
}

bool solarSensorsFresh() {
  return solarMeasurementReady && uint32_t(millis() - lastSolarMeasurement) <= SOLAR_MAX_MEASUREMENT_AGE;
}

void trackPumpRuntime(bool force = false) {
  if (!runtimeReady) return;
  const uint32_t now = millis();
  if (!force && uint32_t(now - runtimeLastUpdate) < 1000) return;
  pumpRuntime.update(now, time(nullptr), runtimeSolarOn,
    runtimePumpOn && runtimePumpDirection == PumpDirection::CombiToBuffer,
    runtimePumpOn && runtimePumpDirection == PumpDirection::BufferToCombi);
  runtimeLastUpdate = now;
}

void savePumpRuntime(bool force = false) {
  if (!runtimeReady) return;
  const uint32_t now = millis();
  if (!force && uint32_t(now - runtimeLastSave) < 15UL * 60 * 1000) return;
  runtimeLastSave = now;
  trackPumpRuntime(true);
  if (!pumpRuntime.dirty()) return;
  const PumpRuntime::State snapshot = pumpRuntime.snapshot();
  const bool ok = runtimePreferences.putBytes("periods1", &snapshot, sizeof(snapshot)) == sizeof(snapshot);
  if (ok) pumpRuntime.saved();
  if (ok != runtimeStorageHealthy) FirmwareLog.println(ok ? "Pumpenlaufzeiten: Flash-Speicherung wieder möglich." : "Pumpenlaufzeiten: Flash-Speicherung fehlgeschlagen; erneuter Versuch folgt.");
  runtimeStorageHealthy = ok;
}

void serviceSolarPump() {
  const TemperatureController& tank = controllers[3];
  const TemperatureController& panel = controllers[4];
  const bool wasOn = solarControl.on();
  const bool paused = modeTransition || otaActive || otaVerificationPending;
  if (paused) solarControl.setManual(false);
  const bool enabled = !paused && (solarControl.manual() || (storageReady && solarSettingsValid && solarSensorsFresh()));
  const bool hold = !solarControl.manual() && !panel.valid &&
    solarPanelGrace.canHold(millis(), wasOn);
  const bool on = solarControl.update(enabled, panel.valid || hold,
    hold ? solarPanelGrace.temperature() : panel.currentTemp, tank.valid, tank.currentTemp, solarStartDifference);
  solarGraceActive = hold && on;
  // Im Testmodus bleibt der echte Active-Low-Ausgang wie beim Start auf HIGH.
  if (!SIMULATION_MODE) digitalWrite(SOLAR_PUMP_PIN, on ? LOW : HIGH);
  const bool physicalOn = on && !SIMULATION_MODE;
  if (physicalOn != runtimeSolarOn) { runtimeSolarOn = physicalOn; trackPumpRuntime(true); }
  if (on != wasOn) FirmwareLog.printf("%sSolarpumpe: %s.\n", SIMULATION_MODE ? "[SIMULATION] " : "", on ? "AN" : "AUS");
}

void printMeasurementHeader();

void servicePump() {
  serviceSolarPump();
  // Beim Sperren des Brenners zuerst Relais 4 ausschalten, bevor Einlagerung
  // wieder möglich wird. Beim Freigeben gilt unten die umgekehrte Reihenfolge.
  if (!oilBurnerRelayOn()) digitalWrite(OIL_BURNER_PIN, HIGH);
  // Die Sperre sitzt vor sämtlichen GPIO-Ausgaben und gilt auch für Handbefehle.
  pump.setChargingAllowed(chargingAllowed(), millis());
  if (!chargingAllowed() && manualDirection == PumpDirection::CombiToBuffer)
    manualPumpOn = false;
  // Immer zuerst ausschalten; die Richtung wird ausschließlich bei AUS verändert.
  if (!SIMULATION_MODE) {
    if (!pump.on()) digitalWrite(PUMP_ENABLE_PIN, HIGH);
    digitalWrite(PUMP_DIRECTION_PIN, pump.direction() == DIRECTION_RELAY_OFF_DIRECTION ? HIGH : LOW);
    if (pump.on()) digitalWrite(PUMP_ENABLE_PIN, LOW);
  }
  const bool physicalOn = pump.on() && !SIMULATION_MODE;
  if (physicalOn != runtimePumpOn || (physicalOn && pump.direction() != runtimePumpDirection)) {
    runtimePumpOn = physicalOn; runtimePumpDirection = pump.direction(); trackPumpRuntime(true);
  } else trackPumpRuntime();
  // Ein Vorladeauftrag ist bereits gestoppt, bevor Relais 4 einschalten kann.
  if (oilBurnerRelayOn()) digitalWrite(OIL_BURNER_PIN, LOW);

  static bool lastOn = false;
  static PumpDirection lastDirection = DIRECTION_RELAY_OFF_DIRECTION;
  static PumpPhase lastPhase = PumpPhase::Ready;
  if (lastOn != pump.on() || lastDirection != pump.direction() || lastPhase != pump.phase()) {
    printMeasurementHeader();
    FirmwareLog.printf("%sBackloadpumpe: %s | Richtung: %s | Steuerung: %s",
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
  TemperatureController& combi = controllers[oilBurnerEnabled ? 1 : 0];
  TemperatureController& buffer = controllers[2];
  TemperatureController& settings = controllers[0];
  const TransferDemand demand = automaticControl.update(AUTOMATIC_PUMP_CONFIGURED && storageReady && autoSettingsValid && heatingModeValid,
    chargingAllowed(), combi.valid, combi.currentTemp,
    buffer.valid, buffer.currentTemp, settings.configValid ? settings.targetTemp : NAN, automaticThresholds);
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
    if (i == 4) solarPanelFaultDisplay.observe(!c.valid, millis());
    if (i == 4 && c.valid) solarPanelGrace.record(c.currentTemp, millis());
    // Fehler sofort auswerten; Panel-Ausfälle dürfen bei laufender Pumpe begrenzt überbrückt werden.
    if (!c.valid && (i == 3 || i == 4)) serviceSolarPump();

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
            (i == 3 || i == 4) ? (solarControl.manual() ? " | Solarpumpe: Handbetrieb aktiv" : solarGraceActive ? " | Solarpumpe: begrenzte Fehlerüberbrückung" : " | Solarregelung: Solarpumpe AUS") :
            manualControl ? " | Manuelle Steuerung der Backloadpumpe aktiv" :
            (i == (oilBurnerEnabled ? 1 : 0) || i == 2) ? " | Automatik: Backloadpumpe AUS" : " | kein Backload-Regelfühler");
        }
      } else {
        if (i == 0) {
          FirmwareLog.printf("%s: %.1f°C | Einlagerung ab: %.1f°C | Startdifferenz: %.1f K",
            c.name, c.currentTemp, c.targetTemp, automaticThresholds.differenceStart);
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
      // Nur der Wandler wird neu aufgesetzt; die manuelle Steuerung bleibt unbeeinflusst.
      // Die nächste reguläre Messung prüft erneut; keine zusätzliche Warteschleife.
      initializeTemperatureSensor(c);
    }
  }
  // Nur tatsächlich ausgegebene Blöcke mit einer Leerzeile abschließen.
  if (logBlockStarted) FirmwareLog.println();
  lastSolarMeasurement = millis();
  solarMeasurementReady = true;
  serviceSolarPump();
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

const char* defaultHostname() { return SIMULATION_MODE ? "backload-test" : "esp32-heizung"; }

void loadHostname() {
  configuredHostname = defaultHostname();
  hostnameStorageReady = hostnamePreferences.begin("backload-net", false);
  hostnameUsesDefault = true;
  if (hostnameStorageReady && hostnamePreferences.isKey("hostname")) {
    const String saved = hostnamePreferences.getString("hostname", "");
    if (strlen(saved.c_str()) == saved.length() && Hostname::valid(saved.c_str(), saved.length())) {
      configuredHostname = saved;
      hostnameUsesDefault = false;
    } else FirmwareLog.println("Ungültiger Hostname gespeichert; Standardname wird verwendet.");
  }
  if (!hostnameStorageReady) FirmwareLog.println("Hostname-Speicher nicht verfügbar; Standardname wird verwendet.");
  if (hostnameStorageReady) {
    const String saved = hostnamePreferences.getString("history-url", "");
    if (HistoryServerUrl::valid(saved.c_str(), saved.length())) historyServerUrl = saved;
    else FirmwareLog.println("Ungültige History-Server-Adresse gespeichert; Link ausgeblendet.");
  }
  activeHostname = configuredHostname;
}

void sendHistoryServerUrl() {
  if (!hostnameStorageReady) { server.send(503, "text/plain; charset=utf-8", "Einstellungsspeicher nicht verfügbar."); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"url\":" + jsonString(historyServerUrl) + "}");
}

void saveHistoryServerUrl() {
  if (otaActive || otaVerificationPending || modeTransition) { server.send(409, "text/plain; charset=utf-8", "Update oder Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden."); return; }
  if (!hostnameStorageReady) { server.send(503, "text/plain; charset=utf-8", "Einstellungsspeicher nicht verfügbar."); return; }
  if (server.args() != 1 || !server.hasArg("url")) { server.send(400, "text/plain; charset=utf-8", "Ungültige Anfrage."); return; }
  String next = server.arg("url");
  next.trim();
  if (!HistoryServerUrl::valid(next.c_str(), next.length())) {
    server.send(400, "text/plain; charset=utf-8", "Bitte eine HTTP- oder HTTPS-Adresse ohne Zugangsdaten eingeben."); return;
  }
  bool saved = true;
  if (!next.length()) {
    if (hostnamePreferences.isKey("history-url")) saved = hostnamePreferences.remove("history-url");
  } else if (next != historyServerUrl || !hostnamePreferences.isKey("history-url")) {
    saved = hostnamePreferences.putString("history-url", next) == next.length() && hostnamePreferences.getString("history-url", "") == next;
  }
  if (!saved) { server.send(500, "text/plain; charset=utf-8", "Adresse konnte nicht gespeichert werden."); return; }
  historyServerUrl = next;
  sendHistoryServerUrl();
}

void saveHostname() {
  if (otaActive || otaVerificationPending || modeTransition) { server.send(409, "text/plain; charset=utf-8", "Update oder Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden."); return; }
  if (!hostnameStorageReady) { server.send(503, "text/plain; charset=utf-8", "Hostname-Speicher nicht verfügbar."); return; }
  if (server.args() != 1 || !server.hasArg("hostname")) { server.send(400, "text/plain; charset=utf-8", "Ungültige Anfrage."); return; }
  String next = server.arg("hostname");
  next.toLowerCase();
  const bool useDefault = next.length() == 0;
  if (!useDefault && (strlen(next.c_str()) != next.length() || !Hostname::valid(next.c_str(), next.length()))) {
    server.send(400, "text/plain; charset=utf-8", "Hostname: 1–31 Buchstaben, Zahlen oder Bindestriche; Anfang und Ende müssen Buchstaben oder Zahlen sein."); return;
  }
  bool saved = true;
  if (useDefault) {
    if (hostnamePreferences.isKey("hostname")) saved = hostnamePreferences.remove("hostname");
    next = defaultHostname();
  } else if (hostnameUsesDefault || next != configuredHostname) {
    saved = hostnamePreferences.putString("hostname", next) == next.length() && hostnamePreferences.getString("hostname", "") == next;
  }
  if (!saved) { server.send(500, "text/plain; charset=utf-8", "Hostname konnte nicht gespeichert werden."); return; }
  hostnameUsesDefault = useDefault;
  configuredHostname = next;
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"saved\":true,\"hostname\":" + jsonString(configuredHostname) +
    ",\"restartRequired\":" + String(configuredHostname != activeHostname ? "true" : "false") + "}");
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

// Der Notzugang wird unabhängig von
// Sensormessung und Pumpe geschaltet. Bootvalidierung und Neueinrichtung
// benötigen den AP; WLAN-Ausfälle lösen den Notzugang nach 30 Sekunden aus.
void processRecoveryAp() {
  if (relayProvisioning.apActive()) return;
  const uint32_t now = millis();
  const bool connected = wifiPhase == WIFI_CONNECTED && WiFi.status() == WL_CONNECTED;
  const bool apActive = (WiFi.getMode() & WIFI_AP) != 0;
  // Ein bereits über den Notzugang laufendes Update nicht durch Abschalten
  // dieses Zugangs unterbrechen. Ist der AP aus, bleibt er beim Update aus.
  const bool reconfiguring = !wifiCredentials.ssid[0] || wifiPhase == WIFI_QUEUED || wifiPhase == WIFI_STARTING || (otaActive && apActive);
  const auto action = recoveryApPolicy.update(now, connected, apActive, !otaVerificationPending, reconfiguring);
  static uint32_t lastAttempt = 0;
  static bool waitingRetry = false;
  if (action == RecoveryApPolicy::Keep) { waitingRetry = false; return; }
  if (waitingRetry && uint32_t(now - lastAttempt) < 5000) return;
  lastAttempt = now;
  waitingRetry = true;
  if (action == RecoveryApPolicy::Disable) {
    if (WiFi.enableAP(false)) FirmwareLog.println("ESP32-Notzugang ausgeschaltet; Heimnetz bleibt aktiv.");
    else FirmwareLog.println("ESP32-Notzugang konnte nicht ausgeschaltet werden.");
  } else {
    WiFi.enableAP(true);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    if (WiFi.softAP(AP_SSID, AP_PASSWORD)) FirmwareLog.println("ESP32-Notzugang aktiviert: http://192.168.4.1");
    else FirmwareLog.println("ESP32-Notzugang konnte nicht aktiviert werden.");
  }
}

String wifiStatusJson() {
  const char* states[] = {"unconfigured", "queued", "starting", "connecting", "connected", "retry"};
  bool connected = wifiPhase == WIFI_CONNECTED && WiFi.status() == WL_CONNECTED;
  return "{\"state\":" + jsonString(states[wifiPhase]) +
    ",\"ssid\":" + jsonString(wifiCredentials.ssid) +
    ",\"hostname\":" + jsonString(configuredHostname) +
    ",\"activeHostname\":" + jsonString(activeHostname) +
    ",\"hostnameDefault\":" + String(hostnameUsesDefault ? "true" : "false") +
    ",\"mdnsReady\":" + String(mdnsReady ? "true" : "false") +
    ",\"changeId\":" + jsonString(wifiChangeId) +
    ",\"rssi\":" + (connected ? String(WiFi.RSSI()) : String("null")) +
    ",\"ip\":" + jsonString(connected ? WiFi.localIP().toString() : String("")) +
    ",\"apRecoveryOnly\":" + String(AP_RECOVERY_ONLY ? "true" : "false") +
    ",\"pairing\":" + String(relayProvisioning.apActive() ? "true" : "false") +
    ",\"apIp\":" + jsonString((WiFi.getMode() & WIFI_AP) ? WiFi.softAPIP().toString() : String("")) + "}";
}

void startWifiScan() {
  if (relayProvisioning.active()) { server.send(409, "text/plain; charset=utf-8", "Einrichtung einer Relaiserweiterung läuft."); return; }
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

void sendRelayPairScan() {
  server.sendHeader("Cache-Control", "no-store");
  if (!wifiScanRequested) { server.send(200, "application/json", "{\"state\":\"idle\"}"); return; }
  const int16_t count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) { server.send(200, "application/json", "{\"state\":\"scanning\"}"); return; }
  if (count < 0) { server.send(200, "application/json", "{\"state\":\"failed\"}"); return; }
  String json = "{\"state\":\"complete\",\"networks\":["; unsigned found = 0;
  for (int i = 0; i < count && found < 12; ++i) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() != 22 || !ssid.startsWith("Relaiserweiterung-") || WiFi.encryptionType(i) == WIFI_AUTH_OPEN) continue;
    bool valid = true;
    for (size_t j = 18; j < 22; ++j) if (!((ssid[j] >= '0' && ssid[j] <= '9') || (ssid[j] >= 'A' && ssid[j] <= 'F'))) valid = false;
    if (!valid) continue;
    if (found++) json += ',';
    json += "{\"ssid\":" + jsonString(ssid) + ",\"open\":false}";
  }
  server.send(200, "application/json", json + "]}");
}
bool allowWifiChange() {
  if (relayProvisioning.active()) { server.send(409, "text/plain; charset=utf-8", "Einrichtung einer Relaiserweiterung läuft."); return false; }
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
    ",\"automaticConfigured\":" + String(AUTOMATIC_PUMP_CONFIGURED && storageReady && autoSettingsValid && heatingModeValid ? "true" : "false") + "}";
}

String automaticStatusJson() {
  // Ein Schalter wählt den Kombifühler und steuert die Ölbrennerfreigabe.
  return "{\"winter\":" + String(oilBurnerEnabled ? "true" : "false") +
    ",\"controlSensorId\":" + String(oilBurnerEnabled ? 1 : 0) +
    String(",\"allowCharging\":") + String(chargingAllowed() ? "true" : "false") +
    ",\"configValid\":" + String(storageReady && autoSettingsValid && heatingModeValid ? "true" : "false") +
    ",\"targetOffset\":0" +
    ",\"differenceStart\":" + jsonNumber(automaticThresholds.differenceStart) +
    ",\"minimumCombi\":" + jsonNumber(automaticThresholds.minimumCombi) +
    ",\"minimumBuffer\":" + jsonNumber(automaticThresholds.minimumBuffer) +
    ",\"differenceStop\":" + jsonNumber(AutomaticControl::STOP_DIFFERENCE) + "}";
}

String solarStatusJson() {
  const TemperatureController& tank = controllers[3];
  const TemperatureController& panel = controllers[4];
  const bool sensorsValid = solarSensorsFresh() && panel.valid && tank.valid && isfinite(panel.currentTemp) && isfinite(tank.currentTemp);
  const bool sensorFaultPending = solarSensorsFresh() && !panel.valid && tank.valid &&
    isfinite(tank.currentTemp) && solarPanelFaultDisplay.pending(millis());
  return "{\"on\":" + String(solarControl.on() ? "true" : "false") +
    ",\"manual\":" + String(solarControl.manual() ? "true" : "false") +
    ",\"requestedOn\":" + String(solarControl.manualOn() ? "true" : "false") +
    ",\"relayOn\":" + String(solarControl.on() && !SIMULATION_MODE ? "true" : "false") +
    ",\"configValid\":" + String(storageReady && solarSettingsValid ? "true" : "false") +
    ",\"sensorsValid\":" + String(sensorsValid ? "true" : "false") +
    ",\"sensorGrace\":" + String(solarGraceActive ? "true" : "false") +
    ",\"sensorFaultPending\":" + String(sensorFaultPending ? "true" : "false") +
    ",\"lastDifference\":" + (sensorFaultPending && solarPanelGrace.hasReading()
      ? jsonNumber(solarPanelGrace.temperature() - tank.currentTemp) : String("null")) +
    ",\"paused\":" + String((modeTransition || otaActive || otaVerificationPending) ? "true" : "false") +
    ",\"differenceStart\":" + (solarSettingsValid ? jsonNumber(solarStartDifference) : String("null")) +
    ",\"differenceStop\":" + jsonNumber(SolarControl::STOP) +
    ",\"difference\":" + (sensorsValid ? jsonNumber(panel.currentTemp - tank.currentTemp) : String("null")) + "}";
}

String heatingStatusJson() {
  return "{\"winter\":" + String(oilBurnerEnabled ? "true" : "false") +
    ",\"configValid\":" + String(heatingModeValid ? "true" : "false") +
    ",\"relayOn\":" + String(oilBurnerRelayOn() ? "true" : "false") + "}";
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
  const bool connected = wifiPhase == WIFI_CONNECTED && WiFi.status() == WL_CONNECTED;
  const bool apActive = WiFi.getMode() & WIFI_AP;
  wifi_ap_record_t apInfo = {};
  const bool apInfoReady = connected && esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK;
  String standards;
  if (apInfoReady) {
    if (apInfo.phy_11b) standards += "802.11b";
    if (apInfo.phy_11g) standards += (standards.length() ? " / " : "") + String("802.11g");
    if (apInfo.phy_11n) standards += (standards.length() ? " / " : "") + String("802.11n");
  }
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
    ",\"psramFreeBytes\":" + String(ESP.getFreePsram()) + ",\"runningSlot\":" + slotJson(running) + ",\"updateSlot\":" + slotJson(next) +
    ",\"wifi\":" + wifiStatusJson() + ",\"wifiDetails\":{\"mac\":" + jsonString(WiFi.macAddress()) +
    ",\"bssid\":" + jsonString(connected ? WiFi.BSSIDstr() : String("")) +
    ",\"channel\":" + String((connected || apActive) ? WiFi.channel() : 0) +
    ",\"auth\":" + (apInfoReady ? String(int(apInfo.authmode)) : String("null")) + ",\"standards\":" + jsonString(standards) +
    ",\"mask\":" + jsonString(connected ? WiFi.subnetMask().toString() : String("")) +
    ",\"gateway\":" + jsonString(connected ? WiFi.gatewayIP().toString() : String("")) +
    ",\"dns1\":" + jsonString(connected ? WiFi.dnsIP(0).toString() : String("")) +
    ",\"dns2\":" + jsonString(connected ? WiFi.dnsIP(1).toString() : String("")) +
    ",\"apActive\":" + String(apActive ? "true" : "false") +
    ",\"apSsid\":" + jsonString(apActive ? WiFi.softAPSSID() : String("")) +
    ",\"apIp\":" + jsonString(apActive ? WiFi.softAPIP().toString() : String("")) +
    ",\"apMac\":" + jsonString(apActive ? WiFi.softAPmacAddress() : String("")) +
    ",\"apClients\":" + String(apActive ? WiFi.softAPgetStationNum() : 0) + "}}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

String runtimePeriodsJson(const PumpRuntime::Period* periods, size_t count) {
  String result = "[";
  bool first = true;
  for (size_t i = 0; i < count; ++i) {
    const auto& period = periods[i];
    if (!period.key) continue;
    if (!first) result += ',';
    first = false;
    result += "{\"period\":" + String(period.key) + ",\"solar\":" + String(period.seconds[0]) +
      ",\"combiToBuffer\":" + String(period.seconds[1]) + ",\"bufferToCombi\":" + String(period.seconds[2]) + "}";
  }
  return result + "]";
}

String pumpRuntimeJson() {
  trackPumpRuntime(true);
  if (!runtimeReady) return "{\"available\":false}";
  const auto& s = pumpRuntime.state();
  return "{\"available\":true,\"storageHealthy\":" + String(runtimeStorageHealthy ? "true" : "false") +
    ",\"clockValid\":" + String(pumpRuntime.clockValid() ? "true" : "false") +
    ",\"days\":" + runtimePeriodsJson(s.days, PumpRuntime::DAYS) +
    ",\"months\":" + runtimePeriodsJson(s.months, PumpRuntime::MONTHS) +
    ",\"years\":" + runtimePeriodsJson(s.years, PumpRuntime::YEARS) +
    ",\"undated\":[" + String(s.undated[0]) + "," + String(s.undated[1]) + "," + String(s.undated[2]) + "]}";
}

String remoteRelayStatusJson(unsigned board = 0) {
  const RemoteRelaySnapshot state = remoteRelays[board].snapshot();
  static const char* forgetNames[] = {"idle", "pending", "done", "failed", "uncertain", "save_failed"};
  String json = "{\"id\":" + String(board) + ",\"ready\":" + String(remoteRelayCardReady[board] ? "true" : "false") +
    ",\"configured\":" + String(state.config.address[0] ? "true" : "false") +
    ",\"online\":" + String(state.online ? "true" : "false") +
    ",\"busy\":" + String(state.busy ? "true" : "false") +
    ",\"updating\":" + String(remoteRelayUpdate.active() ? "true" : "false") +
    ",\"mask\":" + String(state.mask) + ",\"forget\":" + jsonString(forgetNames[state.forgetState]) + ",\"address\":" + jsonString(state.config.address) + ",\"version\":" + jsonString(state.version) + ",\"labels\":[";
  for (int i = 0; i < 4; ++i) { if (i) json += ','; json += jsonString(state.config.labels[i]); }
  return json + "]}";
}
bool remoteRelayPostAllowed();
bool remoteRelayAnyBusy() {
  for (unsigned i = 0; i < REMOTE_RELAY_COUNT; ++i) if (remoteRelays[i].snapshot().busy) return true;
  return false;
}
bool remoteRelayBoardArgument(unsigned& board) {
  board = 0;
  if (server.hasArg("board")) {
    const String value = server.arg("board");
    if (value.length() != 1 || value[0] < '0' || value[0] >= '0' + REMOTE_RELAY_COUNT) {
      server.send(400, "text/plain; charset=utf-8", "Ungültige Relaiserweiterung."); return false;
    }
    board = unsigned(value[0] - '0');
  }
  if (!remoteRelayCardReady[board]) { server.send(503, "text/plain", "Relaiserweiterung nicht verfügbar."); return false; }
  return true;
}
String relayDiscoveryJson() {
  const RelayDiscoverySnapshot snapshot = relayDiscovery.snapshot();
  String json = "{\"ready\":" + String(relayDiscoveryReady ? "true" : "false") +
    ",\"available\":" + String(snapshot.available ? "true" : "false") +
    ",\"searching\":" + String(snapshot.searching ? "true" : "false") +
    ",\"failed\":" + String(snapshot.failed ? "true" : "false") + ",\"devices\":[";
  for (size_t i = 0; i < snapshot.results.size(); ++i) {
    if (i) json += ',';
    const RelayDiscoveryEntry& entry = snapshot.results[i];
    json += "{\"id\":" + jsonString(entry.id) + ",\"name\":" + jsonString(entry.name) + ",\"address\":" + jsonString(entry.address) + "}";
  }
  return json + "]}";
}
void startRelayDiscovery() {
  if (!remoteRelayPostAllowed()) return;
  if (!relayDiscoveryReady || !relayDiscovery.search()) { server.send(409, "text/plain; charset=utf-8", "Suche läuft bereits oder Heim-WLAN ist nicht verbunden."); return; }
  server.send(202, "application/json", "{\"started\":true}");
}
String relayProvisioningJson() {
  const RelayProvisionSnapshot snapshot = relayProvisioning.snapshot();
  return "{\"ready\":" + String(relayProvisioningReady ? "true" : "false") + ",\"state\":" + jsonString(snapshot.state) +
    ",\"message\":" + jsonString(snapshot.message) + ",\"board\":" + String(snapshot.board) + "}";
}
void startRelayProvisioning() {
  if (!remoteRelayPostAllowed()) return;
  unsigned board = 0; if (!remoteRelayBoardArgument(board)) return;
  if (!relayProvisioningReady || !relayDiscoveryReady || remoteRelayUpdate.active() ||
      wifiPhase != WIFI_CONNECTED || WiFi.status() != WL_CONNECTED ||
      (wifiScanRequested && WiFi.scanComplete() == WIFI_SCAN_RUNNING) || server.args() != 2 ||
      !relayProvisioning.start(board, server.arg("ssid"), wifiCredentials.ssid, wifiCredentials.password)) {
    server.send(409, "text/plain; charset=utf-8", "Einrichtung nicht gestartet. Freien Platz, AP-Namen und Heim-WLAN prüfen; der Notzugang darf nicht von anderen Geräten genutzt werden."); return;
  }
  server.send(202, "application/json", "{\"started\":true}");
}
void cancelRelayProvisioning() {
  if (!remoteRelayPostAllowed()) return;
  if (server.args() != 0 || !relayProvisioning.cancel()) { server.send(409, "text/plain", "Keine Einrichtung aktiv."); return; }
  server.send(200, "application/json", "{\"canceled\":true}");
}
bool remoteRelayPostAllowed() {
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return false; }
  if (!remoteRelayReady) { server.send(503, "text/plain", "Relaisanbindung nicht verfügbar."); return false; }
  if (otaActive || otaVerificationPending || modeTransition) { server.send(409, "text/plain", "Update oder Neustart läuft."); return false; }
  return true;
}
void saveRemoteRelayConfig() {
  if (!remoteRelayPostAllowed()) return;
  if (remoteRelayUpdate.active() || relayProvisioning.active()) { server.send(409, "text/plain", "Einrichtung oder Update der Relaiserweiterung läuft."); return; }
  unsigned board = 0; if (!remoteRelayBoardArgument(board)) return;
  const String address = server.arg("address"), key = server.arg("key");
  if (address.isEmpty()) { server.send(400, "text/plain; charset=utf-8", "IP-Adresse eingeben. Zum Entfernen die Funktion ‚Erweiterung vergessen‘ verwenden."); return; }
  if (address.length()) for (unsigned i = 0; i < REMOTE_RELAY_COUNT; ++i) {
    if (i == board) continue;
    const RemoteRelaySnapshot other = remoteRelays[i].snapshot();
    if (other.config.address[0] && (address == other.config.address || (key.length() && key == other.config.key))) {
      server.send(409, "text/plain; charset=utf-8", "Diese Relaiserweiterung ist bereits eingerichtet."); return;
    }
  }
  String labels[4];
  for (int i = 0; i < 4; ++i) labels[i] = server.arg("label" + String(i + 1));
  if (!server.hasArg("address") || !server.hasArg("key") || !remoteRelays[board].configure(server.arg("address"), server.arg("key"), labels)) {
    server.send(400, "text/plain; charset=utf-8", "IP-Adresse, Schlüssel oder Beschriftungen prüfen; eventuell ist die Karte gerade beschäftigt oder der Speicher nicht verfügbar."); return;
  }
  server.send(200, "application/json", "{\"saved\":true}");
}
void saveRemoteRelayCommand() {
  if (!remoteRelayPostAllowed()) return;
  if (remoteRelayUpdate.active()) { server.send(409, "text/plain", "Update der Relaiskarte läuft."); return; }
  unsigned board = 0; if (!remoteRelayBoardArgument(board)) return;
  const String id = server.arg("id"), value = server.arg("value");
  if (id.length() != 1 || id[0] < '1' || id[0] > '4' || (value != "0" && value != "1")) { server.send(400, "text/plain", "Ungültiger Schaltauftrag."); return; }
  if (!remoteRelays[board].command(id[0] - '0', value == "1")) { server.send(409, "text/plain; charset=utf-8", "Relaiskarte nicht erreichbar oder beschäftigt."); return; }
  server.send(202, "application/json", "{\"accepted\":true}");
}
void forgetRemoteRelay() {
  if (!remoteRelayPostAllowed()) return;
  if (remoteRelayUpdate.active() || relayProvisioning.active()) { server.send(409, "text/plain", "Einrichtung oder Update läuft."); return; }
  unsigned board = 0;
  if (!remoteRelayBoardArgument(board)) return;
  if (server.args() != 1) { server.send(400, "text/plain", "Ungültige Auswahl."); return; }
  if (!remoteRelays[board].forget()) { server.send(409, "text/plain; charset=utf-8", "Erweiterung nicht erreichbar oder beschäftigt; sie bleibt gespeichert."); return; }
  server.send(202, "application/json", "{\"accepted\":true}");
}
void discardRemoteRelay() {
  if (!remoteRelayPostAllowed()) return;
  if (remoteRelayUpdate.active() || relayProvisioning.active()) { server.send(409, "text/plain", "Einrichtung oder Update läuft."); return; }
  unsigned board = 0; if (!remoteRelayBoardArgument(board)) return;
  if (server.args() != 1 || !remoteRelays[board].clearAssociation()) { server.send(409, "text/plain; charset=utf-8", "Zuordnung konnte nicht entfernt werden."); return; }
  server.send(200, "application/json", "{\"removed\":true}");
}
bool otaNumber(const String& text, uint32_t& number);
void prepareRemoteRelayUpdate() {
  if (!remoteRelayPostAllowed()) return;
  if (!remoteRelayUpdateReady || relayProvisioning.active()) { server.send(409, "text/plain; charset=utf-8", "Relais-Update während der Einrichtung oder ohne Update-Speicher nicht verfügbar."); return; }
  unsigned board = 0; if (!remoteRelayBoardArgument(board)) return;
  uint32_t size = 0; char token[33];
  if (server.args() != (server.hasArg("board") ? 2 : 1) || !otaNumber(server.arg("size"), size) || !remoteRelayUpdate.prepare(size, remoteRelays[board].snapshot(), token)) {
    server.send(409, "text/plain; charset=utf-8", "Relaiskarte nicht erreichbar, beschäftigt, Datei zu groß oder nicht genügend PSRAM."); return;
  }
  server.send(200, "application/json", "{\"token\":\"" + String(token) + "\"}");
}
void startRemoteRelayGithubUpdate() {
  if (!remoteRelayPostAllowed()) return;
  if (!remoteRelayUpdateReady || relayProvisioning.active()) { server.send(409, "text/plain; charset=utf-8", "Relais-Update während der Einrichtung oder ohne Update-Speicher nicht verfügbar."); return; }
  unsigned board = 0; if (!remoteRelayBoardArgument(board)) return;
  uint32_t size = 0; char token[33];
  if (server.args() != (server.hasArg("board") ? 4 : 3) || !otaNumber(server.arg("size"), size) ||
      !remoteRelayUpdate.prepareGithub(size, server.arg("url"), server.arg("sha256"), remoteRelays[board].snapshot(), token)) {
    server.send(400, "text/plain; charset=utf-8", "Release ungültig, Relaiskarte nicht erreichbar oder nicht genügend Speicher."); return;
  }
  server.send(202, "application/json", "{\"token\":\"" + String(token) + "\"}");
}
void receiveRemoteRelayUpdateChunk() {
  if (!remoteRelayPostAllowed()) return;
  uint32_t offset = 0;
  if (server.args() != 3 || !otaNumber(server.arg("offset"), offset) || !remoteRelayUpdate.chunk(server.arg("token"), offset, server.arg("data"))) {
    server.send(400, "text/plain; charset=utf-8", "Update-Datenblock ungültig oder nicht erwartet."); return;
  }
  RemoteRelayUpdateStatus status;
  if (!remoteRelayUpdate.status(server.arg("token"), status)) { server.send(409, "text/plain", "Update nicht verfügbar."); return; }
  server.send(200, "application/json", "{\"received\":" + String(status.received) + "}");
}
void startRemoteRelayUpdate() {
  if (!remoteRelayPostAllowed()) return;
  if (server.args() != 1 || !remoteRelayUpdate.start(server.arg("token"))) { server.send(409, "text/plain", "Update nicht vollständig oder nicht verfügbar."); return; }
  server.send(202, "application/json", "{\"started\":true}");
}
void cancelRemoteRelayUpdate() {
  if (!remoteRelayPostAllowed()) return;
  if (server.args() != 1 || !remoteRelayUpdate.cancel(server.arg("token"))) { server.send(409, "text/plain", "Abbruch nicht möglich."); return; }
  server.send(200, "application/json", "{\"canceled\":true}");
}
void remoteRelayUpdateStatus() {
  if (!remoteRelayPostAllowed()) return;
  RemoteRelayUpdateStatus status;
  if (server.args() != 1 || !remoteRelayUpdate.status(server.arg("token"), status)) { server.send(404, "text/plain", "Update nicht gefunden."); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"state\":" + jsonString(status.state) + ",\"error\":" + jsonString(status.error) +
    ",\"size\":" + String(status.size) + ",\"received\":" + String(status.received) + ",\"sent\":" + String(status.sent) + "}");
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
      "\",\"csPin\":" + String(unsigned(c.csPin)) +
      ",\"temperature\":" + (c.valid ? String(c.currentTemp, 1) : String("null")) +
      ",\"valid\":" + String(c.valid ? "true" : "false") +
      ",\"fault\":" + String(c.fault) +
      ",\"rawRtd\":" + String(c.rawRtd) +
      ",\"sensorError\":" + (c.sensorError ? jsonString(c.sensorError) : String("null")) +
      ",\"sensorFaultPending\":" + String(i == 4 && !c.valid && solarPanelFaultDisplay.pending(millis()) ? "true" : "false") +
      ",\"lastValidTemperature\":" + (i == 4 && !c.valid && solarPanelFaultDisplay.pending(millis()) && solarPanelGrace.hasReading()
        ? jsonNumber(solarPanelGrace.temperature()) : String("null")) +
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
    ",\"heating\":" + heatingStatusJson() + ",\"solar\":" + solarStatusJson() + ",\"pumpRuntime\":" + pumpRuntimeJson() +
    ",\"hardware\":{\"clk\":" + String(SPI_CLK) + ",\"mosi\":" + String(SPI_MOSI) +
    ",\"miso\":" + String(SPI_MISO) + ",\"pump\":" + String(unsigned(PUMP_ENABLE_PIN)) +
    ",\"direction\":" + String(unsigned(PUMP_DIRECTION_PIN)) + ",\"solar\":" + String(unsigned(SOLAR_PUMP_PIN)) +
    ",\"oil\":" + String(unsigned(OIL_BURNER_PIN)) + ",\"led\":" + String(LED_PIN) + ",\"rgbLed\":" + String(RGB_STATUS_PIN) +
    ",\"directionOff\":" + String(DIRECTION_RELAY_OFF_DIRECTION == PumpDirection::BufferToCombi ? 1 : 0) + "}" +
    ",\"statusLed\":{\"state\":" + jsonString(StatusLed::name(statusLed.mode())) +
    ",\"available\":" + String(statusLedPinSafe ? "true" : "false") + "}" +
    ",\"remoteRelay\":" + remoteRelayStatusJson() + ",\"remoteRelays\":[";
  for (unsigned i = 0; i < REMOTE_RELAY_COUNT; ++i) { if (i) json += ','; json += remoteRelayStatusJson(i); }
  json += "],\"relayDiscovery\":" + relayDiscoveryJson() + ",\"relayProvisioning\":" + relayProvisioningJson() + ",\"wifi\":" + wifiStatusJson() + ",\"clock\":" + clockStatusJson() +
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
  AutomaticThresholds next = automaticThresholds;
  if ((server.args() != 1 && server.args() != 3) || !server.hasArg("start") ||
      (server.args() == 3 && (!server.hasArg("minimumCombi") || !server.hasArg("minimumBuffer"))) ||
      !parseNumber(server.arg("start"), next.differenceStart) ||
      (server.hasArg("minimumCombi") && (!parseNumber(server.arg("minimumCombi"), next.minimumCombi) || floorf(next.minimumCombi) != next.minimumCombi)) ||
      (server.hasArg("minimumBuffer") && (!parseNumber(server.arg("minimumBuffer"), next.minimumBuffer) || floorf(next.minimumBuffer) != next.minimumBuffer)) ||
      !AutomaticControl::validThresholds(next)) {
    server.send(400, "text/plain; charset=utf-8", "Startdifferenz muss 5–10 K und beide Mindesttemperaturen müssen ganze Zahlen von 0–80 °C sein.");
    return;
  }
  // Versionskennung und beide Grenzen gemeinsam mit der Startdifferenz atomar
  // speichern; ältere Firmware ignoriert die stillgelegten Slots 0/2/3.
  const float saved[5] = {next.minimumCombi, next.differenceStart, AutomaticControl::MINIMUM_FORMAT_MARKER, next.minimumBuffer, 1.0f};
  if (!storageReady || preferences.putBytes("auto1", saved, sizeof(saved)) != sizeof(saved)) {
    server.send(500, "text/plain; charset=utf-8", "Speicherfehler. Regelung wurde nicht übernommen.");
    return;
  }
  // Die Startdifferenz gilt beim nächsten Start. Eine erhöhte Mindesttemperatur
  // stoppt einen laufenden Transfer sofort nach der aktuellen Messung.
  automaticThresholds = next;
  autoSettingsValid = true;
  measureTemperatures();
  lastMeasurement = millis();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", automaticStatusJson());
}

void loadSolarSettings() {
  if (storageReady && preferences.isKey("solar1")) {
    float saved = NAN;
    solarSettingsValid = preferences.getBytesLength("solar1") == sizeof(saved) &&
      preferences.getBytes("solar1", &saved, sizeof(saved)) == sizeof(saved) && SolarControl::validStart(saved);
    if (solarSettingsValid) solarStartDifference = saved;
    else FirmwareLog.println("Ungültige Solarregelung: Solarpumpe gesperrt; Startdifferenz erneut speichern.");
  }
}

void saveSolarSettings() {
  if (otaActive || otaVerificationPending || modeTransition) { server.send(409, "text/plain; charset=utf-8", "Update oder Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden."); return; }
  float next;
  if (server.args() != 1 || !parseNumber(server.arg("start"), next) || !SolarControl::validStart(next)) {
    server.send(400, "text/plain; charset=utf-8", "Startdifferenz muss zwischen 3 und 10 K in 0,1-K-Schritten liegen."); return;
  }
  if (!storageReady || preferences.putBytes("solar1", &next, sizeof(next)) != sizeof(next)) {
    server.send(500, "text/plain; charset=utf-8", "Speicherfehler. Solarregelung wurde nicht übernommen."); return;
  }
  solarStartDifference = next;
  solarSettingsValid = true;
  serviceSolarPump();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", solarStatusJson());
}

void controlSolarPump() {
  if (otaActive || otaVerificationPending || modeTransition) { server.send(409, "text/plain; charset=utf-8", "Update oder Neustart läuft."); return; }
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden."); return; }
  const String action = server.arg("action"), value = server.arg("value");
  if (server.args() != 2 || (action != "manual" && action != "power") || (value != "0" && value != "1")) {
    server.send(400, "text/plain; charset=utf-8", "Ungültiger Solarpumpenbefehl."); return;
  }
  const bool enabled = value == "1";
  if (action == "manual") solarControl.setManual(enabled);
  else if (!solarControl.setManualOn(enabled)) {
    server.send(409, "text/plain; charset=utf-8", "Zuerst den Handbetrieb der Solarpumpe einschalten."); return;
  }
  serviceSolarPump();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", solarStatusJson());
}

void applyHeatingMode(bool next) {
  const bool modeChanged = !heatingModeValid || oilBurnerEnabled != next;
  oilBurnerEnabled = next;
  heatingModeValid = true;
  if (modeChanged && !manualControl) {
    automaticControl.reset();
    pump.command(false, pump.direction(), millis());
  }
  servicePump(); // Sperre und Pumpe zuerst anwenden, dann Ölfreigabe schalten.
  updateAutomaticPump(); // Gewählten Fühler und mögliche Rückladung neu prüfen.
}

void saveHeatingMode() {
  if (otaActive || otaVerificationPending || modeTransition) {
    server.send(409, "text/plain; charset=utf-8", "Update oder Neustart läuft. Bitte danach erneut schalten."); return;
  }
  if (server.header("X-Requested-With") != "HeatingUI") {
    server.send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden."); return;
  }
  const String value = server.arg("winter");
  if (server.args() != 1 || (value != "0" && value != "1")) {
    server.send(400, "text/plain; charset=utf-8", "Ungültige Sommer-/Winterumschaltung."); return;
  }
  const bool next = value == "1";
  if (!storageReady || preferences.putBool("oil1", next) != 1) {
    server.send(500, "text/plain; charset=utf-8", "Betriebsart konnte nicht gespeichert werden. Schaltung unverändert."); return;
  }
  applyHeatingMode(next);
  FirmwareLog.printf("%sBetriebsart: %s | Ölbrennerfreigabe: %s | Relais 4: %s | Kombi -> Puffer: %s.\n\n",
    SIMULATION_MODE ? "[SIMULATION] " : "", next ? "Winter" : "Sommer", next ? "AN" : "AUS",
    oilBurnerRelayOn() ? "AN" : "AUS", chargingAllowed() ? "temperaturabhängig möglich" : "gesperrt");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", heatingStatusJson());
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
      server.send(409, "text/plain; charset=utf-8", "Zuerst die manuelle Steuerung einschalten.");
      return;
    }
    const bool nextOn = action == "power" ? enabled : manualPumpOn;
    const PumpDirection nextDirection = action == "direction"
      ? (enabled ? PumpDirection::BufferToCombi : PumpDirection::CombiToBuffer) : manualDirection;
    if (nextOn && nextDirection == PumpDirection::CombiToBuffer && !chargingAllowed()) {
      server.send(409, "text/plain; charset=utf-8", "Kombi -> Puffer gesperrt: Ölbrenner freigegeben oder Betriebsart unbekannt. Rückladung bleibt möglich."); return;
    }
    manualPumpOn = nextOn;
    manualDirection = nextDirection;
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
  if (remoteRelayAnyBusy() || remoteRelayUpdate.active() || relayProvisioning.active()) { server.send(409, "text/plain; charset=utf-8", "Die Relaiskarte ist beschäftigt. Bitte erneut versuchen."); return; }
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
  FirmwareLog.printf("OTA-Diagnose: Abbruch; Update-Fehler=%u (%s).\n",
    unsigned(Update.getError()), Update.errorString());
  if (Update.isRunning()) Update.abort();
  otaActive = false; otaFinished = false; otaToken = "";
  automaticControl.reset();
  FirmwareLog.println("Firmware-Update abgebrochen. Manuelle Steuerung bleibt ausgeschaltet.\n");
}

void logOtaPartition(const char* name, const esp_partition_t* partition) {
  if (!partition) { FirmwareLog.printf("OTA-Diagnose: %s: nicht vorhanden.\n", name); return; }
  FirmwareLog.printf("OTA-Diagnose: %s: %s, Adresse=0x%08lX, Groesse=%lu Bytes, Typ=%u, Subtyp=%u.\n",
    name, partition->label, (unsigned long)partition->address, (unsigned long)partition->size,
    unsigned(partition->type), unsigned(partition->subtype));
}

void logOtaEnvironment(const char* stage) {
  FirmwareLog.printf("OTA-Diagnose: %s | Laufzeit=%lu ms | Heap frei=%lu Bytes, groesster Block=%lu Bytes.\n",
    stage, millis(), (unsigned long)ESP.getFreeHeap(),
    (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  logOtaPartition("Aktuelle Partition", esp_ota_get_running_partition());
  logOtaPartition("Naechste OTA-Partition", esp_ota_get_next_update_partition(nullptr));
}

void logOtaProgress(size_t received, size_t total) {
  if (!total) return;
  const unsigned percent = unsigned((uint64_t(received) * 100) / total);
  if (percent >= otaLoggedPercent + 10 || (percent == 100 && otaLoggedPercent < 100)) {
    otaLoggedPercent = percent;
    FirmwareLog.printf("OTA-Diagnose: Fortschritt %u%% (%u/%u Bytes).\n", percent, unsigned(received), unsigned(total));
  }
}

bool checkOtaRequest() {
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return false; }
  if (!otaActive || otaFinished || githubUpdateActive || server.arg("token") != otaToken) { server.send(409, "text/plain", "Kein passendes Update aktiv."); return false; }
  otaLastActivity = millis();
  return true;
}

void prepareOta() {
  FirmwareLog.printf("OTA-Diagnose: Manueller Upload angefordert; Argumente=%u.\n", unsigned(server.args()));
  logOtaEnvironment("Vorbereitung manuell");
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return; }
  if (otaActive || otaVerificationPending || modeTransition || remoteRelayAnyBusy() || remoteRelayUpdate.active() || relayProvisioning.active()) { server.send(409, "text/plain", "Steuerung ist beschäftigt."); return; }
  uint32_t size = 0;
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (server.args() != 1 || !otaNumber(server.arg("size"), size) || size < 1024 || !next || size > next->size) {
    FirmwareLog.printf("OTA-Diagnose: Vorbereitung abgelehnt; Dateigroesse=%lu, Zielgroesse=%lu Bytes.\n",
      (unsigned long)size, next ? (unsigned long)next->size : 0UL);
    server.send(400, "text/plain; charset=utf-8", "Ungültige Dateigröße oder keine passende OTA-Partition."); return;
  }
  FirmwareLog.printf("OTA-Diagnose: Dateigroesse=%lu Bytes; Pumpe wird ausgeschaltet.\n", (unsigned long)size);
  otaActive = true; otaFinished = false;
  manualControl = false; manualPumpOn = false; automaticControl.reset();
  pump.command(false, pump.direction(), millis()); servicePump();
  otaStoppedAt = otaLastActivity = millis();
  FirmwareLog.println("OTA-Diagnose: Pumpe ausgeschaltet; Update.begin wird aufgerufen.");
  if (!Update.begin(size, U_FLASH)) { cancelOta(); server.send(500, "text/plain", "Update konnte nicht gestartet werden."); return; }
  otaExpectedSize = size; otaReceived = 0; otaLastChunkLength = 0; otaLoggedPercent = 0;
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
    FirmwareLog.printf("OTA-Diagnose: Ungueltiger Datenblock bei Offset %lu.\n", (unsigned long)offset);
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
    FirmwareLog.printf("OTA-Diagnose: Blockposition=%lu, erwartet=%u, Blockgroesse=%u.\n",
      (unsigned long)offset, unsigned(otaReceived), unsigned(length));
    cancelOta(); server.send(400, "text/plain; charset=utf-8", "Datenblöcke stimmen nicht überein. Update abgebrochen."); return;
  }
  if (Update.write(decoded, length) != length) {
    cancelOta(); server.send(500, "text/plain; charset=utf-8", "Schreibfehler. Update abgebrochen."); return;
  }
  otaLastChunkOffset = offset; otaLastChunkLength = length; memcpy(otaLastChunkHash, hash, sizeof(hash));
  otaReceived += length;
  logOtaProgress(otaReceived, otaExpectedSize);
  server.send(200, "application/json", "{\"received\":" + String(unsigned(otaReceived)) + "}");
}

void finishOta() {
  if (!checkOtaRequest()) return;
  FirmwareLog.printf("OTA-Diagnose: Abschluss angefordert; empfangen=%u/%u Bytes.\n", unsigned(otaReceived), unsigned(otaExpectedSize));
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
  FirmwareLog.println("OTA-Diagnose: Browser hat Abbruch angefordert.");
  cancelOta(); server.send(200, "application/json", "{\"aborted\":true}");
}

enum GithubDownloadResult { GITHUB_OK, GITHUB_RETRY, GITHUB_ERROR };
GithubDownloadResult githubDownloadAttempt(char* resultError, size_t resultErrorSize);

// Ein Versuch startet mit leerem Update-Puffer und einem neuen SHA-256-Kontext.
GithubDownloadResult githubDownloadAttempt(char* resultError, size_t resultErrorSize) {
  const char* error = nullptr;
  bool retryable = false;
  char requestError[256] = {};
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
  if (!error && code != HTTP_CODE_OK) {
    retryable = code <= 0 || code == 408 || code == 429 || code >= 500;
    if (code > 0) {
      snprintf(requestError, sizeof(requestError), "Download-Server antwortet mit HTTP %d.", code);
    } else {
      char secureError[120] = {};
      const int secureCode = client.lastError(secureError, sizeof(secureError));
      if (secureCode < -1) {
        snprintf(requestError, sizeof(requestError), "Verbindung fehlgeschlagen: HTTPClient %d (%s); Verbindungscode %d: %.100s.",
                 code, HTTPClient::errorToString(code).c_str(), secureCode, secureError);
      } else if (secureCode == -1) {
        snprintf(requestError, sizeof(requestError), "Verbindung fehlgeschlagen: HTTPClient %d (%s); Verbindungscode -1 (Ursache nicht eindeutig).",
                 code, HTTPClient::errorToString(code).c_str());
      } else {
        snprintf(requestError, sizeof(requestError), "Verbindung fehlgeschlagen: HTTPClient %d (%s); kein weiterer Verbindungscode verfügbar.",
                 code, HTTPClient::errorToString(code).c_str());
      }
    }
    error = requestError;
  }
  if (!error && http.getSize() != int(otaExpectedSize)) error = "Dateigröße stimmt nicht mit dem Release überein.";
  uint32_t total = 0, lastData = millis();
  uint8_t buffer[2048];
  WiFiClient* stream = error ? nullptr : http.getStreamPtr();
  while (!error && total < otaExpectedSize) {
    if (githubUpdateCancel.load()) { error = "Update wurde abgebrochen."; break; }
    const int available = stream->available();
    if (available <= 0) {
      if (available < 0 || !http.connected()) { error = "Verbindung während des Downloads unterbrochen."; retryable = true; break; }
      if (millis() - lastData > 15000) { error = "Zeitlimit: keine Daten während des Downloads."; retryable = true; break; }
      delay(10); continue;
    }
    size_t length = size_t(available) < sizeof(buffer) ? size_t(available) : sizeof(buffer);
    if (length > otaExpectedSize - total) length = otaExpectedSize - total;
    int read = stream->read(buffer, length);
    if (read <= 0) {
      if (!http.connected()) { error = "Verbindung beim Lesen des Downloads unterbrochen."; retryable = true; break; }
      if (millis() - lastData > 15000) { error = "Zeitlimit beim Lesen des Downloads."; retryable = true; break; }
      delay(10); continue;
    }
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
  if (!error) return GITHUB_OK;
  snprintf(resultError, resultErrorSize, "%s", error);
  return retryable ? GITHUB_RETRY : GITHUB_ERROR;
}

// Download und begrenzte Wiederholungen laufen auf einem eigenen Task. Bei
// jedem Neuversuch wird die unvollständige Firmware verworfen und vollständig
// neu geladen; erst ein erfolgreiches Image mit passender Prüfsumme wird aktiv.
void githubDownloadTask(void*) {
  char error[sizeof(githubUpdateError)] = {};
  GithubDownloadResult result = GITHUB_ERROR;
  for (unsigned attempt = 1; attempt <= GITHUB_DOWNLOAD_ATTEMPTS; ++attempt) {
    if (githubUpdateCancel.load()) { snprintf(error, sizeof(error), "Update wurde abgebrochen."); break; }
    if (attempt > 1) {
      Update.abort();
      delay(1000);
      if (githubUpdateCancel.load()) { snprintf(error, sizeof(error), "Update wurde abgebrochen."); break; }
      if (!Update.begin(otaExpectedSize, U_FLASH)) {
        snprintf(error, sizeof(error), "Neuversuch konnte nicht gestartet werden: %s.", Update.errorString());
        break;
      }
    }
    githubUpdateBytes.store(0);
    githubUpdateAttempt.store(attempt);
    result = githubDownloadAttempt(error, sizeof(error));
    if (result != GITHUB_RETRY) break;
    if (attempt < GITHUB_DOWNLOAD_ATTEMPTS)
      snprintf(githubRetryErrors[attempt - 1], sizeof(githubRetryErrors[0]), "%s", error);
  }
  if (result == GITHUB_OK) githubUpdateState.store(GH_DOWNLOADED);
  else { snprintf(githubUpdateError, sizeof(githubUpdateError), "%s", error); githubUpdateState.store(GH_FAILED); }
  vTaskDelete(nullptr);
}

void startGithubOta() {
  FirmwareLog.printf("OTA-Diagnose: GitHub-Update angefordert; Argumente=%u.\n", unsigned(server.args()));
  logOtaEnvironment("Vorbereitung GitHub");
  if (server.header("X-Requested-With") != "HeatingUI") { server.send(403, "text/plain", "Bitte das Webinterface verwenden."); return; }
  if (otaActive || otaVerificationPending || modeTransition || remoteRelayAnyBusy() || remoteRelayUpdate.active() || relayProvisioning.active()) { server.send(409, "text/plain", "Steuerung ist beschäftigt."); return; }
  const String url = server.arg("url"), digest = server.arg("sha256");
  const String prefix = "https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/", suffix = "/Backload.ino.bin";
  uint32_t size = 0; const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (server.args() != 3 || !url.startsWith(prefix) || !url.endsWith(suffix) || url.length() <= prefix.length() + suffix.length() ||
      !otaNumber(server.arg("size"), size) || size < 1024 || !next || size > next->size || digest.length() != 64) {
    FirmwareLog.printf("OTA-Diagnose: GitHub-Vorbereitung abgelehnt; Dateigroesse=%lu, Zielgroesse=%lu Bytes.\n",
      (unsigned long)size, next ? (unsigned long)next->size : 0UL);
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
  FirmwareLog.printf("OTA-Diagnose: GitHub-Dateigroesse=%lu Bytes; Pumpe wird ausgeschaltet.\n", (unsigned long)size);
  otaActive = true; otaFinished = false;
  manualControl = false; manualPumpOn = false; automaticControl.reset();
  pump.command(false, pump.direction(), millis()); servicePump();
  otaStoppedAt = otaLastActivity = millis();
  FirmwareLog.println("OTA-Diagnose: Pumpe ausgeschaltet; Update.begin wird aufgerufen.");
  if (!Update.begin(size, U_FLASH)) { cancelOta(); server.send(500, "text/plain", "Update konnte nicht gestartet werden."); return; }
  otaExpectedSize = size; otaReceived = 0;
  char token[17]; snprintf(token, sizeof(token), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  otaToken = token; githubUpdateToken = token; githubUpdateUrl = url; githubExpectedHash = digest;
  githubUpdateBytes.store(0); githubUpdateAttempt.store(0); githubUpdateCancel.store(false); githubUpdateError[0] = '\0'; otaLoggedPercent = 0;
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
    ",\"size\":" + String(unsigned(otaExpectedSize)) + ",\"attempt\":" + String(githubUpdateAttempt.load()) +
    ",\"maxAttempts\":" + String(GITHUB_DOWNLOAD_ATTEMPTS) + ",\"error\":" + jsonString(state == GH_FAILED ? githubUpdateError : "") + "}");
}

void serviceStatusLed(uint32_t now) {
  if (!statusLedPinSafe) return;
  bool fault = !storageReady || !heatingModeValid || !autoSettingsValid || !solarSettingsValid || !solarSensorsFresh() || !controllers[0].configValid;
  for (int i = 0; i < numControllers; ++i) fault = fault || controllers[i].sensorError != nullptr;
  const StatusLedMode mode = StatusLed::select(fault, otaActive || otaVerificationPending, manualControl || solarControl.manual(), SIMULATION_MODE);
  if (statusLed.update(now, mode)) {
    const StatusLedColor color = statusLed.color();
    neopixelWrite(RGB_STATUS_PIN, color.red, color.green, color.blue);
  }
}

void setup() {
  // Auch im Testbetrieb die echten Active-Low-Ausgänge auf AUS halten.
  digitalWrite(PUMP_ENABLE_PIN, HIGH);
  pinMode(PUMP_ENABLE_PIN, OUTPUT);
  digitalWrite(PUMP_DIRECTION_PIN, HIGH);
  pinMode(PUMP_DIRECTION_PIN, OUTPUT);
  digitalWrite(SOLAR_PUMP_PIN, HIGH);
  pinMode(SOLAR_PUMP_PIN, OUTPUT);
  digitalWrite(OIL_BURNER_PIN, HIGH);
  pinMode(OIL_BURNER_PIN, OUTPUT);
  esp_ota_img_states_t imageState;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &imageState) == ESP_OK && imageState == ESP_OTA_IMG_PENDING_VERIFY)
    otaVerificationPending = true;
  Preferences mode;
  if (mode.begin("backload-mode", true)) {
    SIMULATION_MODE = mode.getBool("test", SIMULATION_MODE);
    mode.end();
  }
  AP_SSID = SIMULATION_MODE ? "ESP32-Backload-Test" : "ESP32-Solarsteuerung";
  // Keine LED-Ausgabe, falls ein Sensor-Chip-Select auf denselben GPIO gelegt wurde.
  statusLedPinSafe = true;
  for (int i = 0; i < numControllers; ++i) if (controllers[i].csPin == RGB_STATUS_PIN) statusLedPinSafe = false;
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
#if ARDUINO_USB_CDC_ON_BOOT
  // Serial ist natives USB-CDC; Serial0 bedient zusätzlich den USB-UART-Wandler.
  Serial0.begin(115200);
#endif
  delay(1000);  // Sensorversorgung vor der Initialisierung stabilisieren lassen
  if (SIMULATION_MODE) simulationPhase = float(esp_random() % 6283U) / 1000.0f;
  FirmwareLog.printf("Heizungssteuerung – Firmware %s\n", FIRMWARE_VERSION);
  FirmwareLog.printf("OTA-Diagnose: Reset-Ursache=%d | Flash=%lu Bytes | PSRAM=%lu Bytes | Core=%d.%d.%d.\n",
    int(esp_reset_reason()), (unsigned long)ESP.getFlashChipSize(), (unsigned long)ESP.getPsramSize(),
    ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  logOtaEnvironment("Systemstart");
  auto* historyBuffer = static_cast<TemperatureHistory::Sample*>(
    heap_caps_malloc(HISTORY_CAPACITY * sizeof(TemperatureHistory::Sample), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  temperatureHistory.attach(historyBuffer, HISTORY_CAPACITY, [](){ servicePump(); yield(); });
  if (temperatureHistory.available()) FirmwareLog.printf("Temperaturverlauf: %u Minuten im PSRAM reserviert.\n", unsigned(HISTORY_CAPACITY));
  else FirmwareLog.println("Temperaturverlauf nicht verfügbar: PSRAM fehlt oder ist zu klein. Regelung läuft weiter.");
  if (SIMULATION_MODE) FirmwareLog.println("[SIMULATION] Aktiv: simulierte Sensoren, Pumpe und Ölfreigabe; Relais am Haupt-ESP AUS, WLAN-Relais schaltbar.");
  runtimeReady = runtimePreferences.begin("pump-runtime", false);
  if (runtimeReady && runtimePreferences.isKey("periods1")) {
    PumpRuntime::State saved{};
    runtimeReady = runtimePreferences.getBytesLength("periods1") == sizeof(saved) &&
      runtimePreferences.getBytes("periods1", &saved, sizeof(saved)) == sizeof(saved) &&
      pumpRuntime.restore(&saved, sizeof(saved));
  }
  runtimeStorageHealthy = runtimeReady;
  if (!runtimeReady) FirmwareLog.println("Pumpenlaufzeiten nicht verfügbar: Flash-Daten unlesbar oder Speicher nicht erreichbar; Daten werden nicht überschrieben.");
  trackPumpRuntime(true);
  storageReady = preferences.begin(SIMULATION_MODE ? "heating-test" : "heating", false);
  if (!storageReady) FirmwareLog.println("NVS nicht verfügbar: Speichern nicht möglich.");
  heatingModeValid = storageReady;
  if (storageReady && preferences.isKey("oil1")) {
    const uint8_t savedMode = preferences.getUChar("oil1", 2);
    heatingModeValid = savedMode <= 1;
    oilBurnerEnabled = savedMode == 1;
  }
  FirmwareLog.printf("Betriebsart: %s | Ölbrennerfreigabe: %s | Relais 4 GPIO %u.\n",
    heatingModeValid ? (oilBurnerEnabled ? "Winter" : "Sommer") : "unbekannt; Einlagerung gesperrt",
    oilBurnerEnabled ? "AN" : "AUS", unsigned(OIL_BURNER_PIN));
  loadSolarSettings();
  float autoSaved[5];
  if (storageReady && preferences.isKey("auto1")) {
    autoSettingsValid = false;
    if (preferences.getBytesLength("auto1") == sizeof(autoSaved) &&
        preferences.getBytes("auto1", autoSaved, sizeof(autoSaved)) == sizeof(autoSaved)) {
      AutomaticThresholds candidate;
      if (AutomaticControl::readSavedThresholds(autoSaved, candidate)) {
        automaticThresholds = candidate;
        // Alte Fühlerwahl niemals als neue Ölbrennerfreigabe übernehmen.
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
        FirmwareLog.printf("KONFIGURATIONSFEHLER bei %s: Einlagerungsstart gesperrt; Rückladung bleibt möglich.\n", c.name);
      }
      String key = "c" + String(int(c.settingsSlot));
      float values[2];
      if (c.configValid && storageReady && preferences.getBytesLength(key.c_str()) == sizeof(values) &&
          preferences.getBytes(key.c_str(), values, sizeof(values)) == sizeof(values)) {
        if (validSettings(*c.limits, values[0], values[1])) {
          c.targetTemp = values[0];
          c.hysteresis = values[1];
        } else {
          FirmwareLog.printf("Alter Sollwert bei %s außerhalb der neuen Grenzen; verwende Starttemperatur %.1f°C. Alten NVS-Datensatz beibehalten.\n", c.name, c.targetTemp);
        }
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
  loadHostname();
  WiFi.persistent(false); // Zugangsdaten ausschließlich im eigenen NVS-Blob verwalten.
  WiFi.setHostname(activeHostname.c_str());
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false); // Wiederholungen werden in processWifi() gesteuert.
  WiFi.disconnect(false, true);
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    FirmwareLog.print("Webinterface: http://"); FirmwareLog.println(WiFi.softAPIP());
  } else {
    FirmwareLog.println("WLAN konnte nicht gestartet werden. Temperaturregelung läuft weiter.");
  }
  mdnsReady = MDNS.begin(activeHostname);
  if (mdnsReady) { MDNS.addService("http", "tcp", 80); FirmwareLog.printf("mDNS: http://%s.local/\n", activeHostname.c_str()); }
  else FirmwareLog.println("mDNS konnte nicht gestartet werden; Zugriff über IP bleibt möglich.");
  if (ROOF_SENSOR_ENABLED && !roofUdp.begin(ROOF_HEARTBEAT_PORT)) {
    FirmwareLog.println("UDP-Empfang für Dachsensor konnte nicht gestartet werden.");
  }
  loadWifiCredentials();
  for (unsigned i = 0; i < REMOTE_RELAY_COUNT; ++i) {
    char key[16]; snprintf(key, sizeof(key), "config%u", i + 1);
    remoteRelayCardReady[i] = remoteRelays[i].begin(key);
    if (!remoteRelayCardReady[i]) FirmwareLog.printf("Relaiserweiterung %u konnte nicht initialisiert werden.\n", i + 1);
  }
  remoteRelayReady = remoteRelayCardReady[0];
  relayDiscoveryReady = relayDiscovery.begin();
  relayProvisioningReady = relayProvisioning.begin(remoteRelays, REMOTE_RELAY_COUNT, &relayDiscovery);
  remoteRelayUpdateReady = remoteRelayUpdate.begin();
  if (!remoteRelayUpdateReady) FirmwareLog.println("Relais-Update konnte nicht initialisiert werden.");
  if (!remoteRelayReady) FirmwareLog.println("WLAN-Relaisanbindung konnte nicht gestartet werden.");
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
  server.on("/api/remote-relay/config", HTTP_POST, saveRemoteRelayConfig);
  server.on("/api/remote-relay/discover", HTTP_POST, startRelayDiscovery);
  server.on("/api/remote-relay/pair", HTTP_POST, startRelayProvisioning);
  server.on("/api/remote-relay/pair/scan", HTTP_GET, sendRelayPairScan);
  server.on("/api/remote-relay/pair/cancel", HTTP_POST, cancelRelayProvisioning);
  server.on("/api/remote-relay/command", HTTP_POST, saveRemoteRelayCommand);
  server.on("/api/remote-relay/forget", HTTP_POST, forgetRemoteRelay);
  server.on("/api/remote-relay/discard", HTTP_POST, discardRemoteRelay);
  server.on("/api/remote-relay/update/prepare", HTTP_POST, prepareRemoteRelayUpdate);
  server.on("/api/remote-relay/update/github", HTTP_POST, startRemoteRelayGithubUpdate);
  server.on("/api/remote-relay/update/chunk", HTTP_POST, receiveRemoteRelayUpdateChunk);
  server.on("/api/remote-relay/update/start", HTTP_POST, startRemoteRelayUpdate);
  server.on("/api/remote-relay/update/status", HTTP_POST, remoteRelayUpdateStatus);
  server.on("/api/remote-relay/update/abort", HTTP_POST, cancelRemoteRelayUpdate);
  server.on("/api/automatic", HTTP_POST, saveAutomaticSettings);
  server.on("/api/solar", HTTP_POST, saveSolarSettings);
  server.on("/api/solar/control", HTTP_POST, controlSolarPump);
  server.on("/api/heating", HTTP_POST, saveHeatingMode);
  server.on("/api/test-mode", HTTP_POST, setTestMode);
  server.on("/api/update/prepare", HTTP_POST, prepareOta);
  server.on("/api/update/chunk", HTTP_POST, writeOtaChunk);
  server.on("/api/update/finish", HTTP_POST, finishOta);
  server.on("/api/update/abort", HTTP_POST, abortOta);
  server.on("/api/update/github", HTTP_POST, startGithubOta);
  server.on("/api/update/github-status", HTTP_POST, githubOtaStatus);
  if (SIMULATION_MODE) server.on("/api/simulation", HTTP_POST, saveSimulationValues);
  server.on("/api/hostname", HTTP_POST, saveHostname);
  server.on("/api/history-server", HTTP_GET, sendHistoryServerUrl);
  server.on("/api/history-server", HTTP_POST, saveHistoryServerUrl);
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
  remoteRelayUpdate.loop();
  relayProvisioning.loop();
  unsigned long now = millis();
  if (otaActive && otaFinished && now - otaLastActivity >= 2500 && now - otaStoppedAt >= 2500) { savePumpRuntime(true); ESP.restart(); }
  if (otaActive && !otaFinished && !githubUpdateActive && now - otaLastActivity >= 30000) {
    FirmwareLog.println("OTA-Diagnose: Seit 30 Sekunden kein Datenblock; Zeitlimit erreicht.");
    cancelOta();
  }
  static unsigned loggedGithubAttempt = 0;
  if (githubUpdateActive) {
    const unsigned attempt = githubUpdateAttempt.load();
    if (attempt && attempt != loggedGithubAttempt) {
      loggedGithubAttempt = attempt; otaLoggedPercent = 0;
      if (attempt > 1)
        FirmwareLog.printf("OTA-Diagnose: GitHub-Download Versuch %u/%u erneut ab Anfang; vorher: %s\n",
          attempt, GITHUB_DOWNLOAD_ATTEMPTS, githubRetryErrors[attempt - 2]);
      else FirmwareLog.printf("OTA-Diagnose: GitHub-Download Versuch %u/%u.\n", attempt, GITHUB_DOWNLOAD_ATTEMPTS);
    }
    logOtaProgress(githubUpdateBytes.load(), otaExpectedSize);
  } else loggedGithubAttempt = 0;
  if (githubUpdateActive && (githubUpdateState.load() == GH_DOWNLOADED || githubUpdateState.load() == GH_FAILED)) {
    const bool downloaded = githubUpdateState.load() == GH_DOWNLOADED;
    githubUpdateActive = false;
    if (downloaded && Update.end()) {
      otaFinished = true; otaLastActivity = millis(); githubUpdateState.store(GH_FINISHED);
      FirmwareLog.println("GitHub-Firmware geprüft. Pumpe AUS, Neustart folgt.\n");
    } else {
      if (downloaded) snprintf(githubUpdateError, sizeof(githubUpdateError), "Firmware-Image ist ungültig.");
      FirmwareLog.printf("GitHub-Update fehlgeschlagen: %s | Empfangen=%u/%u Bytes.\n",
        githubUpdateError, unsigned(githubUpdateBytes.load()), unsigned(otaExpectedSize));
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
  if (modeTransition && now - modeRestartAt >= 2500) { savePumpRuntime(true); ESP.restart(); }
  serviceStatusLed(now);
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
  processRecoveryAp();
  if (ROOF_SENSOR_ENABLED) processRoofHeartbeat();
  processWebSerialMonitor();
  servicePump();
  savePumpRuntime();
  delay(1);
}
