#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "gfx.h"
esp_err_t ssd1680_bus_init(void);
/* Wakes the panel (HW reset + init), shows `fb`, then deep-sleeps it.
 * full=false: partial (differential) update against `prev`. */
esp_err_t ssd1680_show(const gfx_fb_t *fb, const gfx_fb_t *prev, bool full);
