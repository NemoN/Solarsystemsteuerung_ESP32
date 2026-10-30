#pragma once

#include <stdint.h>

enum class PumpDirection : uint8_t { CombiToBuffer, BufferToCombi };
enum class PumpPhase : uint8_t { Ready, Stopping, Settling };

// GPIO-unabhängig: dieselbe Umschaltfolge gilt für Hand- und Automatikbetrieb.
class PumpControl {
public:
  static constexpr uint32_t STOP_BEFORE_REVERSE_MS = 2000;
  static constexpr uint32_t DIRECTION_SETTLE_MS = 100;

  explicit PumpControl(PumpDirection initialDirection = PumpDirection::CombiToBuffer)
    : direction_(initialDirection), requestedDirection_(initialDirection) {}

  void command(bool on, PumpDirection direction, uint32_t now);
  void update(uint32_t now);

  bool on() const { return on_; }
  PumpDirection direction() const { return direction_; }
  bool requestedOn() const { return requestedOn_; }
  PumpDirection requestedDirection() const { return requestedDirection_; }
  PumpPhase phase() const { return phase_; }
  bool switching() const { return phase_ != PumpPhase::Ready; }
  uint32_t remainingMs(uint32_t now) const;

private:
  bool on_ = false;
  bool requestedOn_ = false;
  PumpDirection direction_ = PumpDirection::CombiToBuffer;
  PumpDirection requestedDirection_ = PumpDirection::CombiToBuffer;
  PumpPhase phase_ = PumpPhase::Ready;
  uint32_t phaseSince_ = 0;
};
