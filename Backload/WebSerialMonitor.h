#pragma once

#include <Print.h>

// Gleiche print/println/printf-Schnittstelle wie Serial, zusätzlich im Browser.
// Serial.begin() bleibt im Hauptsketch; USB funktioniert auch vor dem Monitorstart.
extern Print& FirmwareLog;

void beginWebSerialMonitor();
void processWebSerialMonitor();
