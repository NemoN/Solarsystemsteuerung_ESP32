#include "StatusLed.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

static void colorIs(StatusLed& led, uint8_t red, uint8_t green, uint8_t blue) {
  const auto color = led.color();
  assert(color.red == red && color.green == green && color.blue == blue);
}

int main() {
  for (unsigned flags = 0; flags < 16; ++flags) {
    const bool fault = flags & 1, update = flags & 2, manual = flags & 4, test = flags & 8;
    const auto expected = fault ? StatusLedMode::Fault : update ? StatusLedMode::Updating :
      manual ? StatusLedMode::Manual : test ? StatusLedMode::Test : StatusLedMode::Normal;
    assert(StatusLed::select(fault, update, manual, test) == expected);
  }
  StatusLed led;
  assert(led.update(0, StatusLedMode::Normal)); colorIs(led, 0, 32, 0);
  assert(!led.update(149, StatusLedMode::Normal));
  assert(led.update(150, StatusLedMode::Normal)); colorIs(led, 0, 0, 0);
  assert(led.update(2000, StatusLedMode::Normal)); colorIs(led, 0, 32, 0);
  assert(led.update(2100, StatusLedMode::Test)); colorIs(led, 24, 0, 36);
  assert(led.update(2350, StatusLedMode::Test)); colorIs(led, 0, 0, 0);
  assert(led.update(2400, StatusLedMode::Manual)); colorIs(led, 0, 0, 36);
  assert(led.update(2900, StatusLedMode::Manual)); colorIs(led, 0, 0, 0);
  assert(led.update(3000, StatusLedMode::Fault)); colorIs(led, 36, 24, 0);
  assert(led.update(3100, StatusLedMode::Fault)); colorIs(led, 0, 0, 0);
  assert(led.update(4400, StatusLedMode::Fault)); colorIs(led, 36, 24, 0);
  assert(led.update(5000, StatusLedMode::Updating)); colorIs(led, 3, 3, 3);
  assert(led.update(6000, StatusLedMode::Updating)); colorIs(led, 30, 30, 30);
  assert(led.update(7000, StatusLedMode::Updating)); colorIs(led, 3, 3, 3);
  assert(led.update(UINT32_MAX - 50, StatusLedMode::Fault)); colorIs(led, 36, 24, 0);
  assert(led.update(49, StatusLedMode::Fault)); colorIs(led, 0, 0, 0);
  std::puts("PASS: RGB-Statuspriorität, Blinkmuster, Zustandswechsel und millis-Überlauf.");
}
