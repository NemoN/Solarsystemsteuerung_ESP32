#include "AutomaticControl.h"
#include <math.h>

bool AutomaticControl::validChargeTarget(float target) {
  return isfinite(target) && target >= MIN_CHARGE_TARGET && target <= MAX_CHARGE_TARGET;
}

bool AutomaticControl::validThresholds(const AutomaticThresholds& t) {
  return isfinite(t.differenceStart) &&
    t.differenceStart >= AutomaticThresholds::MIN_DIFFERENCE_START &&
    t.differenceStart <= AutomaticThresholds::MAX_DIFFERENCE_START &&
    isfinite(t.minimumCombi) && isfinite(t.minimumBuffer) &&
    t.minimumCombi >= AutomaticThresholds::MIN_TEMPERATURE &&
    t.minimumCombi <= AutomaticThresholds::MAX_TEMPERATURE &&
    t.minimumBuffer >= AutomaticThresholds::MIN_TEMPERATURE &&
    t.minimumBuffer <= AutomaticThresholds::MAX_TEMPERATURE;
}

bool AutomaticControl::readSavedThresholds(const float saved[5], AutomaticThresholds& t) {
  const bool current = saved[2] == MINIMUM_FORMAT_MARKER;
  const AutomaticThresholds candidate = {saved[1],
    current ? saved[0] : AutomaticThresholds::DEFAULT_MIN_TEMPERATURE,
    current ? saved[3] : AutomaticThresholds::DEFAULT_MIN_TEMPERATURE};
  if (!validThresholds(candidate)) return false;
  t = candidate;
  return true;
}

TransferDemand AutomaticControl::update(bool enabled, bool allowCharging, bool combiValid,
    float combi, bool bufferValid, float buffer, float target, const AutomaticThresholds& t) {
  return update(enabled, allowCharging, combiValid, combi, combiValid, combi,
                bufferValid, buffer, target, t);
}

TransferDemand AutomaticControl::update(bool enabled, bool allowCharging, bool topValid, float top,
    bool middleValid, float middle, bool bufferValid, float buffer, float target, const AutomaticThresholds& t) {
  if (!enabled || !middleValid || !bufferValid || !isfinite(middle) || !isfinite(buffer) ||
      !validThresholds(t) || middle < t.minimumCombi || buffer < t.minimumBuffer) {
    reset();
    return demand_;
  }
  if (demand_ == TransferDemand::CombiToBuffer &&
      (!allowCharging || !topValid || !isfinite(top) || top - buffer <= STOP_DIFFERENCE)) reset();
  if (demand_ == TransferDemand::BufferToCombi && buffer - middle <= STOP_DIFFERENCE) reset();
  if (demand_ == TransferDemand::Off) {
    // Die separate Einlagerungs-Starttemperatur gilt nur beim Start.
    if (allowCharging && topValid && isfinite(top) && validChargeTarget(target) && top >= target && top - buffer >= t.differenceStart)
      demand_ = TransferDemand::CombiToBuffer;
    else if (buffer - middle >= t.differenceStart)
      demand_ = TransferDemand::BufferToCombi;
  }
  return demand_;
}
