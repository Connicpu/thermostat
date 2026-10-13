/* settings.c - NVS persistence (namespace "tstat", one versioned blob). */
#include "settings.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

#define NS "tstat"
#define KEY "cfg"
#define VERSION 2

typedef struct { uint8_t version; persist_t p; } blob_t;

static void defaults(persist_t *p)
{
    memset(p, 0, sizeof *p);
    p->s = (ctrl_settings_t){.mode = CTRL_MODE_OFF, .fan = CTRL_FAN_AUTO, .heat_sp = 2000, .cool_sp = 2450};
    p->boost_sp = 2200;
    p->boost_minutes = 30;
    p->boost_dir = BOOST_DIR_AUTO;
}

void settings_load(persist_t *p)
{
    defaults(p);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return;
    blob_t b;
    size_t n = sizeof b;
    if (nvs_get_blob(h, KEY, &b, &n) == ESP_OK && n == sizeof b && b.version == VERSION) *p = b.p;
    nvs_close(h);
    ctrl_params_t prm = CTRL_PARAMS_DEFAULT();
    ctrl_sanitize_settings(&prm, &p->s, true);
    if (p->calib_tenths > 25 || p->calib_tenths < -25) p->calib_tenths = 0;
    if (p->boost_sp < prm.min_sp || p->boost_sp > prm.max_sp) p->boost_sp = 2200;
    if (p->boost_minutes < BOOST_TIMED_MIN_MIN || p->boost_minutes > BOOST_TIMED_MAX_MIN) p->boost_minutes = 30;
    if (p->boost_dir > BOOST_DIR_COOL) p->boost_dir = BOOST_DIR_AUTO;
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
