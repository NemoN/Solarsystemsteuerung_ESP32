#include <Adafruit_MAX31865.h>
#include <Adafruit_SPIDevice.h>
#include <math.h>

#include "SensorReading.h"

namespace {
bool readConfiguration(Adafruit_SPIDevice& spi, uint8_t& result) {
  const uint8_t address = MAX31865_CONFIG_REG;
  return spi.write_then_read(&address, 1, &result, 1);
}
} // namespace

SensorReading readTemperatureSensor(Adafruit_MAX31865& sensor,
  Adafruit_SPIDevice& diagnosticSpi, float nominalResistance, float referenceResistance) {
  SensorReading reading = {};
  bool configReadOk = readConfiguration(diagnosticSpi, reading.configBefore);
  // Ein wechselndes Bit zurücklesen: die POR-Grenzwerte allein beweisen keine
  // funktionierenden Schreibzugriffe. Die Bibliothek schaltet Bias danach selbst.
  sensor.enableBias(true);
  configReadOk &= readConfiguration(diagnosticSpi, reading.configWithBias);
  // Genau eine Wandlung: Temperatur und Diagnose beziehen sich auf denselben Rohwert.
  reading.rawRtd = sensor.readRTD();
  // Vor readFault(): dessen automatischer Prüflauf überschreibt das 1-Shot-Bit.
  configReadOk &= readConfiguration(diagnosticSpi, reading.configAfterConversion);
  reading.fault = sensor.readFault();
  reading.lowerThreshold = sensor.getLowerThreshold();
  reading.upperThreshold = sensor.getUpperThreshold();
  reading.temperature = sensor.calculateTemperature(reading.rawRtd,
    nominalResistance, referenceResistance);

  // begin() setzt die Grenzen auf 0x0000/0xFFFF, bestätigt aber keinen SPI-Kontakt.
  // Ein durchgehend LOW/HIGH gelesener Bus fällt durch diese Gegenprüfung auf.
  // D0 der Grenzregister ist laut Datenblatt "don't care" und wird ignoriert.
  if (!configReadOk || (reading.lowerThreshold & 0xFFFE) != 0x0000 ||
      (reading.upperThreshold & 0xFFFE) != 0xFFFE) {
    reading.error = "MAX31865-Konfiguration nicht lesbar";
  } else if ((reading.configWithBias &
              (MAX31865_CONFIG_BIAS | MAX31865_CONFIG_MODEAUTO | MAX31865_CONFIG_FILT50HZ)) !=
             MAX31865_CONFIG_BIAS) {
    reading.error = "MAX31865 uebernimmt Messkonfiguration nicht";
  } else if (reading.configAfterConversion & MAX31865_CONFIG_1SHOT) {
    reading.error = "RTD-Wandlung nicht abgeschlossen";
  } else if (reading.fault != 0) {
    reading.error = "MAX31865 meldet einen Sensorfehler";
  } else if (reading.rawRtd == 0 || reading.rawRtd >= 0x7FFF) {
    // Rohwert 0 ergibt in der Bibliothek endliche -242,02 °C, auch bei Fehlercode 0.
    reading.error = "Kein gueltiger RTD-Rohwert";
  } else if (!isfinite(reading.temperature) ||
             reading.temperature < -200.0f || reading.temperature > 850.0f) {
    // Breiter Pt100/Pt1000-Messbereich, unabhängig von den Sollwertgrenzen.
    reading.error = "Temperatur ausserhalb des Sensorbereichs";
  }
  if (!reading.valid()) reading.temperature = NAN;
  return reading;
}

bool temperatureRelayState(bool previousState, bool readingValid,
  float temperature, float target, float hysteresis) {
  if (!readingValid || !isfinite(temperature)) return false;
  if (temperature >= target) return false;
  if (temperature < target - hysteresis) return true;
  return previousState;
}
