#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct RelayDiagnostics {
  uint32_t uptime;
  int32_t rssi;
  uint8_t resetReason, watchdogState, watchdogRestarts, otaError;
  bool storageHealthy;
};

inline bool parseRelayDiagnostics(const char* data, size_t size, RelayDiagnostics& out) {
  const char prefix[] = "RLYDIAG1:";
  if (!data || size <= sizeof(prefix)-1 || size > 100 || memcmp(data, prefix, sizeof(prefix)-1)) return false;
  size_t pos = sizeof(prefix)-1;
  uint32_t values[7] = {};
  for (unsigned field = 0; field < 7; ++field) {
    const bool negative = field == 1 && pos < size && data[pos] == '-';
    if (negative) ++pos;
    if (pos >= size || data[pos] < '0' || data[pos] > '9') return false;
    uint64_t value = 0;
    do {
      value = value * 10 + unsigned(data[pos++] - '0');
      if (value > UINT32_MAX) return false;
    } while (pos < size && data[pos] >= '0' && data[pos] <= '9');
    if (field == 1 && ((!negative && value != 0) || value > 127)) return false;
    values[field] = static_cast<uint32_t>(value);
    if (field < 6) { if (pos >= size || data[pos++] != ':') return false; }
  }
  if (pos != size || values[2] > 255 || values[3] > 3 || values[4] > 2 || values[5] > 5 || values[6] > 1) return false;
  RelayDiagnostics parsed = {values[0], -static_cast<int32_t>(values[1]), static_cast<uint8_t>(values[2]),
    static_cast<uint8_t>(values[3]), static_cast<uint8_t>(values[4]), static_cast<uint8_t>(values[5]), values[6] != 0};
  out = parsed; return true;
}

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
