#pragma once

#include <stdint.h>

// Bit 0 = Montag, Bit 6 = Sonntag. Bei Tageswechsel zählt der Starttag.
struct NightQuiet {
  bool enabled = false;
  uint16_t start = 22 * 60;
  uint16_t end = 8 * 60;
  uint8_t days = 0x7f;

  bool valid() const { return start < 1440 && end < 1440 && start != end && days <= 0x7f; }
  bool active(int weekday, int minute) const {
    if (!enabled || !valid() || weekday < 0 || weekday > 6 || minute < 0 || minute >= 1440) return false;
    // tm_wday: Sonntag = 0; unsere Maske beginnt mit Montag.
    int day = (weekday + 6) % 7;
    if (start < end) return minute >= start && minute < end && (days & (1u << day));
    if (minute >= start) return days & (1u << day);
    if (minute < end) return days & (1u << ((day + 6) % 7));
    return false;
  }
};
