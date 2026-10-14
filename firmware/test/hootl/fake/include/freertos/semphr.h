#pragma once
#include "freertos/queue.h"
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t s);
