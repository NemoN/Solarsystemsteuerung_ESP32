#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>
#define PROGMEM
class String {
  std::string s;
 public:
  String() = default; String(const char* p): s(p ? p : "") {} String(const std::string& v): s(v) {}
  size_t length() const { return s.size(); } const char* c_str() const { return s.c_str(); }
  int indexOf(char c, int from = 0) const { auto n=s.find(c,from); return n==std::string::npos ? -1 : int(n); }
  String substring(int from, int to) const { return s.substr(from,to-from); }
  String substring(int from) const { return s.substr(from); }
  void trim() { auto a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos){s.clear();return;} auto b=s.find_last_not_of(" \t\r\n"); s=s.substr(a,b-a+1); }
  bool startsWith(const char* p) const { return s.rfind(p,0)==0; }
  bool operator==(const String& v) const { return s==v.s; } bool operator!=(const String& v) const { return s!=v.s; }
  String operator+(const String& v) const { return s+v.s; }
};
inline String operator+(const char* a,const String& b){return String(a)+b;}
struct AsyncWebHeader { String v; String value() const {return v;} };
struct AsyncWebParameter { String v; String value() const {return v;} };
struct AsyncWebServerResponse { int status; String body; std::map<std::string,String> headers; void addHeader(const char* k,const String& v){headers[k]=v;} };
enum WebRequestMethod { HTTP_GET=1, HTTP_POST=2 };
struct AsyncWebServerRequest {
  WebRequestMethod m=HTTP_GET; String path="/"; std::map<std::string,AsyncWebHeader> headers; std::map<std::string,AsyncWebParameter> params;
  int status=0; String response,location; std::map<std::string,String> responseHeaders;
  WebRequestMethod method()const{return m;} String url()const{return path;}
  bool hasHeader(const char* s)const{return headers.count(s);} const AsyncWebHeader* getHeader(const char* s)const{return &headers.at(s);}
  bool hasParam(const char* s,bool)const{return params.count(s);} const AsyncWebParameter* getParam(const char* s,bool)const{return &params.at(s);}
  void send(int code,const char*,const char* value){status=code;response=value;}
  void send(AsyncWebServerResponse* res){status=res->status;response=res->body;responseHeaders=res->headers;delete res;}
  void redirect(const char* p){status=302;location=p;}
  AsyncWebServerResponse* beginResponse(int code,const char*,const char* data){return new AsyncWebServerResponse{code,data,{}};}
  AsyncWebServerResponse* beginResponse_P(int code,const char* type,const char* data){return beginResponse(code,type,data);}
};
using ArMiddlewareNext=std::function<void()>;
using ArMiddlewareCallback=std::function<void(AsyncWebServerRequest*,ArMiddlewareNext)>;
struct AsyncWebServer {
  ArMiddlewareCallback middleware;
  std::map<std::string,std::function<void(AsyncWebServerRequest*)>> routes;
  void addMiddleware(ArMiddlewareCallback cb){middleware=cb;}
  void on(const char* path,WebRequestMethod method,std::function<void(AsyncWebServerRequest*)> cb){routes[std::to_string(method)+path]=cb;}
  void dispatch(AsyncWebServerRequest& req){middleware(&req,[&]{auto it=routes.find(std::to_string(req.m)+req.path.c_str());if(it!=routes.end())it->second(&req);else req.send(200,"text/plain","handled");});}
};
