#pragma once

#include <stdint.h>

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
