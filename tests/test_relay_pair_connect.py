from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).parents[1] / 'Relaiskarte/Relaiskarte.ino').read_text()
function = source[source.index('void searchPairingAp(uint32_t now) {'):source.index('\nvoid updatePairButton(', source.index('void searchPairingAp(uint32_t now) {'))]
harness = r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
struct Config { char ssid[33] = {}; } config;
bool pairWindow=false,pairConnecting=false,connecting=false;
uint32_t lastPairScan=0,connectAt=0;
struct { uint64_t getEfuseMac(){return 0x123456785020ULL;} } ESP;
struct { int count=0; char ssid[32]={},pass[32]={}; void begin(const char* s,const char* p){count++;std::snprintf(ssid,sizeof(ssid),"%s",s);std::snprintf(pass,sizeof(pass),"%s",p);} } WiFi;
struct {void println(const char*){}} Serial;
''' + function + r'''
int main(){
 searchPairingAp(8000);assert(WiFi.count==0);
 pairWindow=true;searchPairingAp(8000);assert(WiFi.count==1&&pairConnecting&&connecting&&connectAt==8000);
 assert(!std::strcmp(WiFi.ssid,"Backload-Pair-5020")&&!std::strcmp(WiFi.pass,"relaypair5020"));
 searchPairingAp(20000);assert(WiFi.count==1);
 pairConnecting=connecting=false;searchPairingAp(15000);assert(WiFi.count==1);
 searchPairingAp(16000);assert(WiFi.count==2);
 config.ssid[0]='x';pairConnecting=connecting=false;searchPairingAp(24000);assert(WiFi.count==2);
 puts("PASS: Direkte Kopplungsverbindung und Wiederholung ohne separaten WLAN-Scan.");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'connect.cpp'
    binary = Path(tmp) / 'connect'
    cpp.write_text(harness)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
