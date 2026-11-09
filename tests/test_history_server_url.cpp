#include "../Backload/HistoryServerUrl.h"
#include <cassert>
#include <cstring>
#include <string>

int main() {
  for (const char* url : {"", "http://192.168.1.10:8088/", "https://history.example.org/archive?x=1#chart", "http://[::1]:8088/"})
    assert(HistoryServerUrl::valid(url, strlen(url)));
  for (const char* url : {"javascript:alert(1)", "//example.org", "http://", "http://:8088", "http://user:pass@example.org/", "http://host:99999", "http://host:", "http://host:0", "http://host\\@attacker/", "http://host/a b", "http://host/\n"})
    assert(!HistoryServerUrl::valid(url, strlen(url)));
  const char embedded[] = {'h','t','t','p',':','/','/','a','\0','b','\0'};
  assert(!HistoryServerUrl::valid(embedded, 10));
  const std::string tooLong = "http://host/" + std::string(HistoryServerUrl::MAX_LENGTH, 'a');
  assert(!HistoryServerUrl::valid(tooLong.c_str(), tooLong.size()));
}
