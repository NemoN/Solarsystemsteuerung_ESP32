#include <Adafruit_MAX31865.h>
#include <Adafruit_SPIDevice.h>
#include <cassert>
#include <cmath>
#include <iostream>

#include "SensorReading.h"

SensorReading read(Adafruit_MAX31865& sensor) {
  Adafruit_SPIDevice spi;
  return readTemperatureSensor(sensor, spi, 1000.0f, 4300.0f);
}

void expectRelayOff(const SensorReading& reading) {
  assert(!reading.valid());
  assert(std::isnan(reading.temperature));
  for (bool previousState : {false, true}) {
    assert(!temperatureRelayState(previousState, reading.valid(),
      reading.temperature, 45.0f, 1.5f));
  }
}

void expectRelayOff(Adafruit_MAX31865& sensor) {
  expectRelayOff(read(sensor));
}

int main() {
  Adafruit_MAX31865 sensor;
  sensor.rawRtd = 0;
  sensor.temperature = -242.02f;
  expectRelayOff(sensor); // Screenshot: Rohwert 0 UND Fehlerregister 0.

  sensor.temperature = 30.0f;
  expectRelayOff(sensor); // Rohwertprüfung unabhängig von der Temperaturumrechnung.

  sensor = Adafruit_MAX31865();
  sensor.lowerThreshold = 0;
  sensor.upperThreshold = 0;
  expectRelayOff(sensor); // Register unlesbar, obwohl Rohwert und Temperatur plausibel aussehen.

  sensor = Adafruit_MAX31865();
  sensor.lowerThreshold = 0xFFFF;
  expectRelayOff(sensor); // Auch den unteren Grenzwert prüfen.

  sensor = Adafruit_MAX31865();
  sensor.rawRtd = 0x7FFF;
  sensor.fault = 0xFF;
  sensor.lowerThreshold = 0xFFFF;
  sensor.upperThreshold = 0xFFFF;
  expectRelayOff(sensor); // SPI liest nur HIGH.

  sensor = Adafruit_MAX31865();
  sensor.rawRtd = 0x7FFF;
  expectRelayOff(sensor); // Vollaussteuerung ohne Fehlerbit.

  // POR-Grenzwerte sind korrekt, aber der Wandler übernimmt keine Schreibbefehle.
  sensor = Adafruit_MAX31865();
  Adafruit_SPIDevice spi;
  spi.configuration[1] = 0x00;
  expectRelayOff(readTemperatureSensor(sensor, spi, 1000.0f, 4300.0f));

  // Ein alter plausibler Rohwert darf bei noch laufender Wandlung nicht regeln.
  spi = Adafruit_SPIDevice();
  spi.configuration[2] = 0x20;
  expectRelayOff(readTemperatureSensor(sensor, spi, 1000.0f, 4300.0f));

  for (uint8_t config : {0x81, 0xC0, 0xFF}) {
    spi = Adafruit_SPIDevice();
    spi.configuration[1] = config;
    expectRelayOff(readTemperatureSensor(sensor, spi, 1000.0f, 4300.0f));
  }

  spi = Adafruit_SPIDevice();
  spi.readable = false;
  expectRelayOff(readTemperatureSensor(sensor, spi, 1000.0f, 4300.0f));

  for (uint8_t fault : {0x80, 0x40, 0x20, 0x10, 0x08, 0x04}) {
    sensor = Adafruit_MAX31865();
    sensor.fault = fault;
    expectRelayOff(sensor);
  }

  for (float invalid : {-242.0f, -200.1f, 850.1f, NAN, INFINITY, -INFINITY}) {
    sensor = Adafruit_MAX31865();
    sensor.temperature = invalid;
    expectRelayOff(sensor);
  }

  for (float valid : {-200.0f, -10.0f, 0.0f, 30.0f, 71.4f, 100.0f, 850.0f}) {
    sensor = Adafruit_MAX31865();
    sensor.temperature = valid;
    const SensorReading reading = read(sensor);
    assert(reading.valid());
    assert(reading.temperature == valid);
    assert(sensor.conversions == 1);
    assert(sensor.biasEnables == 1);
    assert(sensor.calculatedRaw == reading.rawRtd);
  }

  sensor.lowerThreshold = 1;
  sensor.upperThreshold = 0xFFFE;
  assert(read(sensor).valid()); // D0 der Grenzregister ist nicht definiert.

  // Ein vorübergehender Fehler bleibt nicht an einer späteren gültigen Messung hängen.
  sensor.rawRtd = 0;
  expectRelayOff(sensor);
  sensor.rawRtd = 9000;
  sensor.temperature = 30.0f;
  const SensorReading recovered = read(sensor);
  assert(recovered.valid());
  assert(temperatureRelayState(false, recovered.valid(), recovered.temperature, 45.0f, 1.5f));

  // Drei Wandler: ein fehlerhafter Messpunkt sperrt nur sein eigenes Relais.
  Adafruit_MAX31865 sensors[3];
  sensors[0].temperature = 71.4f;
  sensors[1].rawRtd = 0;
  sensors[2].temperature = 30.0f;
  bool relayStates[3] = {};
  for (unsigned i = 0; i < 3; ++i) {
    const SensorReading reading = read(sensors[i]);
    relayStates[i] = temperatureRelayState(true, reading.valid(), reading.temperature, 45.0f, 1.5f);
  }
  assert(!relayStates[0] && !relayStates[1] && relayStates[2]);

  // Vorhandene Hysterese einschließlich der beiden Schaltgrenzen erhalten.
  assert(temperatureRelayState(false, true, 43.4f, 45.0f, 1.5f));
  assert(!temperatureRelayState(false, true, 43.5f, 45.0f, 1.5f));
  assert(temperatureRelayState(true, true, 43.5f, 45.0f, 1.5f));
  assert(temperatureRelayState(true, true, 44.0f, 45.0f, 1.5f));
  assert(!temperatureRelayState(false, true, 44.0f, 45.0f, 1.5f));
  assert(!temperatureRelayState(true, true, 45.0f, 45.0f, 1.5f));
  assert(!temperatureRelayState(true, false, 30.0f, 45.0f, 1.5f));
  assert(!temperatureRelayState(true, true, NAN, 45.0f, 1.5f));

  std::cout << "PASS: invalid raw readings, SPI read/write failures, pending conversion, all fault bits, range, recovery, sensor isolation, relay hysteresis.\n";
}
