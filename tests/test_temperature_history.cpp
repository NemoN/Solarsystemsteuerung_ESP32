#include "TemperatureHistory.h"
#include <cassert>
#include <cstdio>
#include <deque>
#include <vector>
using H = TemperatureHistory;
static H::Sample sample(int n) { H::Sample s; for (auto &v : s.values) v = int16_t(n); return s; }
static int progressCalls = 0;
static void progress() { ++progressCalls; }
int main() {
  H h; auto a = sample(101), b = sample(202), c = sample(303);
  assert(!h.record(100, a)); H::Sample memory[5]; h.attach(memory, 5);
  assert(h.record(100, a)); assert(!h.record(100, b)); assert(!h.record(99, b));
  assert(h.record(102, b)); assert(h.size()==3 && h.firstMinute()==100 && h.lastMinute()==102);
  assert(h.at(0)->values[0]==101 && h.at(1)->values[0]==H::MISSING && h.at(2)->values[4]==202);
  assert(h.record(104, c)); assert(h.size()==5 && h.at(3)->values[0]==H::MISSING);
  assert(h.record(106, a)); assert(h.size()==5 && h.firstMinute()==102 && h.lastMinute()==106);
  assert(h.at(0)->values[0]==202 && h.at(1)->values[0]==H::MISSING && h.at(2)->values[0]==303 && h.at(3)->values[0]==H::MISSING && h.at(4)->values[0]==101);
  assert(h.at(5)==nullptr); assert(h.record(200,b)); assert(h.size()==1 && h.firstMinute()==200 && h.at(0)->values[0]==202);
  h.attach(nullptr, 100); assert(!h.available() && h.size()==0 && !h.record(300,c));
  std::vector<H::Sample> big(6000); h.attach(big.data(), big.size(), progress);
  assert(h.record(100, a)); assert(h.record(5100, b));
  assert(progressCalls > 0 && h.size()==5001 && h.at(3000)->values[2]==H::MISSING);
  H::Sample randomMemory[37]; h.attach(randomMemory,37);
  std::deque<H::Sample> expected; uint32_t first=0,last=0,now=1000,seed=1;
  for (int i=0; i<10000; ++i) {
    seed=seed*1664525u+1013904223u;
    uint32_t delta=(seed>>24)%45; now+=delta;
    H::Sample v=sample(i%10000); v.values[i%5]=H::MISSING;
    const bool accepted=h.record(now,v);
    if (expected.empty()) { first=last=now; expected.push_back(v); assert(accepted); }
    else if (!delta) assert(!accepted);
    else if (delta>=37) { expected.clear(); expected.push_back(v); first=last=now; assert(accepted); }
    else {
      for (uint32_t j=1;j<delta;++j) { auto missing=sample(H::MISSING); expected.push_back(missing); if(expected.size()>37){expected.pop_front();++first;} }
      expected.push_back(v); if(expected.size()>37){expected.pop_front();++first;} last=now; assert(accepted);
    }
    assert(h.size()==expected.size() && h.firstMinute()==first && h.lastMinute()==last);
    for(size_t k=0;k<expected.size();++k) for(size_t j=0;j<5;++j) assert(h.at(k)->values[j]==expected[k].values[j]);
  }
  puts("PASS: PSRAM-Verlauf: Minutengrenzen, Sensorlücken, Ringspeicher, Zeitsprünge.");
}
