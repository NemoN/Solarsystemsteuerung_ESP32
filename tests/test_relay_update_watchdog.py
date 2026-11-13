"""Prüft den echten Neustartpfad des Relais-Update-Watchdogs ohne Hardware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
start = sketch.index('void serviceRelayUpdateWatchdog() {')
end = sketch.index('\n}\n', start) + 3
harness = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
uint32_t now=0,modeRestartAt=0;
uint32_t millis(){return now;}
constexpr uint8_t RELAY_WATCHDOG_MAX_RESTARTS=2;
constexpr uint32_t RELAY_WATCHDOG_STABLE_MS=30UL*60*1000;
bool relayWatchdogStorageReady=true,relayWatchdogDeniedLogged=false;
bool relayWatchdogIdleTiming=false;
uint32_t relayWatchdogIdleSince=0;
uint8_t relayWatchdogRestarts=0;
bool modeTransition=false,otaActive=false,otaVerificationPending=false;
bool manualControl=true,manualPumpOn=true,physicalPump=true;
struct RemoteRelayUpdate {
 enum class WatchdogAction:uint8_t {None,CancelRequested,RestartRequired};
 WatchdogAction action=WatchdogAction::None; bool idle=false;
 bool blockedMessage=false;
 WatchdogAction watchdog(){return action;} bool idleForWatchdogRecovery(){return idle;}
 void watchdogRestartBlocked(const char*){blockedMessage=true;}
} remoteRelayUpdate;
struct Preferences {
 uint8_t stored=0; bool failWrite=false,failRead=false;
 size_t putUChar(const char*,uint8_t value){if(failWrite)return 0;stored=value;return 1;}
 uint8_t getUChar(const char*,uint8_t fallback){return failRead?fallback:stored;}
} relayWatchdogPreferences;
struct Automatic {bool resetCalled=false;void reset(){resetCalled=true;}} automaticControl;
struct Pump {bool on=true;int direction(){return 0;}void command(bool x,int,uint32_t){on=x;}} pump;
void servicePump(){assert(modeTransition&&!pump.on);physicalPump=false;}
struct Log {void println(const char*){} template<typename... T> void printf(const char*,T...) {}} FirmwareLog;
''' + sketch[start:end] + r'''
int main(){
 using Action=RemoteRelayUpdate::WatchdogAction;
 remoteRelayUpdate.action=Action::CancelRequested;serviceRelayUpdateWatchdog();
 assert(!modeTransition&&relayWatchdogRestarts==0&&physicalPump);
 remoteRelayUpdate.action=Action::RestartRequired;otaActive=true;serviceRelayUpdateWatchdog();
 assert(!modeTransition&&relayWatchdogRestarts==0);otaActive=false;
 relayWatchdogPreferences.failWrite=true;serviceRelayUpdateWatchdog();
 assert(!modeTransition&&!relayWatchdogStorageReady&&relayWatchdogRestarts==0);
 relayWatchdogStorageReady=true;relayWatchdogPreferences.failWrite=false;
 now=123;serviceRelayUpdateWatchdog();
 assert(modeTransition&&!physicalPump&&!manualControl&&!manualPumpOn&&automaticControl.resetCalled);
 assert(relayWatchdogRestarts==1&&relayWatchdogPreferences.stored==1&&modeRestartAt==123);
 // Eine wiederholt blockierte Aufgabe kann höchstens zwei automatische Neustarts auslösen.
 modeTransition=false;manualControl=manualPumpOn=physicalPump=true;pump.on=true;
 serviceRelayUpdateWatchdog();assert(modeTransition&&relayWatchdogRestarts==2);
 modeTransition=false;manualControl=manualPumpOn=physicalPump=true;pump.on=true;
 serviceRelayUpdateWatchdog();assert(!modeTransition&&physicalPump&&relayWatchdogRestarts==2&&relayWatchdogDeniedLogged);
 assert(remoteRelayUpdate.blockedMessage);
 // Keine Freigabe bei aktivem Update, erst nach 30 Minuten stabiler Laufzeit.
 remoteRelayUpdate.action=Action::None;now=RELAY_WATCHDOG_STABLE_MS;remoteRelayUpdate.idle=false;
 serviceRelayUpdateWatchdog();assert(relayWatchdogRestarts==2);
 remoteRelayUpdate.idle=true;serviceRelayUpdateWatchdog();assert(relayWatchdogRestarts==2);
 now+=RELAY_WATCHDOG_STABLE_MS-1;serviceRelayUpdateWatchdog();assert(relayWatchdogRestarts==2);
 now+=1;serviceRelayUpdateWatchdog();
 assert(relayWatchdogRestarts==0&&relayWatchdogPreferences.stored==0&&!relayWatchdogDeniedLogged);
 puts("PASS: Watchdog-Abbruch, Abschaltung, persistente Neustartbegrenzung und stabile Erholung.");
}
'''
with tempfile.TemporaryDirectory(prefix='relay-update-watchdog-') as directory:
    source = Path(directory) / 'test.cpp'
    binary = Path(directory) / 'test'
    source.write_text(harness)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
