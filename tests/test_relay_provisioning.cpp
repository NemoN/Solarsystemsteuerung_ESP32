#include "../Backload/RelayProvisioning.h"
#include "../Backload/RelayPairAuth.h"
#include <HTTPClient.h>
#include <cassert>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>
uint32_t mockMillis=0;
ESPClass ESP;
WiFiClass WiFi;
TaskFunction_t mockTask=nullptr;
void* mockTaskArg=nullptr;
std::vector<uint8_t> mockGithubImage;
size_t mockGithubOffset=0,mockGithubStopAfter=size_t(-1);
int mockGithubAttempts=0,mockGithubFailures=0,mockGithubCode=200;
long mockGithubReportedSize=-1,mockRelayReportedSize=-1;
std::map<std::string,std::vector<unsigned char>> mockPreferences;
std::function<void(AsyncWebServerRequest*)> mockPairHandler, mockPairHello;
bool mockPairServerRunning=false;
RelayDiscoverySnapshot mockDiscovery={};
unsigned searches=0;bool fakeIdentity=false;
const char* expectedKey="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
void mockAdvance(uint32_t n){mockMillis+=n;}
bool RelayDiscovery::begin(){return true;}
bool RelayDiscovery::search(){++searches;return true;}
RelayDiscoverySnapshot RelayDiscovery::snapshot(){return mockDiscovery;}
int mockHttp(const String& url,const String* body,String& response){
 assert(std::string(url.c_str()).find("/api/identity")!=std::string::npos);
 assert(body&&std::string(body->c_str()).find("nonce=")==0);
 const std::string nonce=std::string(body->c_str()).substr(6);
 assert(nonce.size()==32&&std::string(body->c_str()).find(expectedKey)==std::string::npos);
 char proof[65];assert(relayPairProof(expectedKey,nonce.c_str(),proof));
 if(fakeIdentity)proof[0]=proof[0]=='a'?'b':'a';response=proof;return 200;
}
static AsyncWebServerRequest claim(const char* id,const char* key,IPAddress local,bool tamper=false){
 AsyncWebServerRequest hello;hello.connection.ip=WiFi.softAPIP();mockPairHello(&hello);assert(hello.responseCode==200);
 RelayPairKey card;char pub[131], proof[121];uint8_t session[32];
 assert(card.generate(pub)&&card.shared(hello.response.c_str(),pub,false,session));
 const std::string aad=std::string("relay-key:")+id;
 assert(relaySecureSeal(session,aad.c_str(),key,proof,sizeof(proof)));
 if(tamper)proof[30]=proof[30]=='a'?'b':'a';
 AsyncWebServerRequest request;request.connection.ip=local;
 request.values["id"].text=id;request.values["pub"].text=pub;request.values["proof"].text=proof;
 mockPairHandler(&request);
 if(request.responseCode==200){
  char clear[193];const std::string outAad=std::string("relay-wifi:")+id;
  assert(!strstr(request.response.c_str(),"password123"));
  assert(relaySecureOpen(session,outAad.c_str(),request.response.c_str(),clear,sizeof(clear)));
  assert(!strcmp(clear,"PAIR1:SG91c2UtV2lGaQ==:cGFzc3dvcmQxMjM="));
 }
 return request;
}
int main(){
 RemoteRelay cards[4];for(int i=0;i<4;i++){char key[16];std::snprintf(key,sizeof(key),"config%d",i+1);assert(cards[i].begin(key));}
 RelayDiscovery discovery;RelayProvisioning pair;assert(pair.begin(cards,4,&discovery));assert(mockPairHandler&&mockPairHello);
 assert(!pair.start(1,"wrong-name","House-WiFi","password123"));
 WiFi.stations=1;WiFi.mode|=WIFI_AP;
 assert(!pair.start(1,"Relaiserweiterung-ABCD","House-WiFi","password123"));
 WiFi.stations=0;WiFi.mode=1;
 assert(pair.start(1,"Relaiserweiterung-ABCD","House-WiFi","password123"));
 assert(pair.active()&&pair.apActive()&&mockPairServerRunning);
 assert(WiFi.status()==WL_CONNECTED&&WiFi.apSsid==String("Backload-Pair-ABCD"));
 auto lanHello=AsyncWebServerRequest{};lanHello.connection.ip=WiFi.localIP();mockPairHello(&lanHello);assert(lanHello.responseCode==403);
 auto wrong=claim("12345678abcd","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",WiFi.softAPIP(),true);assert(wrong.responseCode==403);
 auto lan=claim("12345678abcd","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",WiFi.localIP());assert(lan.responseCode==403);
 auto wrongId=claim("12345678abce","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",WiFi.softAPIP());assert(wrongId.responseCode==403);
 auto ok=claim("12345678abcd","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",WiFi.softAPIP());assert(ok.responseCode==200);
 auto retry=claim("12345678abcd","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",WiFi.softAPIP());assert(retry.responseCode==200);
 auto different=claim("12345678abcd","cccccccccccccccccccccccccccccccc",WiFi.softAPIP());assert(different.responseCode==403);
 assert(!cards[1].snapshot().config.address[0]);
 mockMillis+=12000;pair.loop();assert(!pair.apActive()&&!mockPairServerRunning&&searches);
 const uint8_t remote[]={192,168,1,11},own[]={192,168,1,2},mask[]={255,255,255,0};
 assert(mockDiscovery.results.add("relais-12345678abcd",remote,own,mask,80));pair.loop();
 assert(std::string(pair.snapshot().state)=="verifying");assert(mockTask);mockTask(mockTaskArg);pair.loop();
 assert(std::string(pair.snapshot().state)=="done");assert(!std::strcmp(cards[1].snapshot().config.address,"192.168.1.11"));
 assert(!std::strcmp(cards[1].snapshot().config.key,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
 assert(!cards[0].snapshot().config.address[0]);
 // A forged discovery result cannot get the stored secret or become configured.
 mockDiscovery=RelayDiscoverySnapshot{};mockMillis+=100;
 assert(pair.start(2,"Relaiserweiterung-ABCD","House-WiFi","password123"));
 expectedKey="cccccccccccccccccccccccccccccccc";
 ok=claim("12345678abcd",expectedKey,WiFi.softAPIP());assert(ok.responseCode==200);
 mockMillis+=12000;pair.loop();const uint8_t forged[]={192,168,1,12};assert(mockDiscovery.results.add("relais-12345678abcd",forged,own,mask,80));
 fakeIdentity=true;pair.loop();mockTask(mockTaskArg);pair.loop();
 assert(std::string(pair.snapshot().state)=="failed"&&!cards[2].snapshot().config.address[0]);
 // Loss of home WLAN ends pairing without persisting a board.
 fakeIdentity=false;assert(pair.start(3,"Relaiserweiterung-ABCD","House-WiFi","password123"));
 WiFi.state=0;pair.loop();assert(std::string(pair.snapshot().state)=="failed"&&!mockPairServerRunning&&!cards[3].snapshot().config.address[0]);
 WiFi.state=WL_CONNECTED;assert(pair.start(3,"Relaiserweiterung-ABCD","House-WiFi","password123"));
 mockMillis+=150000;pair.loop();assert(std::string(pair.snapshot().state)=="failed"&&!mockPairServerRunning);
 assert(pair.start(3,"Relaiserweiterung-ABCD","House-WiFi","password123"));
 assert(pair.cancel()&&!pair.active()&&!mockPairServerRunning&&!cards[3].snapshot().config.address[0]);
 std::puts("PASS: Automatische WLAN-Kopplung, Identitätsprüfung, Mehrfachkarten und Fehlerfälle.");
}
