#include "NightQuiet.h"
#include <cassert>

int main() {
  NightQuiet q;
  assert(q.valid() && !q.enabled && q.start == 1320 && q.end == 480 && q.days == 127);
  assert(!q.active(1, 1320));
  q.enabled = true;
  q.days = 1; // Montag
  assert(!q.active(1, 1319));
  assert(q.active(1, 1320));
  assert(q.active(2, 0));
  assert(q.active(2, 479));
  assert(!q.active(2, 480));
  assert(!q.active(0, 1380));
  q.days = 64; // Sonntag auf Montag
  assert(q.active(0, 1320));
  assert(q.active(1, 479));
  assert(!q.active(1, 1320));
  q.start = 600; q.end = 720; q.days = 2; // Dienstag tagsüber
  assert(!q.active(2, 599));
  assert(q.active(2, 600));
  assert(q.active(2, 719));
  assert(!q.active(2, 720));
  assert(!q.active(1, 600));
  q.days = 0; assert(!q.active(2, 600));
  q.end = q.start; assert(!q.valid());
  q.end = 1440; assert(!q.valid());
  q.end = 720; q.days = 128; assert(!q.valid());
}
