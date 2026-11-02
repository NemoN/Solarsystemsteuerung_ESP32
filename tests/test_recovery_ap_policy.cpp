#include "RecoveryApPolicy.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
  using P = RecoveryApPolicy;
  P policy(30000);
  // AP bleibt beim Start und für eine unbestätigte OTA-Version aktiv.
  assert(policy.update(0, false, true, false, false) == P::Keep);
  assert(policy.update(1000, true, true, false, false) == P::Keep);
  assert(policy.update(40000, true, true, false, false) == P::Keep);
  assert(policy.update(40001, true, true, true, false) == P::Disable);
  assert(policy.update(40002, true, false, true, false) == P::Keep);
  // Kurze Ausfälle lassen den AP aus; bei längerem Ausfall kommt er zurück.
  assert(policy.update(50000, false, false, true, false) == P::Keep);
  assert(policy.update(79999, false, false, true, false) == P::Keep);
  assert(policy.update(80000, false, false, true, false) == P::Enable);
  assert(policy.update(80001, false, true, true, false) == P::Keep);
  assert(policy.update(81000, true, true, true, false) == P::Keep);
  assert(policy.update(110999, true, true, true, false) == P::Keep);
  assert(policy.update(111000, true, true, true, false) == P::Disable);
  // Neue WLAN-Einstellungen und OTA-Startprüfung erzwingen den Notzugang.
  assert(policy.update(112000, true, false, true, true) == P::Enable);
  assert(policy.update(112001, true, false, false, false) == P::Enable);

  P rollover(30000);
  const uint32_t start = UINT32_MAX - 10000;
  assert(rollover.update(start, true, true, true, false) == P::Keep);
  assert(rollover.update(start + uint32_t(29999), true, true, true, false) == P::Keep);
  assert(rollover.update(start + uint32_t(30000), true, true, true, false) == P::Disable);
  std::puts("PASS: AP-Vergleich, Startprüfung, WLAN-Ausfall, Neueinrichtung und Zeitüberlauf.");
}
