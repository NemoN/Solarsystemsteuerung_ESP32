#pragma once
using SemaphoreHandle_t = void*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return reinterpret_cast<void*>(1);}
#ifdef MOCK_SEMAPHORE_BUSY
extern bool mockSemaphoreBusy;
inline int xSemaphoreTake(SemaphoreHandle_t,int){return mockSemaphoreBusy ? 0 : 1;}
#else
inline int xSemaphoreTake(SemaphoreHandle_t,int){return 1;}
#endif
inline int xSemaphoreGive(SemaphoreHandle_t){return 1;}
