"""Hosttest der echten Sketch-Funktionen mit GPIO-Protokoll und Sensorwerten."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sketch = (ROOT / "Backload/Backload.ino").read_text()


def function(signature):
    start = sketch.index(signature + " {")
    return sketch[start:sketch.index("\n}\n", start) + 3]


harness = r'''
#include "AutomaticControl.h"
#include "PumpControl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <utility>

constexpr int LOW = 0, HIGH = 1;
constexpr int PUMP_ENABLE_PIN = 3, PUMP_DIRECTION_PIN = 8, OIL_BURNER_PIN = 10;
constexpr PumpDirection DIRECTION_RELAY_OFF_DIRECTION = PumpDirection::CombiToBuffer;
PumpControl pump;
AutomaticControl automaticControl;
AutomaticThresholds automaticThresholds = {8};
bool oilBurnerEnabled=false, heatingModeValid=true, SIMULATION_MODE=false;
bool modeTransition=false, otaActive=false, otaVerificationPending=false;
bool manualControl=false, manualPumpOn=false, storageReady=true, autoSettingsValid=true;
constexpr bool AUTOMATIC_PUMP_CONFIGURED=true;
PumpDirection manualDirection=PumpDirection::CombiToBuffer;
uint32_t now=0;
uint32_t millis() { return now; }
int pins[16];
std::vector<std::pair<int,int>> writes;
void digitalWrite(int pin,int value) {
  pins[pin]=value; writes.emplace_back(pin,value);
  // Zu keinem Zeitpunkt dürfen Ölfreigabe und Vorladung gleichzeitig anliegen.
  assert(!(pins[OIL_BURNER_PIN]==LOW && pins[PUMP_ENABLE_PIN]==LOW &&
    pins[PUMP_DIRECTION_PIN]==HIGH));
}
struct MockLog {
  template<class... T> void printf(const char*,T...) {}
  void print(const char*) {}
  void println() {}
} FirmwareLog;
void printMeasurementHeader() {}
void serviceSolarPump() {}
const char* pumpDirectionName(PumpDirection) { return "test"; }
struct TemperatureController { bool valid,configValid; float currentTemp,targetTemp; };
TemperatureController controllers[5]={{true,true,68,68},{false,false,NAN,0},
  {true,false,60,0},{false,false,NAN,0},{false,false,NAN,0}};
'''
harness += "\n".join(function(signature) for signature in (
    "bool chargingAllowed()", "bool oilBurnerRelayOn()",
    "void servicePump()", "void updateAutomaticPump()", "void applyHeatingMode(bool next)"))
harness += r'''
int main() {
  for (int& pin:pins) pin=HIGH;
  // Sommer: oben und Puffer regeln; Mitte/unten/Solar dürfen fehlen.
  updateAutomaticPump();
  assert(pump.on() && pins[PUMP_ENABLE_PIN]==LOW);
  controllers[1]={true,false,800,0};
  controllers[3]={true,false,-150,0};
  controllers[4]={true,false,800,0};
  updateAutomaticPump(); assert(pump.on());
  controllers[2].valid=false;
  updateAutomaticPump(); assert(!pump.on());
  controllers[2].valid=true;
  updateAutomaticPump(); assert(pump.on());
  controllers[0].valid=false;
  updateAutomaticPump(); assert(!pump.on());
  controllers[0].valid=true;
  updateAutomaticPump(); assert(pump.on());

  // Winter sperrt eine laufende Vorladung vor dem Einschalten des Brennerrelais.
  writes.clear(); applyHeatingMode(true);
  assert(!pump.on() && pins[PUMP_ENABLE_PIN]==HIGH && pins[OIL_BURNER_PIN]==LOW);
  assert(writes.front()==std::make_pair(PUMP_ENABLE_PIN,HIGH));
  assert(writes.back()==std::make_pair(OIL_BURNER_PIN,LOW));
  updateAutomaticPump(); assert(!pump.on());

  // Auch direkte Handbefehle umgehen die Sperre nicht.
  manualControl=true;manualPumpOn=true;
  pump.command(true,PumpDirection::CombiToBuffer,now);
  servicePump();assert(!pump.on() && !manualPumpOn);
  manualControl=false;

  // Rückladung bei 37/45°C bleibt im Winter möglich, inklusive Umkehrpause.
  controllers[1].currentTemp=37;controllers[2].currentTemp=45;
  now=100;updateAutomaticPump();assert(!pump.on());
  now=2099;servicePump();assert(!pump.on());
  now=2100;servicePump();assert(!pump.on());
  now=2200;servicePump();assert(pump.on());
  assert(pump.direction()==PumpDirection::BufferToCombi && pins[OIL_BURNER_PIN]==LOW);
  manualControl=true;manualDirection=PumpDirection::BufferToCombi;manualPumpOn=true;
  servicePump();assert(pump.on() && manualPumpOn);
  manualControl=false;
  // Beim Fühlerwechsel muss eine Rückladung die Startdifferenz erneut erfüllen.
  controllers[0].currentTemp=44;applyHeatingMode(false);
  assert(!pump.on() && pins[OIL_BURNER_PIN]==HIGH);
  applyHeatingMode(true);assert(pump.on() && pins[OIL_BURNER_PIN]==LOW);
  // Winter verwendet Mitte: oben darf fehlen, ein Fehler an Mitte stoppt.
  controllers[0].valid=false;updateAutomaticPump();assert(pump.on());
  controllers[1].valid=false;updateAutomaticPump();assert(!pump.on());
  controllers[1].valid=true;updateAutomaticPump();assert(pump.on());
  controllers[0].valid=true;
  controllers[0].configValid=false;updateAutomaticPump();assert(pump.on());
  controllers[0].configValid=true;
  controllers[1].currentTemp=41;updateAutomaticPump();assert(pump.on());
  controllers[1].currentTemp=42;updateAutomaticPump();assert(!pump.on());
  controllers[1].currentTemp=37;updateAutomaticPump();assert(pump.on());

  // Unbekannte Betriebsart: Fühlerwahl unbekannt, Automatik und Brenner AUS.
  heatingModeValid=false;servicePump();assert(pump.on() && pins[OIL_BURNER_PIN]==HIGH);
  updateAutomaticPump();assert(!pump.on());
  heatingModeValid=true;
  otaActive=true;servicePump();assert(pins[OIL_BURNER_PIN]==HIGH);
  otaActive=false;servicePump();assert(pins[OIL_BURNER_PIN]==LOW);
  otaVerificationPending=true;servicePump();assert(pins[OIL_BURNER_PIN]==HIGH);
  otaVerificationPending=false;
  modeTransition=true;servicePump();assert(pins[OIL_BURNER_PIN]==HIGH);
  modeTransition=false;

  // Simulation berücksichtigt die Sperre, lässt sämtliche physischen Relais aus.
  pump=PumpControl();automaticControl.reset();
  for (int& pin:pins) pin=HIGH;
  SIMULATION_MODE=true;
  controllers[1].currentTemp=37;
  updateAutomaticPump();now+=2200;servicePump();now+=100;servicePump();
  assert(pump.on() && pins[PUMP_ENABLE_PIN]==HIGH && pins[PUMP_DIRECTION_PIN]==HIGH &&
    pins[OIL_BURNER_PIN]==HIGH);
  puts("PASS: Sommer-/Winterfühler, 3K-Stopp, Sensorfehler, Ölsperre, GPIO-Schaltreihenfolge und Simulation.");
}
'''

with tempfile.TemporaryDirectory(prefix="backload-heating-tests-") as temp:
    source = Path(temp) / "integration.cpp"
    source.write_text(harness)
    binary = Path(temp) / "integration"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(ROOT / "Backload"),
                    str(source), str(ROOT / "Backload/AutomaticControl.cpp"),
                    str(ROOT / "Backload/PumpControl.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
