"""Prüft die echte HTTP-Speicherfunktion mit simuliertem Webserver und NVS."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
start = sketch.index('void saveAutomaticSettings() {')
function = sketch[start:sketch.index('\n}\n', start) + 3]
harness = r'''
#include "AutomaticControl.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
using std::isfinite;
bool otaActive=false,otaVerificationPending=false,modeTransition=false,storageReady=true,autoSettingsValid=true;
AutomaticThresholds automaticThresholds={8,35,35};
unsigned long lastMeasurement=0, now=100; int measurements=0;
unsigned long millis(){return now;}
void measureTemperatures(){++measurements;}
std::string automaticStatusJson(){return "ok";}
bool parseNumber(const std::string& s,float& out){char* end=nullptr;out=strtof(s.c_str(),&end);return !s.empty()&&end==s.c_str()+s.size()&&isfinite(out);}
struct Server {
 std::map<std::string,std::string> params; std::string headerValue="HeatingUI",body; int status=0;
 int args(){return int(params.size());}
 bool hasArg(const char* key){return params.count(key)!=0;}
 std::string arg(const char* key){return params[key];}
 std::string header(const char*){return headerValue;}
 void send(int code,const char*,const char* text){status=code;body=text;}
 void send(int code,const char*,const std::string& text){status=code;body=text;}
 void sendHeader(const char*,const char*){}
} server;
struct Preferences {
 std::vector<unsigned char> bytes; bool fail=false;
 size_t putBytes(const char*,const void* data,size_t length){if(fail)return 0;bytes.assign((const unsigned char*)data,(const unsigned char*)data+length);return length;}
} preferences;
'''
harness += function
harness += r'''
int main(){
 server.params={{"start","9"},{"minimumCombi","40"},{"minimumBuffer","45"}}; saveAutomaticSettings();
 assert(server.status==200 && measurements==1 && automaticThresholds.differenceStart==9 && automaticThresholds.minimumCombi==40 && automaticThresholds.minimumBuffer==45);
 assert(preferences.bytes.size()==5*sizeof(float));
 float saved[5]; memcpy(saved,preferences.bytes.data(),sizeof saved); AutomaticThresholds reloaded;
 assert(AutomaticControl::readSavedThresholds(saved,reloaded));
 assert(reloaded.differenceStart==9 && reloaded.minimumCombi==40 && reloaded.minimumBuffer==45);
 // Ältere Clients dürfen die Startdifferenz ändern, ohne die Grenzen zu löschen.
 server.params={{"start","8"}}; saveAutomaticSettings();
 assert(server.status==200 && automaticThresholds.differenceStart==8 && automaticThresholds.minimumCombi==40 && automaticThresholds.minimumBuffer==45);
 server.params={{"start","9"},{"minimumCombi","40.5"},{"minimumBuffer","45"}};saveAutomaticSettings();
 assert(server.status==400 && automaticThresholds.minimumCombi==40);
 server.params={{"start","9"},{"minimumCombi","40"},{"minimumBuffer","81"}};saveAutomaticSettings();
 assert(server.status==400 && automaticThresholds.minimumBuffer==45);
 server.params={{"start","9"},{"other","20"}};saveAutomaticSettings();assert(server.status==400);
 server.params={{"minimumCombi","20"}};saveAutomaticSettings();assert(server.status==400);
 server.params={{"start","9"},{"minimumCombi","45"},{"minimumBuffer","50"}};preferences.fail=true;saveAutomaticSettings();
 assert(server.status==500 && automaticThresholds.minimumCombi==40 && automaticThresholds.minimumBuffer==45);
 preferences.fail=false;server.headerValue="bad";saveAutomaticSettings();assert(server.status==403);
 puts("PASS: Automatik-Einstellungen, atomare NVS-Speicherung, Migration, Fehler und alte Clients.");
}
'''
with tempfile.TemporaryDirectory(prefix='backload-auto-settings-') as tmp:
 source=Path(tmp)/'test.cpp'; binary=Path(tmp)/'test'; source.write_text(harness)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++11','-Wall','-Wextra','-Werror','-I'+str(root/'Backload'),str(source),str(root/'Backload/AutomaticControl.cpp'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
