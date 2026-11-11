#include "../Backload/RelayResponse.h"
#include <cassert>
#include <cstring>
#include <string>

struct Stream {
  std::string data;
  size_t offset = 0;
  int available() { return int(data.size() - offset); }
  int read(uint8_t* out, size_t count) {
    std::memcpy(out, data.data() + offset, count);
    offset += count;
    return int(count);
  }
};

int main() {
  uint32_t now = UINT32_MAX - 1000;
  auto clock = [&]() { return now; };
  auto wait = [&]() { now += 10; };
  char response[8] = {};
  Stream complete; complete.data = "RLY1:7";
  assert(readRelayResponse(complete, 6, response, sizeof(response), clock, wait));
  assert(!std::strcmp(response, "RLY1:7"));

  Stream incomplete; incomplete.data = "RLY1:";
  const uint32_t start = now;
  assert(!readRelayResponse(incomplete, 6, response, sizeof(response), clock, wait));
  assert(uint32_t(now - start) == 2500); // Frist gilt auch über millis()-Überlauf.
  assert(!readRelayResponse(incomplete, 8, response, sizeof(response), clock, wait));
  assert(!readRelayResponse(incomplete, 0, response, sizeof(response), clock, wait));
}
