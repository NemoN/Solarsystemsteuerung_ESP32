#pragma once
#include <stdint.h>

enum class TransferDemand : uint8_t { Off, CombiToBuffer, BufferToCombi };

struct AutomaticThresholds {
  float targetOffset;  // Startabstand zum Sollwert
  float differenceStart;
  float differenceStop;
};

// Zustandsbehaftete Temperaturregelung; keine GPIO- oder Zeitabhängigkeit.
class AutomaticControl {
public:
  static bool validThresholds(const AutomaticThresholds& thresholds);
  TransferDemand update(bool enabled, bool allowCharging, bool combiValid,
                        float combi, bool bufferValid, float buffer,
                        float target, const AutomaticThresholds& thresholds);
  void reset() { demand_ = TransferDemand::Off; }
  TransferDemand demand() const { return demand_; }
private:
  TransferDemand demand_ = TransferDemand::Off;
};
