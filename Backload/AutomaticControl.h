#pragma once
#include <stdint.h>

enum class TransferDemand : uint8_t { Off, CombiToBuffer, BufferToCombi };

struct AutomaticThresholds {
  static constexpr float MIN_DIFFERENCE_START = 5.0f;
  static constexpr float MAX_DIFFERENCE_START = 10.0f;
  float differenceStart;
};

// Zustandsbehaftete Temperaturregelung; keine GPIO- oder Zeitabhängigkeit.
// allowCharging sperrt ausschließlich die Einlagerung. Die Rückladung bleibt
// unabhängig davon aktiv. Relais 4 sperrt die Einlagerung zusätzlich im Handbetrieb.
class AutomaticControl {
public:
  static constexpr float MIN_CHARGE_TARGET = 65.0f;
  static constexpr float MAX_CHARGE_TARGET = 70.0f;
  static bool validChargeTarget(float target);
  static bool validThresholds(const AutomaticThresholds& thresholds);
  // Historisches auto1-Format: Nur Slot 1 ist weiterhin wirksam.
  static bool readSavedThresholds(const float saved[5], AutomaticThresholds& thresholds);
  TransferDemand update(bool enabled, bool allowCharging, bool combiValid,
                        float combi, bool bufferValid, float buffer,
                        float target, const AutomaticThresholds& thresholds);
  void reset() { demand_ = TransferDemand::Off; }
  TransferDemand demand() const { return demand_; }
private:
  TransferDemand demand_ = TransferDemand::Off;
};
