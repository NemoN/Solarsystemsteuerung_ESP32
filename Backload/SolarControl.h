#pragma once
#include <math.h>
#include <stdint.h>

// Nur die zuletzt gültige Panelmessung merken, niemals einen Fehler als Messung.
class SolarPanelGrace {
 public:
  static constexpr uint32_t MAX_AGE_MS = 60000;
  void record(float temperature, uint32_t now) {
    if (!isfinite(temperature)) return;
    temperature_ = temperature;
    lastValid_ = now;
    ready_ = true;
  }
  bool canHold(uint32_t now, bool pumpOn) const {
    return ready_ && pumpOn && uint32_t(now - lastValid_) <= MAX_AGE_MS;
  }
  float temperature() const { return temperature_; }
  bool hasReading() const { return ready_; }
 private:
  float temperature_ = 0;
  uint32_t lastValid_ = 0;
  bool ready_ = false;
};

// Temperaturdifferenzregelung und ausdrücklich gewählter Handbetrieb.
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
    if (!enabled) {
      manualOn_ = false;
      on_ = false;
    } else if (manual_) {
      on_ = manualOn_;
    } else if (!panelValid || !tankValid || !isfinite(panel) || !isfinite(tank) || !validStart(start)) {
      on_ = false;
    } else if (on_) {
      if (panel - tank <= STOP) on_ = false;
    } else if (panel - tank >= start) {
      on_ = true;
    }
    return on_;
  }
  bool on() const { return on_; }
  bool manual() const { return manual_; }
  bool manualOn() const { return manualOn_; }
  void setManual(bool manual) {
    if (manual_ == manual) return;
    manual_ = manual;
    manualOn_ = false;
    on_ = false;
  }
  bool setManualOn(bool on) {
    if (!manual_) return false;
    manualOn_ = on;
    return true;
  }
 private:
  bool on_ = false;
  bool manual_ = false;
  bool manualOn_ = false;
};
