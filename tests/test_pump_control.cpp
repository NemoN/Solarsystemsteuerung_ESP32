#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "PumpControl.h"

int main() {
  const auto forward = PumpDirection::CombiToBuffer;
  const auto reverse = PumpDirection::BufferToCombi;
  PumpControl pump;
  assert(!pump.on() && !pump.switching() && !pump.requestedOn());

  pump.command(true, forward, 100);
  assert(pump.on());
  pump.command(true, reverse, 150);
  assert(!pump.on() && pump.direction() == forward && pump.switching());
  // Wiederholte Anforderungen dürfen die Zwei-Sekunden-Frist nicht verlängern.
  pump.command(true, reverse, 1000);
  pump.update(2149);
  assert(!pump.on() && pump.direction() == forward);
  pump.update(2150);
  assert(!pump.on() && pump.direction() == reverse);
  pump.update(2249);
  assert(!pump.on());
  pump.update(2250);
  assert(pump.on() && !pump.switching());

  // AUS während der Pause verhindert den sonst vorgesehenen Wiederanlauf.
  pump.command(true, forward, 3000);
  pump.command(false, forward, 3500);
  pump.update(5000);
  assert(!pump.on() && pump.direction() == forward);
  pump.update(5100);
  assert(!pump.on() && !pump.switching());

  // Ein zurückgenommener Wechsel behält die bisherige Richtung bei.
  pump.command(true, forward, 6000);
  pump.command(true, reverse, 6100);
  pump.command(true, forward, 6200);
  assert(pump.on() && pump.direction() == forward && !pump.switching());

  // Auch bei ausgeschalteter Pumpe erst warten, dann die Richtung umschalten.
  pump.command(false, reverse, 7000);
  assert(!pump.on() && pump.direction() == forward);
  pump.update(8999);
  assert(pump.direction() == forward);
  pump.update(9000);
  assert(!pump.on() && pump.direction() == reverse);
  pump.command(true, forward, 9050); // neue Richtung während der Relais-Beruhigung
  assert(!pump.on() && pump.direction() == reverse);
  pump.update(9100);
  pump.update(11099);
  assert(!pump.on() && pump.direction() == reverse);
  pump.update(11100);
  assert(!pump.on() && pump.direction() == forward);
  pump.update(11200);
  assert(pump.on());

  // millis()-Überlauf darf keine Pause verkürzen oder die Pumpe blockieren.
  PumpControl wrap;
  const uint32_t start = UINT32_MAX - 1000;
  wrap.command(true, reverse, start);
  wrap.update(start + uint32_t(1999));
  assert(!wrap.on() && wrap.direction() == forward);
  wrap.update(start + uint32_t(2000));
  assert(!wrap.on() && wrap.direction() == reverse);
  wrap.command(false, reverse, start + uint32_t(2050));
  wrap.update(start + uint32_t(2100));
  assert(!wrap.on() && !wrap.switching() && wrap.remainingMs(0) == 0);

  PumpControl inverted(PumpDirection::BufferToCombi);
  assert(!inverted.on() && inverted.direction() == reverse);
  inverted.command(true, reverse, 0);
  assert(inverted.on() && !inverted.switching());

  // Unterschiedliche, teils widersprüchliche Bedienfolgen prüfen die physischen
  // Invarianten unabhängig von der internen Phaseneinteilung, auch über millis=0.
  PumpControl stress;
  uint32_t now = UINT32_MAX - 5000;
  uint32_t offSince = now;
  uint32_t directionSince = now - uint32_t(100);
  uint32_t random = 0x5A17u;
  unsigned changes = 0, starts = 0;
  for (unsigned i = 0; i < 10000; ++i) {
    random = random * 1664525u + 1013904223u;
    now += random % 1500u;
    const bool wasOn = stress.on();
    const auto oldDirection = stress.direction();
    if (i % 3 == 0) {
      stress.command((random & 0x100u) != 0,
        random & 0x200u ? reverse : forward, now);
    } else stress.update(now);
    if (wasOn && !stress.on()) offSince = now;
    if (stress.direction() != oldDirection) {
      assert(!stress.on());
      assert(uint32_t(now - offSince) >= 2000);
      directionSince = now;
      ++changes;
    }
    if (!wasOn && stress.on()) {
      assert(uint32_t(now - directionSince) >= 100);
      ++starts;
    }
    if (!stress.requestedOn()) assert(!stress.on());
  }
  assert(changes > 50 && starts > 50);

  puts("PASS: pump stop, 2 s reversal pause, relay settling, cancellation, repeated commands, restart prevention, millis rollover, 10000-command invariant check.");
}
