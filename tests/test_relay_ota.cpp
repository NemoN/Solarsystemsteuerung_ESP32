#include "../Relaiskarte/RelayOta.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
uint32_t mockMillis=0;
uint32_t mockRunningAddress=0x10000;
ESPClass ESP;
MockUpdate Update;
static const std::string token(32,'a');
static std::string base64(const uint8_t* p,size_t n) {
 const char* t="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string s;
 for(size_t i=0;i<n;i+=3){unsigned x=unsigned(p[i])<<16;if(i+1<n)x|=unsigned(p[i+1])<<8;if(i+2<n)x|=p[i+2];s+=t[(x>>18)&63];s+=t[(x>>12)&63];s+=i+1<n?t[(x>>6)&63]:'=';s+=i+2<n?t[x&63]:'=';}return s;
}
static std::string digest(const std::vector<uint8_t>& image){
 uint8_t hash[32];mbedtls_sha256_ret(image.data(),image.size(),hash,0);const char* h="0123456789abcdef";std::string s;
 for(uint8_t c:hash){s+=h[c>>4];s+=h[c&15];}return s;
}
static WebServer prepare(size_t n,const std::string& hash){WebServer s;s.values={{"token",token},{"sha256",hash},{"size",std::to_string(n)}};return s;}
static WebServer part(size_t offset,const uint8_t* p,size_t n){WebServer s;s.values={{"token",token},{"offset",std::to_string(offset)},{"data",base64(p,n)}};return s;}
static WebServer tokenOnly(){WebServer s;s.values={{"token",token}};return s;}
int main(){
 std::vector<uint8_t> image(4096,0x3b);image[0]=0xe9;image[12]=image[13]=0;
 const std::string marker=RELAY_FIRMWARE_MARKER;std::copy(marker.begin(),marker.end(),image.begin()+2039);
 RelayOta ota;
 auto invalid=prepare(image.size(),std::string(64,'q'));ota.prepare(invalid);assert(invalid.code==400&&!ota.active());
 auto p=prepare(image.size(),digest(image));ota.prepare(p);assert(p.code==200&&ota.active()&&Update.running);
 auto again=prepare(image.size(),digest(image));ota.prepare(again);assert(again.code==200);
 auto c=part(0,image.data(),2048);ota.chunk(c);assert(c.code==200&&Update.writes==1);
 auto retry=part(0,image.data(),2048);ota.chunk(retry);assert(retry.code==200&&Update.writes==1);
 auto bad=part(2000,image.data()+2048,2048);ota.chunk(bad);assert(bad.code==400&&!ota.active()&&!Update.running);
 auto p2=prepare(image.size(),std::string(64,'0'));ota.prepare(p2);assert(p2.code==200);
 auto c1=part(0,image.data(),2048),c2=part(2048,image.data()+2048,2048);ota.chunk(c1);ota.chunk(c2);
 auto fail=tokenOnly();ota.finish(fail);assert(fail.code==400&&!Update.finished&&!ota.active());
 auto p3=prepare(image.size(),digest(image));ota.prepare(p3);assert(p3.code==200);
 c1=part(0,image.data(),2048);c2=part(2048,image.data()+2048,2048);ota.chunk(c1);ota.chunk(c2);
 auto wrong=tokenOnly();wrong.values["token"]=std::string(32,'b');ota.finish(wrong);assert(wrong.code==409&&ota.active());
 auto done=tokenOnly();ota.finish(done);assert(done.code==200&&Update.finished&&ota.active()&&Update.image==image);
 mockMillis+=1499;ota.loop();assert(ESP.restarts==0);mockMillis++;ota.loop();assert(ESP.restarts==1);
 RelayOta timeout;auto p4=prepare(image.size(),digest(image));timeout.prepare(p4);assert(p4.code==200);mockMillis+=45000;timeout.loop();assert(!timeout.active()&&!Update.running);
 std::puts("PASS: Relais-OTA: Prüfung, Blockwiederholung, Fehler, Prüfsumme, Neustart und Zeitlimit.");
}
