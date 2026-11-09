#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Striktes Antwortformat der eigenen Relaiskarte: RLY1:<0..15>.
inline bool parseRemoteRelayState(const char* data, size_t size, uint8_t& mask) {
  if (!data || size < 6 || size > 7 || memcmp(data, "RLY1:", 5) != 0) return false;
  if (data[5] < '0' || data[5] > '9') return false;
  unsigned value = data[5] - '0';
  if (size == 7) {
    if (data[5] == '0' || data[6] < '0' || data[6] > '9') return false;
    value = value * 10 + data[6] - '0';
  }
  if (value > 15) return false;
  mask = static_cast<uint8_t>(value);
  return true;
}
