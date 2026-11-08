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
  if (!enabled || !combiValid || !bufferValid || !isfinite(combi) || !isfinite(buffer) ||
      !validThresholds(t) || combi < t.minimumCombi || buffer < t.minimumBuffer) {
    reset();
    return demand_;
  }
  if (demand_ == TransferDemand::CombiToBuffer &&
      (!allowCharging || combi - buffer <= STOP_DIFFERENCE)) reset();
  if (demand_ == TransferDemand::BufferToCombi && buffer - combi <= STOP_DIFFERENCE) reset();
  if (demand_ == TransferDemand::Off) {
    // Die separate Einlagerungs-Starttemperatur gilt nur beim Start.
    if (allowCharging && validChargeTarget(target) && combi >= target && combi - buffer >= t.differenceStart)
      demand_ = TransferDemand::CombiToBuffer;
    else if (buffer - combi >= t.differenceStart)
      demand_ = TransferDemand::BufferToCombi;
  }
  return demand_;
}
