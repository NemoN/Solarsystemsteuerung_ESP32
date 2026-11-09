"""Prüft Speichern, Laden und Fehler der echten HTTP-Handler mit simuliertem NVS."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
start = sketch.index('void loadHostname() {')
end = sketch.index('void saveHostname() {', start)
harness = r'''
#include "HistoryServerUrl.h"
#include "Hostname.h"
#include <cassert>
#include <map>
#include <string>
struct String : std::string {
 using std::string::string;
 String(const std::string& s):std::string(s){}
 void trim(){const auto a=find_first_not_of(" \t\r\n");if(a==npos){clear();return;}const auto b=find_last_not_of(" \t\r\n");assign(substr(a,b-a+1));}
};
bool hostnameStorageReady=false,hostnameUsesDefault=true,otaActive=false,otaVerificationPending=false,modeTransition=false;
String configuredHostname,activeHostname,historyServerUrl;
const char* defaultHostname(){return "esp32-heizung";}
String jsonString(const String& s){return String("\"")+s+"\"";}
struct Log {void println(const char*){}} FirmwareLog;
struct Storage {
 std::map<std::string,String> values; bool fail=false;
 bool begin(const char*,bool){return true;}
 bool isKey(const char* k){return values.count(k);}
 String getString(const char* k,const char* fallback){return isKey(k)?values[k]:String(fallback);}
 size_t putString(const char* k,const String& v){if(fail)return 0;values[k]=v;return v.length();}
 bool remove(const char* k){if(fail)return false;values.erase(k);return true;}
} hostnamePreferences;
struct Server {
 std::map<std::string,String> params; String headerValue="HeatingUI",body; int status=0;
 int args(){return int(params.size());}
 bool hasArg(const char* k){return params.count(k);}
 String arg(const char* k){return params[k];}
 String header(const char*){return headerValue;}
 void send(int code,const char*,const String& text){status=code;body=text;}
 void sendHeader(const char*,const char*){}
} server;
'''
harness += sketch[start:end]
harness += r'''
int main(){
 loadHostname();assert(hostnameStorageReady && historyServerUrl.empty());
 server.params={{"url"," http://history.local:8088/ "}};saveHistoryServerUrl();
 assert(server.status==200 && historyServerUrl=="http://history.local:8088/");
 assert(hostnamePreferences.getString("history-url","")==historyServerUrl);
 historyServerUrl="";loadHostname();assert(historyServerUrl=="http://history.local:8088/");
 server.params={{"url","javascript:alert(1)"}};saveHistoryServerUrl();assert(server.status==400 && historyServerUrl=="http://history.local:8088/");
 server.params={{"url","http://new.local/"}};hostnamePreferences.fail=true;saveHistoryServerUrl();assert(server.status==500 && historyServerUrl=="http://history.local:8088/");
 server.params={{"url",""}};saveHistoryServerUrl();assert(server.status==500 && !historyServerUrl.empty());
 hostnamePreferences.fail=false;server.headerValue="wrong";saveHistoryServerUrl();assert(server.status==403);
 server.headerValue="HeatingUI";saveHistoryServerUrl();assert(server.status==200 && historyServerUrl.empty() && !hostnamePreferences.isKey("history-url"));
 historyServerUrl="http://old/";loadHostname();assert(historyServerUrl.empty());
}
'''
with tempfile.TemporaryDirectory(prefix='backload-history-settings-') as directory:
    source = Path(directory) / 'test.cpp'
    binary = Path(directory) / 'test'
    source.write_text(harness)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I' + str(root / 'Backload'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('History-Server-Einstellung: Speichern, Neustart, Löschen und Fehler OK')
