#pragma once
#include <cstdint>
using TaskFunction_t = void(*)(void*);
constexpr int pdPASS=1;
constexpr int portMAX_DELAY=-1;
inline uint32_t pdMS_TO_TICKS(uint32_t ms){return ms;}
extern TaskFunction_t mockTask;
extern void* mockTaskArg;
void mockAdvance(uint32_t);
inline int xTaskCreate(TaskFunction_t fn,const char*,uint32_t,void* arg,int,void*){mockTask=fn;mockTaskArg=arg;return pdPASS;}
inline void vTaskDelay(uint32_t n){mockAdvance(n);}
inline void vTaskDelete(void*){}
