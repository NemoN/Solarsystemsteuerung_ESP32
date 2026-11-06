#include "PumpRuntime.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

static time_t local(int year, int month, int day, int hour, int minute, int second, int dst = -1) {
  tm t{}; t.tm_year = year - 1900; t.tm_mon = month - 1; t.tm_mday = day;
  t.tm_hour = hour; t.tm_min = minute; t.tm_sec = second; t.tm_isdst = dst;
  return mktime(&t);
}
template <size_t N> static const PumpRuntime::Period& period(const PumpRuntime::Period (&rows)[N], uint32_t key) {
  for (const auto& row : rows) if (row.key == key) return row;
  assert(false); return rows[0];
}
int main() {
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1); tzset();
  PumpRuntime rt;
  auto t = local(2026, 12, 31, 23, 59, 58);
  rt.update(0, t, true, true, false);
  rt.update(1000, t + 1, true, true, false);
  rt.update(3000, t + 3, true, false, true);
  rt.update(5000, t + 5, false, false, false);
  assert(period(rt.state().days, 20261231).seconds[0] == 2);
  assert(period(rt.state().days, 20270101).seconds[0] == 3);
  assert(period(rt.state().days, 20261231).seconds[1] == 2);
  assert(period(rt.state().days, 20270101).seconds[1] == 1);
  assert(period(rt.state().days, 20270101).seconds[2] == 2);
  assert(period(rt.state().months, 202612).seconds[0] == 2);
  assert(period(rt.state().years, 2027).seconds[0] == 3);
  auto saved = rt.snapshot(); PumpRuntime restored;
  assert(restored.restore(&saved, sizeof(saved)));
  assert(period(restored.state().days, 20270101).seconds[2] == 2);
  saved.days[0].seconds[0]++; assert(!restored.restore(&saved, sizeof(saved)));
  PumpRuntime unknown; unknown.update(0, 0, true, false, false);
  unknown.update(3500, 0, true, false, false);
  unknown.update(4500, t, false, false, false);
  assert(unknown.state().undated[0] == 4);
  assert(period(unknown.state().days, 20261231).seconds[0] == 0);
  PumpRuntime wrap; wrap.update(0xfffffff0U, t, true, false, false);
  wrap.update(0x000007c0U, t + 2, false, false, false);
  assert(period(wrap.state().days, 20261231).seconds[0] == 2);
  PumpRuntime dst; auto before = local(2026, 10, 25, 2, 59, 58, 1);
  dst.update(0, before, false, false, true);
  dst.update(5000, before + 5, false, false, false);
  assert(period(dst.state().days, 20261025).seconds[2] == 5);
  PumpRuntime spring; auto springStart = local(2026, 3, 29, 0, 0, 0);
  spring.update(0, springStart, true, false, false);
  spring.update(23UL * 3600 * 1000, springStart + 23 * 3600, false, false, false);
  assert(period(spring.state().days, 20260329).seconds[0] == 23 * 3600);
  assert(period(spring.state().days, 20260330).seconds[0] == 0);
  PumpRuntime retention; auto start = local(2026, 1, 1, 12, 0, 0);
  for (uint32_t day = 0; day < 33; ++day)
    retention.update(day * 86400000UL, start + day * 86400UL, false, false, false);
  for (const auto& row : retention.state().days) assert(row.key != 20260101);
  assert(period(retention.state().days, 20260202).seconds[0] == 0);
  PumpRuntime correction; correction.update(0, t, true, false, false);
  correction.update(1000, t - 3600, false, false, false);
  assert(period(correction.state().days, 20261231).seconds[0] == 1);
  std::cout << "PASS: Laufzeiten, Richtungen, Tages-/Jahreswechsel, Sommerzeit, unsynchronisierte Uhr, millis-Überlauf und Flash-Prüfsumme.\n";
}
