#pragma once
#include <stddef.h>
#include <string.h>

namespace HistoryServerUrl {
constexpr size_t MAX_LENGTH = 255;

inline bool valid(const char* url, size_t length) {
  if (!url || strlen(url) != length || length > MAX_LENGTH) return false;
  if (length == 0) return true;
  size_t start = 0;
  if (strncmp(url, "http://", 7) == 0) start = 7;
  else if (strncmp(url, "https://", 8) == 0) start = 8;
  else return false;
  size_t end = start;
  while (end < length && url[end] != '/' && url[end] != '?' && url[end] != '#') ++end;
  if (start == end) return false;
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(url[i]);
    if (c <= 32 || c >= 127 || c == '\\' || c == '"' || c == '<' || c == '>') return false;
  }
  size_t port = end;
  if (url[start] == '[') {
    size_t close = start + 1;
    while (close < end && url[close] != ']') {
      const char c = url[close++];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == ':' || c == '.')) return false;
    }
    if (close == start + 1 || close == end) return false;
    if (close + 1 < end) {
      if (url[close + 1] != ':') return false;
      port = close + 2;
    }
  } else {
    size_t hostEnd = start;
    while (hostEnd < end && url[hostEnd] != ':') {
      const char c = url[hostEnd++];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_')) return false;
    }
    if (hostEnd == start) return false;
    if (hostEnd < end) port = hostEnd + 1;
  }
  if (port < end) {
    unsigned value = 0;
    for (size_t i = port; i < end; ++i) {
      if (url[i] < '0' || url[i] > '9') return false;
      value = value * 10 + (url[i] - '0');
      if (value > 65535) return false;
    }
    if (!value) return false;
  } else if (end > start && url[end - 1] == ':') return false;
  return true;
}
}
