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
#include "sensor_stuck.h"
#include "settings.h"
#include "schedule.h"
#include "ui.h"
#include "menu.h"
#include "fw_version.h"
#include "zb_thermostat.h"

static const char *TAG = "app";

#define LOOP_MS             250
#define SAVE_DELAY_MS       5000          /* coalesce NVS writes */
#define HWDT_ARM_AFTER_MS   10000         /* arm STWD100 once the loop is proven healthy */
#define DISPLAY_STALL_MS    30000         /* display task must loop at least this often */
#define OTA_VALID_AFTER_MS  60000      /* min healthy uptime before confirming a new image */
#define OTA_JOIN_TIMEOUT_MS (10 * 60 * 1000) /* roll back if never rejoined Zigbee by then */
#define BANNER_MS           4000
#define BOOT_LOOP_LIMIT     5             /* this many resets without BOOT_STABLE_MS uptime -> safe mode */
#define BOOT_STABLE_MS      (3 * 60 * 1000)
#define SAFE_MODE_RETRY_MS  (30 * 60 * 1000) /* safe mode: try a normal boot again after this */

/* Structured lines for the HOOTL scenario runner / field logs: "EVT <what> ...". */
#define EVT(...) ESP_LOGI("evt", __VA_ARGS__)

static bool s_nvs_ok;                 /* settings can be persisted */
static bool s_safe_mode;              /* boot loop detected: outputs held off */
static bool s_outputs_ok = true;      /* hvac_outputs_init() succeeded */

/* on-device menu (pure state machine in components/ui/menu.c) */
static menu_t s_menu;
static uint8_t s_full_epoch;          /* bumped -> display does a full refresh */
static sensor_reading_t s_raw;        /* last reading, calibration removed (Device info) */

static SemaphoreHandle_t s_mtx;     /* guards s_persist */
static persist_t s_persist;
static bool s_dirty;
static uint64_t s_dirty_at;
static QueueHandle_t s_btn_q;
static volatile uint16_t s_identify_s;
static volatile int s_ota_pct = -1;

typedef struct { uint8_t which, ev; } btn_msg_t;
static QueueHandle_t s_boost_q;      /* uint8_t ZB_BOOST_CMD_* from Zigbee or the UP+DOWN combo */
static uint8_t s_boost_result;       /* ZB_BOOST_RESULT_* last outcome, reported to HA */
static bool s_boost_active;          /* mirror for the UI combo */

static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

/* ---------------- shared settings ---------------- */

/* Mode / fan / main setpoints. A heat or cool setpoint that differs from the
 * current program setpoint is stored in the CURRENT program period (day or
 * night, see schedule.h); with no clock that is the day period. */
static void settings_update(const ctrl_settings_t *s, bool keep_heat)
{
    (void)keep_heat;
    ctrl_params_t p = CTRL_PARAMS_DEFAULT();
    uint16_t minute = 0;
    bool known = zb_thermostat_local_minute(&minute);
    ctrl_settings_t n = *s;
    ctrl_sanitize_settings(&p, &n, true);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    int16_t h, c;
    sched_program_sp(&s_persist.sched, known, minute, &h, &c);
    if (n.heat_sp != h) sched_edit_current(&p, &s_persist.sched, true, known, minute, n.heat_sp);
    if (n.cool_sp != c) sched_edit_current(&p, &s_persist.sched, false, known, minute, n.cool_sp);
    s_persist.s.mode = n.mode;
    s_persist.s.fan = n.fan;
    s_dirty = true;
    s_dirty_at = now_ms();
    xSemaphoreGive(s_mtx);
}

/* Snapshot; s.heat_sp / s.cool_sp are the current program setpoints (no morning boost). */
static persist_t settings_get(void)
{
    uint16_t minute = 0;
    bool known = zb_thermostat_local_minute(&minute);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    persist_t p = s_persist;
    xSemaphoreGive(s_mtx);
    sched_program_sp(&p.sched, known, minute, &p.s.heat_sp, &p.s.cool_sp);
    return p;
}

static const char *s_banner;
static uint64_t s_banner_until;

static void banner(const char *b)
{
    s_banner = b;
    s_banner_until = now_ms() + BANNER_MS;
}


