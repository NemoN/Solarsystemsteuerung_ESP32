from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).parents[1] / 'Relaiskarte/Relaiskarte.ino').read_text()
start = source.index('void updatePairButton(uint32_t now) {')
end = source.index('\nvoid setup()', start)
function = source[start:end]
harness = r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#define HIGH 1
int button=0;
int digitalRead(int) { return button; }
struct Config { char ssid[33] = {}; } config;
bool storeReady=true,pairButtonReady=false,pairButtonHeld=false,pairWindow=false;
bool pairConnecting=false,connecting=false;
uint32_t pairButtonAt=0,pairWindowAt=0,lastPairScan=0;
struct { int deleted=0,disconnected=0; void scanDelete(){deleted++;} void disconnect(bool,bool){disconnected++;} } WiFi;
struct { void println(const char*){} } Serial;
''' + function + r'''
int main() {
 updatePairButton(10); updatePairButton(2000); assert(!pairWindow); // Boot with button held.
 button=1; updatePairButton(2001); button=0; updatePairButton(2010);
 updatePairButton(3009); assert(!pairWindow); updatePairButton(3010); assert(pairWindow);
 updatePairButton(5000); assert(pairWindowAt==3010); // A long press does not restart the window.
 pairConnecting=connecting=true;button=1;updatePairButton(123010);
 assert(!pairWindow&&!pairConnecting&&!connecting&&WiFi.disconnected==1);
 config.ssid[0]='x';button=0;updatePairButton(124000);updatePairButton(126000);assert(!pairWindow);
 puts("PASS: IO0-Freigabe, Entprellung, Zeitablauf und Abbruch.");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'button.cpp'
    binary = Path(tmp) / 'button'
    cpp.write_text(harness)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
