#include "SolarControl.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  SolarControl pump;
  assert(!SolarControl::validStart(2.99f) && !SolarControl::validStart(10.01f));
  assert(SolarControl::validStart(3) && SolarControl::validStart(10) && SolarControl::validStart(5.5f));
  assert(!SolarControl::validStart(5.55f));
  assert(!SolarControl::validStart(NAN) && !SolarControl::validStart(INFINITY));
  assert(!pump.update(true,true,44.9f,true,40,5));
  assert(pump.update(true,true,45,true,40,5));              // >= Start
  assert(pump.update(true,true,42,true,40,5));              // Hysterese halten
  assert(!pump.update(true,true,41,true,40,5));             // <= Stopp
  assert(!pump.update(true,true,42,true,40,5));
  assert(pump.update(true,true,43,true,40,3));
  assert(!pump.update(true,false,43,true,40,3));           // Panelfehler
  assert(pump.update(true,true,50,true,40,10));
  assert(!pump.update(true,true,50,false,40,10));           // Speicherfühlerfehler
  assert(!pump.update(true,true,NAN,true,40,3));
  assert(!pump.update(true,true,50,true,INFINITY,3));
  assert(!pump.update(true,true,50,true,40,2));
  assert(pump.update(true,true,50,true,40,10));
  assert(!pump.update(false,true,50,true,40,10));           // Update / Neustart
  assert(!pump.update(true,true,42,true,40,3));             // kein Wiederanlauf unter Start
  std::puts("PASS: Solargrenzen, Hysterese, Sensorfehler und Sperren.");
}
