/* settings.c - NVS persistence (namespace "tstat", one versioned blob). */
#include "settings.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

#define NS "tstat"
#define KEY "cfg"
#include "sdkconfig.h"

#define VERSION 4

typedef struct { uint8_t version; persist_t p; } blob_t;

/* v2 layout (before the swing setting); migrated on load. */
typedef struct {
    ctrl_settings_t s;
    int8_t calib_tenths;
    int16_t boost_sp;
    uint16_t boost_minutes;
    uint8_t boost_dir;
} persist_v2_t;
typedef struct { uint8_t version; persist_v2_t p; } blob_v2_t;

/* v3 layout (before the units setting), laid out exactly as persist_t was. */
typedef struct {
    ctrl_settings_t s;
    int8_t calib_tenths;
    int16_t boost_sp;
    uint16_t boost_minutes;
    uint8_t boost_dir;
    int16_t swing;
} persist_v3_t;
typedef struct { uint8_t version; persist_v3_t p; } blob_v3_t;

static void defaults(persist_t *p)
{
    memset(p, 0, sizeof *p);
    p->s = (ctrl_settings_t){.mode = CTRL_MODE_OFF, .fan = CTRL_FAN_AUTO, .heat_sp = 2000, .cool_sp = 2450};
    p->boost_sp = 2200;
    p->boost_minutes = 30;
    p->boost_dir = BOOST_DIR_AUTO;
    p->swing = CTRL_SWING_DEFAULT;
    p->fahrenheit = CONFIG_THERMO_DISPLAY_FAHRENHEIT ? 1 : 0;
}

void settings_load(persist_t *p)
{
    defaults(p);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return;
    union { blob_t v4; blob_v3_t v3; blob_v2_t v2; } b;
    size_t n = sizeof b;
    if (nvs_get_blob(h, KEY, &b, &n) == ESP_OK) {
        if (n == sizeof b.v4 && b.v4.version == VERSION) {
            *p = b.v4.p;
        } else if (n == sizeof b.v3 && b.v3.version == 3) {   /* keep settings, add default units */
            p->s = b.v3.p.s;
            p->calib_tenths = b.v3.p.calib_tenths;
            p->boost_sp = b.v3.p.boost_sp;
            p->boost_minutes = b.v3.p.boost_minutes;
            p->boost_dir = b.v3.p.boost_dir;
            p->swing = b.v3.p.swing;
        } else if (n == sizeof b.v2 && b.v2.version == 2) {   /* keep settings, add default swing */
            p->s = b.v2.p.s;
            p->calib_tenths = b.v2.p.calib_tenths;
            p->boost_sp = b.v2.p.boost_sp;
            p->boost_minutes = b.v2.p.boost_minutes;
            p->boost_dir = b.v2.p.boost_dir;
        }
    }
    nvs_close(h);
    ctrl_params_t prm = CTRL_PARAMS_DEFAULT();
    ctrl_sanitize_settings(&prm, &p->s, true);
    if (p->calib_tenths > 25 || p->calib_tenths < -25) p->calib_tenths = 0;
    if (p->boost_sp < prm.min_sp || p->boost_sp > prm.max_sp) p->boost_sp = 2200;
    if (p->boost_minutes < BOOST_TIMED_MIN_MIN || p->boost_minutes > BOOST_TIMED_MAX_MIN) p->boost_minutes = 30;
    if (p->boost_dir > BOOST_DIR_COOL) p->boost_dir = BOOST_DIR_AUTO;
    if (p->fahrenheit > 1) p->fahrenheit = CONFIG_THERMO_DISPLAY_FAHRENHEIT ? 1 : 0;
    if (p->swing < CTRL_SWING_MIN || p->swing > CTRL_SWING_MAX) p->swing = CTRL_SWING_DEFAULT;
}

bool settings_save(const persist_t *p)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    blob_t b = {.version = VERSION, .p = *p};
    esp_err_t e = nvs_set_blob(h, KEY, &b, sizeof b);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) ESP_LOGE("settings", "save failed: %s", esp_err_to_name(e));
    return e == ESP_OK;
}

void settings_erase(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
}
