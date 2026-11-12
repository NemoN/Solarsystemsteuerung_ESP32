"""Hosttest der echten Sketch-Funktionen mit GPIO-Protokoll und Sensorwerten."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sketch = (ROOT / "Backload/Backload.ino").read_text()
for name, pin in (("PUMP_ENABLE_PIN", 3), ("PUMP_DIRECTION_PIN", 8), ("OIL_BURNER_PIN", 17)):
    assert re.search(rf'\b{name}\s*=\s*{pin}\s*;', sketch)


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
constexpr int PUMP_ENABLE_PIN = 3, PUMP_DIRECTION_PIN = 8, OIL_BURNER_PIN = 17;
constexpr PumpDirection DIRECTION_RELAY_OFF_DIRECTION = PumpDirection::CombiToBuffer;
PumpControl pump;
AutomaticControl automaticControl;
AutomaticThresholds automaticThresholds = {8};
bool oilBurnerEnabled=false, heatingModeValid=true, SIMULATION_MODE=false;
bool modeTransition=false, otaActive=false, otaVerificationPending=false;
bool manualControl=false, manualPumpOn=false, storageReady=true, autoSettingsValid=true;
constexpr bool AUTOMATIC_PUMP_CONFIGURED=true;
PumpDirection manualDirection=PumpDirection::CombiToBuffer;
bool runtimePumpOn=false;
PumpDirection runtimePumpDirection=PumpDirection::CombiToBuffer;
void trackPumpRuntime(bool = false) {}
uint32_t now=0;
uint32_t millis() { return now; }
int pins[40];
std::vector<std::pair<int,int>> writes;
void digitalWrite(int pin,int value) {
  pins[pin]=value; writes.emplace_back(pin,value);
  // Die Ölsperre gilt für die Automatik, im Handbetrieb ist sie überbrückbar.
  assert(manualControl || !(pins[OIL_BURNER_PIN]==LOW && pins[PUMP_ENABLE_PIN]==LOW &&
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
TemperatureController controllers[5]={{true,true,68,68},{true,false,55,0},
  {true,false,60,0},{false,false,NAN,0},{false,false,NAN,0}};
'''
harness += "\n".join(function(signature) for signature in (
    "bool chargingAllowed()", "bool oilBurnerRelayOn()",
    "void servicePump()", "void updateAutomaticPump()", "void applyHeatingMode(bool next)"))
harness += r'''
int main() {
  for (int& pin:pins) pin=HIGH;
  // Einlagerung nutzt oben, Mindesttemperatur immer Mitte.
  updateAutomaticPump(); assert(pump.on() && pump.direction()==PumpDirection::CombiToBuffer);
  controllers[1].currentTemp=34;updateAutomaticPump();assert(!pump.on());
  controllers[1].currentTemp=55;updateAutomaticPump();assert(pump.on());
  controllers[0].valid=false;updateAutomaticPump();assert(!pump.on());
  controllers[0].valid=true;updateAutomaticPump();assert(pump.on());
  // Ölfreigabe stoppt automatische Einlagerung, bevor das Relais einschaltet.
  writes.clear();applyHeatingMode(true);
  assert(!pump.on() && pins[PUMP_ENABLE_PIN]==HIGH && pins[OIL_BURNER_PIN]==LOW);
  assert(writes.front()==std::make_pair(PUMP_ENABLE_PIN,HIGH));
  // Handbetrieb erlaubt ausdrücklich beide Ausgänge gleichzeitig.
  manualControl=true;manualPumpOn=true;manualDirection=PumpDirection::CombiToBuffer;
  servicePump();pump.command(true,manualDirection,now);servicePump();
  assert(pump.on() && pins[PUMP_ENABLE_PIN]==LOW && pins[OIL_BURNER_PIN]==LOW);
  applyHeatingMode(false);assert(pump.on() && pins[OIL_BURNER_PIN]==HIGH);
  applyHeatingMode(true);assert(pump.on() && pins[OIL_BURNER_PIN]==LOW);
  // Wechsel zurück zu Automatik stoppt Vorladung bei aktiver Ölfreigabe.
  manualControl=false;pump.command(false,manualDirection,now);servicePump();updateAutomaticPump();assert(!pump.on());
  // Rückladung nutzt Mitte unabhängig von Ölfreigabe und oberem Sensor.
  pump=PumpControl(PumpDirection::BufferToCombi);automaticControl.reset();
  controllers[0].valid=false;controllers[1].currentTemp=37;controllers[2].currentTemp=45;
  updateAutomaticPump();assert(pump.on() && pump.direction()==PumpDirection::BufferToCombi);
  controllers[1].currentTemp=42;updateAutomaticPump();assert(!pump.on());
  controllers[1].currentTemp=37;updateAutomaticPump();assert(pump.on());
  controllers[1].valid=false;updateAutomaticPump();assert(!pump.on());
  controllers[1].valid=true;updateAutomaticPump();assert(pump.on());
  controllers[2].currentTemp=34;updateAutomaticPump();assert(!pump.on());
  controllers[2].currentTemp=45;updateAutomaticPump();assert(pump.on());
  heatingModeValid=false;updateAutomaticPump();assert(!pump.on());
  heatingModeValid=true;otaActive=true;servicePump();assert(pins[OIL_BURNER_PIN]==HIGH);
  otaActive=false;otaVerificationPending=true;servicePump();assert(pins[OIL_BURNER_PIN]==HIGH);
  otaVerificationPending=false;modeTransition=true;servicePump();assert(pins[OIL_BURNER_PIN]==HIGH);
  modeTransition=false;
  // Simulation schaltet keine physischen Relais.
  heatingModeValid=true;SIMULATION_MODE=true;pump=PumpControl(PumpDirection::BufferToCombi);
  for (int& pin:pins) pin=HIGH;
  automaticControl.reset();updateAutomaticPump();
  assert(pump.on() && !runtimePumpOn && pins[PUMP_ENABLE_PIN]==HIGH && pins[OIL_BURNER_PIN]==HIGH);
  puts("PASS: Richtungssensoren, Mindesttemperaturen, Ölsperre, Handbetrieb und Simulation.");
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
