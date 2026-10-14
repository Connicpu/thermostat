#pragma once
#include "freertos/FreeRTOS.h"
BaseType_t xTaskCreate(
    TaskFunction_t fn,
    const char *name,
    uint32_t stack,
    void *arg,
    UBaseType_t prio,
    TaskHandle_t *out);
void vTaskDelete(TaskHandle_t t);
void vTaskDelay(TickType_t ticks);
void vTaskDelayUntil(TickType_t *prev, TickType_t inc);
TickType_t xTaskGetTickCount(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait);
BaseType_t xTaskNotifyGive(TaskHandle_t t);
