#pragma once
#include <math.h>

// Temperaturdifferenzregelung ohne Hardwarezugriff; bei Fehler immer AUS.
class SolarControl {
 public:
  static constexpr float MIN_START = 3.0f;
  static constexpr float MAX_START = 10.0f;
  static constexpr float DEFAULT_START = 5.0f;
  static constexpr float STOP = 1.0f;

  static bool validStart(float value) {
    return isfinite(value) && value >= MIN_START && value <= MAX_START &&
      fabsf(value * 10.0f - roundf(value * 10.0f)) < 0.0001f;
  }
  bool update(bool enabled, bool panelValid, float panel, bool tankValid, float tank, float start) {
    if (!enabled || !panelValid || !tankValid || !isfinite(panel) || !isfinite(tank) || !validStart(start)) {
      on_ = false;
    } else if (on_) {
      if (panel - tank <= STOP) on_ = false;
    } else if (panel - tank >= start) {
      on_ = true;
    }
    return on_;
  }
  bool on() const { return on_; }
 private:
  bool on_ = false;
};
