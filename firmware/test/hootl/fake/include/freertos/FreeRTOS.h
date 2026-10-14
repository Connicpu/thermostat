/* HOOTL fake FreeRTOS: a deterministic, cooperative virtual-time kernel (see fake/kernel.c).
 * Exactly one task runs at a time; a task runs until it blocks (delay, queue,
 * semaphore, notify). When every task is blocked the virtual clock jumps to the
 * next wake-up. 1 tick = 1 ms. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef struct SimTask *TaskHandle_t;
typedef struct SimQueue *QueueHandle_t;
typedef QueueHandle_t SemaphoreHandle_t;
typedef void (*TaskFunction_t)(void *);
typedef struct
{
    int dummy;
} portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(m)        ((void)(m))
#define portEXIT_CRITICAL(m)         ((void)(m))
#define portMAX_DELAY                0xFFFFFFFFu
#define portTICK_PERIOD_MS           1
#define pdTRUE                       1
#define pdFALSE                      0
#define pdPASS                       1
#define pdFAIL                       0
#define pdMS_TO_TICKS(ms)            ((TickType_t)(ms))
#define configTICK_RATE_HZ           1000
