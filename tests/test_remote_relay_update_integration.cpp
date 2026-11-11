#include "../Backload/RemoteRelayUpdate.h"
#include "../Relaiskarte/RelayOta.h"
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <WiFi.h>
#include <cassert>
#include <cstdio>
#include <map>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
uint32_t mockMillis=0, mockRunningAddress=0x10000;
ESPClass ESP;
MockUpdate Update;
WiFiClass WiFi;
std::vector<uint8_t> mockGithubImage;
size_t mockGithubOffset=0, mockGithubStopAfter=size_t(-1);
int mockGithubAttempts=0, mockGithubFailures=0, mockGithubCode=200;
long mockGithubReportedSize=-1,mockRelayReportedSize=-1;
TaskFunction_t mockTask=nullptr;
void* mockTaskArg=nullptr;
RelayOta board;
bool lostChunk=false,lostFinish=false,failedChunk=false,booted=false,rollbackBoot=false;
unsigned transientChunkFailures=0;
unsigned chunkCalls=0;uint32_t bootAt=0;
void mockAdvance(uint32_t ms){
 mockMillis+=ms;
 if(!booted){board.loop();if(ESP.restarts){booted=true;bootAt=mockMillis;mockRunningAddress=rollbackBoot?0x10000:0x150000;}}
}
static int hexdigit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static std::string decode(const std::string& s){std::string r;for(size_t i=0;i<s.size();i++){if(s[i]=='%'&&i+2<s.size()){int a=hexdigit(s[i+1]),b=hexdigit(s[i+2]);assert(a>=0&&b>=0);r+=char(a*16+b);i+=2;}else if(s[i]=='+')r+=' ';else r+=s[i];}return r;}
int mockHttp(const String& url,const String* body,String& response){
 const std::string u=url.c_str();WebServer s;
 if(body){std::string text=body->c_str();size_t pos=0;while(pos<text.size()){size_t end=text.find('&',pos);if(end==std::string::npos)end=text.size();auto eq=text.find('=',pos);assert(eq<end);s.values[decode(text.substr(pos,eq-pos))]=decode(text.substr(eq+1,end-eq-1));pos=end+1;}}
 if(u.find("/api/ota/info")!=std::string::npos)board.info(s,booted&&mockMillis-bootAt<10000);
 else if(u.find("/api/ota/prepare")!=std::string::npos)board.prepare(s);
 else if(u.find("/api/ota/chunk")!=std::string::npos){
  ++chunkCalls;
  if(failedChunk)return -1;
  if(transientChunkFailures){--transientChunkFailures;return -1;}
  board.chunk(s);
  if(lostChunk){lostChunk=false;return -1;}
 }
 else if(u.find("/api/ota/finish")!=std::string::npos){board.finish(s);if(lostFinish){lostFinish=false;return -1;}}
 else if(u.find("/api/ota/abort")!=std::string::npos)board.abort(s);
 else return 404;
 response=s.response;return s.code;
}
static String b64(const uint8_t* data,size_t n){std::vector<uint8_t> out(((n+2)/3)*4+1);size_t len=0;assert(mbedtls_base64_encode(out.data(),out.size(),&len,data,n)==0);return String(reinterpret_cast<char*>(out.data()));}
static std::string hashImage(const std::vector<uint8_t>& data){
 uint8_t hash[32];assert(mbedtls_sha256_ret(data.data(),data.size(),hash,0)==0);
 const char* digits="0123456789abcdef";std::string out;
 for(uint8_t byte:hash){out+=digits[byte>>4];out+=digits[byte&15];}return out;
}
static RemoteRelaySnapshot relay(){RemoteRelaySnapshot r={};r.online=true;std::strcpy(r.config.address,"192.168.1.3");std::strcpy(r.config.key,"0123456789abcdef0123456789abcdef");return r;}
static std::vector<uint8_t> firmware(){std::vector<uint8_t> b(4096,0x3b);b[0]=0xe9;b[12]=b[13]=0;std::string m=RELAY_FIRMWARE_MARKER;std::copy(m.begin(),m.end(),b.begin()+2039);return b;}
int main(int argc,char** argv){
 RemoteRelayUpdate uploader;assert(uploader.begin());auto f=firmware();char token[33];
 assert(uploader.prepare(f.size(),relay(),token));assert(uploader.active());
 assert(uploader.chunk(token,0,b64(f.data(),2048)));assert(uploader.chunk(token,0,b64(f.data(),2048)));
 assert(!uploader.chunk(token,3,b64(f.data(),2048)));assert(uploader.chunk(token,2048,b64(f.data()+2048,2048)));
 RemoteRelayUpdateStatus st;assert(uploader.status(token,st)&&st.received==f.size());
 lostChunk=true;lostFinish=true;assert(uploader.start(token));assert(mockTask);mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="done"&&st.sent==f.size());
 assert(!uploader.active()&&Update.finished&&Update.image==f&&Update.writes==2&&chunkCalls==3);
 assert(booted&&mockRunningAddress==0x150000&&mockMillis-bootAt>=10000);
 // Auch vier kurzzeitige Verbindungsfehler lassen sich ohne doppeltes Schreiben überbrücken.
 board=RelayOta();Update=MockUpdate();ESP.restarts=0;mockRunningAddress=0x10000;booted=false;mockTask=nullptr;chunkCalls=0;
 transientChunkFailures=4;
 assert(uploader.prepare(f.size(),relay(),token));assert(uploader.chunk(token,0,b64(f.data(),f.size())));
 assert(uploader.start(token));mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="done"&&Update.image==f&&Update.writes==2&&chunkCalls==6);
 // Falsches Image wird verworfen, bevor die Relaiskarte eine Anfrage erhält.
 board=RelayOta();Update=MockUpdate();ESP.restarts=0;mockRunningAddress=0x10000;booted=false;mockTask=nullptr;chunkCalls=0;
 auto bad=f;bad[12]=9;
 assert(uploader.prepare(bad.size(),relay(),token));assert(uploader.chunk(token,0,b64(bad.data(),bad.size())));
 assert(uploader.start(token));mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="failed"&&!board.active()&&chunkCalls==0&&!Update.finished);
 // Nach einem Verbindungsfehler bricht der Haupt-ESP das Update der Karte ab.
 failedChunk=true;mockTask=nullptr;
 assert(uploader.prepare(f.size(),relay(),token));assert(uploader.chunk(token,0,b64(f.data(),f.size())));
 assert(uploader.start(token));mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="failed"&&!board.active()&&!Update.running&&!Update.finished);
 assert(std::string(st.error).find("Verbindungsfehler -1")!=std::string::npos);
 failedChunk=false;
 // Rückfall auf den alten Slot wird nicht als erfolgreich angezeigt.
 board=RelayOta();Update=MockUpdate();ESP.restarts=0;mockRunningAddress=0x10000;booted=false;mockTask=nullptr;rollbackBoot=true;
 assert(uploader.prepare(f.size(),relay(),token));assert(uploader.chunk(token,0,b64(f.data(),f.size())));
 assert(uploader.start(token));mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="failed");
 assert(std::string(st.error).find("bisherigen Firmware")!=std::string::npos);
 rollbackBoot=false;
 // Ein abgebrochener Browserupload gibt den Zwischenspeicher wieder frei.
 assert(uploader.prepare(f.size(),relay(),token));assert(uploader.cancel(token));assert(!uploader.active());
 assert(uploader.status(token,st)&&std::string(st.state)=="canceled");
 if(argc==2){
  std::ifstream input(argv[1],std::ios::binary);assert(input.good());
  std::vector<uint8_t> compiled((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());assert(compiled.size()>4096);
  board=RelayOta();Update=MockUpdate();ESP.restarts=0;mockRunningAddress=0x10000;booted=false;mockTask=nullptr;
  assert(uploader.prepare(compiled.size(),relay(),token));
  for(size_t offset=0;offset<compiled.size();offset+=4096){size_t n=std::min(size_t(4096),compiled.size()-offset);assert(uploader.chunk(token,offset,b64(compiled.data()+offset,n)));}
  assert(uploader.start(token));mockTask(mockTaskArg);
  assert(uploader.status(token,st)&&std::string(st.state)=="done"&&Update.image==compiled);
  std::puts("PASS: Kompilierte Relaiskarten-Firmware vollständig und mit gültiger Kennung übertragen.");
 }
 // GitHub: Download, Retry, Integritätsprüfung und Übergabe an die Karte.
 const String url("https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/v2.5.0/Relaiskarte.ino.bin");
 const String sha(hashImage(f));
 board=RelayOta();Update=MockUpdate();ESP.restarts=0;mockRunningAddress=0x10000;booted=false;mockTask=nullptr;
 mockGithubImage=f;mockGithubFailures=1;mockGithubAttempts=0;mockGithubStopAfter=size_t(-1);mockGithubReportedSize=-1;mockGithubCode=200;
 assert(!uploader.prepareGithub(f.size(),"https://example.org/file.bin",sha,relay(),token));
 assert(!uploader.prepareGithub(f.size(),"https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/v2.5.0/Backload.ino.bin",sha,relay(),token));
 assert(!uploader.prepareGithub(f.size(),url,String(std::string(64,'X')),relay(),token));
 assert(uploader.prepareGithub(f.size(),url,sha,relay(),token));assert(uploader.active());
 assert(uploader.status(token,st)&&std::string(st.state)=="downloading");mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="done"&&st.received==f.size());
 assert(mockGithubAttempts==2&&Update.image==f&&Update.finished);
 // Eine falsche Prüfsumme oder ein unvollständiger Download erreicht die Karte nicht.
 board=RelayOta();Update=MockUpdate();ESP.restarts=0;mockRunningAddress=0x10000;booted=false;mockTask=nullptr;chunkCalls=0;
 mockGithubAttempts=0;mockGithubFailures=0;
 assert(uploader.prepareGithub(f.size(),url,String(std::string(64,'0')),relay(),token));mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="failed"&&std::string(st.error).find("SHA-256")!=std::string::npos);
 assert(!board.active()&&chunkCalls==0&&!Update.finished);
 mockGithubStopAfter=1024;mockGithubAttempts=0;
 assert(uploader.prepareGithub(f.size(),url,sha,relay(),token));mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="failed"&&mockGithubAttempts==3&&!board.active());
 mockGithubStopAfter=size_t(-1);mockGithubReportedSize=1024;mockGithubAttempts=0;
 assert(uploader.prepareGithub(f.size(),url,sha,relay(),token));mockTask(mockTaskArg);
 assert(uploader.status(token,st)&&std::string(st.state)=="failed"&&mockGithubAttempts==1&&!board.active());
 mockGithubReportedSize=-1;
 std::puts("PASS: GitHub-Download, Wiederholung, Größe, Prüfsumme und erlaubte Release-URL.");
 std::puts("PASS: Haupt-ESP und Relaiskarte: OTA, verlorene Antworten, Neustart, Abbruch und falsches Image.");
}
