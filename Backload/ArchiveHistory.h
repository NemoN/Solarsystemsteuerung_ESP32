#pragma once
#include <stddef.h>
#include <stdint.h>

// A compact, separate snapshot: the archive's quarter-hour readings are not
// invented minute readings and never enter the local control/history buffer.
class ArchiveHistory {
 public:
  static constexpr size_t SENSOR_COUNT = 5;
  static constexpr size_t MAX_POINTS = 64;
  static constexpr int16_t MISSING = 0x7f7f;
  struct Point { uint32_t time; int16_t value; };
  struct Series { Point points[MAX_POINTS]; size_t size = 0; bool present = false; };

  bool parse(const char* json, uint32_t now);
  uint32_t interval = 0;
  Series sensors[SENSOR_COUNT];
};
