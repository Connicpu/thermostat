/* Host tests for the versioned settings blob (main/settings_codec.c). */
#include "tinytest.h"
#include "settings.h"
#include <string.h>

static void defaults_sane(void)
{
    persist_t p, q;
    settings_defaults(&p, true);
    CHECK_EQ(p.s.mode, CTRL_MODE_OFF);
    CHECK_EQ(p.s.fan, CTRL_FAN_AUTO);
    CHECK_EQ(p.fahrenheit, 1);
    CHECK_EQ(p.sched.boost_offset, 50);
    CHECK_EQ(p.sched.day_heat_start, 360);
    q = p;
    settings_sanitize(&q, true);                 /* defaults are already in range */
    CHECK(memcmp(&p, &q, sizeof p) == 0);
}

static void roundtrip(void)
{
    persist_t p, q;
    settings_defaults(&p, false);
    p.s.mode = CTRL_MODE_COOL; p.s.fan = CTRL_FAN_ON;
    p.sched.day_heat_start = 400; p.sched.night_cool = 2300; p.swing = 70; p.calib_tenths = -4;
    uint8_t buf[SETTINGS_BLOB_MAX];
    size_t n = settings_encode(&p, buf, sizeof buf);
    CHECK(n > 0 && n <= SETTINGS_BLOB_MAX);
    CHECK_EQ(buf[0], SETTINGS_VERSION);
    CHECK_EQ(SETTINGS_VERSION, 1);
    settings_defaults(&q, false);
    CHECK(settings_decode(buf, n, &q));
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    CHECK_EQ(settings_encode(&p, buf, 4), 0u);   /* buffer too small */
}

static void unknown_version_or_size_rejected(void)
{
    persist_t p, q;
    settings_defaults(&p, false);
    p.s.mode = CTRL_MODE_HEAT;
    uint8_t buf[SETTINGS_BLOB_MAX];
    size_t n = settings_encode(&p, buf, sizeof buf);
    settings_defaults(&q, false);
    persist_t before = q;
    buf[0] = 0;
    CHECK(!settings_decode(buf, n, &q));
    buf[0] = SETTINGS_VERSION;
    CHECK(!settings_decode(buf, n - 1, &q));     /* truncated */
    CHECK(!settings_decode(buf, 0, &q));
    CHECK(memcmp(&before, &q, sizeof q) == 0);   /* untouched -> caller keeps defaults */
}

/* Forward compatibility (design rule in settings_codec.c): a blob from the next firmware
 * version (higher version, extra fields appended) loads its known prefix. */
static void newer_longer_blob_loads_known_prefix(void)
{
    persist_t p, q;
    settings_defaults(&p, false);
    p.s.mode = CTRL_MODE_COOL; p.sched.night_heat = 1650; p.swing = 30;
    uint8_t buf[SETTINGS_BLOB_MAX];
    memset(buf, 0xA5, sizeof buf);
    size_t n = settings_encode(&p, buf, sizeof buf);
    buf[0] = SETTINGS_VERSION + 1;               /* "v2" with 16 appended bytes of new fields */
    size_t longer = n + 16;
    CHECK(longer <= SETTINGS_BLOB_MAX);
    settings_defaults(&q, false);
    CHECK(settings_decode(buf, longer, &q));
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    /* same version, longer blob also fine; full buffer size too */
    buf[0] = SETTINGS_VERSION;
    CHECK(settings_decode(buf, longer, &q));
    CHECK(settings_decode(buf, SETTINGS_BLOB_MAX, &q));
    /* re-encoding (what the older firmware saves after a rollback) is its own shorter layout */
    uint8_t out[SETTINGS_BLOB_MAX];
    CHECK_EQ(settings_encode(&q, out, sizeof out), n);
    CHECK_EQ(out[0], SETTINGS_VERSION);
}

static void sanitize_bad_values(void)
{
    persist_t p; settings_defaults(&p, false);
    p.s.mode = 0x01;                             /* old AUTO value: no longer a mode */
    p.swing = 999; p.fahrenheit = 7; p.sched.boost_min = 999; p.calib_tenths = 99;
    p.boost_minutes = 0; p.boost_dir = 9; p.boost_sp = 9000;
    settings_sanitize(&p, true);
    CHECK_EQ(p.s.mode, CTRL_MODE_OFF);
    CHECK_EQ(p.swing, CTRL_SWING_DEFAULT);
    CHECK_EQ(p.fahrenheit, 1);
    CHECK_EQ(p.sched.boost_min, SCHED_BOOST_MAX_MIN);
    CHECK_EQ(p.calib_tenths, 0);
    CHECK_EQ(p.boost_minutes, 30);
    CHECK_EQ(p.boost_dir, BOOST_DIR_AUTO);
    CHECK_EQ(p.boost_sp, 2200);
}

int main(void)
{
    RUN(defaults_sane);
    RUN(roundtrip);
    RUN(unknown_version_or_size_rejected);
    RUN(newer_longer_blob_loads_known_prefix);
    RUN(sanitize_bad_values);
    DONE();
}
