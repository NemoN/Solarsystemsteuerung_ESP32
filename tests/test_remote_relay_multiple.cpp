#include "../Backload/RemoteRelay.h"
#include <WiFi.h>
#include <cassert>
#include <cstdio>
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
long mockGithubReportedSize=-1;
std::map<std::string,std::vector<unsigned char>> mockPreferences;
void mockAdvance(uint32_t n){mockMillis+=n;}
int mockHttp(const String&,const String*,String&){return -1;}
int main(){
 RemoteRelayConfig old={};std::strcpy(old.address,"192.168.1.10");std::strcpy(old.key,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
 for(int i=0;i<4;i++)std::snprintf(old.labels[i],sizeof(old.labels[i]),"Alt %d",i+1);
 Preferences legacy;assert(legacy.begin("remote-relay",false));assert(legacy.putBytes("config1",&old,sizeof(old))==sizeof(old));
 RemoteRelay first,second;assert(first.begin("config1")&&second.begin("config2"));
 assert(!std::strcmp(first.snapshot().config.address,"192.168.1.10"));
 assert(!std::strcmp(first.snapshot().config.labels[0],"Alt 1"));
 assert(!second.snapshot().config.address[0]);
 String labels[]={"Ventil","Pumpe","Licht","Reserve"};
 assert(second.configure("192.168.1.11","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",labels));
 assert(!std::strcmp(second.snapshot().config.labels[0],"Ventil"));
 assert(!std::strcmp(first.snapshot().config.address,"192.168.1.10"));
 assert(!first.configure("192.168.1.20","",labels)); // Schlüssel nicht an neue Adresse senden.
 assert(first.configure("192.168.1.10","",labels));
 RemoteRelay restored1,restored2;assert(restored1.begin("config1")&&restored2.begin("config2"));
 assert(!std::strcmp(restored1.snapshot().config.address,"192.168.1.10"));
 assert(!std::strcmp(restored2.snapshot().config.address,"192.168.1.11"));
 assert(!std::strcmp(restored2.snapshot().config.key,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
 std::puts("PASS: Mehrere Relaiserweiterungen, getrennte Einstellungen und Übernahme vorhandener Daten.");
}
