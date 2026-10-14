/* settings_codec.c - pure (de)serialisation of the settings blob.
 * No ESP-IDF dependencies: host-tested in test/host/test_settings.c. NVS I/O is in settings.c.
 *
 * Blob = {uint8_t version; Persist p}, at most SETTINGS_BLOB_MAX bytes.
 *
 * DESIGN RULE (Connie): every NVS layout change must be one-version backward AND
 * forward compatible, so an OTA rollback keeps working:
 *  - A newer layout may only APPEND fields at the end of Persist. Never reorder,
 *    resize, retype or repurpose an existing field, and keep the blob within
 *    SETTINGS_BLOB_MAX (older firmware reads into a buffer of that size).
 *  - Firmware one version older must be able to read the newer blob: its decoder
 *    accepts any version >= its own and size >= what it expects, takes the known
 *    prefix and ignores the rest. Data in the appended fields need not survive a
 *    rollback (the older firmware rewrites the blob in its own, shorter layout).
 *  - Newer firmware must still load the older blob: bump SETTINGS_VERSION, keep the
 *    old size/version, and fill the appended fields with defaults in a migration
 *    branch in SettingsDecode().
 * This v1 decoder already follows the rule (see SettingsDecode()). */
#include "core.h"
#include "settings.h"
#include <string.h>

typedef struct { U8 version; Persist p; } Blob;
_Static_assert(sizeof(Blob) <= SETTINGS_BLOB_MAX, "settings blob outgrew SETTINGS_BLOB_MAX");

void SettingsDefaults(Persist *pP, bool fahrenheit)
{
    memset(pP, 0, sizeof *pP);
    pP->s = (CtrlSettings){.mode = kCtrlModeOff, .fan = kCtrlFanAuto, .heatSp = 2000, .coolSp = 2450};
    pP->boostSp = 2200;
    pP->boostMinutes = 30;
    pP->boostDir = kBoostDirAuto;
    pP->swing = CTRL_SWING_DEFAULT;
    pP->fahrenheit = fahrenheit ? 1 : 0;
    pP->sched = (SchedCfg)SCHED_CFG_DEFAULT();
}

bool SettingsDecode(const void *pData, size_t n, Persist *pP)
{
    Blob b;
    /* Same or newer layout (version >= ours, at least our size): read the known prefix,
     * ignore appended fields. Newer firmware adds migration branches for older, shorter
     * blobs here (e.g. version 1 with sizeof(blob_v1_t) -> defaults for new fields). */
    if (n < sizeof b) return false;
    memcpy(&b, pData, sizeof b);
    if (b.version < SETTINGS_VERSION) return false;
    *pP = b.p;
    return true;
}

void SettingsSanitize(Persist *pP, bool fahrenheitDefault)
{
    CtrlParams prm = CTRL_PARAMS_DEFAULT();
    CtrlSanitizeSettings(&prm, &pP->s, true);
    SchedSanitize(&prm, &pP->sched);
    if (pP->calibTenths > 25 || pP->calibTenths < -25) pP->calibTenths = 0;
    if (pP->boostSp < prm.minSp || pP->boostSp > prm.maxSp) pP->boostSp = 2200;
    if (pP->boostMinutes < BOOST_TIMED_MIN_MIN || pP->boostMinutes > BOOST_TIMED_MAX_MIN) pP->boostMinutes = 30;
    if (pP->boostDir > kBoostDirCool) pP->boostDir = kBoostDirAuto;
    if (pP->fahrenheit > 1) pP->fahrenheit = fahrenheitDefault ? 1 : 0;
    if (pP->swing < CTRL_SWING_MIN || pP->swing > CTRL_SWING_MAX) pP->swing = CTRL_SWING_DEFAULT;
}

size_t SettingsEncode(const Persist *pP, void *pBuf, size_t n)
{
    Blob b;
    memset(&b, 0, sizeof b);
    b.version = SETTINGS_VERSION;
    b.p = *pP;
    if (n < sizeof b) return 0;
    memcpy(pBuf, &b, sizeof b);
    return sizeof b;
}
