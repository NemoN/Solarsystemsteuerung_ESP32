#include "PumpControl.h"

void PumpControl::command(bool on, PumpDirection direction, uint32_t now) {
  requestedOn_ = on;
  requestedDirection_ = direction;
  update(now);
}

void PumpControl::update(uint32_t now) {
  if (!requestedOn_) on_ = false;

  // Nach dem Schalten der Richtung das Relais erst abfallen/anziehen lassen.
  if (phase_ == PumpPhase::Settling) {
    on_ = false;
    if (uint32_t(now - phaseSince_) < DIRECTION_SETTLE_MS) return;
    phase_ = PumpPhase::Ready;
  }

  if (requestedDirection_ != direction_) {
    on_ = false;
    if (phase_ != PumpPhase::Stopping) {
      phase_ = PumpPhase::Stopping;
      phaseSince_ = now;
    }
    if (uint32_t(now - phaseSince_) >= STOP_BEFORE_REVERSE_MS) {
      direction_ = requestedDirection_;
      phase_ = PumpPhase::Settling;
      phaseSince_ = now;
    }
    return;
  }

  // Ein vor dem Richtungswechsel zurückgenommener Auftrag startet nichts um.
  phase_ = PumpPhase::Ready;
  on_ = requestedOn_;
}

uint32_t PumpControl::remainingMs(uint32_t now) const {
  const uint32_t elapsed = uint32_t(now - phaseSince_);
  if (phase_ == PumpPhase::Stopping) {
    return (elapsed < STOP_BEFORE_REVERSE_MS ? STOP_BEFORE_REVERSE_MS - elapsed : 0)
      + DIRECTION_SETTLE_MS;
  }
  if (phase_ == PumpPhase::Settling) {
    return elapsed < DIRECTION_SETTLE_MS ? DIRECTION_SETTLE_MS - elapsed : 0;
  }
  return 0;
}
