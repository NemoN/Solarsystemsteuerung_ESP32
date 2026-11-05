#pragma once
#include <vector>
#include <cstring>
extern std::vector<uint8_t> mockRecord;
extern bool mockOpen, mockWrite;
struct Preferences {
 bool begin(const char*,bool){return mockOpen;} void end(){}
 bool isKey(const char*){return !mockRecord.empty();}
 size_t putBytes(const char*,const void* p,size_t n){if(!mockWrite)return 0;mockRecord.assign((const uint8_t*)p,(const uint8_t*)p+n);return n;}
 size_t getBytesLength(const char*){return mockRecord.size();}
 size_t getBytes(const char*,void* p,size_t n){if(n!=mockRecord.size())return 0;memcpy(p,mockRecord.data(),n);return n;}
};
