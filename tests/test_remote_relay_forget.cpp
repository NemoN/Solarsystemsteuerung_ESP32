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
bool incompleteBody=false;
int mockHttp(const String& url,const String* body,String& response){
 if(std::string(url.c_str()).find("/api/factory-reset")!=std::string::npos){assert(body);++resetCalls;if(resetCode==200)response="RESET1:OK";return resetCode;}
 if(std::string(url.c_str()).find("/api/version")!=std::string::npos){++versionCalls;if(versionCode==200)response="RLYVER1:2.7.0";return versionCode;}
 if(body)++commandCalls;
 if(stateCode==200)response=incompleteBody?"RLY1:":"RLY1:0";
 return stateCode;
}
void oneIteration(){try{mockTask(mockTaskArg);assert(false);}catch(const EndIteration&) {}}
int main(){
 RemoteRelay card;assert(card.begin("config1"));String labels[]={"A","B","C","D"};
 assert(card.configure("192.168.1.10","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",labels));
 oneIteration();assert(card.snapshot().online);assert(!std::strcmp(card.snapshot().version,"2.7.0")&&versionCalls==1);
 stateCode=503;mockMillis+=2000;oneIteration();assert(card.snapshot().online&&!std::strcmp(card.snapshot().version,"2.7.0"));
 mockMillis+=5000;oneIteration();assert(!card.snapshot().online&&!card.snapshot().version[0]);
 stateCode=200;oneIteration();assert(card.snapshot().online&&!std::strcmp(card.snapshot().version,"2.7.0"));
 assert(card.command(1,true));stateCode=503;oneIteration();assert(!card.snapshot().online);
 stateCode=200;oneIteration();assert(card.snapshot().online);
 assert(card.command(1,true));incompleteBody=true;mockRelayReportedSize=6;
 const int beforeCommand=commandCalls;const uint32_t beforeWait=mockMillis;oneIteration();
 assert(mockMillis-beforeWait>=2500 && !card.snapshot().busy && !card.snapshot().online && commandCalls==beforeCommand+1);
 incompleteBody=false;mockRelayReportedSize=-1;mockMillis+=2000;oneIteration();
 assert(card.snapshot().online && !card.snapshot().busy && commandCalls==beforeCommand+1);
 const int previousVersionCalls=versionCalls;
 oneIteration();assert(versionCalls==previousVersionCalls); // Die Version wird nicht bei jeder Zustandsabfrage angefordert.
 versionCode=404;mockMillis+=10001;oneIteration();assert(card.snapshot().online&&!card.snapshot().version[0]&&versionCalls==previousVersionCalls+1);
 versionCode=200;mockMillis+=10001;oneIteration();assert(!std::strcmp(card.snapshot().version,"2.7.0")&&versionCalls==previousVersionCalls+2);
 String edited[]={"Licht","Ventil","Pumpe","Reserve"};
 assert(card.configure("192.168.1.10","",edited));assert(card.snapshot().online);
 assert(!std::strcmp(card.snapshot().config.labels[0],"Licht")&&!std::strcmp(card.snapshot().version,"2.7.0"));
 assert(card.forget());assert(!card.forget());
 oneIteration();auto done=card.snapshot();assert(done.forgetState==RemoteRelay::FORGET_DONE&&!done.config.address[0]&&!done.config.key[0]&&resetCalls==1);
 assert(card.configure("192.168.1.10","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",labels));oneIteration();assert(card.forget());resetCode=-1;oneIteration();
 auto uncertain=card.snapshot();assert(uncertain.forgetState==RemoteRelay::FORGET_UNCERTAIN&&uncertain.config.address[0]&&resetCalls==2);
 oneIteration();assert(resetCalls==2); // Never repeat a reset request automatically.
 assert(card.forget());resetCode=404;oneIteration();auto rejected=card.snapshot();assert(rejected.forgetState==RemoteRelay::FORGET_FAILED&&rejected.config.address[0]);
 assert(card.forget());resetCode=200;mockPreferenceFailures=1;oneIteration();
 assert(card.snapshot().forgetState==RemoteRelay::FORGET_SAVE_FAILED&&card.snapshot().config.address[0]);
 assert(card.clearAssociation());assert(!card.snapshot().config.address[0]);assert(resetCalls==4);
 RemoteRelay older;assert(older.begin("config2"));assert(older.configure("192.168.1.11","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",labels));
 versionCode=404;oneIteration();assert(older.snapshot().online&&!older.snapshot().version[0]);
 std::puts("PASS: Bestätigter Reset, unklare Antwort und abgelehnte alte Firmware.");
}
