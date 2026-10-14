#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "gfx.h"
esp_err_t Ssd1680BusInit(void);
/* Wakes the panel (HW reset + init), shows `fb`, then deep-sleeps it.
 * full=false: partial (differential) update against `prev`. */
esp_err_t Ssd1680Show(const GfxFb *pFb, const GfxFb *pPrev, bool full);
