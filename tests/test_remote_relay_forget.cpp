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
int mockGithubAttempts=0,mockGithubFailures=0,mockGithubCode=200; long mockGithubReportedSize=-1;
std::map<std::string,std::vector<unsigned char>> mockPreferences;
int mockPreferenceFailures=0;
struct EndIteration{};
void mockAdvance(uint32_t n){mockMillis+=n;throw EndIteration{};}
int resetCode=200,resetCalls=0;
int mockHttp(const String& url,const String* body,String& response){
 if(std::string(url.c_str()).find("/api/factory-reset")!=std::string::npos){assert(body);++resetCalls;if(resetCode==200)response="RESET1:OK";return resetCode;}
 response="RLY1:0";return 200;
}
void oneIteration(){try{mockTask(mockTaskArg);assert(false);}catch(const EndIteration&) {}}
int main(){
 RemoteRelay card;assert(card.begin("config1"));String labels[]={"A","B","C","D"};
 assert(card.configure("192.168.1.10","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",labels));
 oneIteration();assert(card.snapshot().online);
 String edited[]={"Licht","Ventil","Pumpe","Reserve"};
 assert(card.configure("192.168.1.10","",edited));assert(card.snapshot().online);
 assert(!std::strcmp(card.snapshot().config.labels[0],"Licht"));
 assert(card.forget());assert(!card.forget());
 oneIteration();auto done=card.snapshot();assert(done.forgetState==RemoteRelay::FORGET_DONE&&!done.config.address[0]&&!done.config.key[0]&&resetCalls==1);
 assert(card.configure("192.168.1.10","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",labels));oneIteration();assert(card.forget());resetCode=-1;oneIteration();
 auto uncertain=card.snapshot();assert(uncertain.forgetState==RemoteRelay::FORGET_UNCERTAIN&&uncertain.config.address[0]&&resetCalls==2);
 oneIteration();assert(resetCalls==2); // Never repeat a reset request automatically.
 assert(card.forget());resetCode=404;oneIteration();auto rejected=card.snapshot();assert(rejected.forgetState==RemoteRelay::FORGET_FAILED&&rejected.config.address[0]);
 assert(card.forget());resetCode=200;mockPreferenceFailures=1;oneIteration();
 assert(card.snapshot().forgetState==RemoteRelay::FORGET_SAVE_FAILED&&card.snapshot().config.address[0]);
 assert(card.clearAssociation());assert(!card.snapshot().config.address[0]);assert(resetCalls==4);
 std::puts("PASS: Bestätigter Reset, unklare Antwort und abgelehnte alte Firmware.");
}
