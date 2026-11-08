#include "AutomaticControl.h"
#include "PumpControl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main() {
  AutomaticControl automatic;
  const AutomaticThresholds t = {8};
  const auto off = TransferDemand::Off;
  const auto charge = TransferDemand::CombiToBuffer;
  const auto discharge = TransferDemand::BufferToCombi;

  // Start genau am Sollwert, ohne zusätzlichen Abstand. Beide Bedingungen nötig.
  assert(automatic.update(true, true, true, 67.9f, true, 50, 68, t) == off);
  assert(automatic.update(true, true, true, 68, true, 60.1f, 68, t) == off);
  assert(automatic.update(true, true, true, 68, true, 60, 68, t) == charge);
  // Der Sollwert ist nur eine Startfreigabe, keine spätere Ausschaltgrenze.
  assert(automatic.update(true, true, true, 67, true, 62, 68, t) == charge);
  assert(automatic.update(true, true, true, 67.1f, true, 64, 68, t) == charge);
  assert(automatic.update(true, true, true, 67.1f, true, 64, 68, {10}) == charge);
  assert(automatic.update(true, true, true, 67, true, 64, 68, t) == off);
  assert(automatic.update(true, true, true, 68, true, 60, 68, t) == charge);
  assert(automatic.update(true, true, true, 63.9f, true, 64, 68, t) == off);

  // Rückladung ohne Einlagerungs-Starttemperatur, auch bei Ölbetrieb.
  assert(automatic.update(true, false, true, 37, true, 44.9f, 68, t) == off);
  assert(automatic.update(true, false, true, 37, true, 45, 68, t) == discharge);
  assert(automatic.update(true, false, true, 40, true, 43.1f, 68, t) == discharge);
  assert(automatic.update(true, false, true, 40, true, 43.1f, 68, {10}) == discharge);
  assert(automatic.update(true, false, true, 40, true, 43, 68, t) == off);
  assert(automatic.update(true, false, true, 72, true, 80, 68, t) == discharge);
  assert(automatic.update(true, false, true, 75.9f, true, 79, 68, t) == discharge);
  assert(automatic.update(true, false, true, 76, true, 79, 68, t) == off);
  // Beide Speicher müssen ihre eigene Grenze erreichen, unabhängig von der
  // Richtung. Ein Unterschreiten stoppt auch einen laufenden Transfer.
  automatic.reset();
  const AutomaticThresholds minimum = {8, 35, 40};
  assert(automatic.update(true, false, true, 20, true, 30, 68, minimum) == off);
  assert(automatic.update(true, false, true, 20, true, 50, 68, minimum) == off);
  assert(automatic.update(true, false, true, 34.9f, true, 50, 68, minimum) == off);
  assert(automatic.update(true, false, true, 35, true, 43, 68, minimum) == discharge);
  assert(automatic.update(true, false, true, 34.9f, true, 50, 68, minimum) == off);
  assert(automatic.update(true, false, true, 35, true, 43, 68, minimum) == discharge);
  assert(automatic.update(true, false, true, 35, true, 39.9f, 68, minimum) == off);
  automatic.reset();
  const AutomaticThresholds highMinimum = {8, 70, 40};
  assert(automatic.update(true, true, true, 69.9f, true, 50, 68, highMinimum) == off);
  assert(automatic.update(true, true, true, 70, true, 50, 68, highMinimum) == charge);
  assert(automatic.update(true, true, true, 69.9f, true, 50, 68, highMinimum) == off);
  assert(automatic.update(true, true, true, 70, true, 40, 68, highMinimum) == charge);
  assert(automatic.update(true, true, true, 70, true, 39.9f, 68, highMinimum) == off);
  automatic.reset();
  // Ein ungültiger Einlagerungssollwert darf eine sinnvolle Rückladung nicht sperren.
  assert(automatic.update(true, true, true, 37, true, 45, NAN, t) == discharge);
  automatic.reset();
  assert(automatic.update(true, true, true, 70, true, 50, NAN, t) == off);
  assert(automatic.update(true, true, true, 70, true, 50, 45, t) == off);

  // Öl sperrt sofort eine laufende Einlagerung, aber keine laufende Rückladung.
  assert(automatic.update(true, true, true, 68, true, 60, 68, t) == charge);
  assert(automatic.update(true, false, true, 68, true, 60, 68, t) == off);
  assert(automatic.update(true, false, true, 37, true, 45, 68, t) == discharge);
  assert(automatic.update(true, true, true, 40, true, 45, 68, t) == discharge);
  assert(automatic.update(true, false, true, 40, true, 45, 68, t) == discharge);

  assert(AutomaticControl::validChargeTarget(68));
  assert(AutomaticControl::validChargeTarget(72));
  assert(!AutomaticControl::validChargeTarget(67.9f));
  assert(!AutomaticControl::validChargeTarget(72.1f));
  assert(!AutomaticControl::validChargeTarget(NAN));
  assert(AutomaticControl::validThresholds({5}));
  assert(AutomaticControl::validThresholds({10}));
  assert(!AutomaticControl::validThresholds({4.9f}));
  assert(!AutomaticControl::validThresholds({10.1f}));
  assert(!AutomaticControl::validThresholds({NAN}));
  assert(AutomaticControl::validThresholds({8, 0}));
  assert(AutomaticControl::validThresholds({8, 80}));
  assert(!AutomaticControl::validThresholds({8, -0.1f}));
  assert(!AutomaticControl::validThresholds({8, 80.1f}));
  assert(!AutomaticControl::validThresholds({8, NAN}));
  assert(!AutomaticControl::validThresholds({8, 35, -1}));
  assert(!AutomaticControl::validThresholds({8, 35, 81}));
  assert(!AutomaticControl::validThresholds({8, 35, NAN}));
  automatic.reset();
  assert(automatic.update(true, true, true, 68, true, 63, 68, {5}) == charge);
  automatic.reset();
  assert(automatic.update(true, true, true, 70, true, 60, 70, {10}) == charge);
  assert(automatic.update(true, true, true, 70, true, 50, 68, {10.1f}) == off);
  assert(automatic.update(true, true, false, NAN, true, 50, 68, t) == off);
  assert(automatic.update(true, true, true, 70, false, NAN, 68, t) == off);
  assert(automatic.update(true, true, true, NAN, true, 50, 68, t) == off);
  assert(automatic.update(false, true, true, 70, true, 50, 68, t) == off);

  // Alte Sollabstände/Stoppschwellen werden ignoriert, nur der Startwert migriert.
  AutomaticThresholds restored = {8};
  const float legacy[5] = {2, 5, 2, 1, 0};
  assert(AutomaticControl::readSavedThresholds(legacy, restored));
  assert(restored.differenceStart == 5);
  assert(restored.minimumCombi == AutomaticThresholds::DEFAULT_MIN_TEMPERATURE);
  assert(restored.minimumBuffer == AutomaticThresholds::DEFAULT_MIN_TEMPERATURE);
  const float retiredFields[5] = {NAN, 10, NAN, NAN, NAN};
  assert(AutomaticControl::readSavedThresholds(retiredFields, restored));
  assert(restored.differenceStart == 10);
  const float newFormat[5] = {38, 9, AutomaticControl::MINIMUM_FORMAT_MARKER, 42, 1};
  assert(AutomaticControl::readSavedThresholds(newFormat, restored));
  assert(restored.differenceStart == 9 && restored.minimumCombi == 38 && restored.minimumBuffer == 42);
  const float invalidMinimum[5] = {0, 8, AutomaticControl::MINIMUM_FORMAT_MARKER, NAN, 1};
  assert(!AutomaticControl::readSavedThresholds(invalidMinimum, restored));
  const float invalidCombiMinimum[5] = {NAN, 8, AutomaticControl::MINIMUM_FORMAT_MARKER, 40, 1};
  assert(!AutomaticControl::readSavedThresholds(invalidCombiMinimum, restored));
  const float invalidLegacy[5] = {2, 12, 2, 0, 1};
  assert(!AutomaticControl::readSavedThresholds(invalidLegacy, restored));
  assert(restored.differenceStart == 9 && restored.minimumCombi == 38 && restored.minimumBuffer == 42);

  // Ein Sprung in die Gegenrichtung durchläuft unverändert die reale Umkehrpause.
  automatic.reset();
  PumpControl pump;
  auto request = [&](float combi, bool bufferValid, float buffer, uint32_t now) {
    const TransferDemand d = automatic.update(true, true, true, combi, bufferValid, buffer, 68, t);
    PumpDirection direction = pump.direction();
    if (d == charge) direction = PumpDirection::CombiToBuffer;
    if (d == discharge) direction = PumpDirection::BufferToCombi;
    pump.command(d != off, direction, now);
  };
  request(68, true, 60, 0);
  assert(pump.on());
  request(37, true, 45, 100);
  assert(!pump.on() && pump.direction() == PumpDirection::CombiToBuffer);
  pump.update(2099);
  assert(!pump.on() && pump.direction() == PumpDirection::CombiToBuffer);
  pump.update(2100);
  assert(!pump.on() && pump.direction() == PumpDirection::BufferToCombi);
  pump.update(2200);
  assert(pump.on());
  request(70, true, 60, 2300);
  assert(!pump.on() && pump.switching());
  request(70, false, NAN, 2400);
  pump.update(5000);
  assert(!pump.on() && !pump.requestedOn());
  // Ausgleich bereits während der Umkehrpause verhindert einen späteren Start.
  automatic.reset();
  pump = PumpControl();
  request(68, true, 60, 0);
  request(37, true, 45, 100);
  assert(!pump.on() && pump.switching());
  request(41, true, 41, 200);
  pump.update(5000);
  assert(!pump.on() && !pump.requestedOn());
  puts("PASS: Startfreigabe 68–72°C, Differenz 5–10K, Stopp bei 3K, Rückladung, Ölsperre, Altdaten und Umkehrpause.");
}
