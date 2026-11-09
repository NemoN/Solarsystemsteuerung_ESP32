#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <cstdio>
using std::min;
using std::size_t;
class String {
  std::string value_;
public:
  String() = default;
  String(const char* v):value_(v?v:""){}
  String(const std::string& v):value_(v){}
  template<class T, typename std::enable_if<std::is_integral<T>::value,int>::type = 0> String(T n):value_(std::to_string(n)){}
  size_t length() const { return value_.size(); }
  bool isEmpty() const { return value_.empty(); }
  void reserve(size_t n) { value_.reserve(n); }
  String& operator+=(char c) { value_ += c; return *this; }
  String& operator+=(const String& s) { value_ += s.value_; return *this; }
  bool startsWith(const String& s) const { return value_.compare(0,s.length(),s.value_)==0; }
  bool endsWith(const String& s) const { return value_.size()>=s.length() && value_.compare(value_.size()-s.length(),s.length(),s.value_)==0; }
  String substring(size_t i) const { return value_.substr(i); }
  String substring(size_t i,size_t end) const { return value_.substr(i,end-i); }
  int indexOf(char c,size_t start=0) const { const auto i=value_.find(c,start);return i==std::string::npos?-1:static_cast<int>(i); }
  char operator[](size_t i) const { return value_[i]; }
  const char* c_str() const { return value_.c_str(); }
  void toCharArray(char* out,size_t n) const { if(n) { std::strncpy(out,value_.c_str(),n-1); out[n-1]=0; } }
  bool operator==(const String& o) const { return value_==o.value_; }
  bool operator!=(const String& o) const { return !(*this==o); }
  friend String operator+(const String& a,const String& b) { return String(a.value_+b.value_); }
};
extern uint32_t mockMillis;
inline uint32_t millis() { return mockMillis; }
struct ESPClass { unsigned restarts=0; void restart() { ++restarts; } };
extern ESPClass ESP;

inline size_t strlcpy(char* dst,const char* src,size_t n){size_t len=std::strlen(src);if(n){size_t m=std::min(len,n-1);std::memcpy(dst,src,m);dst[m]=0;}return len;}
