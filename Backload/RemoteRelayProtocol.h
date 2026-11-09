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
inline bool parseRemoteRelayVersion(const char* data, size_t size, char out[32]) {
  const char prefix[] = "RLYVER1:"; const size_t start = sizeof(prefix)-1;
  if (!data || size <= start || size >= start+32 || memcmp(data, prefix, start) || data[start] < '0' || data[start] > '9') return false;
  for (size_t i = start; i < size; ++i) {
    const char c = data[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '+')) return false;
  }
  memcpy(out, data+start, size-start); out[size-start] = '\0'; return true;
}
