/* display.h - e-paper task. Owns the SPI bus and the panel. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include "esp_err.h"
#include "display_model.h"

esp_err_t DisplayInit(void);              /* starts the display task */
/* Publish new content. urgent=true (user pressed a button) refreshes soon
 * with a partial update; otherwise changes are batched (>= 3 min apart). */
void DisplayPost(const DisplayModel *pM, bool urgent);
/* Last time (ms since boot) the display task looped; used by the health check. */
U64 DisplayLastAliveMs(void);
bool     DisplayRunning(void);   /* false if DisplayInit() failed */
