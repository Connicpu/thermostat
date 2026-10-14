/* settings.c - NVS persistence: namespace "tstat", one versioned blob under the
 * stable key "cfg3" (0.2.x prototypes used "cfg" with another layout). Format and versioning: settings_codec.c. */
#include "core.h"
#include "settings.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include "sdkconfig.h"

#define NS "tstat"
#define KEY "cfg3"   /* new key: never collides with 0.2.x's tstat/cfg layout */
#define FDEF (CONFIG_THERMO_DISPLAY_FAHRENHEIT ? true : false)

static U8 s_buf[SETTINGS_BLOB_MAX];

void SettingsLoad(Persist *pP)
{
    SettingsDefaults(pP, FDEF);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        size_t n = sizeof s_buf;
        esp_err_t e = nvs_get_blob(h, KEY, s_buf, &n);
        if (e == ESP_OK && !SettingsDecode(s_buf, n, pP)) {
            ESP_LOGW("settings", "unknown settings blob (%u bytes, version %d): defaults", (unsigned)n,
                     n ? (int)s_buf[0] : -1);
            SettingsDefaults(pP, FDEF);
        }
        nvs_close(h);
    }
    SettingsSanitize(pP, FDEF);
}

bool SettingsSave(const Persist *pP)
{
    size_t n = SettingsEncode(pP, s_buf, sizeof s_buf);
    if (!n) return false;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t e = nvs_set_blob(h, KEY, s_buf, n);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) ESP_LOGE("settings", "save failed: %s", esp_err_to_name(e));
    return e == ESP_OK;
}

void SettingsErase(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
}
