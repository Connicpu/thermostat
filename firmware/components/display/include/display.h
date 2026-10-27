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
 * Publish new screen content. Thread-safe (mutex); never blocks on the panel. The panel BUSY
 * wait lives in the display task (vTaskDelay), so this returns without waiting on the glass and
 * the control task's watchdog kick is unaffected.
 *
 * The model is copied and compared with memcmp(); unchanged content is ignored unless urgent.
 * urgent=true (a button, or a sensor-fault / "Starting sensor..." edge): the task is woken and
 * refreshes URGENT_SETTLE_MS (400 ms) after the last urgent post. Otherwise the clock and
 * sensor/link/running changes share one background slot: the next local-minute rollover once
 * pM->clockKnown (about 60 s, from the extrapolated minute, not a free-running 60 s timer), or at
 * most one refresh per 3 min while the
 * clock is unknown. A full (de-ghosting) refresh runs when 3 h have passed AND 40 partials have
 * been shown (the later of the two), or at 6 h, after a failed refresh (retried a few seconds
 * later while the model is still dirty, not held for the next minute), and whenever
 * pM->fullEpoch changes; all other refreshes are partial. A no-op before DisplayInit().
 */
void DisplayPost(const DisplayModel *pM, bool urgent);

/**
 * Returns the monotonic time (ms since boot) at which the display task last ran its loop (every
 * ≤200 ms); used by the health check to detect a stalled panel task. The task can stamp this
 * after the app task has already sampled its clock, so compare the two with
 * DisplayHeartbeatStalled() rather than subtracting them.
 */
U64 DisplayLastAliveMs(void);

/** Returns true if the display task was started (DisplayInit() succeeded). */
bool DisplayRunning(void);
