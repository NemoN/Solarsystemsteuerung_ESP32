#pragma once
#include <stdint.h>

// Anzeigeverzögerung: Messwert und Regelung bleiben davon unabhängig.
class SensorFaultDelay {
 public:
  static constexpr uint32_t DELAY_MS = 60000;
  void observe(bool failed, uint32_t now) {
    if (!failed) active_ = false;
    else if (!active_) { active_ = true; since_ = now; }
  }
  bool pending(uint32_t now) const {
    return active_ && uint32_t(now - since_) <= DELAY_MS;
  }
 private:
  uint32_t since_ = 0;
  bool active_ = false;
};

class Adafruit_MAX31865;
class Adafruit_SPIDevice;

struct SensorReading {
  uint16_t rawRtd;
  uint8_t fault;
  uint16_t lowerThreshold;
  uint16_t upperThreshold;
  uint8_t configBefore;
  uint8_t configWithBias;
  uint8_t configAfterConversion;
  float temperature;
  const char* error;

  bool valid() const { return error == nullptr; }
};

SensorReading readTemperatureSensor(Adafruit_MAX31865& sensor,
  Adafruit_SPIDevice& diagnosticSpi, float nominalResistance, float referenceResistance);

bool temperatureRelayState(bool previousState, bool readingValid,
  float temperature, float target, float hysteresis);
