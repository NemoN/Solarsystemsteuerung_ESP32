"""Exercise the real firmware-check scheduler with fake Wi-Fi, NVS and clock."""
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
#include "FirmwareRelease.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
using String=std::string;
const char* FIRMWARE_VERSION="3.3.0";
struct Log {void println(const char*) {} template<class... T> void printf(const char*,T...) {}} FirmwareLog;
struct Storage {
  bool ready=true; uint32_t last=0; String latest;
  bool begin(const char*,bool) {return ready;}
  uint32_t getUInt(const char*,uint32_t) {return last;}
  String getString(const char*,const char*) {return latest;}
  size_t putUInt(const char*,uint32_t n) {last=n;return 4;}
  size_t putString(const char*,const char* s) {latest=s;return latest.size();}
} firmwareCheckPreferences;
struct Wifi {bool online=false;int status(){return online?1:0;}} WiFi;
constexpr int WL_CONNECTED=1,pdPASS=1;
bool firmwareCheckStorageReady=false,firmwareCheckStartFailed=false;
uint32_t firmwareCheckLastAttempt=0,firmwareCheckStartFailure=0,ms=0,epoch=0;
char firmwareLatestTag[32]={},firmwareCandidateTag[32]={};
enum FirmwareCheckState {FIRMWARE_CHECK_IDLE,FIRMWARE_CHECK_RUNNING,FIRMWARE_CHECK_READY,FIRMWARE_CHECK_FAILED};
std::atomic<int> firmwareCheckState(FIRMWARE_CHECK_IDLE);
constexpr int ARCHIVE_RUNNING=1;
std::atomic<int> archiveFetchState(0);
bool otaActive=false,otaVerificationPending=false,modeTransition=false;
uint32_t millis(){return ms;}
uint32_t fakeTime(void*){return epoch;}
#define time(x) fakeTime(x)
size_t strlcpy(char* dst,const char* src,size_t n){size_t len=strlen(src);if(n){size_t count=len<n-1?len:n-1;memcpy(dst,src,count);dst[count]=0;}return len;}
void firmwareCheckTask(void*) {}
bool taskStarts=true; int launches=0;
int xTaskCreate(void(*)(void*),const char*,int,void*,int,void*){++launches;return taskStarts?pdPASS:0;}
'''
harness += function('void loadFirmwareCheck()')
harness += function('void serviceFirmwareCheck()')
harness += r'''
int main() {
  loadFirmwareCheck();
  serviceFirmwareCheck();assert(!launches);
  WiFi.online=true;serviceFirmwareCheck();assert(!launches);
  epoch=1704067200;archiveFetchState=ARCHIVE_RUNNING;serviceFirmwareCheck();assert(!launches);
  archiveFetchState=0;otaActive=true;serviceFirmwareCheck();assert(!launches);
  otaActive=false;serviceFirmwareCheck();assert(launches==1 && firmwareCheckState==FIRMWARE_CHECK_RUNNING);
  assert(firmwareCheckPreferences.last==epoch);
  strcpy(firmwareCandidateTag,"v3.4.0");firmwareCheckState=FIRMWARE_CHECK_READY;
  serviceFirmwareCheck();assert(!strcmp(firmwareLatestTag,"v3.4.0") && firmwareCheckPreferences.latest=="v3.4.0");
  epoch+=86399;serviceFirmwareCheck();assert(launches==1);
  ++epoch;serviceFirmwareCheck();assert(launches==2);
  firmwareCheckState=FIRMWARE_CHECK_FAILED;serviceFirmwareCheck();
  assert(!strcmp(firmwareLatestTag,"v3.4.0") && firmwareCheckPreferences.last==epoch);
  firmwareLatestTag[0]=0;firmwareCheckLastAttempt=0;loadFirmwareCheck();
  assert(!strcmp(firmwareLatestTag,"v3.4.0") && firmwareCheckLastAttempt==epoch);
  serviceFirmwareCheck();assert(launches==2); // A reboot does not trigger another check.
  epoch+=86400;taskStarts=false;serviceFirmwareCheck();assert(launches==3 && firmwareCheckLastAttempt<epoch);
  ms=59999;serviceFirmwareCheck();assert(launches==3);
  ms=60000;taskStarts=true;serviceFirmwareCheck();assert(launches==4 && firmwareCheckPreferences.last==epoch);
}
'''
flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcjson'], text=True).split()
with tempfile.TemporaryDirectory(prefix='backload-firmware-scheduler-') as tmp:
    source, binary = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
    source.write_text(harness)
    subprocess.run([os.environ.get('CXX','c++'), '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root / 'Backload'), str(source), str(root / 'Backload/FirmwareRelease.cpp'),
                    *flags, '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: tägliche Updateprüfung, Neustarts, Netzwerkfehler und belegte Steuerung.')
