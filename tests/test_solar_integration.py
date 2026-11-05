"""Echte Sketch-Funktionen mit simuliertem GPIO, Webserver und NVS prüfen."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
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
constexpr int HIGH=1,LOW=0,SOLAR_PUMP_PIN=9;
bool storageReady=true,solarSettingsValid=true,modeTransition=false,otaActive=false,otaVerificationPending=false,SIMULATION_MODE=false;
float solarStartDifference=SolarControl::DEFAULT_START;
uint32_t now=0,lastSolarMeasurement=0;bool solarMeasurementReady=true;
constexpr uint32_t SOLAR_MAX_MEASUREMENT_AGE=10000;
uint32_t millis(){return now;}
SolarControl solarControl;
struct TemperatureController {bool valid;float currentTemp;};
TemperatureController controllers[5]={{},{},{},{true,40},{true,45}};
int pin=HIGH,writes=0;
void digitalWrite(int gpio,int value){assert(gpio==9);pin=value;++writes;}
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
harness += '\n'.join(function(sig) for sig in ('bool solarSensorsFresh()', 'void serviceSolarPump()', 'void loadSolarSettings()', 'void saveSolarSettings()'))
harness += r'''
int main(){
  serviceSolarPump();assert(solarControl.on() && pin==LOW);
  controllers[4].currentTemp=42;serviceSolarPump();assert(solarControl.on() && pin==LOW);
  controllers[3].valid=false;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  controllers[3].valid=true;controllers[4].currentTemp=45;serviceSolarPump();assert(solarControl.on());
  otaActive=true;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  otaActive=false;serviceSolarPump();assert(solarControl.on() && pin==LOW);
  otaVerificationPending=true;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  otaVerificationPending=false;modeTransition=true;serviceSolarPump();assert(!solarControl.on() && pin==HIGH);
  modeTransition=false;SIMULATION_MODE=true;pin=HIGH;int before=writes;serviceSolarPump();
  assert(solarControl.on() && pin==HIGH && writes==before);
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
  std::puts("PASS: Solarrelais, Testmodus, Update, Speicherfehler und Wiederherstellung.");
}
'''
with tempfile.TemporaryDirectory(prefix='backload-solar-') as temp:
    source=Path(temp)/'test.cpp'; binary=Path(temp)/'test'
    source.write_text(harness)
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-I'+str(root/'Backload'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
