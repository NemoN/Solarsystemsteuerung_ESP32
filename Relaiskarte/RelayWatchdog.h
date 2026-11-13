#pragma once
#include <stdint.h>

// Pure timing policy; storage and the actual restart are handled separately.
class RelayWatchdog {
public:
  enum class Action : uint8_t { None, Restart, LimitReached, StorageError, ClearCount };
  static constexpr uint32_t STALL_MS = 60000;
  static constexpr uint32_t HEALTHY_MS = 30000;
  static constexpr uint32_t STABLE_MS = 30UL * 60 * 1000;
  static constexpr uint8_t MAX_RESTARTS = 2;

  explicit RelayWatchdog(uint32_t now) : healthySince_(now) {}
  Action check(uint32_t now, uint32_t heartbeat, uint8_t restarts, bool storageReady) {
    const uint32_t elapsed = now - heartbeat;
    if (elapsed >= HEALTHY_MS) { healthySince_ = now; unhealthy_ = true; }
    else if (unhealthy_) { healthySince_ = now; unhealthy_ = false; }
    if (elapsed >= STALL_MS) {
      if (handled_) return Action::None;
      handled_ = true;
      if (!storageReady) return Action::StorageError;
      if (restarts >= MAX_RESTARTS) return Action::LimitReached;
      return Action::Restart;
    }
    handled_ = false;
    if (elapsed < HEALTHY_MS && restarts && storageReady && uint32_t(now - healthySince_) >= STABLE_MS) {
      healthySince_ = now;
      return Action::ClearCount;
    }
    return Action::None;
  }
private:
  uint32_t healthySince_;
  bool handled_ = false, unhealthy_ = false;
};
