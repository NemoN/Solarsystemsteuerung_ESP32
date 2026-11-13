from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
main=(root/'Backload/Backload.ino').read_text()
relay=(root/'Relaiskarte/Relaiskarte.ino').read_text()
proxy=(root/'Backload/WebSerialMonitor.cpp').read_text()
def function(source,signature):
    start=source.index(signature+' {')
    return source[start:source.index('\n}\n',start)+3]
assert 'server.on("/api/reboot", HTTP_POST, rebootController)' in main
assert 'dashboard.server().on("/api/reboot", HTTP_POST, proxyRequest)' in proxy
assert 'server.on("/api/remote-relay/reboot", HTTP_POST, rebootRemoteRelay)' in main
assert 'dashboard.server().on("/api/remote-relay/reboot", HTTP_POST, proxyRequest)' in proxy
assert 'server.on("/api/reboot", HTTP_POST, rebootRelayBoard)' in relay
assert 'if (modeTransition && now - modeRestartAt >= 2500) { savePumpRuntime(true); ESP.restart(); }' in main
assert 'if (rebootPending) { if (now - rebootAt >= 500) ESP.restart(); delay(1); return; }' in relay
harness=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
uint32_t now=42;uint32_t millis(){return now;}
struct Server {std::string h="HeatingUI",body;unsigned n=0;int status=0;
 std::string header(const char*){return h;} unsigned args(){return n;}
 void send(int code,const char*,const char* text){status=code;body=text;}
 void sendHeader(const char*,const char*){}
} server;
bool otaActive=false,otaVerificationPending=false,modeTransition=false,busy=false;
unsigned long modeRestartAt=0;
bool remoteRelayAnyBusy(){return busy;}
struct Active {bool value=false;bool active(){return value;}} relayProvisioning,relayOta;
struct RemoteRelayUpdate {
 enum class WatchdogAction {None,CancelRequested,RestartRequired};
 bool value=false;WatchdogAction action=WatchdogAction::None;
 bool active(){return value;} WatchdogAction watchdog(){return action;}
} remoteRelayUpdate;
bool manualControl=true,manualPumpOn=true,physicalPump=true,physicalSolar=true,oilRelease=true;
struct Automatic {bool resetCalled=false;void reset(){resetCalled=true;}} automaticControl;
struct Pump {bool on=true;int direction(){return 1;}void command(bool next,int,uint32_t){on=next;}} pump;
void servicePump(){assert(modeTransition&&!pump.on);physicalPump=physicalSolar=oilRelease=false;}
struct Log {void println(const char*){}} FirmwareLog;
''' + function(main,'void rebootController()') + r'''
bool auth=true,bootVerificationPending=false,factoryResetPending=false,rebootPending=false,pairConnecting=false,reconnect=false;
uint32_t rebootAt=0;
bool authorized(){return auth;}
''' + function(relay,'void rebootRelayBoard()') + r'''
int main(){
 server.h="evil";rebootController();assert(server.status==403&&!modeTransition&&physicalPump);
 server.h="HeatingUI";server.n=1;rebootController();assert(server.status==400&&!modeTransition);
 server.n=0;otaActive=true;rebootController();assert(server.status==409&&!modeTransition);otaActive=false;
 otaVerificationPending=true;rebootController();assert(server.status==409&&!modeTransition);otaVerificationPending=false;
 busy=true;rebootController();assert(server.status==409&&!modeTransition);busy=false;
 remoteRelayUpdate.value=true;rebootController();assert(server.status==409&&!modeTransition&&server.body.find("Update der Relaiskarte")!=std::string::npos);remoteRelayUpdate.value=false;
 relayProvisioning.value=true;rebootController();assert(server.status==409&&!modeTransition);relayProvisioning.value=false;
 // Nach festgestelltem Hänger bleibt ein ausdrücklicher manueller Neustart möglich.
 remoteRelayUpdate.value=true;remoteRelayUpdate.action=RemoteRelayUpdate::WatchdogAction::RestartRequired;
 rebootController();assert(server.status==202&&modeTransition&&!manualControl&&!manualPumpOn&&automaticControl.resetCalled);
 assert(!physicalPump&&!physicalSolar&&!oilRelease&&modeRestartAt==42);
 rebootController();assert(server.status==409);
 auth=false;rebootRelayBoard();assert(server.status==403&&!rebootPending);
 auth=true;server.n=1;rebootRelayBoard();assert(server.status==400&&!rebootPending);server.n=0;
 relayOta.value=true;rebootRelayBoard();assert(server.status==409&&!rebootPending);relayOta.value=false;
 bootVerificationPending=true;rebootRelayBoard();assert(server.status==409&&!rebootPending);bootVerificationPending=false;
 factoryResetPending=true;rebootRelayBoard();assert(server.status==409&&!rebootPending);factoryResetPending=false;
 pairConnecting=true;rebootRelayBoard();assert(server.status==409&&!rebootPending);pairConnecting=false;
 reconnect=true;rebootRelayBoard();assert(server.status==409&&!rebootPending);reconnect=false;
 rebootRelayBoard();assert(server.status==200&&server.body=="REBOOT1:OK"&&rebootPending&&rebootAt==42);
 rebootRelayBoard();assert(server.status==409);
 puts("PASS: Authentifizierter Neustart, Schutzsperren und sicheres Abschalten.");
}
'''
with tempfile.TemporaryDirectory(prefix='backload-reboot-') as temp:
    cpp=Path(temp)/'test.cpp';binary=Path(temp)/'test';cpp.write_text(harness)
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
