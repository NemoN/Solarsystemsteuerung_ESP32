"""Exercise the real scheduler with a fake clock, Wi-Fi and task launcher."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Backload/Backload.ino').read_text()
start = source.index('void serviceArchiveHistory() {')
end = source.index('\n}\n', start) + 3
harness = r'''
#include "ArchiveHistory.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <string>
#define min(a,b) ((a)<(b)?(a):(b))
using String = std::string;
struct Log { void println(const char*) {} } FirmwareLog;
struct Wifi { bool connected=false; int status() {return connected ? 1 : 0;} } WiFi;
constexpr int WL_CONNECTED=1, pdPASS=1;
bool SIMULATION_MODE=false, otaActive=false, otaVerificationPending=false, modeTransition=false;
String historyServerUrl;
enum ArchiveFetchState { ARCHIVE_IDLE, ARCHIVE_RUNNING, ARCHIVE_READY, ARCHIVE_FAILED };
std::atomic<int> archiveFetchState(ARCHIVE_IDLE);
ArchiveHistory archiveHistory, archiveCandidate;
char archiveFetchUrl[256] = {};
uint32_t archiveFetchTime=0, archiveGeneration=0, archiveFetchGeneration=0;
uint32_t archiveLastAttempt=0, archiveRetryDelay=0, ms=0, epoch=0;
bool archiveLoaded=false;
uint32_t millis() {return ms;}
uint32_t fakeTime(void*) {return epoch;}
#define time(x) fakeTime(x)
size_t strlcpy(char* dst, const char* src, size_t n) {size_t len=strlen(src);if(n){size_t count=len<n-1?len:n-1;memcpy(dst,src,count);dst[count]=0;}return len;}
void archiveFetchTask(void*) {}
int launches=0;
int xTaskCreate(void(*)(void*), const char*, int, void*, int, void*) {++launches;return pdPASS;}
'''
harness += source[start:end]
harness += r'''
int main() {
  historyServerUrl="http://archive.local";
  serviceArchiveHistory();assert(launches==0);
  WiFi.connected=true;serviceArchiveHistory();assert(launches==0); // No valid clock.
  epoch=1704067200;SIMULATION_MODE=true;serviceArchiveHistory();assert(launches==0);
  SIMULATION_MODE=false;otaActive=true;serviceArchiveHistory();assert(launches==0);
  otaActive=false;serviceArchiveHistory();assert(launches==1 && archiveFetchState==ARCHIVE_RUNNING);
  serviceArchiveHistory();assert(launches==1);
  archiveFetchState=ARCHIVE_FAILED;serviceArchiveHistory();assert(archiveRetryDelay==15000 && launches==1);
  ms=14999;serviceArchiveHistory();assert(launches==1);
  ms=15000;serviceArchiveHistory();assert(launches==2);
  archiveFetchState=ARCHIVE_FAILED;serviceArchiveHistory();assert(archiveRetryDelay==30000);
  ms=45000;serviceArchiveHistory();assert(launches==3);
  ++archiveGeneration;archiveRetryDelay=0;archiveFetchState=ARCHIVE_READY;
  serviceArchiveHistory();assert(!archiveLoaded && launches==4); // Ignore obsolete URL response.
  archiveCandidate.interval=900;archiveFetchState=ARCHIVE_READY;serviceArchiveHistory();
  assert(archiveLoaded && archiveHistory.interval==900 && launches==4);
}
'''
with tempfile.TemporaryDirectory(prefix='backload-archive-scheduler-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test'
    cpp.write_text(harness)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I' + str(root / 'Backload'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: Archivabruf: WLAN, Uhrzeit, Testmodus, OTA, Wiederholung und geänderte Adresse.')
