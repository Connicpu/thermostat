/* display.h - e-paper task. Owns the SPI bus and the panel. */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "display_model.h"

esp_err_t display_init(void);              /* starts the display task */
/* Publish new content. urgent=true (user pressed a button) refreshes soon
 * with a partial update; otherwise changes are batched (>= 3 min apart). */
void display_post(const display_model_t *m, bool urgent);
/* Last time (ms since boot) the display task looped; used by the health check. */
uint64_t display_last_alive_ms(void);
bool     display_running(void);   /* false if display_init() failed */
