#include "AutomaticControl.h"
#include <math.h>

bool AutomaticControl::validChargeTarget(float target) {
  return isfinite(target) && target >= MIN_CHARGE_TARGET && target <= MAX_CHARGE_TARGET;
}

bool AutomaticControl::validThresholds(const AutomaticThresholds& t) {
  return isfinite(t.differenceStart) &&
    t.differenceStart >= AutomaticThresholds::MIN_DIFFERENCE_START &&
    t.differenceStart <= AutomaticThresholds::MAX_DIFFERENCE_START;
}

bool AutomaticControl::readSavedThresholds(const float saved[5], AutomaticThresholds& t) {
  const AutomaticThresholds candidate = {saved[1]};
  if (!validThresholds(candidate)) return false;
  t = candidate;
  return true;
}

TransferDemand AutomaticControl::update(bool enabled, bool allowCharging, bool combiValid,
    float combi, bool bufferValid, float buffer, float target, const AutomaticThresholds& t) {
  if (!enabled || !combiValid || !bufferValid || !isfinite(combi) || !isfinite(buffer) ||
      !validThresholds(t)) {
    reset();
    return demand_;
  }
  if (demand_ == TransferDemand::CombiToBuffer &&
      (!allowCharging || combi <= buffer)) reset();
  if (demand_ == TransferDemand::BufferToCombi && buffer <= combi) reset();
  if (demand_ == TransferDemand::Off) {
    // Die Mindesttemperatur gilt ausschließlich für den Start der Einlagerung.
    if (allowCharging && validChargeTarget(target) && combi >= target && combi - buffer >= t.differenceStart)
      demand_ = TransferDemand::CombiToBuffer;
    else if (buffer - combi >= t.differenceStart)
      demand_ = TransferDemand::BufferToCombi;
  }
  return demand_;
}
