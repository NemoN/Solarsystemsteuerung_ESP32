#pragma once
#include <stdint.h>

enum class TransferDemand : uint8_t { Off, CombiToBuffer, BufferToCombi };

struct AutomaticThresholds {
  static constexpr float MIN_DIFFERENCE_START = 5.0f;
  static constexpr float MAX_DIFFERENCE_START = 10.0f;
  static constexpr float MIN_TEMPERATURE = 0.0f;
  static constexpr float MAX_TEMPERATURE = 80.0f;
  static constexpr float DEFAULT_MIN_TEMPERATURE = 35.0f;
  float differenceStart;
  float minimumCombi;
  float minimumBuffer;
  AutomaticThresholds(float difference = 8.0f, float combi = DEFAULT_MIN_TEMPERATURE,
                      float buffer = DEFAULT_MIN_TEMPERATURE)
    : differenceStart(difference), minimumCombi(combi), minimumBuffer(buffer) {}
};

// Zustandsbehaftete Temperaturregelung; keine GPIO- oder Zeitabhängigkeit.
// allowCharging sperrt ausschließlich die Einlagerung. Die Rückladung bleibt
// unabhängig davon aktiv. Mindestwerte beziehen sich auf Kombi Mitte und Puffer.
class AutomaticControl {
public:
  static constexpr float MIN_CHARGE_TARGET = 68.0f;
  static constexpr float MAX_CHARGE_TARGET = 72.0f;
  static constexpr float STOP_DIFFERENCE = 3.0f;
  static bool validChargeTarget(float target);
  static bool validThresholds(const AutomaticThresholds& thresholds);
  // Historische auto1-Daten ohne Kennung übernehmen nur die Startdifferenz.
  static constexpr float MINIMUM_FORMAT_MARKER = 731042.0f;
  static bool readSavedThresholds(const float saved[5], AutomaticThresholds& thresholds);
  TransferDemand update(bool enabled, bool allowCharging, bool combiValid,
                        float combi, bool bufferValid, float buffer,
                        float target, const AutomaticThresholds& thresholds);
  TransferDemand update(bool enabled, bool allowCharging, bool topValid, float top,
                        bool middleValid, float middle, bool bufferValid, float buffer,
                        float target, const AutomaticThresholds& thresholds);
  void reset() { demand_ = TransferDemand::Off; }
  TransferDemand demand() const { return demand_; }
private:
  TransferDemand demand_ = TransferDemand::Off;
};
