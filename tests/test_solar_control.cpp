#include "SolarControl.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  SolarPanelGrace grace;
  assert(!grace.hasReading());
  assert(!grace.canHold(0,true));
  grace.record(60,UINT32_MAX-5);
  assert(grace.hasReading());
  assert(grace.ageMs(5)==11);
  assert(grace.ageMs(59994)==60000);
  assert(grace.canHold(5,true) && grace.temperature()==60);
  assert(!grace.canHold(5,false));
  assert(grace.canHold(59994,true)); // Exakt 60 Sekunden, auch über millis()-Überlauf.
  assert(!grace.canHold(59995,true));
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
  assert(!pump.setManualOn(true));                          // Handbefehl nur im Handbetrieb
  pump.setManual(true);assert(pump.manual()&&!pump.on()&&!pump.manualOn());
  assert(pump.setManualOn(true));
  assert(pump.update(true,false,NAN,false,NAN,NAN));       // Sensoren und Grenzen im Handbetrieb übergehen
  assert(pump.on()&&pump.manualOn());
  pump.setManual(true);assert(pump.on()&&pump.manualOn()); // Wiederholte Auswahl verändert Zustand nicht
  assert(pump.setManualOn(false));assert(!pump.update(true,false,NAN,false,NAN,NAN));
  assert(pump.setManualOn(true));assert(pump.update(true,false,NAN,false,NAN,NAN));
  assert(!pump.update(false,false,NAN,false,NAN,NAN));    // Update/Neustart sperrt auch den Handbetrieb
  assert(!pump.manualOn()&&!pump.update(true,false,NAN,false,NAN,NAN));
  pump.setManual(false);assert(!pump.manual()&&!pump.on());
  assert(!pump.update(true,false,NAN,false,NAN,5));         // Sensorprüfung in Automatik wieder aktiv
  assert(pump.update(true,true,50,true,40,5));
  std::puts("PASS: Solargrenzen, Hysterese, Sensorfehler und Sperren.");
}
