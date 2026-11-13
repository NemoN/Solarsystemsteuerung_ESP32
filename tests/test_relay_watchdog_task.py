"""Exercise the actual card watchdog task using a deterministic host clock and NVS double."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Relaiskarte/Relaiskarte.ino').read_text()
start = sketch.index('bool saveWatchdogCount(')
end = sketch.index('// Neue OTA-Firmware', start)
harness = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "RelayWatchdog.h"
std::atomic<uint32_t> loopHeartbeat{0};
std::atomic<uint8_t> watchdogCount{0},watchdogState{3};
bool watchdogStorageReady=true, failWrite=false, failRead=false, recoverDuringWrite=false;
uint32_t now=0; unsigned ticks=0; bool restarted=false;
uint32_t millis(){return now;}
#define pdMS_TO_TICKS(n) (n)
void vTaskDelay(uint32_t n){if(++ticks>65)throw 1;now+=n;}
struct Store {uint8_t value=0; size_t putUChar(const char*,uint8_t v){if(failWrite)return 0;value=v;if(recoverDuringWrite){loopHeartbeat.store(now);recoverDuringWrite=false;}return 1;} uint8_t getUChar(const char*,uint8_t fallback){return failRead?fallback:value;}} watchdogPrefs;
struct Logger {void println(const char*){}} Serial;
struct Esp {void restart(){restarted=true;throw 2;}} ESP;
''' + sketch[start:end] + r'''
void init(uint8_t count,bool ready){now=ticks=0;loopHeartbeat.store(0);watchdogCount.store(count);watchdogState.store(3);watchdogStorageReady=ready;watchdogPrefs.value=count;restarted=failWrite=failRead=recoverDuringWrite=false;}
void run(){try{watchdogTask(nullptr);}catch(int){}}
int main(){
 init(0,true);run();assert(restarted&&watchdogPrefs.value==1&&watchdogCount.load()==1);
 init(2,true);run();assert(!restarted&&watchdogPrefs.value==2&&watchdogState.load()==1);
 init(0,false);run();assert(!restarted&&watchdogState.load()==2);
 init(0,true);failWrite=true;run();assert(!restarted&&watchdogState.load()==2);
 init(0,true);failRead=true;run();assert(!restarted&&watchdogState.load()==2);
 init(0,true);recoverDuringWrite=true;run();assert(!restarted&&watchdogPrefs.value==0&&watchdogCount.load()==0);
 std::puts("PASS: Echter Überwachungstask: Neustart, Begrenzung, Speicherfehler und Erholung während des Speicherns.");
}
'''
with tempfile.TemporaryDirectory(prefix='relay-watchdog-task-') as directory:
    source = Path(directory) / 'test.cpp'
    binary = Path(directory) / 'test'
    source.write_text(harness)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'Relaiskarte'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
