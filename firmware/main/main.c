/* main.c - Thermostat V3 application: wires sensor -> control -> outputs,
 * buttons/display, Zigbee, persistence and the external watchdog.
 *
 * Task layout
 *   app (this file, prio 6) : 250 ms loop. Sensor read, control step,
 *                             output write, UI events, Zigbee publish,
 *                             health check + STWD100 kick.
 *   display (prio 3)        : e-paper rendering/refresh (slow, blocking).
 *   zigbee (prio 5)         : ZBOSS main loop.
 *   esp_timer               : 10 ms button sampling -> queue.
 */
#include <string.h>
#include "control.h"
#include "display.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_pm.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hvac_outputs.h"
#include "hw_watchdog.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sensor.h"
#include "settings.h"
#include "ui.h"
#include "zb_thermostat.h"

static const char *TAG = "app";

#define LOOP_MS             250
#define SAVE_DELAY_MS       5000          /* coalesce NVS writes */
#define HWDT_ARM_AFTER_MS   10000         /* arm STWD100 once the loop is proven healthy */
#define DISPLAY_STALL_MS    30000         /* display task must loop at least this often */
#define OTA_VALID_AFTER_MS  60000
#define BANNER_MS           4000

static const bool FAHRENHEIT = CONFIG_THERMO_DISPLAY_FAHRENHEIT;

static SemaphoreHandle_t s_mtx;     /* guards s_persist */
static persist_t s_persist;
static bool s_dirty;
static uint64_t s_dirty_at;
static QueueHandle_t s_btn_q;
static volatile uint16_t s_identify_s;
static volatile int s_ota_pct = -1;

typedef struct { uint8_t which, ev; } btn_msg_t;

static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

/* ---------------- shared settings ---------------- */

static void settings_update(const ctrl_settings_t *s, bool keep_heat)
{
    ctrl_params_t p = CTRL_PARAMS_DEFAULT();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.s = *s;
    ctrl_sanitize_settings(&p, &s_persist.s, keep_heat);
    s_dirty = true;
    s_dirty_at = now_ms();
    xSemaphoreGive(s_mtx);
}

static persist_t settings_get(void)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    persist_t p = s_persist;
    xSemaphoreGive(s_mtx);
    return p;
}

/* ---------------- Zigbee callbacks (Zigbee task) ---------------- */

static void zb_on_settings(const ctrl_settings_t *s, bool keep_heat) { settings_update(s, keep_heat); }

static void zb_on_calib(int8_t c)
{
    if (c > 25) c = 25;
    if (c < -25) c = -25;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.calib_tenths = c;
    s_dirty = true;
    s_dirty_at = now_ms();
    xSemaphoreGive(s_mtx);
}

static void zb_on_identify(uint16_t secs) { s_identify_s = secs; }
static void zb_on_ota(int pct) { s_ota_pct = pct; }
static void zb_before_reboot(void) { hvac_outputs_all_off(); }

/* ---------------- buttons (esp_timer task -> queue) ---------------- */

static void on_button(ui_button_t which, btn_event_t ev)
{
    btn_msg_t m = {(uint8_t)which, (uint8_t)ev};
    xQueueSend(s_btn_q, &m, 0);
}

/* Step a setpoint by one display unit (1 °F or 0.5 °C). */
static int16_t step_sp(int16_t sp, int dir)
{
    if (FAHRENHEIT) {
        int32_t f10 = (int32_t)sp * 9 / 5 + 3200;                 /* c°F */
        int32_t f = (f10 >= 0 ? f10 + 50 : f10 - 50) / 100 + dir; /* whole °F */
        int32_t c = ((f * 100 - 3200) * 5 + (f >= 32 ? 4 : -4)) / 9;
        return (int16_t)c;
    }
    int32_t half = (sp + (sp >= 0 ? 25 : -25)) / 50 + dir;
    return (int16_t)(half * 50);
}

static const char *s_banner;
static uint64_t s_banner_until;

static void banner(const char *b)
{
    s_banner = b;
    s_banner_until = now_ms() + BANNER_MS;
}

