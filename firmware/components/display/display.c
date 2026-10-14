/* display.c - e-paper refresh policy.
 * - Button-driven changes: partial refresh ~400 ms after the last change.
 * - Background changes (temp, humidity, link, running state): at most one
 *   refresh per 3 min (Good Display / Waveshare guidance), partial.
 * - A full refresh every 20 partials or every 6 h clears ghosting, and
 *   whenever DisplayModel.fullEpoch changes (leaving the menu). */
#include "core.h"
#include "display.h"
#include "ssd1680.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#define URGENT_SETTLE_MS     400
#define BACKGROUND_MIN_MS    (3 * 60 * 1000)
#define FULL_EVERY_PARTIALS  20
#define FULL_EVERY_MS        (6ULL * 3600 * 1000)

static const char *kTag = "display";
static SemaphoreHandle_t s_mtx;
static TaskHandle_t s_task;
static DisplayModel s_model;
static bool s_dirty, s_urgent;
static U64 s_urgentAt;
static volatile U64 s_alive;
static GfxFb s_fb, s_prev;

static U64 NowMs(void) { return (U64)(esp_timer_get_time() / 1000); }

U64 DisplayLastAliveMs(void) { return s_alive; }
bool DisplayRunning(void) { return s_task != NULL; }

void DisplayPost(const DisplayModel *pM, bool urgent)
{
    if (!s_mtx) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (memcmp(&s_model, pM, sizeof *pM) != 0 || urgent) {
        s_model = *pM;
        s_dirty = true;
        if (urgent) { s_urgent = true; s_urgentAt = NowMs(); }
    }
    xSemaphoreGive(s_mtx);
    if (urgent && s_task) xTaskNotifyGive(s_task);
}

static void DisplayTask(void *pArg)
{
    U64 lastRefresh = 0, lastFull = 0;
    int partials = 0;
    U8 epoch = 0;
    bool first = true;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
        U64 now = NowMs();
        s_alive = now;

        DisplayModel m;
        bool go = false;
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        if (s_dirty) {
            if (s_urgent) go = now - s_urgentAt >= URGENT_SETTLE_MS;
            else go = first || now - lastRefresh >= BACKGROUND_MIN_MS;
        }
        if (go) { m = s_model; s_dirty = s_urgent = false; }
        xSemaphoreGive(s_mtx);
        if (!go) continue;

        DisplayRender(&s_fb, &m);
        bool wantFull = m.fullEpoch != epoch;   /* menu exit / reset warning: clear ghosting */
        epoch = m.fullEpoch;
        if (!first && !wantFull && memcmp(&s_fb, &s_prev, sizeof s_fb) == 0) continue;
        bool full = first || wantFull || partials >= FULL_EVERY_PARTIALS || now - lastFull >= FULL_EVERY_MS;
        esp_err_t err = Ssd1680Show(&s_fb, &s_prev, full);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "refresh failed: %s", esp_err_to_name(err));
            first = true;              /* retry with a full refresh next time */
            lastRefresh = now;
            continue;
        }
        s_prev = s_fb;
        lastRefresh = NowMs();
        if (full) { lastFull = lastRefresh; partials = 0; } else partials++;
        first = false;
    }
}

esp_err_t DisplayInit(void)
{
    esp_err_t err = Ssd1680BusInit();
    if (err != ESP_OK) return err;
    s_mtx = xSemaphoreCreateMutex();
    GfxClear(&s_prev);
    s_alive = NowMs();
    xTaskCreate(DisplayTask, "display", 4096, NULL, 3, &s_task);
    return ESP_OK;
}
