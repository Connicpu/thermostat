/* display.h - e-paper task. Owns the SPI bus and the panel. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include "esp_err.h"
#include "display_model.h"

/**
 * Bring up the SPI bus and panel GPIOs (Ssd1680BusInit()), create the model mutex, and start
 * the display task (4 KB stack, priority 3). The first refresh after boot is a full one.
 *
 * Returns ESP_OK, or the bus init error (no task is started; DisplayRunning() stays false).
 */
esp_err_t DisplayInit(void);

/**
 * Publish new screen content. Thread-safe (mutex); never blocks on the panel.
 *
 * The model is copied and compared with memcmp(); unchanged content is ignored unless urgent.
 * urgent=true (the user pressed a button): the task is woken and refreshes URGENT_SETTLE_MS
 * (400 ms) after the last urgent post. Otherwise changes are batched to at most one refresh per
 * 3 min. A full (de-ghosting) refresh is used every 20 partials, every 6 h, after a failed
 * refresh, and whenever pM->fullEpoch changes; all other refreshes are partial.
 * A no-op before DisplayInit().
 */
void DisplayPost(const DisplayModel *pM, bool urgent);

/**
 * Returns the monotonic time (ms since boot) at which the display task last ran its loop (every
 * ≤200 ms); used by the health check to detect a stalled panel task.
 */
U64 DisplayLastAliveMs(void);

/** Returns true if the display task was started (DisplayInit() succeeded). */
bool DisplayRunning(void);