/* returns true if the display should refresh promptly */
static bool handle_button(const btn_msg_t *m)
{
    persist_t p = settings_get();
    ctrl_settings_t s = p.s;
    int dir = m->which == UI_BTN_UP ? 1 : -1;

    switch (m->which) {
    case UI_BTN_UP:
    case UI_BTN_DOWN:
        if (m->ev != BTN_EV_PRESS && m->ev != BTN_EV_REPEAT) return false;
        switch (s.mode) {
        case CTRL_MODE_HEAT: s.heat_sp = step_sp(s.heat_sp, dir); settings_update(&s, true); break;
        case CTRL_MODE_COOL: s.cool_sp = step_sp(s.cool_sp, dir); settings_update(&s, false); break;
        case CTRL_MODE_AUTO: {
            int16_t nh = step_sp(s.heat_sp, dir);
            s.cool_sp = (int16_t)(s.cool_sp + (nh - s.heat_sp));
            s.heat_sp = nh;
            settings_update(&s, dir > 0 ? false : true);
            break;
        }
        default: banner("Mode is OFF - press MODE"); break;
        }
        return true;
    case UI_BTN_MODE:
        if (m->ev == BTN_EV_PRESS) {
            s.mode = ctrl_next_mode(s.mode);
            settings_update(&s, true);
            return true;
        }
        if (m->ev == BTN_EV_LONG) {
            banner("Pairing... (keep holding to reset)");
            zb_thermostat_pair();
            return true;
        }
        if (m->ev == BTN_EV_VLONG) {
            ESP_LOGW(TAG, "factory reset");
            hvac_outputs_all_off();
            settings_erase();
            display_model_t dm = {0};
            dm.banner = "FACTORY RESET";
            display_post(&dm, true);
            for (int i = 0; i < 12; i++) {          /* ~3 s for the panel to show it; keep the dogs fed */
                vTaskDelay(pdMS_TO_TICKS(250));
                esp_task_wdt_reset();
                if (hw_watchdog_armed()) hw_watchdog_kick();
            }
            zb_thermostat_factory_reset();          /* erases Zigbee NVRAM and restarts */
            esp_restart();
        }
        return false;
    }
    return false;
}

/* ---------------- LED ---------------- */

static void led_update(uint64_t now, const ctrl_output_t *o, link_state_t link)
{
    bool on;
    if (s_identify_s)                     on = (now / 250) & 1;          /* identify: 2 Hz */
    else if (o->fault != CTRL_FAULT_NONE) on = (now / 125) & 1;          /* fault: fast */
    else if (link == LINK_PAIRING)        on = (now % 2000) < 100;       /* pairing: blip */
    else                                  on = false;                    /* keep heat off the sensor */
    ui_led(on);
}

/* ---------------- main loop ---------------- */

