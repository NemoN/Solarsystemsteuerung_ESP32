#include "../Backload/RemoteRelay.h"
#include <WiFi.h>
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
uint32_t mockMillis=2000;
ESPClass ESP; WiFiClass WiFi;
TaskFunction_t mockTask=nullptr; void* mockTaskArg=nullptr;
std::vector<uint8_t> mockGithubImage; size_t mockGithubOffset=0,mockGithubStopAfter=size_t(-1);
int mockGithubAttempts=0,mockGithubFailures=0,mockGithubCode=200; long mockGithubReportedSize=-1,mockRelayReportedSize=-1;
std::map<std::string,std::vector<unsigned char>> mockPreferences;
int mockPreferenceFailures=0;
struct EndIteration{};
void mockAdvance(uint32_t n){mockMillis+=n;if(n!=10)throw EndIteration{};}
int resetCode=200,resetCalls=0,versionCode=200,versionCalls=0,stateCode=200,commandCalls=0;
int rebootCode=200,rebootCalls=0;
bool incompleteBody=false;
int mockHttp(const String& url,const String* body,String& response){
 if(std::string(url.c_str()).find("/api/reboot")!=std::string::npos){assert(body);++rebootCalls;if(rebootCode==200)response="REBOOT1:OK";return rebootCode;}
 if(std::string(url.c_str()).find("/api/factory-reset")!=std::string::npos){assert(body);++resetCalls;if(resetCode==200)response="RESET1:OK";return resetCode;}
 if(std::string(url.c_str()).find("/api/version")!=std::string::npos){++versionCalls;if(versionCode==200)response="RLYVER1:2.7.0";return versionCode;}
 if(body)++commandCalls;
 if(stateCode==200)response=incompleteBody?"RLY1:":"RLY1:0";
 return stateCode;
}
void oneIteration(){try{mockTask(mockTaskArg);assert(false);}catch(const EndIteration&) {}}
int main(){
 RemoteRelay card;assert(card.begin("config1"));String labels[]={"A","B","C","D"};
 assert(!card.reboot());assert(card.configure("192.168.1.10","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",labels));
 assert(!card.reboot());oneIteration();assert(card.snapshot().online);
 assert(card.reboot());assert(!card.reboot());assert(card.snapshot().busy);assert(!card.command(1,true));
 oneIteration();auto sent=card.snapshot();assert(sent.rebootState==RemoteRelay::REBOOT_SENT&&!sent.online&&!sent.busy&&rebootCalls==1);
 oneIteration();assert(card.snapshot().online&&card.snapshot().rebootState==RemoteRelay::REBOOT_ONLINE&&rebootCalls==1);
 assert(card.reboot());rebootCode=404;oneIteration();assert(card.snapshot().rebootState==RemoteRelay::REBOOT_UNSUPPORTED&&card.snapshot().online&&rebootCalls==2);
 assert(card.reboot());rebootCode=-1;oneIteration();assert(card.snapshot().rebootState==RemoteRelay::REBOOT_UNCERTAIN&&!card.snapshot().online&&rebootCalls==3);
 oneIteration();assert(card.snapshot().online&&card.snapshot().rebootState==RemoteRelay::REBOOT_UNCERTAIN&&rebootCalls==3);
 assert(card.reboot());rebootCode=200;const uint32_t sentAt=mockMillis;oneIteration();assert(card.snapshot().rebootState==RemoteRelay::REBOOT_SENT&&rebootCalls==4);
 stateCode=503;mockMillis=sentAt+29899;oneIteration();assert(card.snapshot().rebootState==RemoteRelay::REBOOT_SENT);
 mockMillis+=1;oneIteration();assert(card.snapshot().rebootState==RemoteRelay::REBOOT_UNCERTAIN&&rebootCalls==4);
 stateCode=200;oneIteration();assert(card.snapshot().online&&card.snapshot().rebootState==RemoteRelay::REBOOT_UNCERTAIN);
 assert(card.reboot());rebootCode=403;oneIteration();assert(card.snapshot().rebootState==RemoteRelay::REBOOT_FAILED&&card.snapshot().online&&rebootCalls==5);
 assert(!std::strcmp(card.snapshot().config.address,"192.168.1.10"));
 assert(!std::strcmp(card.snapshot().config.labels[0],"A"));
 std::puts("PASS: Relais-Neustart, Wiedererreichbarkeit, alte Firmware und verlorene Antwort ohne Wiederholung.");
}
