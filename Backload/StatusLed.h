#pragma once
#include <stdint.h>

// Zustandsauswahl und Animation ohne Hardwarezugriff oder Warteschleifen.
enum class StatusLedMode : uint8_t { Normal, Test, Manual, Updating, Fault };
struct StatusLedColor {
  uint8_t red, green, blue;
  bool operator==(const StatusLedColor& other) const {
    return red == other.red && green == other.green && blue == other.blue;
  }
};

class StatusLed {
 public:
  static StatusLedMode select(bool fault, bool updating, bool manual, bool test) {
    if (fault) return StatusLedMode::Fault;
    if (updating) return StatusLedMode::Updating;
    if (manual) return StatusLedMode::Manual;
    if (test) return StatusLedMode::Test;
    return StatusLedMode::Normal;
  }
  static const char* name(StatusLedMode mode) {
    switch (mode) {
      case StatusLedMode::Fault: return "fault";
      case StatusLedMode::Updating: return "updating";
      case StatusLedMode::Manual: return "manual";
      case StatusLedMode::Test: return "test";
      default: return "normal";
    }
  }
  bool update(uint32_t now, StatusLedMode next) {
    if (!started_ || next != mode_) {
      started_ = true;
      mode_ = next;
      since_ = now;
    }
    const uint32_t elapsed = now - since_;
    StatusLedColor color = {0, 0, 0};
    switch (mode_) {
      case StatusLedMode::Fault:
        if (elapsed % 1400U < 100U) color = {36, 24, 0};
        break;
      case StatusLedMode::Updating: {
        const uint32_t phase = elapsed % 2000U;
        const uint32_t triangle = phase <= 1000U ? phase : 2000U - phase;
        const uint8_t value = uint8_t(3U + 27U * triangle / 1000U);
        color = {value, value, value};
        break;
      }
      case StatusLedMode::Manual:
        if (elapsed % 1600U < 500U) color = {0, 0, 36};
        break;
      case StatusLedMode::Test:
        if (elapsed % 1600U < 250U) color = {24, 0, 36};
        break;
      case StatusLedMode::Normal:
        if (elapsed % 2000U < 150U) color = {0, 32, 0};
        break;
    }
    if (hasColor_ && color == color_) return false;
    color_ = color;
    hasColor_ = true;
    return true;
  }
  StatusLedMode mode() const { return mode_; }
  StatusLedColor color() const { return color_; }
 private:
  bool started_ = false, hasColor_ = false;
  uint32_t since_ = 0;
  StatusLedMode mode_ = StatusLedMode::Normal;
  StatusLedColor color_ = {0, 0, 0};
};
