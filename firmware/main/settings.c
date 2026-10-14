/* settings.c - NVS persistence: namespace "tstat", one versioned blob under the
 * stable key "cfg". Format and versioning: settings_codec.c. */
#include "settings.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include "sdkconfig.h"

#define NS "tstat"
#define KEY "cfg"
#define FDEF (CONFIG_THERMO_DISPLAY_FAHRENHEIT ? true : false)

static uint8_t s_buf[SETTINGS_BLOB_MAX];

void settings_load(persist_t *p)
{
    settings_defaults(p, FDEF);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        size_t n = sizeof s_buf;
        esp_err_t e = nvs_get_blob(h, KEY, s_buf, &n);
        if (e == ESP_OK && !settings_decode(s_buf, n, p)) {
            ESP_LOGW("settings", "unknown settings blob (%u bytes, version %u): defaults", (unsigned)n, s_buf[0]);
            settings_defaults(p, FDEF);
        }
        nvs_close(h);
    }
    settings_sanitize(p, FDEF);
}

bool settings_save(const persist_t *p)
{
    size_t n = settings_encode(p, s_buf, sizeof s_buf);
    if (!n) return false;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t e = nvs_set_blob(h, KEY, s_buf, n);
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
