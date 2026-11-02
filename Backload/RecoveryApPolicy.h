#pragma once
#include <stdint.h>

// Testvariante: AP erst nach stabiler STA-Verbindung abschalten und bei
// längerem Verbindungsausfall oder einer Neueinrichtung wieder bereitstellen.
class RecoveryApPolicy {
 public:
  enum Action { Keep, Enable, Disable };
  explicit RecoveryApPolicy(uint32_t intervalMs) : interval(intervalMs) {}

  Action update(uint32_t now, bool connected, bool apActive,
                bool bootVerified, bool reconfiguring) {
    if (!initialized || connected != lastConnected) {
      initialized = true;
      lastConnected = connected;
      changedAt = now;
    }
    if (!bootVerified || reconfiguring) return apActive ? Keep : Enable;
    if (uint32_t(now - changedAt) < interval) return Keep;
    if (connected && apActive) return Disable;
    if (!connected && !apActive) return Enable;
    return Keep;
  }

 private:
  uint32_t interval, changedAt = 0;
  bool initialized = false, lastConnected = false;
};
