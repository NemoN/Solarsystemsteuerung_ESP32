// Exercise the production access-control code with mock HTTP, NVS and time.
// The cryptographic primitives are stubbed; firmware uses ESP32 mbedTLS.
#include <cassert>
#include <iostream>
#include "../Backload/WebAccess.cpp"

std::vector<uint8_t> mockRecord;
bool mockOpen=true, mockWrite=true;
int64_t mockTime=100;
int socketsClosed=0;
void closed(){++socketsClosed;}

AsyncWebServerRequest makeRequest(WebRequestMethod method,const char* path,const char* cookie=nullptr) {
  AsyncWebServerRequest req; req.m=method;req.path=path;
  req.headers["Host"]={"192.168.1.2"};req.headers["Origin"]={"http://192.168.1.2"};
  if(method==HTTP_POST)req.headers["X-Requested-With"]={"HeatingUI"};
  if(cookie)req.headers["Cookie"]={cookie};
  return req;
}
AsyncWebServerRequest call(AsyncWebServer& server,WebRequestMethod method,const char* path,const char* cookie=nullptr,const char* password=nullptr){
  auto req=makeRequest(method,path,cookie);
  if(password)req.params["password"]={password};
  server.dispatch(req);return req;
}
std::string cookie(const AsyncWebServerRequest& req){
  std::string value=req.responseHeaders.at("Set-Cookie").c_str();
  return value.substr(0,value.find(';'));
}
int main(){
  beginWebAccess(closed);
  AsyncWebServer main,monitor;attachWebAccess(main,true);attachWebAccess(monitor,false);
  assert(call(main,HTTP_GET,"/").status==200);
  assert(call(main,HTTP_GET,"/api/status").status==200);
  auto csrf=makeRequest(HTTP_POST,"/auth/password");csrf.params["password"]={"abcdefgh"};csrf.headers["Origin"]={"http://evil.example"};main.dispatch(csrf);assert(csrf.status==403 && mockRecord.empty());
  assert(call(main,HTTP_POST,"/auth/password",nullptr,"abcdefgh").status==200);
  assert(socketsClosed==1 && !mockRecord.empty());
  assert(call(main,HTTP_GET,"/").status==302);
  assert(call(main,HTTP_GET,"/api/pump").status==401);
  assert(call(main,HTTP_GET,"/assets/esp32-board.jpg").status==302);
  assert(call(main,HTTP_POST,"/api/hostname",nullptr,"test").status==401);
  assert(call(main,HTTP_POST,"/api/solar",nullptr,"test").status==401);
  assert(call(main,HTTP_GET,"/api/status").status==200);
  assert(call(monitor,HTTP_GET,"/").status==302);
  assert(call(monitor,HTTP_GET,"/konsole-ws").status==401);
  assert(call(main,HTTP_GET,"/ws").status==401);
  assert(call(main,HTTP_POST,"/auth/login",nullptr,"incorrect").status==401);
  auto logged=call(main,HTTP_POST,"/auth/login",nullptr,"abcdefgh");assert(logged.status==200);
  std::string c=cookie(logged);
  assert(call(main,HTTP_GET,"/",c.c_str()).status==200);
  assert(call(main,HTTP_GET,"/ws",c.c_str()).status==200);
  assert(call(monitor,HTTP_GET,"/konsole-ws",c.c_str()).status==200);
  auto cross=makeRequest(HTTP_GET,"/ws",c.c_str());cross.headers["Origin"]={"http://evil.example"};main.dispatch(cross);assert(cross.status==403);
  assert(call(main,HTTP_POST,"/auth/password",c.c_str(),"newpassword").status==200);
  assert(socketsClosed==2 && call(main,HTTP_GET,"/ws",c.c_str()).status==401);
  for (int i=0;i<5;i++) assert(call(main,HTTP_POST,"/auth/login",nullptr,"abcdefgh").status==401);
  assert(call(main,HTTP_POST,"/auth/login",nullptr,"newpassword").status==429);
  mockTime+=60LL*1000000+1;
  logged=call(main,HTTP_POST,"/auth/login",nullptr,"newpassword");assert(logged.status==200);c=cookie(logged);
  mockTime+=SESSION_US+1;processWebAccess();assert(socketsClosed==3);
  assert(call(main,HTTP_GET,"/ws",c.c_str()).status==401);
  // Reload from the device-wide NVS record, and reject corrupt records.
  beginWebAccess(closed);assert(call(main,HTTP_GET,"/").status==302);
  logged=call(main,HTTP_POST,"/auth/login",nullptr,"newpassword");assert(logged.status==200);c=cookie(logged);
  mockWrite=false;assert(call(main,HTTP_POST,"/auth/password",c.c_str(),"otherpass").status==500);
  assert(call(main,HTTP_GET,"/").status==302);
  mockWrite=true;beginWebAccess(closed);
  logged=call(main,HTTP_POST,"/auth/login",nullptr,"newpassword");assert(logged.status==200);c=cookie(logged);
  assert(call(main,HTTP_POST,"/auth/password",c.c_str(),"").status==200);
  assert(call(main,HTTP_GET,"/").status==200);
  mockRecord[4]^=1;beginWebAccess(closed);
  assert(call(main,HTTP_GET,"/").status==302);
  assert(call(main,HTTP_POST,"/auth/login",nullptr,"newpassword").status==503);
  assert(call(main,HTTP_GET,"/api/status").status==200);
  mockOpen=false;beginWebAccess(closed);
  assert(call(main,HTTP_GET,"/").status==302);
  assert(call(main,HTTP_POST,"/auth/login",nullptr,"newpassword").status==503);
  std::cout<<"Web access tests passed\n";
}
