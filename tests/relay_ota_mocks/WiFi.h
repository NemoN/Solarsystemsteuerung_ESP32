#pragma once
#include "Arduino.h"
#include <cstdio>
#include <vector>
constexpr int WL_CONNECTED=3;
struct IPAddress {
 uint8_t b[4]={0,0,0,0};
 IPAddress()=default;
 IPAddress(uint8_t a,uint8_t c,uint8_t d,uint8_t e){b[0]=a;b[1]=c;b[2]=d;b[3]=e;}
 bool fromString(const String& s){return fromString(s.c_str());}
 bool fromString(const char* s){unsigned a,c,d,e;char tail;int n=std::sscanf(s,"%u.%u.%u.%u%c",&a,&c,&d,&e,&tail);if(n!=4||a>255||c>255||d>255||e>255)return false;b[0]=a;b[1]=c;b[2]=d;b[3]=e;return true;}
 String toString()const{char text[16];std::snprintf(text,sizeof(text),"%u.%u.%u.%u",b[0],b[1],b[2],b[3]);return String(text);}
 uint8_t operator[](int i)const{return b[i];}
 bool operator==(const IPAddress& o)const{return std::memcmp(b,o.b,4)==0;}
 bool operator!=(const IPAddress& o)const{return !(*this==o);}
};
constexpr int WIFI_AP=2;
struct WiFiClass {
 int state=WL_CONNECTED,mode=1,stations=0;bool apStartOk=true;IPAddress apIp=IPAddress(192,168,4,1);String apSsid,apPassword;
 int status()const{return state;} int getMode()const{return mode;} int softAPgetStationNum()const{return stations;}
 IPAddress localIP()const{return IPAddress(192,168,1,2);} IPAddress subnetMask()const{return IPAddress(255,255,255,0);} IPAddress softAPIP()const{return apIp;}
 bool enableAP(bool on){if(on)mode|=WIFI_AP;else mode&=~WIFI_AP;return true;}
 bool softAPConfig(IPAddress ip,IPAddress,IPAddress){apIp=ip;return true;}
 bool softAP(const char* name,const char* password,int,bool,int){apSsid=name;apPassword=password;return apStartOk;}
 int channel()const{return 6;}
};
extern WiFiClass WiFi;
extern std::vector<uint8_t> mockGithubImage;
extern size_t mockGithubOffset, mockGithubStopAfter;
class WiFiClient {
public:
 int available() { return mockGithubOffset < mockGithubImage.size() && mockGithubOffset < mockGithubStopAfter ? int(std::min(size_t(1024), std::min(mockGithubImage.size() - mockGithubOffset, mockGithubStopAfter - mockGithubOffset))) : 0; }
 int read(uint8_t* dest,size_t count) { size_t n=std::min(count,size_t(available())); if(!n)return 0;std::memcpy(dest,mockGithubImage.data()+mockGithubOffset,n);mockGithubOffset+=n;return static_cast<int>(n); }
};
