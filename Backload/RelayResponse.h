#pragma once
#include <stddef.h>
#include <stdint.h>

// HTTPClient::getString() kann bei fehlendem Body trotz Timeout unbegrenzt warten.
// Die kurze Antwort muss innerhalb einer Gesamtfrist vollständig eintreffen.
template<typename Stream, typename Clock, typename Wait>
bool readRelayResponse(Stream& stream, size_t expected, char* out, size_t capacity,
                       Clock clock, Wait wait, uint32_t timeoutMs = 2500) {
  if (!out || !capacity || !expected || expected >= capacity) return false;
  size_t received = 0;
  const uint32_t start = clock();
  while (received < expected) {
    if (uint32_t(clock() - start) >= timeoutMs) return false;
    const int available = stream.available();
    if (available < 0) return false;
    if (available > 0) {
      const size_t remaining = expected - received;
      const size_t count = size_t(available) < remaining ? size_t(available) : remaining;
      const int n = stream.read(reinterpret_cast<uint8_t*>(out + received), count);
      if (n < 0 || size_t(n) > count) return false;
      received += size_t(n);
      if (n) continue;
    }
    wait();
  }
  out[received] = '\0';
  return true;
}
