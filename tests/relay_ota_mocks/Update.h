#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
constexpr int U_FLASH=0;
struct MockUpdate {
 bool running=false, finished=false, failWrite=false, failEnd=false; size_t expected=0; unsigned writes=0, aborts=0;
 std::vector<uint8_t> image;
 bool begin(size_t n,int) { if(running)return false; expected=n; image.clear(); running=true; finished=false; return true; }
 bool isRunning() const { return running; }
 void abort() { running=false; ++aborts; }
 size_t write(const uint8_t* p,size_t n) { if(failWrite)return 0; image.insert(image.end(),p,p+n);++writes;return n; }
 bool end() { if(failEnd||image.size()!=expected)return false; running=false;finished=true;return true; }
};
extern MockUpdate Update;
