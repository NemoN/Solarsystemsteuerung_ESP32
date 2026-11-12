#include "ArchiveHistory.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
  constexpr uint32_t now = 1700000000;
  const auto ms = [](uint32_t seconds) { return std::to_string(uint64_t(seconds) * 1000); };
  const std::string json = "{\"interval_seconds\":900,\"series\":["
    "{\"id\":4,\"points\":[{\"x\":" + ms(now - 1800) + ",\"y\":27.55},"
    "{\"x\":" + ms(now - 900) + ",\"y\":null}]},"
    "{\"id\":0,\"points\":[{\"x\":" + ms(now - 21601) + ",\"y\":21},"
    "{\"x\":" + ms(now - 21600) + ",\"y\":22},"
    "{\"x\":" + ms(now + 1) + ",\"y\":23}]},"
    "{\"id\":99,\"points\":[]}]}";
  ArchiveHistory archive;
  assert(archive.parse(json.c_str(), now));
  assert(archive.interval == 900 && archive.sensors[0].size == 1);
  assert(archive.sensors[0].points[0].time == now - 21600);
  assert(archive.sensors[0].points[0].value == 220);
  assert(!archive.sensors[1].present && archive.sensors[4].size == 2);
  assert(archive.sensors[4].points[0].value == 276);
  assert(archive.sensors[4].points[1].value == ArchiveHistory::MISSING);
  assert(!archive.parse("{\"interval_seconds\":900,\"series\":[{\"id\":0,\"points\":[]},{\"id\":0,\"points\":[]}]}", now));
  assert(!archive.parse("{\"interval_seconds\":900,\"series\":[]} trailing", now));
  assert(!archive.parse("{\"interval_seconds\":0,\"series\":[]}", now));
  const std::string backwards = "{\"interval_seconds\":900,\"series\":[{\"id\":0,\"points\":[{\"x\":" + ms(now) + ",\"y\":1},{\"x\":" + ms(now - 1) + ",\"y\":2}]}]}";
  assert(!archive.parse(backwards.c_str(), now));
  const std::string badValue = "{\"interval_seconds\":900,\"series\":[{\"id\":0,\"points\":[{\"x\":" + ms(now) + ",\"y\":851}]}]}";
  assert(!archive.parse(badValue.c_str(), now));
  std::string tooMany = "{\"interval_seconds\":60,\"series\":[{\"id\":0,\"points\":[";
  for (uint32_t i = 0; i <= ArchiveHistory::MAX_POINTS; ++i) {
    if (i) tooMany += ',';
    tooMany += "{\"x\":" + ms(now - 12000 + i * 60) + ",\"y\":1}";
  }
  tooMany += "]}]}";
  assert(!archive.parse(tooMany.c_str(), now));
  assert(archive.sensors[4].size == 2); // A failed parse never replaces a good snapshot.
  std::puts("PASS: Archivdaten: Sensor-IDs, UTC, Grenzen, Lücken und ungültige Antworten.");
}