/* ---------------- Zigbee callbacks (Zigbee task) ---------------- */

static void zb_on_settings(const ctrl_settings_t *s, uint8_t what)
{
    /* Start from the live program setpoints so a mode/fan write can never copy a
     * stale setpoint into a period that just started; then apply the one field written. */
    persist_t p = settings_get();
    ctrl_settings_t n = p.s;
    n.mode = s->mode;
    n.fan = s->fan;
    if (what == ZB_SET_HEAT_SP) n.heat_sp = s->heat_sp;
    if (what == ZB_SET_COOL_SP) n.cool_sp = s->cool_sp;
    settings_update(&n, what != ZB_SET_COOL_SP);
}

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

static void zb_on_boost_params(int16_t sp, uint16_t minutes, uint8_t dir)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.boost_sp = sp;
    s_persist.boost_minutes = minutes;
    s_persist.boost_dir = dir;
    s_dirty = true;
    s_dirty_at = now_ms();
    xSemaphoreGive(s_mtx);
}

static void zb_on_swing(int16_t sw)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.swing = ctrl_clamp_swing(sw);
    s_dirty = true;
    s_dirty_at = now_ms();
    xSemaphoreGive(s_mtx);
}

static void zb_on_sched(const sched_cfg_t *c)
{
    ctrl_params_t prm = CTRL_PARAMS_DEFAULT();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_persist.sched = *c;
    sched_sanitize(&prm, &s_persist.sched);
    s_dirty = true;
    s_dirty_at = now_ms();
    xSemaphoreGive(s_mtx);
}

static void zb_on_boost_cmd(uint8_t cmd) { xQueueSend(s_boost_q, &cmd, 0); }

static uint8_t end_to_result(ctrl_boost_end_t e)
{
    switch (e) {
    case BOOST_END_REACHED:      return ZB_BOOST_RESULT_REACHED;
    case BOOST_END_EXPIRED:      return ZB_BOOST_RESULT_EXPIRED;
    case BOOST_END_CANCELLED:    return ZB_BOOST_RESULT_CANCELLED;
    case BOOST_END_MODE_CHANGED: return ZB_BOOST_RESULT_MODE_CHANGED;
    case BOOST_END_FAULT:        return ZB_BOOST_RESULT_FAULT;
    case BOOST_END_LIMIT:        return ZB_BOOST_RESULT_LIMIT;
    default:                     return ZB_BOOST_RESULT_NONE;
    }
}

static uint8_t start_to_result(ctrl_boost_result_t r)
{
    switch (r) {
    case BOOST_OK:               return ZB_BOOST_RESULT_STARTED;
    case BOOST_ERR_FAULT:        return ZB_BOOST_RESULT_REJECTED_FAULT;
    case BOOST_ERR_AT_TARGET:    return ZB_BOOST_RESULT_REJECTED_AT_TARGET;
    case BOOST_ERR_NO_DIRECTION: return ZB_BOOST_RESULT_REJECTED_NO_DIRECTION;
    default:                     return ZB_BOOST_RESULT_REJECTED_ARG;
    }
}

static void handle_boost_cmd(ctrl_t *ctl, uint8_t cmd, uint64_t now)
{
    persist_t p = settings_get();
    ctrl_boost_result_t r;
    switch (cmd) {
    case ZB_BOOST_CMD_CANCEL:
        if (ctrl_boost_status(ctl, now).kind != BOOST_NONE) {
            ctrl_boost_cancel(ctl);
            s_boost_result = ZB_BOOST_RESULT_CANCELLED;
        }
        return;
    case ZB_BOOST_CMD_START_TARGET:
        r = ctrl_boost_start(ctl, &p.s, BOOST_TO_TARGET, (ctrl_boost_dir_t)p.boost_dir, p.boost_sp, 0, now);
        break;
    case ZB_BOOST_CMD_START_TIMED:
        r = ctrl_boost_start(ctl, &p.s, BOOST_TIMED, (ctrl_boost_dir_t)p.boost_dir, 0, p.boost_minutes, now);
        break;
    default:
        r = BOOST_ERR_ARG;
        break;
    }
    s_boost_result = start_to_result(r);
    ESP_LOGI(TAG, "boost cmd %u -> result %u", cmd, s_boost_result);
    if (r != BOOST_OK) banner(r == BOOST_ERR_AT_TARGET ? "Boost: already there" :
                              r == BOOST_ERR_FAULT ? "Boost: sensor fault" :
                              r == BOOST_ERR_NO_DIRECTION ? "Boost: set mode HEAT/COOL" : "Boost: invalid");
}

