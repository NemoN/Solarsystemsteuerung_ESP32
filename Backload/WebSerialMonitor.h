#pragma once

#include <Print.h>

// Gleiche print/println/printf-Schnittstelle wie Serial, zusätzlich im Browser.
// Die seriellen Ausgänge werden im Hauptsketch vor dem Monitorstart initialisiert.
extern Print& FirmwareLog;

void beginWebSerialMonitor();
void processWebSerialMonitor();
