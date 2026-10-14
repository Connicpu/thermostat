/* display.c - e-paper refresh policy.
 * - Button-driven changes: partial refresh ~400 ms after the last change.
 * - Background changes (temp, humidity, link, running state): at most one
 *   refresh per 3 min (Good Display / Waveshare guidance), partial.
 * - A full refresh every 20 partials or every 6 h clears ghosting, and
 *   whenever display_model_t.full_epoch changes (leaving the menu). */
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

static const char *TAG = "display";
static SemaphoreHandle_t s_mtx;
static TaskHandle_t s_task;
static display_model_t s_model;
static bool s_dirty, s_urgent;
static uint64_t s_urgent_at;
static volatile uint64_t s_alive;
static gfx_fb_t s_fb, s_prev;

static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

uint64_t display_last_alive_ms(void) { return s_alive; }
bool display_running(void) { return s_task != NULL; }

void display_post(const display_model_t *m, bool urgent)
{
    if (!s_mtx) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (memcmp(&s_model, m, sizeof *m) != 0 || urgent) {
        s_model = *m;
        s_dirty = true;
        if (urgent) { s_urgent = true; s_urgent_at = now_ms(); }
    }
    xSemaphoreGive(s_mtx);
    if (urgent && s_task) xTaskNotifyGive(s_task);
}

static void display_task(void *arg)
{
    uint64_t last_refresh = 0, last_full = 0;
    int partials = 0;
    uint8_t epoch = 0;
    bool first = true;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
        uint64_t now = now_ms();
        s_alive = now;

        display_model_t m;
        bool go = false;
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        if (s_dirty) {
            if (s_urgent) go = now - s_urgent_at >= URGENT_SETTLE_MS;
            else go = first || now - last_refresh >= BACKGROUND_MIN_MS;
        }
        if (go) { m = s_model; s_dirty = s_urgent = false; }
        xSemaphoreGive(s_mtx);
        if (!go) continue;

        display_render(&s_fb, &m);
        bool want_full = m.full_epoch != epoch;   /* menu exit / reset warning: clear ghosting */
        epoch = m.full_epoch;
        if (!first && !want_full && memcmp(&s_fb, &s_prev, sizeof s_fb) == 0) continue;
        bool full = first || want_full || partials >= FULL_EVERY_PARTIALS || now - last_full >= FULL_EVERY_MS;
        esp_err_t err = ssd1680_show(&s_fb, &s_prev, full);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "refresh failed: %s", esp_err_to_name(err));
            first = true;              /* retry with a full refresh next time */
            last_refresh = now;
            continue;
        }
        s_prev = s_fb;
        last_refresh = now_ms();
        if (full) { last_full = last_refresh; partials = 0; } else partials++;
        first = false;
    }
}

esp_err_t display_init(void)
{
    esp_err_t err = ssd1680_bus_init();
    if (err != ESP_OK) return err;
    s_mtx = xSemaphoreCreateMutex();
    gfx_clear(&s_prev);
    s_alive = now_ms();
    xTaskCreate(display_task, "display", 4096, NULL, 3, &s_task);
    return ESP_OK;
}
