#pragma once
#include <stddef.h>

// ESP32 Arduino 2.0.13 stores the DHCP hostname in a 32-byte buffer.
namespace Hostname {
constexpr size_t MAX_LENGTH = 31;
inline bool alphanumeric(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
inline bool valid(const char* name, size_t length) {
  if (!name || !length || length > MAX_LENGTH || !alphanumeric(name[0]) || !alphanumeric(name[length - 1])) return false;
  for (size_t i = 0; i < length; ++i) if (!alphanumeric(name[i]) && name[i] != '-') return false;
  return true;
}
}
