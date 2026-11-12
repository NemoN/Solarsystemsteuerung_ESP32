#include "FirmwareRelease.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>

static std::string release(const char* version, const std::string& extra = "") {
  return std::string("{\"tag_name\":\"") + version + "\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"Backload.ino.bin\",\"size\":1200000,\"digest\":\"sha256:" + std::string(64,'a') + "\",\"browser_download_url\":\"https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/" + version + "/Backload.ino.bin\"}]" + extra + "}";
}

int main() {
  int cmp = 0;
  assert(FirmwareRelease::compare("v3.3.1", "3.3.0", cmp) && cmp > 0);
  assert(FirmwareRelease::compare("v3.3.0-mit-ap", "3.3.0-ohne-ap", cmp) && cmp == 0);
  assert(FirmwareRelease::compare("3.2.99", "3.3.0", cmp) && cmp < 0);
  assert(!FirmwareRelease::compare("v3.4.0-beta", "3.3.0", cmp));
  assert(!FirmwareRelease::compare("v4294967296.0.0", "3.3.0", cmp));
  assert(!FirmwareRelease::checkDue(0, 0));
  assert(FirmwareRelease::checkDue(1704067200, 0));
  assert(!FirmwareRelease::checkDue(1704153599, 1704067200));
  assert(FirmwareRelease::checkDue(1704153600, 1704067200));
  assert(FirmwareRelease::checkDue(1704067200, 1704153600));
  char tag[32] = {};
  assert(FirmwareRelease::parseLatest(release("v3.4.0").c_str(), tag, sizeof(tag)));
  assert(!strcmp(tag, "v3.4.0"));
  const auto invalid = [&](std::string text, const std::string& from, const std::string& to) {
    const auto pos = text.find(from); assert(pos != std::string::npos); text.replace(pos, from.size(), to);
    return FirmwareRelease::parseLatest(text.c_str(), tag, sizeof(tag));
  };
  assert(!invalid(release("v3.4.0"), "\"prerelease\":false", "\"prerelease\":true"));
  assert(!invalid(release("v3.4.0"), "\"draft\":false", "\"draft\":true"));
  assert(!invalid(release("v3.4.0"), "Backload.ino.bin", "Andere.bin"));
  assert(!invalid(release("v3.4.0"), "sha256:", "sha512:"));
  assert(!invalid(release("v3.4.0"), "\"size\":1200000", "\"size\":4000000"));
  assert(!invalid(release("v3.4.0"), "https://github.com/", "https://example.org/"));
  assert(!FirmwareRelease::parseLatest((release("v3.4.0") + "garbage").c_str(), tag, sizeof(tag)));
  assert(!strcmp(tag, "v3.4.0"));
  std::puts("PASS: stabile Releases, Versionsvergleich, Prüfintervall und Firmware-Artefakt.");
}
