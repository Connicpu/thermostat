/* settings_codec.c - pure (de)serialisation of the settings blob.
 * No ESP-IDF dependencies: host-tested in test/host/test_settings.c. NVS I/O is in settings.c.
 *
 * Blob = {uint8_t version; persist_t p}, at most SETTINGS_BLOB_MAX bytes.
 *
 * DESIGN RULE (Connie): every NVS layout change must be one-version backward AND
 * forward compatible, so an OTA rollback keeps working:
 *  - A newer layout may only APPEND fields at the end of persist_t. Never reorder,
 *    resize, retype or repurpose an existing field, and keep the blob within
 *    SETTINGS_BLOB_MAX (older firmware reads into a buffer of that size).
 *  - Firmware one version older must be able to read the newer blob: its decoder
 *    accepts any version >= its own and size >= what it expects, takes the known
 *    prefix and ignores the rest. Data in the appended fields need not survive a
 *    rollback (the older firmware rewrites the blob in its own, shorter layout).
 *  - Newer firmware must still load the older blob: bump SETTINGS_VERSION, keep the
 *    old size/version, and fill the appended fields with defaults in a migration
 *    branch in settings_decode().
 * This v1 decoder already follows the rule (see settings_decode()). */
#include "settings.h"
#include <string.h>

typedef struct { uint8_t version; persist_t p; } blob_t;
_Static_assert(sizeof(blob_t) <= SETTINGS_BLOB_MAX, "settings blob outgrew SETTINGS_BLOB_MAX");

void settings_defaults(persist_t *p, bool fahrenheit)
{
    memset(p, 0, sizeof *p);
    p->s = (ctrl_settings_t){.mode = CTRL_MODE_OFF, .fan = CTRL_FAN_AUTO, .heat_sp = 2000, .cool_sp = 2450};
    p->boost_sp = 2200;
    p->boost_minutes = 30;
    p->boost_dir = BOOST_DIR_AUTO;
    p->swing = CTRL_SWING_DEFAULT;
    p->fahrenheit = fahrenheit ? 1 : 0;
    p->sched = (sched_cfg_t)SCHED_CFG_DEFAULT();
}

bool settings_decode(const void *data, size_t n, persist_t *p)
{
    blob_t b;
    /* Same or newer layout (version >= ours, at least our size): read the known prefix,
     * ignore appended fields. Newer firmware adds migration branches for older, shorter
     * blobs here (e.g. version 1 with sizeof(blob_v1_t) -> defaults for new fields). */
    if (n < sizeof b) return false;
    memcpy(&b, data, sizeof b);
    if (b.version < SETTINGS_VERSION) return false;
    *p = b.p;
    return true;
}

void settings_sanitize(persist_t *p, bool fahrenheit_default)
{
    ctrl_params_t prm = CTRL_PARAMS_DEFAULT();
    ctrl_sanitize_settings(&prm, &p->s, true);
    sched_sanitize(&prm, &p->sched);
    if (p->calib_tenths > 25 || p->calib_tenths < -25) p->calib_tenths = 0;
    if (p->boost_sp < prm.min_sp || p->boost_sp > prm.max_sp) p->boost_sp = 2200;
    if (p->boost_minutes < BOOST_TIMED_MIN_MIN || p->boost_minutes > BOOST_TIMED_MAX_MIN) p->boost_minutes = 30;
    if (p->boost_dir > BOOST_DIR_COOL) p->boost_dir = BOOST_DIR_AUTO;
    if (p->fahrenheit > 1) p->fahrenheit = fahrenheit_default ? 1 : 0;
    if (p->swing < CTRL_SWING_MIN || p->swing > CTRL_SWING_MAX) p->swing = CTRL_SWING_DEFAULT;
}

size_t settings_encode(const persist_t *p, void *buf, size_t n)
{
    blob_t b;
    memset(&b, 0, sizeof b);
    b.version = SETTINGS_VERSION;
    b.p = *p;
    if (n < sizeof b) return 0;
    memcpy(buf, &b, sizeof b);
    return sizeof b;
}
