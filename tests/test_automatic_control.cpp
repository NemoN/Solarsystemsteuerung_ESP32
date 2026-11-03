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

  // Rückladung ohne Sollwertgrenze, auch bei niedrigen Temperaturen und Ölbetrieb.
  assert(automatic.update(true, false, true, 37, true, 44.9f, 68, t) == off);
  assert(automatic.update(true, false, true, 37, true, 45, 68, t) == discharge);
  assert(automatic.update(true, false, true, 40, true, 43.1f, 68, t) == discharge);
  assert(automatic.update(true, false, true, 40, true, 43.1f, 68, {10}) == discharge);
  assert(automatic.update(true, false, true, 40, true, 43, 68, t) == off);
  assert(automatic.update(true, false, true, 72, true, 80, 68, t) == discharge);
  assert(automatic.update(true, false, true, 75.9f, true, 79, 68, t) == discharge);
  assert(automatic.update(true, false, true, 76, true, 79, 68, t) == off);
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
  const float retiredFields[5] = {NAN, 10, NAN, NAN, NAN};
  assert(AutomaticControl::readSavedThresholds(retiredFields, restored));
  assert(restored.differenceStart == 10);
  const float invalidLegacy[5] = {2, 12, 2, 0, 1};
  assert(!AutomaticControl::readSavedThresholds(invalidLegacy, restored));
  assert(restored.differenceStart == 10);

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