static void zb_on_identify(uint16_t secs) { s_identify_s = secs; }
static volatile uint64_t s_ota_pct_at;
static void zb_on_ota(int pct) { s_ota_pct = pct; s_ota_pct_at = now_ms(); }
#define OTA_STALE_MS (5 * 60 * 1000)   /* no OTA progress for this long: the transfer is dead */
static void zb_before_reboot(void) { hvac_outputs_all_off(); }

/* ---------------- buttons (esp_timer task -> queue) ---------------- */

static void on_button(ui_button_t which, btn_event_t ev)
{
    btn_msg_t m = {(uint8_t)which, (uint8_t)ev};
    xQueueSend(s_btn_q, &m, 0);
}

/* returns true if the display should refresh promptly */
static bool handle_home_key(const btn_msg_t *m)
{
    persist_t p = settings_get();
    ctrl_settings_t s = p.s;
    int dir = m->which == UI_BTN_UP ? 1 : -1;
    const bool F = p.fahrenheit;
    static ctrl_settings_t before_nudge;
    static uint64_t nudge_at;

    switch (m->which) {
    case UI_BTN_COMBO: {
        /* UP+DOWN together: toggle a timed boost. Undo the setpoint nudge the
         * first of the two presses caused. */
        if (now_ms() - nudge_at < 1500) settings_update(&before_nudge, true);
        uint8_t cmd = s_boost_active ? ZB_BOOST_CMD_CANCEL : ZB_BOOST_CMD_START_TIMED;
        xQueueSend(s_boost_q, &cmd, 0);
        return true;
    }
    case UI_BTN_UP:
    case UI_BTN_DOWN:
        if (m->ev != BTN_EV_PRESS && m->ev != BTN_EV_REPEAT) return false;
        if (ui_combo_active()) return false;
        if (m->ev == BTN_EV_PRESS) { before_nudge = s; nudge_at = now_ms(); }
        switch (s.mode) {
        case CTRL_MODE_HEAT: s.heat_sp = menu_step_temp(s.heat_sp, dir, F); settings_update(&s, true); break;
        case CTRL_MODE_COOL: s.cool_sp = menu_step_temp(s.cool_sp, dir, F); settings_update(&s, false); break;
        default: banner("Mode is OFF - M: menu > Mode"); break;
        }
        return true;
    default:
        return false;
    }
    return false;
}

static void factory_reset(void)
{
    ESP_LOGW(TAG, "factory reset");
    hvac_outputs_all_off();
    settings_erase();
    display_model_t dm = {0};
    dm.banner = "FACTORY RESET";
    dm.full_epoch = (uint8_t)(s_full_epoch + 1);
    display_post(&dm, true);
    for (int i = 0; i < 12; i++) {          /* ~3 s for the panel to show it; keep the dogs fed */
        vTaskDelay(pdMS_TO_TICKS(250));
        esp_task_wdt_reset();
        if (hw_watchdog_armed()) hw_watchdog_kick();
    }
    zb_thermostat_factory_reset();          /* erases Zigbee NVRAM and restarts */
    esp_restart();
}

static void persist_edit(void (*fn)(persist_t *, int32_t), int32_t v)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    fn(&s_persist, v);
    s_dirty = true;
    s_dirty_at = now_ms();
    xSemaphoreGive(s_mtx);
}
static void set_boost_sp(persist_t *p, int32_t v)  { p->boost_sp = (int16_t)v; }
static void set_boost_min(persist_t *p, int32_t v) { p->boost_minutes = (uint16_t)v; }
static void set_swing(persist_t *p, int32_t v)     { p->swing = ctrl_clamp_swing(v); }
static void set_units(persist_t *p, int32_t v)     { p->fahrenheit = v ? 1 : 0; }

