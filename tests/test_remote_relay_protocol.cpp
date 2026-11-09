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
  std::puts("PASS: Relaisprotokoll akzeptiert nur gültige Zustandsantworten.");
}
