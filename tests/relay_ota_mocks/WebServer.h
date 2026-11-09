#pragma once
#include "Arduino.h"
#include <map>
struct WebServer {
 std::map<std::string,String> values; int code=0; String response;
 String arg(const char* key) { auto i=values.find(key); return i==values.end()?String():i->second; }
 int args() { return static_cast<int>(values.size()); }
 void send(int c,const char*,const String& r) { code=c; response=r; }
 void sendHeader(const char*,const char*) {}
};
