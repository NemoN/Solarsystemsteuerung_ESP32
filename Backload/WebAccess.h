#pragma once
#ifdef BACKLOAD_HOST_TEST
#include <ESPAsyncWebServer.h>
#else
#include "src/ESPAsyncWebServer/src/ESPAsyncWebServer.h"
#endif

// Initialize before starting either HTTP server. The callback closes both
// websocket servers whenever their authorization may have changed.
void beginWebAccess(void (*closeSockets)());
void attachWebAccess(AsyncWebServer& server, bool mainServer);
void processWebAccess();
