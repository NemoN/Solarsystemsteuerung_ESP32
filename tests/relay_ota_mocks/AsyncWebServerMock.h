#pragma once
#include "WiFi.h"
#include <functional>
#include <map>
#include <string>
constexpr int HTTP_POST=2;
constexpr int HTTP_GET=1;
struct AsyncWebHeader { String text; String value()const{return text;} };
struct AsyncWebParameter { String text; String value()const{return text;} };
struct AsyncClientMock { IPAddress ip; IPAddress localIP()const{return ip;} };
struct AsyncWebServerResponse { int code; String text; void addHeader(const char*,const char*){} };
struct AsyncWebServerRequest {
 AsyncClientMock connection;std::map<std::string,AsyncWebHeader> headers;std::map<std::string,AsyncWebParameter> values;int responseCode=0;String response;
 AsyncClientMock* client(){return &connection;}
 bool hasHeader(const char* k){return headers.count(k);}
 AsyncWebHeader* getHeader(const char* k){return &headers.at(k);}
 bool hasParam(const char* k,bool){return values.count(k);}
 AsyncWebParameter* getParam(const char* k,bool){return &values.at(k);}
 size_t params(){return values.size();}
 void send(int code,const char*,const String& text){responseCode=code;response=text;}
 AsyncWebServerResponse* beginResponse(int code,const char*,const String& text){return new AsyncWebServerResponse{code,text};}
 void send(AsyncWebServerResponse* reply){responseCode=reply->code;response=reply->text;delete reply;}
};
extern std::function<void(AsyncWebServerRequest*)> mockPairHandler;
extern std::function<void(AsyncWebServerRequest*)> mockPairHello;
extern bool mockPairServerRunning;
struct AsyncWebServer {
 explicit AsyncWebServer(int){}
 void on(const char* path,int,std::function<void(AsyncWebServerRequest*)> f){if(std::string(path)=="/hello")mockPairHello=f;else mockPairHandler=f;}
 void onNotFound(std::function<void(AsyncWebServerRequest*)>){}
 void begin(){mockPairServerRunning=true;}
 void end(){mockPairServerRunning=false;}
};
