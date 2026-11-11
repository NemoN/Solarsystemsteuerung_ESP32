#pragma once
#include "WiFi.h"
constexpr int HTTP_CODE_OK=200, HTTPC_STRICT_FOLLOW_REDIRECTS=1;
int mockHttp(const String& url,const String* body,String& response);
extern int mockGithubAttempts, mockGithubFailures, mockGithubCode;
extern long mockGithubReportedSize;
extern long mockRelayReportedSize;
struct HTTPClient {
 String url,response; int code=0; bool github=false; WiFiClient stream;
 void setConnectTimeout(int){} void setTimeout(int){} void setReuse(bool){}
 void setFollowRedirects(int){} void setRedirectLimit(int){} void setUserAgent(const char*){}
 bool begin(WiFiClient&,const String& u){url=u;github=u.startsWith("https://github.com/");return true;}
 void addHeader(const char*,const char*){}
 int POST(const String& body){code=mockHttp(url,&body,response);stream.setRelayResponse(response);return code;}
 int GET(){if(github){++mockGithubAttempts;mockGithubOffset=0;if(mockGithubFailures>0){--mockGithubFailures;return code=-1;}return code=mockGithubCode;}code=mockHttp(url,nullptr,response);stream.setRelayResponse(response);return code;}
 int getSize(){return github?(mockGithubReportedSize>=0?int(mockGithubReportedSize):int(mockGithubImage.size())):(mockRelayReportedSize>=0?int(mockRelayReportedSize):int(response.length()));}
 String getString(){return response;}
 WiFiClient* getStreamPtr(){return &stream;}
 bool connected(){return mockGithubOffset<mockGithubStopAfter;}
 void end(){}
};
