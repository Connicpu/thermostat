/* HOOTL fake: esp_timer on the virtual clock. Callbacks run in the sim "esp_timer" task. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
typedef struct SimTimer *esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void *arg);
typedef enum { ESP_TIMER_TASK } esp_timer_dispatch_t;
typedef struct { esp_timer_cb_t callback; void *arg; esp_timer_dispatch_t dispatch_method; const char *name; bool skip_unhandled_events; } esp_timer_create_args_t;
int64_t   esp_timer_get_time(void);
esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *out);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t period_us);
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us);
esp_err_t esp_timer_stop(esp_timer_handle_t t);
