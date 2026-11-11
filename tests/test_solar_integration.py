"""Echte Sketch-Funktionen mit simuliertem GPIO, Webserver und NVS prüfen."""
from pathlib import Path
import subprocess
import tempfile
import re

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
proxy = (root / 'Backload/WebSerialMonitor.cpp').read_text()
assert re.search(r'\bSOLAR_PUMP_PIN\s*=\s*18\s*;', sketch)
assert 'server.on("/api/solar/control", HTTP_POST, controlSolarPump)' in sketch
assert 'dashboard.server().on("/api/solar/control", HTTP_POST, proxyRequest)' in proxy
def function(signature):
    start = sketch.index(signature + ' {')
    return sketch[start:sketch.index('\n}\n', start) + 3]

harness = r'''
#include "SolarControl.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
using String=std::string;
constexpr int HIGH=1,LOW=0,SOLAR_PUMP_PIN=18;
bool storageReady=true,solarSettingsValid=true,modeTransition=false,otaActive=false,otaVerificationPending=false,SIMULATION_MODE=false;
bool runtimeSolarOn=false;
void trackPumpRuntime(bool = false) {}
float solarStartDifference=SolarControl::DEFAULT_START;
uint32_t now=0,lastSolarMeasurement=0;bool solarMeasurementReady=true;
constexpr uint32_t SOLAR_MAX_MEASUREMENT_AGE=10000;
uint32_t millis(){return now;}
SolarControl solarControl;
SolarPanelGrace solarPanelGrace;
bool solarGraceActive=false;
struct TemperatureController {bool valid;float currentTemp;uint8_t fault;const char* sensorError;};
TemperatureController controllers[5]={{},{},{},{true,40,0,nullptr},{true,45,0,nullptr}};
int pin=HIGH,writes=0;
void digitalWrite(int gpio,int value){assert(gpio==18);pin=value;++writes;}
struct Log {template<typename... T> void printf(const char*,T...){} void println(const char*){}} FirmwareLog;
struct Prefs {
  std::vector<unsigned char> data;
  bool writeOk=true,readOk=true;
  bool isKey(const char*) {return !data.empty();}
  size_t getBytesLength(const char*) {return data.size();}
  size_t getBytes(const char*,void* out,size_t size) {if(!readOk||size!=data.size())return 0;memcpy(out,data.data(),size);return size;}
  size_t putBytes(const char*,const void* in,size_t size) {if(!writeOk)return 0;data.assign((const unsigned char*)in,(const unsigned char*)in+size);return size;}
} preferences;
struct Server {
  std::map<std::string,std::string> args_;
  std::string header_="HeatingUI",body;
  int status=0;
  std::string header(const char*) {return header_;}
  size_t args(){return args_.size();}
  std::string arg(const char* key) {return args_[key];}
  void send(int code,const char*,const char* text){status=code;body=text;}
  void send(int code,const char*,const std::string& text){status=code;body=text;}
  void sendHeader(const char*,const char*){}
} server;
bool parseNumber(std::string s,float& result) {char* end; result=strtof(s.c_str(),&end);return !s.empty() && end==s.c_str()+s.size() && isfinite(result);}
std::string solarStatusJson(){return "ok";}
'''
harness += '\n'.join(function(sig) for sig in ('bool solarSensorsFresh()', 'void serviceSolarPump()', 'void loadSolarSettings()', 'void saveSolarSettings()', 'void controlSolarPump()'))
harness += r'''
int main(){
  solarPanelGrace.record(45,now);
  serviceSolarPump();assert(solarControl.on() && pin==LOW && runtimeSolarOn);
  controllers[4].valid=false;controllers[4].currentTemp=NAN;controllers[4].fault=0x10;
  controllers[4].sensorError="MAX31865 meldet einen Sensorfehler";
  now=5000;lastSolarMeasurement=now;serviceSolarPump();assert(solarControl.on()&&solarGraceActive&&pin==LOW);
  controllers[3].valid=false;serviceSolarPump();assert(!solarControl.on()&&!solarGraceActive);
  controllers[3].valid=true;serviceSolarPump();assert(!solarControl.on());
  controllers[4].valid=true;controllers[4].currentTemp=45;solarPanelGrace.record(45,now);
  serviceSolarPump();assert(solarControl.on());
  controllers[4].valid=false;controllers[4].currentTemp=NAN;serviceSolarPump();assert(solarGraceActive);
  controllers[3].currentTemp=44;serviceSolarPump();assert(!solarControl.on()&&!solarGraceActive&&pin==HIGH);
  controllers[3].currentTemp=40;serviceSolarPump();assert(!solarControl.on()); // Kein Neustart mit altem Wert
  controllers[4].valid=true;controllers[4].currentTemp=45;serviceSolarPump();assert(solarControl.on());
  controllers[4].valid=false;controllers[4].fault=0x18;now=15000;lastSolarMeasurement=now;
  serviceSolarPump();assert(solarControl.on()&&solarGraceActive);
  now=15001;serviceSolarPump();assert(!solarControl.on()&&!solarGraceActive&&pin==HIGH);
  controllers[4].valid=true;controllers[4].currentTemp=45;now=0;lastSolarMeasurement=0;
  solarPanelGrace.record(45,now);serviceSolarPump();assert(solarControl.on());
  controllers[4].valid=false;controllers[4].fault=0x14;serviceSolarPump();assert(!solarControl.on());
  controllers[4].valid=true;serviceSolarPump();assert(solarControl.on());
  controllers[4].valid=false;controllers[4].fault=0x08;controllers[4].sensorError="MAX31865-Konfiguration nicht lesbar";
  serviceSolarPump();assert(!solarControl.on());
  controllers[4].valid=true;controllers[4].fault=0;controllers[4].sensorError=nullptr;
  serviceSolarPump();assert(solarControl.on());
  controllers[4].currentTemp=42;serviceSolarPump();assert(solarControl.on() && pin==LOW);
  controllers[3].valid=false;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  controllers[3].valid=true;controllers[4].currentTemp=45;serviceSolarPump();assert(solarControl.on());
  otaActive=true;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  otaActive=false;serviceSolarPump();assert(solarControl.on() && pin==LOW);
  otaVerificationPending=true;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  otaVerificationPending=false;modeTransition=true;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  modeTransition=false;SIMULATION_MODE=true;pin=HIGH;int before=writes;serviceSolarPump();
  assert(solarControl.on() && pin==HIGH && writes==before && !runtimeSolarOn);
  controllers[4].valid=false;serviceSolarPump();assert(!solarControl.on() && pin==HIGH && writes==before);
  controllers[4].valid=true;
  server.args_["start"]="7.5";saveSolarSettings();assert(server.status==200 && solarStartDifference==7.5f);
  solarStartDifference=5;loadSolarSettings();assert(solarSettingsValid && solarStartDifference==7.5f);
  preferences.writeOk=false;server.args_["start"]="6";saveSolarSettings();assert(server.status==500 && solarStartDifference==7.5f);
  preferences.writeOk=true;server.args_["start"]="11";saveSolarSettings();assert(server.status==400 && solarStartDifference==7.5f);
  server.args_["start"]="3";server.header_="evil";saveSolarSettings();assert(server.status==403);
  server.header_="HeatingUI";otaActive=true;saveSolarSettings();assert(server.status==409);otaActive=false;
  float bad=NAN;preferences.putBytes("solar1",&bad,sizeof(bad));loadSolarSettings();assert(!solarSettingsValid);
  SIMULATION_MODE=false;controllers[4].currentTemp=60;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  preferences.data.resize(1);loadSolarSettings();assert(!solarSettingsValid);
  server.args_["start"]="3";saveSolarSettings();assert(server.status==200 && solarSettingsValid && solarControl.on() && pin==LOW);
  storageReady=false;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  storageReady=true;now=10001;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  lastSolarMeasurement=now;serviceSolarPump();assert(solarControl.on() && pin==LOW);
  lastSolarMeasurement=UINT32_MAX-5;now=5;assert(solarSensorsFresh());
  now=10005;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  server.args_={{"action","power"},{"value","1"}};controlSolarPump();assert(server.status==409&&!solarControl.on());
  server.args_={{"action","manual"},{"value","1"}};controlSolarPump();assert(server.status==200&&solarControl.manual()&&!solarControl.on()&&pin==HIGH);
  server.args_={{"action","power"},{"value","1"}};controlSolarPump();assert(server.status==200&&solarControl.on()&&pin==LOW&&runtimeSolarOn);
  controllers[3].valid=false;controllers[4].valid=false;serviceSolarPump();assert(solarControl.on()&&pin==LOW);
  storageReady=false;solarSettingsValid=false;serviceSolarPump();assert(solarControl.on()&&pin==LOW);
  otaActive=true;serviceSolarPump();assert(!solarControl.manual()&&!solarControl.on()&&pin==HIGH&&!runtimeSolarOn);
  controlSolarPump();assert(server.status==409&&!solarControl.on());
  otaActive=false;storageReady=true;solarSettingsValid=true;
  SIMULATION_MODE=true;server.args_={{"action","manual"},{"value","1"}};controlSolarPump();assert(server.status==200&&solarControl.manual()&&!solarControl.on());
  server.args_={{"action","power"},{"value","1"}};before=writes;controlSolarPump();assert(server.status==200&&solarControl.on()&&pin==HIGH&&writes==before&&!runtimeSolarOn);
  server.args_={{"action","manual"},{"value","0"}};controlSolarPump();assert(server.status==200&&!solarControl.manual()&&!solarControl.on());
  SIMULATION_MODE=false;server.header_="evil";server.args_={{"action","manual"},{"value","1"}};controlSolarPump();assert(server.status==403&&!solarControl.manual());
  server.header_="HeatingUI";server.args_={{"action","wrong"},{"value","1"}};controlSolarPump();assert(server.status==400);
  server.args_={{"action","manual"},{"value","wrong"}};controlSolarPump();assert(server.status==400);
  std::puts("PASS: Solarrelais, Testmodus, Update, Speicherfehler und Wiederherstellung.");
}
'''
with tempfile.TemporaryDirectory(prefix='backload-solar-') as temp:
    source=Path(temp)/'test.cpp'; binary=Path(temp)/'test'
    source.write_text(harness)
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-I'+str(root/'Backload'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
