#include "../Relaiskarte/RelayState.h"
#include <cassert>
#include <cstdio>

int main() {
  RelayState state;
  unsigned saves = 0, writes = 0, persisted = 0;
  bool storage = true;
  auto save = [&](uint8_t v) { ++saves; if (!storage) return false; persisted = v; return true; };
  auto apply = [&](unsigned channel, bool on) { assert(channel < 4); assert(bool(persisted & (1U << channel)) == on); ++writes; };
  assert(state.mask() == 0);
  assert(state.set(0, true, save, apply));
  assert(state.set(3, true, save, apply));
  assert(state.mask() == 9 && persisted == 9 && saves == 2 && writes == 2);
  assert(state.set(0, true, save, apply) && saves == 2 && writes == 2);
  storage = false;
  assert(!state.set(0, false, save, apply));
  assert(state.mask() == 9 && persisted == 9 && writes == 2);
  assert(!state.set(4, true, save, apply) && writes == 2);
  RelayState restored;
  assert(restored.restore(persisted) && restored.mask() == 9);
  assert(!restored.restore(16) && restored.mask() == 9);
  storage = true;
  assert(restored.set(3, false, save, apply) && restored.mask() == 1 && persisted == 1);
  std::puts("PASS: Relaiszustände, Wiederherstellung, Schreibfehler und Flash-Schreibminimierung.");
}
