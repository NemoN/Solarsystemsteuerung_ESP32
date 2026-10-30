#pragma once

#include <stdint.h>

#define MAX31865_CONFIG_REG 0x00
#define MAX31865_CONFIG_BIAS 0x80
#define MAX31865_CONFIG_MODEAUTO 0x40
#define MAX31865_CONFIG_1SHOT 0x20
#define MAX31865_CONFIG_FILT50HZ 0x01

// Austauschbare Busantworten für den Test der echten SensorReading.cpp.
class Adafruit_MAX31865 {
public:
  uint16_t rawRtd = 9000;
  uint8_t fault = 0;
  uint16_t lowerThreshold = 0;
  uint16_t upperThreshold = 0xFFFF;
  float temperature = 30.0f;
  unsigned conversions = 0;
  uint16_t calculatedRaw = 0;
  unsigned biasEnables = 0;

  void enableBias(bool enable) { if (enable) ++biasEnables; }
  uint16_t readRTD() { ++conversions; return rawRtd; }
  uint8_t readFault() { return fault; }
  uint16_t getLowerThreshold() { return lowerThreshold; }
  uint16_t getUpperThreshold() { return upperThreshold; }
  float calculateTemperature(uint16_t raw, float, float) {
    calculatedRaw = raw;
    return temperature;
  }
};
