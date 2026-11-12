"""HTTP-Handler der Freigabebezeichnung mit simuliertem NVS prüfen."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
start = sketch.index('void saveOilBurnerLabel() {')
end = sketch.index('\n}\n', start) + 3
harness = r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
struct String : std::string {
 using std::string::string;
 String(const std::string& s):std::string(s){}
 void trim(){auto a=find_first_not_of(" \t\r\n");if(a==npos){clear();return;}auto b=find_last_not_of(" \t\r\n");assign(substr(a,b-a+1));}
};
bool storageReady=true;String oilBurnerLabel="Ölbrenner";
String heatingStatusJson(){return oilBurnerLabel;}
struct Storage {
 std::map<std::string,String> values;bool fail=false;
 size_t putString(const char* k,const String& v){if(fail)return 0;values[k]=v;return v.length();}
 String getString(const char* k,const char* def){return values.count(k)?values[k]:String(def);}
} preferences;
struct Server {
 std::map<std::string,String> params;String hdr="HeatingUI",body;int status=0;
 String header(const char*){return hdr;}int args(){return int(params.size());}
 bool hasArg(const char* k){return params.count(k);}String arg(const char* k){return params[k];}
 void send(int code,const char*,const String& value){status=code;body=value;}
 void sendHeader(const char*,const char*){}
} server;
''' + sketch[start:end] + r'''
int main(){
 server.params={{"label","  Heizung  "}};saveOilBurnerLabel();
 assert(server.status==200 && oilBurnerLabel=="Heizung" && preferences.getString("oil-label","")=="Heizung");
 server.params={{"label",""}};saveOilBurnerLabel();assert(server.status==400 && oilBurnerLabel=="Heizung");
 server.params={{"label",std::string(41,'x')}};saveOilBurnerLabel();assert(server.status==400);
 server.params={{"label","Name\nTest"}};saveOilBurnerLabel();assert(server.status==400);
 server.params={{"label","Neuer Name"}};preferences.fail=true;saveOilBurnerLabel();assert(server.status==500 && oilBurnerLabel=="Heizung");
 preferences.fail=false;server.hdr="wrong";saveOilBurnerLabel();assert(server.status==403 && oilBurnerLabel=="Heizung");
}
'''
with tempfile.TemporaryDirectory(prefix='backload-label-') as tmp:
    source = Path(tmp) / 'test.cpp'
    binary = Path(tmp) / 'test'
    source.write_text(harness)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++11', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: Ölbrennerbezeichnung, Eingabevalidierung und Speicherfehler.')
