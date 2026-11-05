#include "Hostname.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
  const auto check=[](const char* name) { return Hostname::valid(name, strlen(name)); };
  assert(check("backload-test"));
  assert(check("a"));
  assert(check("1-anlage-2"));
  assert(check("abcdefghijklmnopqrstuvwxyz12345")); // 31 bytes fit the ESP32 buffer
  assert(!check("abcdefghijklmnopqrstuvwxyz123456"));
  assert(!check(""));
  assert(!check("-anlage"));
  assert(!check("anlage-"));
  assert(!check("anlage.local"));
  assert(!check("anlage name"));
  assert(!check("Anlage")); // normalisiert die API vor der Prüfung
  assert(!Hostname::valid(nullptr, 1));
  const char embedded[] = {'a', '\0', 'b'};
  assert(!Hostname::valid(embedded, sizeof(embedded)));
  std::cout << "PASS: Hostname-Grenzen und erlaubte Zeichen.\n";
}
