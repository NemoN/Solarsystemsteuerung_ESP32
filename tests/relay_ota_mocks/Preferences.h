#pragma once
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <vector>
extern std::map<std::string,std::vector<unsigned char>> mockPreferences;
#ifdef MOCK_PREF_FAILURE
extern int mockPreferenceFailures;
#endif
class Preferences {
 std::string ns_;
public:
 bool begin(const char* ns,bool){ns_=ns;return true;}
 size_t getBytesLength(const char* key){auto i=mockPreferences.find(ns_+":"+key);return i==mockPreferences.end()?0:i->second.size();}
 size_t getBytes(const char* key,void* out,size_t n){auto i=mockPreferences.find(ns_+":"+key);if(i==mockPreferences.end()||i->second.size()>n)return 0;std::memcpy(out,i->second.data(),i->second.size());return i->second.size();}
 size_t putBytes(const char* key,const void* in,size_t n){
#ifdef MOCK_PREF_FAILURE
  if(mockPreferenceFailures>0){--mockPreferenceFailures;return 0;}
#endif
  auto* p=static_cast<const unsigned char*>(in);mockPreferences[ns_+":"+key]=std::vector<unsigned char>(p,p+n);return n;
 }
};
