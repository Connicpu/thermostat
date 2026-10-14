/* settings_codec.c - pure (de)serialisation of the settings blob.
 * No ESP-IDF dependencies: host-tested in test/host/test_settings.c. NVS I/O is in settings.c.
 *
 * Blob = {uint8_t version; persist_t p}. Only SETTINGS_VERSION is understood today; an
 * unknown version or size loads the defaults. To change the layout later: bump
 * SETTINGS_VERSION, keep the old layout as a struct here and add a migration branch in
 * settings_decode() (the size + version check tells the layouts apart). */
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
    if (n != sizeof b) return false;
    memcpy(&b, data, sizeof b);
    if (b.version != SETTINGS_VERSION) return false;
    /* future: else if (n == sizeof(blob_v1_t) && version == 1) { migrate } */
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
