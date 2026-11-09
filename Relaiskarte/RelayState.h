#pragma once
#include <stdint.h>

// Ein Schaltauftrag wird zuerst dauerhaft gespeichert und dann ausgeführt.
class RelayState {
public:
  uint8_t mask() const { return mask_; }
  bool restore(unsigned value) {
    if (value > 15) return false;
    mask_ = static_cast<uint8_t>(value);
    return true;
  }
  template<typename Save, typename Apply>
  bool set(unsigned index, bool on, Save save, Apply apply) {
    if (index >= 4) return false;
    const uint8_t bit = 1U << index;
    const uint8_t next = on ? (mask_ | bit) : (mask_ & ~bit);
    if (next == mask_) return true;
    if (!save(next)) return false;
    mask_ = next;
    apply(index, on);
    return true;
  }
private:
  uint8_t mask_ = 0;
};
