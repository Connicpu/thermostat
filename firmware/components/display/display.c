/* display.c - e-paper refresh policy.
 * - Button-driven changes: partial refresh ~400 ms after the last one.
 * - A sensor-fault or "Starting sensor..." edge the app task has a token for uses that
 *   same path. The app task's bucket is 3 tokens, with one token back every 60 s.
 * - One background slot, shared by the clock and by sensor/link/running changes:
 *   the next local minute once the Zigbee clock is known, else at most one
 *   refresh per 3 min. No separate clock-only path.
 * - A full refresh when 3 h have passed AND 40 partials have been shown, or at
 *   6 h, and whenever DisplayModel.fullEpoch changes (leaving the menu). */
#include "core.h"
#include "display.h"
#include "display_policy.h"
#include "ssd1680.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#define URGENT_SETTLE_MS 400

/**
 * Display state. s_mtx guards s_model, s_dirty, s_urgent and s_urgentAt (shared with DisplayPost());
 * s_alive is the task's heartbeat (ms); s_fb/s_prev are only touched by the display task (current
 * frame and last frame shown, for partial updates and skip-if-unchanged).
 */
static const char *kTag = "display";
static SemaphoreHandle_t s_mtx;
static TaskHandle_t s_task;
static DisplayModel s_model;
static bool s_dirty, s_urgent;
static U64 s_urgentAt;
static volatile U64 s_alive;
static GfxFb s_fb, s_prev;

/** See display.h. */
U64 DisplayLastAliveMs(void)
{
    return s_alive;
}

/** See display.h. */
bool DisplayRunning(void)
{
    return s_task != NULL;
}

/** Copy the model under the mutex, wake the task if urgent. See display.h. */
void DisplayPost(const DisplayModel *pM, bool urgent)
{
    if (!s_mtx)
        return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (memcmp(&s_model, pM, sizeof *pM) != 0 || urgent)
    {
        s_model = *pM;
        s_dirty = true;
        if (urgent)
        {
            s_urgent = true;
            s_urgentAt = NowMs();
        }
    }
    xSemaphoreGive(s_mtx);
    if (urgent && s_task)
        xTaskNotifyGive(s_task);
}

/**
 * Record the local minute already written into pM->localMinute (extrapolated before render), so a
 * skipped identical frame does not immediately schedule another one. A failed show does not call
 * this: the minute was not painted, and the retry is the first-and-dirty backoff.
 */
static void NotePaintedMinute(const DisplayModel *pM, int *pPainted)
{
    if (pM->clockKnown)
        *pPainted = (int)pM->localMinute;
}

/**
 * Display task. Every ≤200 ms (or when notified): heartbeat, then decide whether to refresh
 * (urgent settle time, or the single background slot), render the model, skip if the frame is
 * unchanged, choose full vs partial, and push it to the panel. A failed refresh is logged and the
 * next one is a full refresh. Ssd1680Show waits on BUSY in this task only (vTaskDelay), so the
 * control task and its watchdog kick keep running; s_alive is refreshed each loop and a single
 * panel wait (a few seconds) stays under the app task's display-stall threshold. The stamp can
 * land after the app task has already sampled its clock; DisplayHeartbeatStalled() treats a
 * heartbeat ahead of that sample as alive.
 */
static void DisplayTask(void *pArg)
{
    U64 lastRefresh = 0, lastFull = 0;
    int partials = 0;
    int paintedMinute = -1; /* -1 until a minute has been shown */
    U8 epoch = 0;
    bool first = true;
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
        U64 now = NowMs();
        s_alive = now;

        DisplayModel m;
        bool go = false;
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        if (s_urgent)
            go = s_dirty && now - s_urgentAt >= URGENT_SETTLE_MS;
        else
            go = DisplayBackgroundDue(
                first,
                s_model.clockKnown,
                s_dirty,
                now,
                lastRefresh,
                s_model.localMinute,
                s_model.minuteStartMs,
                paintedMinute);
        if (go)
        {
            m = s_model;
            s_dirty = s_urgent = false;
        }
        xSemaphoreGive(s_mtx);
        if (!go)
            continue;

        if (m.clockKnown)
            m.localMinute = DisplayClockMinute(m.localMinute, m.minuteStartMs, now);
        DisplayRender(&s_fb, &m);
        bool wantFull = m.fullEpoch != epoch; /* menu exit / reset warning: clear ghosting */
        epoch = m.fullEpoch;
        if (!first && !wantFull && memcmp(&s_fb, &s_prev, sizeof s_fb) == 0)
        {
            /* Clock known and the minute did not change the glass (time dropped behind a long
             * banner): remember the minute so the next loop does not spin. */
            if (m.clockKnown)
            {
                lastRefresh = now;
                NotePaintedMinute(&m, &paintedMinute);
            }
            continue;
        }
        bool full = DisplayRefreshIsFull(first, wantFull, partials, now, lastFull);
        /* Panel BUSY is polled with vTaskDelay inside Ssd1680Show, on this task only. */
        esp_err_t err = Ssd1680Show(&s_fb, &s_prev, full);
        if (err != ESP_OK)
        {
            ESP_LOGE(kTag, "refresh failed: %s", esp_err_to_name(err));
            first = true; /* retry with a full refresh next time */
            lastRefresh = now;
            /* Do not record the minute: it is not on the glass. Mark the model dirty again so
             * the clock-known path retries after DISPLAY_FAIL_BACKOFF_MS instead of the next
             * rollover. The backoff is what keeps a dead panel off the 200 ms loop. */
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_dirty = true;
            xSemaphoreGive(s_mtx);
            continue;
        }
        s_prev = s_fb;
        lastRefresh = NowMs();
        NotePaintedMinute(&m, &paintedMinute);
        /* After a successful show, so a model update that never reaches the glass is not logged. */
        if (m.view.kind == kViewHome || m.view.kind == kViewHomeBanner)
            ESP_LOGI(kTag, "EVT panel %s", DisplayRunWord(m.fault, m.sensorStarting, m.w, m.y, m.g, m.waiting));
        if (full)
        {
            lastFull = lastRefresh;
            partials = 0;
        }
        else
            partials++;
        first = false;
    }
}

/** See display.h. */
esp_err_t DisplayInit(void)
{
    esp_err_t err = Ssd1680BusInit();
    if (err != ESP_OK)
        return err;
    s_mtx = xSemaphoreCreateMutex();
    GfxClear(&s_prev);
    s_alive = NowMs();
    xTaskCreate(DisplayTask, "display", 4096, NULL, 3, &s_task);
    return ESP_OK;
}