static menu_ctx_t menu_ctx(const persist_t *p, uint64_t now)
{
    return (menu_ctx_t){
        .mode = p->s.mode, .fan = p->s.fan, .boost_active = s_boost_active, .boost_sp = p->boost_sp,
        .boost_minutes = p->boost_minutes, .swing = p->swing, .sched = p->sched, .fahrenheit = p->fahrenheit,
        .link = zb_thermostat_link(), .fw_version = FW_VERSION,
        .raw_valid = s_raw.valid, .raw_temp = s_raw.temp_c100, .raw_rh = s_raw.rh_c100,
        .uptime_s = (uint32_t)(now / 1000),
    };
}

static bool apply_menu_action(const menu_action_t *a)
{
    uint8_t cmd;
    persist_t p = settings_get();
    switch (a->kind) {
    case MA_SET_MODE:
        p.s.mode = (ctrl_mode_t)a->value;
        settings_update(&p.s, true);
        break;
    case MA_SET_FAN:
        p.s.fan = (ctrl_fan_t)a->value;
        settings_update(&p.s, true);
        break;
    case MA_BOOST_TARGET:
        persist_edit(set_boost_sp, a->value);
        cmd = ZB_BOOST_CMD_START_TARGET; xQueueSend(s_boost_q, &cmd, 0);
        break;
    case MA_BOOST_TIMED:
        persist_edit(set_boost_min, a->value);
        cmd = ZB_BOOST_CMD_START_TIMED; xQueueSend(s_boost_q, &cmd, 0);
        break;
    case MA_BOOST_CANCEL:
        cmd = ZB_BOOST_CMD_CANCEL; xQueueSend(s_boost_q, &cmd, 0);
        break;
    case MA_SET_SWING: persist_edit(set_swing, a->value); break;
    case MA_SET_SCHED: {   /* persisted like any setting; zb_thermostat_publish() reports the change to HA */
        ctrl_params_t prm = CTRL_PARAMS_DEFAULT();
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        sched_set(&s_persist.sched, (sched_item_t)a->item, a->value);
        sched_sanitize(&prm, &s_persist.sched);
        s_dirty = true;
        s_dirty_at = now_ms();
        xSemaphoreGive(s_mtx);
        break;
    }
    case MA_SET_UNITS: persist_edit(set_units, a->value); break;
    case MA_PAIR:
        banner(zb_thermostat_link() == LINK_JOINED ? "Already paired (reset to re-pair)" : "Pairing...");
        zb_thermostat_pair();
        break;
    case MA_FACTORY_RESET:
        factory_reset();
        break;
    default: break;
    }
    if (a->full_refresh) s_full_epoch++;
    return a->redraw || a->full_refresh;
}

