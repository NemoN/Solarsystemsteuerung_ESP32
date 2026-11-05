#pragma once
#include <ESPAsyncWebServer.h>

// Initialize before starting either HTTP server. The callback closes both
// websocket servers whenever their authorization may have changed.
void beginWebAccess(void (*closeSockets)());
void attachWebAccess(AsyncWebServer& server, bool mainServer);
void processWebAccess();
