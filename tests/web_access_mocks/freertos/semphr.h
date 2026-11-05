#pragma once
using SemaphoreHandle_t = void*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return (void*)1;}
inline int xSemaphoreTake(SemaphoreHandle_t,int){return 1;}
inline void xSemaphoreGive(SemaphoreHandle_t){}