/* returns true if the display should refresh promptly */
static bool handle_button(const btn_msg_t *m)
{
    static const menu_key_t KEYS[] = {MENU_KEY_UP, MENU_KEY_DOWN, MENU_KEY_M, MENU_KEY_COMBO};
    persist_t p = settings_get();
    menu_ctx_t c = menu_ctx(&p, now_ms());
    /* while UP+DOWN are held together their single events are not menu moves */
    if ((m->which == UI_BTN_UP || m->which == UI_BTN_DOWN) && ui_combo_active() && !menu_is_home(&s_menu)) return false;
    menu_action_t a = menu_key(&s_menu, &c, KEYS[m->which], (btn_event_t)m->ev, now_ms());
    if (a.kind == MA_HOME_KEY) return handle_home_key(m);
    return apply_menu_action(&a);
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

/* ---------------- boot-loop guard ---------------- */

/* Count boots that did not reach BOOT_STABLE_MS of uptime (NVS "boot"/"n").
 * Whatever keeps resetting the chip (watchdog, panic, brown-out), after
 * BOOT_LOOP_LIMIT of them the thermostat stays up in safe mode instead:
 * outputs off, STWD100 not armed, the fault on screen and on Zigbee.
 * Deliberate software restarts (ESP_RST_SW: OTA reboot/rollback, factory reset,
 * safe-mode retry) are not counted. Power-on resets ARE counted, because an
 * STWD100 reset pulls CHIP_EN and reports as ESP_RST_POWERON; so 5 quick power
 * cycles also enter safe mode (it ends by itself after SAFE_MODE_RETRY_MS).
 * The counter lives in its own NVS namespace and survives a factory reset. */
static uint8_t boot_count_rw(int op)   /* op: 1 = increment, 0 = clear, -1 = read, else 0x100|n = set n */
{
    nvs_handle_t h;
    uint8_t n = 0;
    if (!s_nvs_ok || nvs_open("boot", NVS_READWRITE, &h) != ESP_OK) return 0;
    if (nvs_get_u8(h, "n", &n) != ESP_OK) n = 0;
    if (op >= 0x100) n = (uint8_t)(op & 0xFF);
    else if (op == 1) n = (uint8_t)(n < 255 ? n + 1 : 255);
    else if (op == 0) n = 0;
    if (op != -1 && nvs_set_u8(h, "n", n) == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return n;
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
    bool ota_marked = false, boot_stable = false;
    hvac_state_t evt_out = {0};
    bool evt_out_init = false, evt_disp_fault = false, evt_out_fault = false, evt_time = false;
    int evt_link = -1, evt_fault = -1;
    ctrl_settings_t evt_s = {0};
    bool evt_s_init = false;

    for (;;) {
        TickType_t wake = xTaskGetTickCount();
        uint64_t now = now_ms();
        persist_t p = settings_get();
        bool urgent = false;

        /* 1. sensor */
        if (now >= next_sensor) {
            next_sensor = now + CONFIG_THERMO_SENSOR_PERIOD_MS;
            rd = sensor_read((int16_t)(CONFIG_THERMO_SELF_HEAT_OFFSET_C100 + p.calib_tenths * 10));
            static sensor_stuck_t stuck;
            static bool stuck_evt;
            bool frozen = rd.valid && sensor_stuck_update(&stuck, rd.temp_c100, rd.rh_c100, now);
            if (frozen != stuck_evt) { EVT("EVT fault sensor frozen=%d", frozen); stuck_evt = frozen; }
            if (frozen) rd.valid = false;   /* a frozen sensor is no sensor: failsafe */
            ctrl_input_t in = {.valid = rd.valid, .temp = rd.temp_c100};
            ctrl_feed_sensor(&ctl, &in, now);
            s_raw = rd;
            s_raw.temp_c100 = (int16_t)(rd.temp_c100 - (CONFIG_THERMO_SELF_HEAT_OFFSET_C100 + p.calib_tenths * 10));
        }

        /* 2. buttons */
        btn_msg_t m;
        while (xQueueReceive(s_btn_q, &m, 0) == pdTRUE) urgent |= handle_button(&m);
        { menu_action_t ma = menu_tick(&s_menu, now); urgent |= apply_menu_action(&ma); }   /* 10 s idle */
        p = settings_get();
        uint8_t bcmd;
        while (xQueueReceive(s_boost_q, &bcmd, 0) == pdTRUE) { handle_boost_cmd(&ctl, bcmd, now); urgent = true; }

        if (ctl.p.hysteresis != p.swing) ctrl_set_swing(&ctl, p.swing);

        /* 3. control + outputs (the interlock lives in hvac_outputs_set) */
        /*    program schedule -> effective setpoints (incl. morning heat boost) */
        uint16_t minute = 0;
        bool time_known = zb_thermostat_local_minute(&minute);
        ctrl_settings_t eff = p.s;
        sched_state_t sst = sched_apply(&prm, &p.sched, time_known, minute, &eff);
        ctrl_output_t o = ctrl_step(&ctl, &eff, now);
        hvac_state_t req = {.w = o.w, .y = o.y, .g = o.g};
        if (s_safe_mode) req = (hvac_state_t){0};
        hvac_state_t act = hvac_outputs_set(req);
        bool out_fault = !s_outputs_ok || hvac_outputs_drive_fault() != 0;
        if (!evt_out_init || act.w != evt_out.w || act.y != evt_out.y || act.g != evt_out.g) {
            EVT("EVT out W=%d Y=%d G=%d", act.w, act.y, act.g);
            evt_out = act; evt_out_init = true;
        }
        if (out_fault && !evt_out_fault) { EVT("EVT fault output mask=0x%x", hvac_outputs_drive_fault()); evt_out_fault = true; }
        if (!out_fault && evt_out_fault) { EVT("EVT fault output cleared"); evt_out_fault = false; }
        if ((int)o.fault != evt_fault) { EVT("EVT ctrl fault=%s", ctrl_fault_str(o.fault)); evt_fault = (int)o.fault; }
        if (!evt_s_init || memcmp(&evt_s, &eff, sizeof eff) != 0) {
            EVT("EVT setpoint mode=%d heat=%d cool=%d fan=%d sched=%d", eff.mode, eff.heat_sp, eff.cool_sp, eff.fan, (int)sst);
            evt_s = eff; evt_s_init = true;
        }
        if (time_known != evt_time) { EVT("EVT time known=%d minute=%u", time_known, minute); evt_time = time_known; }
        ctrl_boost_status_t bs = ctrl_boost_status(&ctl, now);
        if (s_boost_active && bs.kind == BOOST_NONE && bs.last_end != BOOST_END_CANCELLED) {
            s_boost_result = end_to_result(bs.last_end);       /* ended by itself */
            urgent = true;
        }
        s_boost_active = bs.kind != BOOST_NONE;
        uint16_t bmin = (uint16_t)((bs.remaining_s + 59) / 60);
        uint8_t bstate = ZB_BOOST_STATE_OFF;
        if (bs.kind == BOOST_TO_TARGET) bstate = bs.dir == CTRL_MODE_HEAT ? ZB_BOOST_STATE_HEAT_TO_TARGET : ZB_BOOST_STATE_COOL_TO_TARGET;
        if (bs.kind == BOOST_TIMED)     bstate = bs.dir == CTRL_MODE_HEAT ? ZB_BOOST_STATE_HEAT_TIMED : ZB_BOOST_STATE_COOL_TIMED;

        /* 4. Zigbee */
        zb_thermostat_state_t zs = {
            .temp_valid = rd.valid && o.fault != CTRL_FAULT_TEMP_RANGE, .temp = rd.temp_c100,
            .rh_valid = rd.valid, .rh = rd.rh_c100, .s = p.s, .w = act.w, .y = act.y, .g = act.g,
            .boost_state = bstate, .boost_remaining_min = bmin, .boost_last_result = s_boost_result,
            .boost_sp = p.boost_sp, .boost_minutes = p.boost_minutes, .swing = p.swing,
            .sched = p.sched, .sched_state = (uint8_t)sst,
        };
        zb_thermostat_publish(&zs);
        link_state_t link = zb_thermostat_link();
        if ((int)link != evt_link) { EVT("EVT link %d", (int)link); evt_link = (int)link; }

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
        dm.fahrenheit = p.fahrenheit;
        {
            menu_ctx_t mc = menu_ctx(&p, now);
            if (!menu_is_home(&s_menu)) mc.raw_temp = (int16_t)(s_raw.temp_c100 / 10 * 10); /* 0.1 steps: fewer refreshes */
            menu_view(&s_menu, &mc, &dm.view);
        }
        dm.full_epoch = s_full_epoch;
        dm.swing = p.swing;
        dm.sched_state = (uint8_t)sst;
        dm.boost = bs.kind;
        dm.boost_dir = bs.dir;
        dm.boost_sp = bs.sp;
        dm.boost_min_left = bs.kind == BOOST_TIMED ? bmin : 0;
        if (s_ota_pct >= 0 && now - s_ota_pct_at >= OTA_STALE_MS) {
            EVT("EVT ota stalled: no progress for %d min, banner cleared", OTA_STALE_MS / 60000);
            s_ota_pct = -1;
        }
        if (s_ota_pct >= 0) {
            snprintf(ota_banner, sizeof ota_banner, "Updating firmware %d%%", s_ota_pct);
            dm.banner = ota_banner;
        } else if (s_safe_mode) {
            dm.banner = "SAFE MODE: repeated resets";
        } else if (out_fault) {
            dm.banner = "OUTPUT FAULT - service";
        } else if (s_banner && now < s_banner_until) {
            dm.banner = s_banner;
        } else if (!s_nvs_ok) {
            dm.banner = "Storage fault: not saved";
        }
        display_post(&dm, urgent);

        /* 6. persistence */
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        bool save = s_dirty && now >= s_dirty_at + SAVE_DELAY_MS;
        if (save) s_dirty = false;
        persist_t snap = s_persist;
        xSemaphoreGive(s_mtx);
        if (save && s_nvs_ok && !settings_save(&snap)) {
            /* write failed (flash full / worn / error): keep running, retry in a minute */
            EVT("EVT fault nvs write: settings not saved, retrying");
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_dirty = true;
            s_dirty_at = now + 60000 - SAVE_DELAY_MS;
            xSemaphoreGive(s_mtx);
        }

        /* 7. LED / identify */
        if (s_identify_s && now - last_ident_tick >= 1000) { s_identify_s--; last_ident_tick = now; }
        if (!s_identify_s) last_ident_tick = now;
        led_update(now, &o, link);

        /* 8. health check -> watchdogs. Kick only if every check passed.
         *    The display is NOT part of it: a dead panel must not stop heating
         *    (or reboot-loop the thermostat); it is reported instead. */
        bool disp_fault = !display_running() || (now - display_last_alive_ms()) >= DISPLAY_STALL_MS;
        if (disp_fault != evt_disp_fault) { EVT("EVT display fault=%d", disp_fault); evt_disp_fault = disp_fault; }
        bool healthy = hvac_outputs_interlock_faults() == 0;
        if (healthy) {
            esp_task_wdt_reset();
#if CONFIG_THERMO_HW_WDT_ENABLE
            if (!s_safe_mode && !hw_watchdog_armed() && now - boot >= HWDT_ARM_AFTER_MS) {
                ESP_LOGI(TAG, "arming STWD100");
                hw_watchdog_arm();
            }
            if (hw_watchdog_armed()) hw_watchdog_kick();
#endif
        } else {
            ESP_LOGE(TAG, "unhealthy: outputs off, letting watchdog bite");
            hvac_outputs_all_off();
        }

        /* 8b. boot-loop guard */
        if (!boot_stable && !s_safe_mode && healthy && now - boot >= BOOT_STABLE_MS) {
            boot_stable = true;
            boot_count_rw(0);
        }
        if (s_safe_mode && now - boot >= SAFE_MODE_RETRY_MS) {
            ESP_LOGW(TAG, "EVT safe mode: retrying a normal boot");
            /* exactly one trial: one more counted reset goes straight back to safe mode */
            boot_count_rw(0x100 | (BOOT_LOOP_LIMIT - 1));
            hvac_outputs_all_off();
            esp_restart();
        }

        /* 9. OTA rollback protection: confirm the image only once it is healthy
         *    AND back on the Zigbee network; if it never rejoins, roll back. */
        if (!ota_marked) {
            esp_ota_img_states_t st;
            const esp_partition_t *run = esp_ota_get_running_partition();
            bool pending = esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
            if (!pending) {
                ota_marked = true;   /* factory/already-confirmed image: nothing to do */
            } else if (healthy && o.fault == CTRL_FAULT_NONE && now - boot >= OTA_VALID_AFTER_MS &&
                       zb_thermostat_link() == LINK_JOINED) {
                ESP_LOGI(TAG, "new image healthy and joined: confirming");
                esp_ota_mark_app_valid_cancel_rollback();
                ota_marked = true;
            } else if (now - boot >= OTA_JOIN_TIMEOUT_MS) {
                esp_err_t rb = ESP_ERR_OTA_ROLLBACK_FAILED;
                /* Check first: forcing the outputs off and then carrying on would bypass
                 * the compressor/heat minimum off times. */
                if (esp_ota_check_rollback_is_possible()) {
                    ESP_LOGE(TAG, "new image not confirmed within %d s: rolling back", OTA_JOIN_TIMEOUT_MS / 1000);
                    hvac_outputs_all_off();
                    rb = esp_ota_mark_app_invalid_rollback_and_reboot();   /* returns only on failure */
                }
                /* No valid image in the other slot: keep running this one instead of
                 * retrying every loop. */
                ESP_LOGE(TAG, "rollback failed (%s): keeping current image", esp_err_to_name(rb));
                esp_ota_mark_app_valid_cancel_rollback();   /* keep this image bootable after the next reset */
                ota_marked = true;
            }
        }

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(LOOP_MS));
    }
}

void app_main(void)
{
    /* Outputs first: guarantee a known-off state before anything else. */
    s_outputs_ok = hvac_outputs_init() == ESP_OK;   /* failure latches an output fault (all off) */
    hw_watchdog_init();      /* disarmed until the main loop proves itself */

    /* Nothing below may abort: a fault here must not turn into a reboot loop. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    s_nvs_ok = err == ESP_OK;
    if (!s_nvs_ok) EVT("EVT fault nvs init %s: running on defaults, nothing is saved", esp_err_to_name(err));

#if CONFIG_PM_ENABLE
    esp_pm_config_t pm = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    if (esp_pm_configure(&pm) != ESP_OK) ESP_LOGW(TAG, "power management not configured");
#endif

    esp_reset_reason_t rr = esp_reset_reason();
    uint8_t boots = boot_count_rw(rr == ESP_RST_SW ? -1 : 1);
    EVT("EVT boot reason=%d count=%u", (int)rr, boots);
    s_safe_mode = boots >= BOOT_LOOP_LIMIT;
    if (s_safe_mode) EVT("EVT safe mode: %u resets without %d min of stable uptime", boots, BOOT_STABLE_MS / 60000);

    s_mtx = xSemaphoreCreateMutex();
    s_btn_q = xQueueCreate(16, sizeof(btn_msg_t));
    s_boost_q = xQueueCreate(4, sizeof(uint8_t));
    settings_load(&s_persist);
    /* no clock yet at boot: mirror the day program */
    sched_program_sp(&s_persist.sched, false, 0, &s_persist.s.heat_sp, &s_persist.s.cool_sp);
    menu_init(&s_menu);
    ESP_LOGI(TAG, "settings: mode %d heat %d cool %d fan %d calib %d", s_persist.s.mode, s_persist.s.heat_sp,
             s_persist.s.cool_sp, s_persist.s.fan, s_persist.calib_tenths);
    EVT("EVT cfg loaded mode=%d heat=%d cool=%d fan=%d", s_persist.s.mode, s_persist.s.heat_sp, s_persist.s.cool_sp,
        s_persist.s.fan);

    if (sensor_init() != ESP_OK) ESP_LOGE(TAG, "EVT fault sensor init (control stays in failsafe)");
    if (display_init() != ESP_OK) ESP_LOGE(TAG, "EVT fault display init (control continues without a screen)");
    if (ui_init(on_button) != ESP_OK) ESP_LOGE(TAG, "EVT fault buttons init (Zigbee control only)");

    /* Local control first: it never waits for Zigbee. */
    if (xTaskCreate(app_task, "app", 4096, NULL, 6, NULL) != pdPASS) {
        ESP_LOGE(TAG, "app task: out of memory");
        esp_restart();
    }

    zb_thermostat_cbs_t cbs = {
        .on_settings = zb_on_settings,
        .on_calibration = zb_on_calib,
        .on_identify = zb_on_identify,
        .on_ota_progress = zb_on_ota,
        .before_ota_reboot = zb_before_reboot,
        .on_boost_params = zb_on_boost_params,
        .on_boost_command = zb_on_boost_cmd,
        .on_swing = zb_on_swing,
        .on_sched = zb_on_sched,
    };
    zb_thermostat_init_t zi = {
        .s = s_persist.s, .calib = s_persist.calib_tenths,
        .boost_sp = s_persist.boost_sp, .boost_minutes = s_persist.boost_minutes, .boost_dir = s_persist.boost_dir,
        .swing = s_persist.swing, .sched = s_persist.sched,
    };
    if (zb_thermostat_start(&cbs, &zi) != ESP_OK)
        ESP_LOGE(TAG, "Zigbee unavailable: local control only (radio fault shown)");
}
