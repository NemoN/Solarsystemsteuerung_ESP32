#include "../Backload/RemoteRelayProtocol.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
  uint8_t mask = 99;
  for (unsigned i = 0; i < 16; ++i) {
    char msg[16]; std::snprintf(msg, sizeof(msg), "RLY1:%u", i);
    assert(parseRemoteRelayState(msg, std::strlen(msg), mask) && mask == i);
  }
  const char* invalid[] = {"", "RLY1:", "RLY1:-1", "RLY1:16", "RLY1:01", "RLY1:1x", "RLY1:1\n", "other:1", "RLY1:123"};
  for (const char* text : invalid) {
    mask = 77;
    assert(!parseRemoteRelayState(text, std::strlen(text), mask) && mask == 77);
  }
  assert(!parseRemoteRelayState(nullptr, 6, mask));
  char version[32] = {};
  const char* good = "RLYVER1:2.7.0";
  assert(parseRemoteRelayVersion(good, std::strlen(good), version) && !std::strcmp(version,"2.7.0"));
  const char* invalidVersions[] = {"", "RLYVER1:", "RLYVER1:two", "RLYVER1:2.7.0\n", "RLY1:2.7.0", "RLYVER1:2<script>"};
  for (const char* value : invalidVersions) assert(!parseRemoteRelayVersion(value, std::strlen(value), version));
  assert(!parseRemoteRelayVersion(nullptr, 9, version));
  std::puts("PASS: Relaisprotokoll akzeptiert nur gültige Zustandsantworten.");
}
