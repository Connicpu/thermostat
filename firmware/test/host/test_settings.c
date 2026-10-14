/* Host tests for the versioned settings blob (main/settings_codec.c). */
#include "core.h"
#include "tinytest.h"
#include "settings.h"
#include <string.h>

static void DefaultsSane(void)
{
    Persist p, q;
    SettingsDefaults(&p, true);
    CHECK_EQ(p.s.mode, kCtrlModeOff);
    CHECK_EQ(p.s.fan, kCtrlFanAuto);
    CHECK_EQ(p.fahrenheit, 1);
    CHECK_EQ(p.sched.boostOffset, 50);
    CHECK_EQ(p.sched.dayHeatStart, 360);
    q = p;
    SettingsSanitize(&q, true);                 /* defaults are already in range */
    CHECK(memcmp(&p, &q, sizeof p) == 0);
}

static void Roundtrip(void)
{
    Persist p, q;
    SettingsDefaults(&p, false);
    p.s.mode = kCtrlModeCool; p.s.fan = kCtrlFanOn;
    p.sched.dayHeatStart = 400; p.sched.nightCool = 2300; p.swing = 70; p.calibTenths = -4;
    U8 buf[SETTINGS_BLOB_MAX];
    size_t n = SettingsEncode(&p, buf, sizeof buf);
    CHECK(n > 0 && n <= SETTINGS_BLOB_MAX);
    CHECK_EQ(buf[0], SETTINGS_VERSION);
    CHECK_EQ(SETTINGS_VERSION, 1);
    SettingsDefaults(&q, false);
    CHECK(SettingsDecode(buf, n, &q));
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    CHECK_EQ(SettingsEncode(&p, buf, 4), 0u);   /* buffer too small */
}

static void UnknownVersionOrSizeRejected(void)
{
    Persist p, q;
    SettingsDefaults(&p, false);
    p.s.mode = kCtrlModeHeat;
    U8 buf[SETTINGS_BLOB_MAX];
    size_t n = SettingsEncode(&p, buf, sizeof buf);
    SettingsDefaults(&q, false);
    Persist before = q;
    buf[0] = 0;
    CHECK(!SettingsDecode(buf, n, &q));
    buf[0] = SETTINGS_VERSION;
    CHECK(!SettingsDecode(buf, n - 1, &q));     /* truncated */
    CHECK(!SettingsDecode(buf, 0, &q));
    CHECK(memcmp(&before, &q, sizeof q) == 0);   /* untouched -> caller keeps defaults */
}

/* Forward compatibility (design rule in settings_codec.c): a blob from the next firmware
 * version (higher version, extra fields appended) loads its known prefix. */
static void NewerLongerBlobLoadsKnownPrefix(void)
{
    Persist p, q;
    SettingsDefaults(&p, false);
    p.s.mode = kCtrlModeCool; p.sched.nightHeat = 1650; p.swing = 30;
    U8 buf[SETTINGS_BLOB_MAX];
    memset(buf, 0xA5, sizeof buf);
    size_t n = SettingsEncode(&p, buf, sizeof buf);
    buf[0] = SETTINGS_VERSION + 1;               /* "v2" with 16 appended bytes of new fields */
    size_t longer = n + 16;
    CHECK(longer <= SETTINGS_BLOB_MAX);
    SettingsDefaults(&q, false);
    CHECK(SettingsDecode(buf, longer, &q));
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    /* same version, longer blob also fine; full buffer size too */
    buf[0] = SETTINGS_VERSION;
    CHECK(SettingsDecode(buf, longer, &q));
    CHECK(SettingsDecode(buf, SETTINGS_BLOB_MAX, &q));
    /* re-encoding (what the older firmware saves after a rollback) is its own shorter layout */
    U8 out[SETTINGS_BLOB_MAX];
    CHECK_EQ(SettingsEncode(&q, out, sizeof out), n);
    CHECK_EQ(out[0], SETTINGS_VERSION);
}

static void SanitizeBadValues(void)
{
    Persist p; SettingsDefaults(&p, false);
    p.s.mode = 0x01;                             /* old AUTO value: no longer a mode */
    p.swing = 999; p.fahrenheit = 7; p.sched.boostMin = 999; p.calibTenths = 99;
    p.boostMinutes = 0; p.boostDir = 9; p.boostSp = 9000;
    SettingsSanitize(&p, true);
    CHECK_EQ(p.s.mode, kCtrlModeOff);
    CHECK_EQ(p.swing, CTRL_SWING_DEFAULT);
    CHECK_EQ(p.fahrenheit, 1);
    CHECK_EQ(p.sched.boostMin, SCHED_BOOST_MAX_MIN);
    CHECK_EQ(p.calibTenths, 0);
    CHECK_EQ(p.boostMinutes, 30);
    CHECK_EQ(p.boostDir, kBoostDirAuto);
    CHECK_EQ(p.boostSp, 2200);
}

int main(void)
{
    RUN(DefaultsSane);
    RUN(Roundtrip);
    RUN(UnknownVersionOrSizeRejected);
    RUN(NewerLongerBlobLoadsKnownPrefix);
    RUN(SanitizeBadValues);
    DONE();
}