static void app_task(void *arg)
{
    ctrl_params_t prm = CTRL_PARAMS_DEFAULT();
    ctrl_t ctl;
    ctrl_init(&ctl, &prm, now_ms());
    esp_task_wdt_add(NULL);

    sensor_reading_t rd = {0};
    uint64_t next_sensor = 0, boot = now_ms(), last_ident_tick = boot;
    bool ota_marked = false;

    for (;;) {
        TickType_t wake = xTaskGetTickCount();
        uint64_t now = now_ms();
        persist_t p = settings_get();
        bool urgent = false;

        /* 1. sensor */
        if (now >= next_sensor) {
            next_sensor = now + CONFIG_THERMO_SENSOR_PERIOD_MS;
            rd = sensor_read((int16_t)(CONFIG_THERMO_SELF_HEAT_OFFSET_C100 + p.calib_tenths * 10));
            ctrl_input_t in = {.valid = rd.valid, .temp = rd.temp_c100};
            ctrl_feed_sensor(&ctl, &in, now);
        }

        /* 2. buttons */
        btn_msg_t m;
        while (xQueueReceive(s_btn_q, &m, 0) == pdTRUE) urgent |= handle_button(&m);
        p = settings_get();

        /* 3. control + outputs (the interlock lives in hvac_outputs_set) */
        ctrl_output_t o = ctrl_step(&ctl, &p.s, now);
        hvac_state_t act = hvac_outputs_set((hvac_state_t){.w = o.w, .y = o.y, .g = o.g});

        /* 4. Zigbee */
        zb_thermostat_state_t zs = {
            .temp_valid = rd.valid && o.fault != CTRL_FAULT_TEMP_RANGE, .temp = rd.temp_c100,
            .rh_valid = rd.valid, .rh = rd.rh_c100, .s = p.s, .w = act.w, .y = act.y, .g = act.g,
        };
        zb_thermostat_publish(&zs);
        link_state_t link = zb_thermostat_link();

        /* 5. display */
        static char ota_banner[24];
        display_model_t dm;
        memset(&dm, 0, sizeof dm);
        dm.temp_valid = zs.temp_valid;
        /* quantise so sensor noise does not cause refreshes */
        dm.temp = (int16_t)((rd.temp_c100 + (rd.temp_c100 >= 0 ? 5 : -5)) / 10 * 10);
        dm.rh_valid = rd.valid;
        dm.rh = (uint16_t)((rd.rh_c100 + 50) / 100 * 100);
        dm.s = p.s;
        dm.w = act.w; dm.y = act.y; dm.g = act.g; dm.waiting = o.waiting;
        dm.fault = o.fault;
        dm.link = link;
        dm.fahrenheit = FAHRENHEIT;
        if (s_ota_pct >= 0) {
            snprintf(ota_banner, sizeof ota_banner, "Updating firmware %d%%", s_ota_pct);
            dm.banner = ota_banner;
        } else if (s_banner && now < s_banner_until) {
            dm.banner = s_banner;
        }
        display_post(&dm, urgent);

        /* 6. persistence */
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        bool save = s_dirty && now - s_dirty_at >= SAVE_DELAY_MS;
        if (save) s_dirty = false;
        persist_t snap = s_persist;
        xSemaphoreGive(s_mtx);
        if (save) settings_save(&snap);

        /* 7. LED / identify */
        if (s_identify_s && now - last_ident_tick >= 1000) { s_identify_s--; last_ident_tick = now; }
        if (!s_identify_s) last_ident_tick = now;
        led_update(now, &o, link);

        /* 8. health check -> watchdogs. Kick only if every check passed. */
        bool healthy = (now - display_last_alive_ms()) < DISPLAY_STALL_MS &&
                       hvac_outputs_interlock_faults() == 0;
        if (healthy) {
            esp_task_wdt_reset();
#if CONFIG_THERMO_HW_WDT_ENABLE
            if (!hw_watchdog_armed() && now - boot >= HWDT_ARM_AFTER_MS) {
                ESP_LOGI(TAG, "arming STWD100");
                hw_watchdog_arm();
            }
            if (hw_watchdog_armed()) hw_watchdog_kick();
#endif
        } else {
            ESP_LOGE(TAG, "unhealthy: outputs off, letting watchdog bite");
            hvac_outputs_all_off();
        }

        /* 9. OTA rollback protection: confirm the image after a healthy minute */
        if (!ota_marked && healthy && o.fault == CTRL_FAULT_NONE && now - boot >= OTA_VALID_AFTER_MS) {
            esp_ota_mark_app_valid_cancel_rollback();
            ota_marked = true;
        }

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(LOOP_MS));
    }
}

void app_main(void)
{
    /* Outputs first: guarantee a known-off state before anything else. */
    hvac_outputs_init();
    hw_watchdog_init();      /* disarmed until the main loop proves itself */

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

#if CONFIG_PM_ENABLE
    esp_pm_config_t pm = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm));
#endif

    s_mtx = xSemaphoreCreateMutex();
    s_btn_q = xQueueCreate(16, sizeof(btn_msg_t));
    settings_load(&s_persist);
    ESP_LOGI(TAG, "settings: mode %d heat %d cool %d fan %d calib %d", s_persist.s.mode, s_persist.s.heat_sp,
             s_persist.s.cool_sp, s_persist.s.fan, s_persist.calib_tenths);

    if (sensor_init() != ESP_OK) ESP_LOGE(TAG, "sensor init failed (control stays in failsafe)");
    if (display_init() != ESP_OK) ESP_LOGE(TAG, "display init failed");
    ESP_ERROR_CHECK(ui_init(on_button));

    zb_thermostat_cbs_t cbs = {
        .on_settings = zb_on_settings,
        .on_calibration = zb_on_calib,
        .on_identify = zb_on_identify,
        .on_ota_progress = zb_on_ota,
        .before_ota_reboot = zb_before_reboot,
    };
    ESP_ERROR_CHECK(zb_thermostat_start(&cbs, &s_persist.s, s_persist.calib_tenths));

    xTaskCreate(app_task, "app", 4096, NULL, 6, NULL);
}
