#include "AutomaticControl.h"
#include <math.h>

bool AutomaticControl::validThresholds(const AutomaticThresholds& t) {
  return isfinite(t.targetOffset) && isfinite(t.differenceStart) &&
    isfinite(t.differenceStop) && t.targetOffset >= 0.1f && t.targetOffset <= 20.0f &&
    t.differenceStart >= 0.1f && t.differenceStart <= 30.0f &&
    t.differenceStop >= 0.0f && t.differenceStop < t.differenceStart;
}

TransferDemand AutomaticControl::update(bool enabled, bool allowCharging, bool combiValid,
    float combi, bool bufferValid, float buffer, float target, const AutomaticThresholds& t) {
  if (!enabled || !combiValid || !bufferValid || !isfinite(combi) || !isfinite(buffer) ||
      !isfinite(target) || !validThresholds(t)) {
    reset();
    return demand_;
  }
  if (demand_ == TransferDemand::CombiToBuffer &&
      (!allowCharging || combi <= target || combi - buffer <= t.differenceStop)) reset();
  if (demand_ == TransferDemand::BufferToCombi &&
      (combi >= target || buffer - combi <= t.differenceStop)) reset();
  if (demand_ == TransferDemand::Off) {
    if (allowCharging && combi >= target + t.targetOffset && combi - buffer >= t.differenceStart)
      demand_ = TransferDemand::CombiToBuffer;
    else if (combi <= target - t.targetOffset && buffer - combi >= t.differenceStart)
      demand_ = TransferDemand::BufferToCombi;
  }
  return demand_;
}
