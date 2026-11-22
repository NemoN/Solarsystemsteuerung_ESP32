"""Testet die Sketch-Handler für Nachtruhe mit simuliertem Webserver und Flash."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
def function(signature):
    start = sketch.index(signature + ' {')
    return sketch[start:sketch.index('\n}\n', start) + 3]

harness = r'''
#include "NightQuiet.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using String = std::string;
bool otaActive=false, otaVerificationPending=false, modeTransition=false, storageReady=true;
bool nightQuietSettingsValid=true;
NightQuiet nightQuiet;
int updates=0;
void updateAutomaticPump(){++updates;}
String nightQuietStatusJson(){return "ok";}
struct Logger {void println(const char*){}} FirmwareLog;
struct Server {
 std::map<String,String> params; String headerValue="HeatingUI"; int status=0;
 int args(){return int(params.size());}
 bool hasArg(const char* key){return params.count(key);}
 String arg(const char* key){return params[key];}
 String header(const char*){return headerValue;}
 void send(int code,const char*,const char*){status=code;}
 void send(int code,const char*,const String&){status=code;}
 void sendHeader(const char*,const char*){}
} server;
struct Preferences {
 std::vector<uint8_t> bytes; bool fail=false;
 bool isKey(const char*){return !bytes.empty();}
 size_t getBytesLength(const char*){return bytes.size();}
 size_t getBytes(const char*,void* data,size_t len){if(bytes.size()!=len)return 0;memcpy(data,bytes.data(),len);return len;}
 size_t putBytes(const char*,const void* data,size_t len){if(fail)return 0; bytes.assign((const uint8_t*)data,(const uint8_t*)data+len);return len;}
} preferences;
'''
harness += '\n'.join(function(sig) for sig in ['bool parseNightQuietTime(const String& value, uint16_t& minutes)', 'void saveNightQuiet()', 'void loadNightQuiet()'])
harness += r'''
int main(){
 server.params={{"enabled","1"},{"start","23:15"},{"end","06:30"},{"days","1000001"}};
 saveNightQuiet();assert(server.status==200 && updates==1 && nightQuiet.enabled && nightQuiet.start==1395 && nightQuiet.end==390 && nightQuiet.days==65);
 nightQuiet=NightQuiet();loadNightQuiet();assert(nightQuietSettingsValid && nightQuiet.enabled && nightQuiet.start==1395 && nightQuiet.days==65);
 server.params["start"]="24:00";saveNightQuiet();assert(server.status==400 && nightQuiet.start==1395);
 server.params["start"]="06:30";saveNightQuiet();assert(server.status==400);
 server.params["start"]="23:15";server.params["days"]="100x001";saveNightQuiet();assert(server.status==400);
 server.params["days"]="1111111";preferences.fail=true;saveNightQuiet();assert(server.status==500 && nightQuiet.days==65);
 preferences.fail=false;preferences.bytes[0]=99;loadNightQuiet();assert(!nightQuietSettingsValid);
 server.headerValue="bad";saveNightQuiet();assert(server.status==403 && !nightQuietSettingsValid);
 server.headerValue="HeatingUI";saveNightQuiet();assert(server.status==200 && nightQuietSettingsValid && nightQuiet.days==127);
 puts("PASS: Nachtruhe speichern, laden, validieren und Speicherfehler behandeln.");
}
'''
with tempfile.TemporaryDirectory(prefix='night-quiet-') as tmp:
    source = Path(tmp)/'test.cpp'
    binary = Path(tmp)/'test'
    source.write_text(harness)
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++11','-Wall','-Wextra','-Werror','-I'+str(root/'Backload'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
