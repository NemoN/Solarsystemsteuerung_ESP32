#include "../Relaiskarte/RelayWatchdog.h"
#include <cassert>
#include <cstdio>

int main() {
  using A = RelayWatchdog::Action;
  RelayWatchdog w(0);
  assert(w.check(59999, 0, 0, true) == A::None);
  assert(w.check(60000, 0, 0, true) == A::Restart);
  assert(w.check(61000, 0, 0, true) == A::None);
  assert(w.check(62000, 62000, 1, true) == A::None);
  assert(w.check(122000, 62000, 1, true) == A::Restart);
  assert(w.check(123000, 123000, 2, true) == A::None);
  assert(w.check(183000, 123000, 2, true) == A::LimitReached);
  assert(w.check(184000, 123000, 2, true) == A::None);
  assert(w.check(185000, 185000, 2, true) == A::None);
  assert(w.check(185000 + RelayWatchdog::STABLE_MS - 1, 185000 + RelayWatchdog::STABLE_MS - 1, 2, true) == A::None);
  assert(w.check(185000 + RelayWatchdog::STABLE_MS, 185000 + RelayWatchdog::STABLE_MS, 2, true) == A::ClearCount);
  RelayWatchdog badStore(0);
  assert(badStore.check(60000, 0, 0, false) == A::StorageError);
  assert(badStore.check(61000, 0, 0, false) == A::None);
  RelayWatchdog rollover(UINT32_MAX - 20000);
  assert(rollover.check(39998, UINT32_MAX - 20000, 0, true) == A::None);
  assert(rollover.check(39999, UINT32_MAX - 20000, 0, true) == A::Restart);
  std::puts("PASS: Relais-Watchdog: Timeout, Neustartbegrenzung, stabile Erholung, Speicherfehler und Zeitüberlauf.");
}
