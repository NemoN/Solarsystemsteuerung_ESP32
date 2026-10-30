#include "AutomaticControl.h"
#include "PumpControl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main() {
  AutomaticControl automatic;
  const AutomaticThresholds t = {2, 5, 2};
  const auto off = TransferDemand::Off;
  const auto charge = TransferDemand::CombiToBuffer;
  const auto discharge = TransferDemand::BufferToCombi;
  assert(automatic.update(true, true, true, 56.9, true, 50, 55, t) == off);
  assert(automatic.update(true, true, true, 57, true, 52, 55, t) == charge);
  assert(automatic.update(true, true, true, 56, true, 52.5, 55, t) == charge);
  assert(automatic.update(true, true, true, 56, true, 54, 55, t) == off);
  assert(automatic.update(true, true, true, 57, true, 52, 55, t) == charge);
  assert(automatic.update(true, true, true, 55, true, 50, 55, t) == off);
  assert(automatic.update(true, false, true, 60, true, 50, 55, t) == off);
  assert(automatic.update(true, false, true, 53, true, 58, 55, t) == discharge);
  assert(automatic.update(true, false, true, 54, true, 57, 55, t) == discharge);
  assert(automatic.update(true, false, true, 55, true, 60, 55, t) == off);
  assert(automatic.update(true, true, true, 53, true, 58, 55, t) == discharge);
  assert(automatic.update(true, true, true, 53, true, 55, 55, t) == off);
  assert(automatic.update(true, true, true, 57, true, 52, 55, t) == charge);
  assert(automatic.update(true, false, true, 57, true, 52, 55, t) == off);
  assert(automatic.update(true, true, true, 57, true, 52, 55, t) == charge);
  assert(automatic.update(true, true, false, NAN, true, 52, 55, t) == off);
  assert(automatic.update(true, true, true, 57, false, NAN, 55, t) == off);
  assert(automatic.update(true, true, true, NAN, true, 52, 55, t) == off);
  assert(automatic.update(true, true, true, 57, true, 52, NAN, t) == off);
  assert(automatic.update(false, true, true, 57, true, 52, 55, t) == off);
  const AutomaticThresholds invalid = {2, 2, 2};
  assert(!automatic.validThresholds(invalid));
  assert(automatic.update(true, true, true, 57, true, 52, 55, invalid) == off);
  // Ein großer Sprung verlangt die andere Richtung; PumpControl erzwingt die Pause.
  assert(automatic.update(true, true, true, 57, true, 52, 55, t) == charge);
  assert(automatic.update(true, true, true, 50, true, 60, 55, t) == discharge);

  // Automatische Umkehr durchläuft dieselbe reale Umschaltfolge wie Handbetrieb.
  automatic.reset();
  PumpControl pump;
  auto request = [&](float combi, bool bufferValid, float buffer, uint32_t now) {
    const TransferDemand d = automatic.update(true, true, true, combi, bufferValid, buffer, 55, t);
    PumpDirection direction = pump.direction();
    if (d == charge) direction = PumpDirection::CombiToBuffer;
    if (d == discharge) direction = PumpDirection::BufferToCombi;
    pump.command(d != off, direction, now);
  };
  request(57, true, 52, 0);
  assert(pump.on());
  request(50, true, 60, 100);
  assert(!pump.on() && pump.direction() == PumpDirection::CombiToBuffer);
  pump.update(2099);
  assert(!pump.on() && pump.direction() == PumpDirection::CombiToBuffer);
  pump.update(2100);
  assert(!pump.on() && pump.direction() == PumpDirection::BufferToCombi);
  pump.update(2200);
  assert(pump.on());
  request(57, true, 52, 2300);
  assert(!pump.on() && pump.switching());
  request(57, false, NAN, 2400);
  pump.update(5000);
  assert(!pump.on() && !pump.requestedOn());
  puts("PASS: Automatikgrenzen, Hysterese, Einlagerungsfreigabe, Sensorfehler und Richtungswechsel.");
}
