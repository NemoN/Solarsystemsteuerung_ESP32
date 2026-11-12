#include "FirmwareRelease.h"
#include <cJSON.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <strings.h>

namespace {
bool parseVersion(const char* text, uint32_t out[3]) {
  if (!text) return false;
  if (*text == 'v' || *text == 'V') ++text;
  for (int i = 0; i < 3; ++i) {
    if (*text < '0' || *text > '9') return false;
    uint32_t value = 0;
    do {
      const unsigned digit = unsigned(*text++ - '0');
      if (value > (UINT32_MAX - digit) / 10) return false;
      value = value * 10 + digit;
    } while (*text >= '0' && *text <= '9');
    out[i] = value;
    if (i < 2 && *text++ != '.') return false;
  }
  return !*text || !strcasecmp(text, "-mit-ap") || !strcasecmp(text, "-ohne-ap");
}
}

bool FirmwareRelease::compare(const char* left, const char* right, int& result) {
  uint32_t a[3], b[3];
  if (!parseVersion(left, a) || !parseVersion(right, b)) return false;
  result = 0;
  for (int i = 0; i < 3; ++i) {
    if (a[i] != b[i]) { result = a[i] > b[i] ? 1 : -1; break; }
  }
  return true;
}

bool FirmwareRelease::checkDue(uint32_t now, uint32_t lastAttempt) {
  return now >= 1704067200UL && (!lastAttempt || now < lastAttempt || now - lastAttempt >= 86400UL);
}

bool FirmwareRelease::parseLatest(const char* text, char* output, size_t capacity) {
  if (!text || !output || !capacity) return false;
  cJSON* root = cJSON_ParseWithOpts(text, nullptr, 1);
  if (!root) return false;
  const cJSON* tag = cJSON_GetObjectItemCaseSensitive(root, "tag_name");
  const cJSON* draft = cJSON_GetObjectItemCaseSensitive(root, "draft");
  const cJSON* prerelease = cJSON_GetObjectItemCaseSensitive(root, "prerelease");
  const cJSON* assets = cJSON_GetObjectItemCaseSensitive(root, "assets");
  uint32_t version[3];
  bool ok = cJSON_IsObject(root) && cJSON_IsString(tag) && parseVersion(tag->valuestring, version) &&
    strlen(tag->valuestring) < capacity && cJSON_IsFalse(draft) && cJSON_IsFalse(prerelease) && cJSON_IsArray(assets);
  bool found = false;
  const cJSON* asset = nullptr;
  cJSON_ArrayForEach(asset, assets) {
    if (!ok) break;
    const cJSON* name = cJSON_GetObjectItemCaseSensitive(asset, "name");
    if (!cJSON_IsString(name) || strcmp(name->valuestring, "Backload.ino.bin")) continue;
    const cJSON* size = cJSON_GetObjectItemCaseSensitive(asset, "size");
    const cJSON* digest = cJSON_GetObjectItemCaseSensitive(asset, "digest");
    const cJSON* url = cJSON_GetObjectItemCaseSensitive(asset, "browser_download_url");
    char expected[192];
    const int length = snprintf(expected, sizeof(expected),
      "https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/%s/Backload.ino.bin", tag->valuestring);
    if (!cJSON_IsNumber(size) || !std::isfinite(size->valuedouble) || floor(size->valuedouble) != size->valuedouble ||
        size->valuedouble < 1024 || size->valuedouble > 3145728 || !cJSON_IsString(digest) ||
        strlen(digest->valuestring) != 71 || strncmp(digest->valuestring, "sha256:", 7) ||
        !cJSON_IsString(url) || length < 0 || size_t(length) >= sizeof(expected) || strcmp(url->valuestring, expected)) continue;
    bool hex = true;
    for (size_t i = 7; i < 71; ++i) {
      const char ch = digest->valuestring[i];
      if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) { hex = false; break; }
    }
    if (hex) { found = true; break; }
  }
  if (ok && found) memcpy(output, tag->valuestring, strlen(tag->valuestring) + 1);
  cJSON_Delete(root);
  return ok && found;
}
