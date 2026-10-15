/* Host tests: SHT40 decoding, button state machine, display rendering
 * (also writes a PBM preview of the screen to display_preview.pbm). */
#include "core.h"
#include "tinytest.h"
#include "sensor_stuck.h"
#include "sht40_parse.h"
#include "buttons.h"
#include "display_model.h"
#include "display_policy.h"
#include "font.h"
#include <string.h>

/** Sht40Crc8() matches the Sensirion datasheet example (0xBEEF -> 0x92). */
static void Sht40CrcKnownVector(void)
{
    const U8 d[2] = {0xBE, 0xEF}; /* Sensirion datasheet example: CRC 0x92 */
    CHECK_EQ(Sht40Crc8(d, 2), 0x92);
}

/** Sht40Parse() converts a known frame to 25.00 °C / 56.50 %RH, rejects a CRC error and clamps RH at 0. */
static void Sht40ParseValues(void)
{
    U8 f[6] = {0x66, 0x66, 0, 0x80, 0x00, 0};
    f[2] = Sht40Crc8(f, 2);
    f[5] = Sht40Crc8(f + 3, 2);
    I16 t;
    U16 h;
    CHECK(Sht40Parse(f, &t, &h));
    CHECK_EQ(t, 2500); /* -45 + 175*0.4 = 25.00 */
    CHECK_EQ(h, 5650); /* -6 + 125*0.5 = 56.50 */
    f[5] ^= 1;
    CHECK(!Sht40Parse(f, &t, &h));
    U8 lo[6] = {0, 0, 0, 0, 0, 0};
    lo[2] = Sht40Crc8(lo, 2);
    lo[5] = Sht40Crc8(lo + 3, 2);
    CHECK(Sht40Parse(lo, &t, &h));
    CHECK_EQ(t, -4500);
    CHECK_EQ(h, 0); /* clamped */
}

/**
 * Feed one raw level to the button for `ms` (in BTN_TICK_MS ticks), collecting up to `max`
 * events.
 *
 * Returns the number of events stored in pEvs.
 */
static int Feed(Btn *pB, bool level, int ms, BtnEvent *pEvs, int max)
{
    int n = 0;
    for (int t = 0; t < ms; t += BTN_TICK_MS)
    {
        BtnEvent e = BtnTick(pB, level);
        if (e && n < max)
            pEvs[n++] = e;
    }
    return n;
}

/**
 * Contact bounce produces no events; an auto-repeat key presses once, then repeats after 600 ms every 200 ms.
 */
static void ButtonsDebounceAndRepeat(void)
{
    Btn b;
    BtnEvent ev[64];
    int n;
    BtnInit(&b, true);
    /* contact bounce: 10 ms toggles must not produce events */
    for (int i = 0; i < 10; i++)
    {
        n = Feed(&b, i & 1, 10, ev, 64);
        CHECK_EQ(n, 0);
    }
    n = Feed(&b, false, 100, ev, 64);
    CHECK_EQ(n, 0);
    n = Feed(&b, true, 100, ev, 64);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvPress);
    n = Feed(&b, true, 1000, ev, 64); /* 600 ms delay then every 200 ms */
    CHECK(n >= 3 && n <= 4);
    CHECK_EQ(ev[0], kBtnEvRepeat);
    n = Feed(&b, false, 100, ev, 64);
    CHECK_EQ(n, 0);
}

/** A key held (stuck/shorted) at boot never fires, not even its release; it works normally once released. */
static void ButtonsStuckAtBootIgnored(void)
{
    Btn b;
    BtnEvent ev[16];
    int n;
    BtnInit(&b, false);
    n = Feed(&b, true, 20000, ev, 16);
    CHECK_EQ(n, 0); /* no HOLD/LONG/WARN/VLONG (factory reset) */
    n = Feed(&b, false, 100, ev, 16);
    CHECK_EQ(n, 0); /* the release is not a PRESS either */
    n = Feed(&b, true, 100, ev, 16);
    CHECK_EQ(n, 0);
    n = Feed(&b, false, 100, ev, 16);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvPress);
    BtnInit(&b, true);
    n = Feed(&b, true, 5000, ev, 16);
    CHECK_EQ(n, 0); /* UP held at boot: no setpoint change */
}

/** A key stuck during runtime stops auto-repeating after BTN_STUCK_MS and recovers after a release. */
static void ButtonsStuckRuntimeRepeatStops(void)
{
    Btn b;
    BtnEvent ev[512];
    int n;
    BtnInit(&b, true);
    Feed(&b, false, 50, ev, 512);
    n = Feed(&b, true, BTN_STUCK_MS, ev, 512);
    CHECK(n > 100);
    CHECK(!b.stuck || n > 100);
    n = Feed(&b, true, 60000, ev, 512);
    CHECK_EQ(n, 0);
    CHECK(b.stuck);
    n = Feed(&b, false, 100, ev, 512);
    CHECK_EQ(n, 0);
    n = Feed(&b, true, 100, ev, 512);
    CHECK_EQ(n, 1);
    CHECK(!b.stuck); /* recovers after release */
}

/**
 * 20 min of identical readings are reported as frozen; any change, or normal RH noise, keeps the sensor alive.
 */
static void SensorFrozenDetection(void)
{
    SensorStuck s = {0};
    U64 t = 0;
    for (; t < SENSOR_STUCK_MS; t += 2000)
        CHECK(!SensorStuckUpdate(&s, 2100, 4500, t));
    CHECK(SensorStuckUpdate(&s, 2100, 4500, t));         /* 20 min identical: frozen */
    CHECK(!SensorStuckUpdate(&s, 2100, 4501, t + 2000)); /* any change: alive again */
    SensorStuck n = {0};                                 /* noisy RH keeps it alive */
    for (t = 0; t < 3 * SENSOR_STUCK_MS; t += 2000)
        CHECK(!SensorStuckUpdate(&n, 2100, (U16)(4500 + (t / 2000) % 2), t));
}

/**
 * MODE: short press on release; hold, long, warn and very-long stages at their thresholds; release after a hold.
 */
static void ButtonsModeShortLong(void)
{
    Btn b;
    BtnEvent ev[8];
    int n;
    BtnInit(&b, false);
    n = Feed(&b, false, 50, ev, 8);
    CHECK_EQ(n, 0); /* arm: released after boot */
    n = Feed(&b, true, 500, ev, 8);
    CHECK_EQ(n, 0); /* nothing until release */
    n = Feed(&b, false, 100, ev, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvPress);
    n = Feed(&b, true, 1600, ev, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvHold);
    n = Feed(&b, false, 100, ev, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvRelease); /* no short press after hold */
    n = Feed(&b, true, 5100, ev, 8);
    CHECK_EQ(n, 2);
    CHECK_EQ(ev[0], kBtnEvHold);
    CHECK_EQ(ev[1], kBtnEvLong);
    n = Feed(&b, true, 2850, ev, 8);
    CHECK_EQ(n, 0); /* ~7.9 s held: no warning yet */
    n = Feed(&b, true, 100, ev, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvWarn);
    n = Feed(&b, true, 6900, ev, 8);
    CHECK_EQ(n, 0);
    n = Feed(&b, true, 100, ev, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvVlong);
    n = Feed(&b, false, 100, ev, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0], kBtnEvRelease);
}

/** FormatTemp() in °C/°F, with and without tenths, including negative values. */
static void TempFormatting(void)
{
    char b[16];
    FormatTemp(b, sizeof b, 2150, false, true);
    CHECK(!strcmp(b, "21.5"));
    FormatTemp(b, sizeof b, 2150, true, true);
    CHECK(!strcmp(b, "70.7"));
    FormatTemp(b, sizeof b, 2150, true, false);
    CHECK(!strcmp(b, "71"));
    FormatTemp(b, sizeof b, -50, false, true);
    CHECK(!strcmp(b, "-0.5"));
    FormatTemp(b, sizeof b, 2000, true, false);
    CHECK(!strcmp(b, "68"));
}

/**
 * Write the frame as a plain PBM (P1) image for visual review; silently skipped if the file can't be opened.
 */
static void WritePbm(const GfxFb *pFb, const char *pName)
{
    FILE *pF = fopen(pName, "w");
    if (!pF)
        return;
    fprintf(pF, "P1\n%d %d\n", GFX_W, GFX_H);
    for (int y = 0; y < GFX_H; y++)
    {
        for (int x = 0; x < GFX_W; x++)
            fputc(GfxGet(pFb, x, y) ? '1' : '0', pF);
        fputc('\n', pF);
    }
    fclose(pF);
}

/**
 * Render a cool/°C/sensor-fault/pairing screen; nothing may be drawn in the last column. Writes display_preview_cool_c.pbm.
 */
static void RenderPreviewCoolCelsiusFault(void)
{
    static GfxFb s_fb;
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = false;
    m.rhValid = false;
    m.s = (CtrlSettings){.mode = kCtrlModeCool, .fan = kCtrlFanOn, .heatSp = 2050, .coolSp = 2450};
    m.fault = kCtrlFaultSensorTimeout;
    m.link = kLinkPairing;
    m.fahrenheit = false;
    m.swing = 100;
    m.schedState = 0; /* no clock yet: holding the day program */
    DisplayRender(&s_fb, &m);
    /* nothing may be drawn past the right edge: last column must be clear except the status rule */
    int edge = 0;
    for (int y = 0; y < 100; y++)
        edge += GfxGet(&s_fb, GFX_W - 1, y);
    CHECK_EQ(edge, 0);
    WritePbm(&s_fb, "display_preview_cool_c.pbm");
}

/**
 * Mismatched pixels between the status-bar time slot and `pHhmm` drawn right-aligned at y = 108.
 * Zero means the clock is exactly the right-aligned string. `inverted` is light-on-dark (boost bar).
 */
static int TimeSlotDiff(const GfxFb *pFb, const char *pHhmm, bool inverted)
{
    int timeW = GfxTextWidth(&kFontSmall, pHhmm);
    int timeX = GFX_W - 4 - timeW;
    static GfxFb s_ref;
    GfxClear(&s_ref);
    if (inverted)
        GfxFillRect(&s_ref, timeX, 108, GFX_W - timeX, kFontSmall.height, true);
    GfxText(&s_ref, &kFontSmall, timeX, 108, pHhmm, !inverted);
    int diff = 0;
    for (int y = 108; y < 108 + kFontSmall.height; y++)
        for (int x = timeX; x < GFX_W; x++)
            diff += GfxGet(pFb, x, y) != GfxGet(&s_ref, x, y);
    return diff;
}

/**
 * Render boost-to-target (°F) and timed cool boost screens; the boost bar is inverted and keeps
 * 24h HH:MM when the clock is synced. Writes PBM previews.
 */
static void RenderPreviewBoosts(void)
{
    static GfxFb s_fb;
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = true;
    m.temp = 2050;
    m.rhValid = true;
    m.rh = 4000;
    m.s = (CtrlSettings){.mode = kCtrlModeHeat, .fan = kCtrlFanAuto, .heatSp = 2000, .coolSp = 2450};
    m.w = true;
    m.link = kLinkJoined;
    m.fahrenheit = true;
    m.schedState = 1; /* Day, clock synced (not "Day (no clock)") */
    m.clockKnown = true;
    m.localMinute = 21 * 60 + 5; /* 21:05, same 24-hour home preview */
    m.boost = kBoostToTarget;
    m.boostDir = kCtrlModeHeat;
    m.boostSp = 2222; /* 72 F */
    DisplayRender(&s_fb, &m);
    WritePbm(&s_fb, "display_preview_boost_target.pbm");
    CHECK(GfxGet(&s_fb, GFX_W - 2, 120)); /* inverted status bar */
    CHECK_EQ(TimeSlotDiff(&s_fb, "21:05", true), 0);
    m.boost = kBoostTimed;
    m.boostDir = kCtrlModeCool;
    m.boostMinLeft = 15;
    m.w = false;
    m.y = true;
    m.g = true;
    m.s.mode = kCtrlModeCool;
    DisplayRender(&s_fb, &m);
    CHECK_EQ(TimeSlotDiff(&s_fb, "21:05", true), 0);
    WritePbm(&s_fb, "display_preview_boost_timed.pbm");
}

/**
 * Render a typical heating screen in °F and check the amount of ink is plausible. Writes display_preview.pbm.
 */
static void RenderPreview(void)
{
    static GfxFb s_fb;
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = true;
    m.temp = 2147;
    m.rhValid = true;
    m.rh = 4630;
    m.s = (CtrlSettings){.mode = kCtrlModeHeat, .fan = kCtrlFanAuto, .heatSp = 2111, .coolSp = 2444};
    m.w = true;
    m.link = kLinkJoined;
    m.fahrenheit = true;
    m.swing = 50;     /* +/-0.9 F */
    m.schedState = 1; /* Day, clock synced (not "Day (no clock)") */
    m.clockKnown = true;
    m.localMinute = 21 * 60 + 5; /* 21:05, 24-hour */
    DisplayRender(&s_fb, &m);
    int black = 0;
    for (int y = 0; y < GFX_H; y++)
        for (int x = 0; x < GFX_W; x++)
            black += GfxGet(&s_fb, x, y);
    CHECK(black > 500 && black < GFX_W * GFX_H / 2);
    WritePbm(&s_fb, "display_preview.pbm");
}

/** Count black pixels in [x0, x1) x [y0, y1). */
static int InkRect(const GfxFb *pFb, int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            n += GfxGet(pFb, x, y);
    return n;
}

/** Count white pixels in the right-hand HH:MM slot of the status bar (y = 108, small font). */
static int TimeSlotWhite(const GfxFb *pFb)
{
    int timeX = GFX_W - 4 - GfxTextWidth(&kFontSmall, "23:59");
    int n = 0;
    for (int y = 108; y < 108 + kFontSmall.height; y++)
        for (int x = timeX; x < GFX_W; x++)
            n += !GfxGet(pFb, x, y);
    return n;
}

/** Black pixels in that same HH:MM slot. */
static int TimeSlotInk(const GfxFb *pFb)
{
    int timeX = GFX_W - 4 - GfxTextWidth(&kFontSmall, "23:59");
    return InkRect(pFb, timeX, 108, GFX_W, 108 + kFontSmall.height);
}

/** Mismatched pixels of `pS` drawn in the small font at (x, y) against the frame. */
static int TextDiff(const GfxFb *pFb, int x, int y, const char *pS)
{
    int w = GfxTextWidth(&kFontSmall, pS);
    static GfxFb s_ref;
    GfxClear(&s_ref);
    GfxText(&s_ref, &kFontSmall, x, y, pS, true);
    int diff = 0;
    for (int yy = y; yy < y + kFontSmall.height; yy++)
        for (int xx = x; xx < x + w; xx++)
            diff += GfxGet(pFb, xx, yy) != GfxGet(&s_ref, xx, yy);
    return diff;
}

/** A home model in heat / °F with the Zigbee link line, used by the clock previews. */
static DisplayModel ClockHome(bool known, U16 minute)
{
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = true;
    m.temp = 2147;
    m.rhValid = true;
    m.rh = 4630;
    m.s = (CtrlSettings){.mode = kCtrlModeHeat, .fan = kCtrlFanAuto, .heatSp = 2111, .coolSp = 2444};
    m.link = kLinkJoined;
    m.fahrenheit = true;
    m.schedState = known ? 1 : 0;
    m.clockKnown = known;
    m.localMinute = minute;
    return m;
}

/** 24h HH:MM, midnight wrap, no time while the clock is unknown, and "Day (no clock)" kept. */
static void ClockOnStatusBar(void)
{
    char b[8];
    FormatHhmm(b, sizeof b, 0);
    CHECK(!strcmp(b, "00:00"));
    FormatHhmm(b, sizeof b, 1439);
    CHECK(!strcmp(b, "23:59"));
    FormatHhmm(b, sizeof b, 60);
    CHECK(!strcmp(b, "01:00"));
    FormatHhmm(b, sizeof b, 1440);
    CHECK(!strcmp(b, "00:00"));

    static GfxFb s_fb, s_midnight;
    DisplayModel unknown = ClockHome(false, 21 * 60 + 5);
    DisplayRender(&s_fb, &unknown);
    CHECK_EQ(TimeSlotInk(&s_fb), 0);
    CHECK_EQ(TextDiff(&s_fb, 8, 84, "Day (no clock)"), 0);

    DisplayModel day = ClockHome(true, 21 * 60 + 5);
    DisplayRender(&s_fb, &day);
    CHECK_EQ(TimeSlotDiff(&s_fb, "21:05", false), 0);
    CHECK(TimeSlotInk(&s_fb) > 0);
    CHECK_EQ(TextDiff(&s_fb, 8, 84, "Day"), 0);
    /* "Day" must not be the prefix of "Day (no clock)" still sitting on the glass */
    {
        int dayW = GfxTextWidth(&kFontSmall, "Day");
        CHECK_EQ(InkRect(&s_fb, 8 + dayW, 84, 8 + dayW + 48, 84 + kFontSmall.height), 0);
    }
    /* four-pixel right margin: the time advance ends at x = 291 */
    CHECK_EQ(InkRect(&s_fb, GFX_W - 4, 108, GFX_W, 108 + kFontSmall.height), 0);

    DisplayModel m2359 = ClockHome(true, 1439);
    DisplayRender(&s_midnight, &m2359);
    CHECK_EQ(TimeSlotDiff(&s_midnight, "23:59", false), 0);
    WritePbm(&s_midnight, "display_preview_midnight.pbm");
    DisplayModel m0000 = ClockHome(true, 0);
    DisplayRender(&s_fb, &m0000);
    CHECK_EQ(TimeSlotDiff(&s_fb, "00:00", false), 0);
    CHECK(memcmp(s_fb.buf, s_midnight.buf, sizeof s_fb.buf) != 0);
    WritePbm(&s_fb, "display_preview_midnight_0000.pbm");
}

/**
 * Longest real status strings at 296 px. Time stays when it fits ("Updating firmware 100%",
 * a boost line, the link line) and is dropped when a fault or long banner would meet it.
 */
static void ClockBannerClash(void)
{
    static GfxFb s_fb;
    DisplayModel m = ClockHome(true, 23 * 60 + 59);

    m.pBanner = "Updating firmware 100%"; /* longest banner that still leaves a gap */
    DisplayRender(&s_fb, &m);
    CHECK_EQ(TimeSlotDiff(&s_fb, "23:59", false), 0);
    WritePbm(&s_fb, "display_preview_clock_banner.pbm");

    m.pBanner = "SAFE MODE: repeated resets";
    DisplayRender(&s_fb, &m);
    CHECK_EQ(TimeSlotInk(&s_fb), 0);
    CHECK_EQ(TextDiff(&s_fb, 4, 108, "SAFE MODE: repeated resets"), 0);

    m.pBanner = "Already paired (reset to re-pair)"; /* longest banner the firmware shows */
    DisplayRender(&s_fb, &m);
    CHECK_EQ(TextDiff(&s_fb, 4, 108, "Already paired (reset to re-pair)"), 0);
    /* The banner itself runs into the time slot; dropping the clock must match a no-clock frame. */
    {
        DisplayModel bare = m;
        static GfxFb s_bare;
        bare.clockKnown = false;
        DisplayRender(&s_bare, &bare);
        CHECK(memcmp(s_fb.buf, s_bare.buf, sizeof s_fb.buf) == 0);
    }
    WritePbm(&s_fb, "display_preview_clock_clash.pbm");

    m.pBanner = "Zigbee: connected";
    /* pBanner replaces the link line; this string is the short one and must keep the time */
    DisplayRender(&s_fb, &m);
    CHECK_EQ(TimeSlotDiff(&s_fb, "23:59", false), 0);

    m.pBanner = NULL;
    m.view.kind = kViewHomeBanner;
    snprintf(m.view.hint, sizeof m.view.hint, "Let go: pair  |  hold on: reset");
    DisplayRender(&s_fb, &m);
    CHECK_EQ(TimeSlotWhite(&s_fb), 0); /* inverted bar, time dropped: no light pixels in the slot */

    memset(&m.view, 0, sizeof m.view);
    m.boost = kBoostTimed;
    m.boostDir = kCtrlModeHeat;
    m.boostMinLeft = 120;
    DisplayRender(&s_fb, &m);
    CHECK(TimeSlotWhite(&s_fb) > 0); /* "BOOST heat 120 min left" fits beside the time */
}

/**
 * Background slot: 3 min while the clock is unknown; once the clock is known, the next local
 * minute (including 23:59 -> 00:00 and a DST jump), not lastRefresh + 60 s. De-ghost waits for
 * 3 h and 40 partials, with a 6 h cap, so 20 once-a-minute partials do not full-flash.
 */
static void RefreshPolicy(void)
{
    CHECK_EQ(DISPLAY_BG_UNKNOWN_MS, 3u * 60u * 1000u);
    CHECK_EQ(DISPLAY_BG_KNOWN_MS, 60u * 1000u);
    CHECK_EQ(DISPLAY_BG_CAP_MS, 90u * 1000u);
    CHECK_EQ(DISPLAY_FAIL_BACKOFF_MS, 5u * 1000u);
    CHECK_EQ(DISPLAY_FULL_GAP_MS, 3ULL * 3600ULL * 1000ULL);
    CHECK_EQ(DISPLAY_FULL_PARTIALS, 40);
    CHECK_EQ(DISPLAY_FULL_CAP_MS, 6ULL * 3600ULL * 1000ULL);

    /* unknown clock: dirty models wait 3 min; a clean model never refreshes */
    CHECK(!DisplayBackgroundDue(false, false, true, 179999, 0, 0, 0, -1));
    CHECK(DisplayBackgroundDue(false, false, true, 180000, 0, 0, 0, -1));
    CHECK(!DisplayBackgroundDue(false, false, false, 180000, 0, 0, 0, -1));
    CHECK(DisplayBackgroundDue(true, false, true, 0, 0, 0, 0, -1));
    CHECK(!DisplayBackgroundDue(true, false, false, 1000, 0, 0, 0, -1));

    U64 start = 1000000; /* monotonic start of local minute 23:59 */
    CHECK_EQ(DisplayClockMinute(1439, start, start + 59999), 1439);
    CHECK_EQ(DisplayClockMinute(1439, start, start + 60000), 0); /* 23:59 -> 00:00 */
    CHECK_EQ(DisplayClockMinute(1439, start, start + 120000), 1);
    CHECK_EQ(DisplayClockMinute(1439, 0, start + 60000), 1439); /* phase unknown: no extrapolate */

    /* urgent refresh 50 s into the minute must not hold the new minute until +60 s */
    CHECK(!DisplayBackgroundDue(false, true, true, start + 59000, start + 50000, 1439, start, 1439));
    CHECK(DisplayBackgroundDue(false, true, false, start + 60000, start + 50000, 1439, start, 1439));
    /* sensor change mid-minute rides along; it does not open a second refresh path */
    CHECK(!DisplayBackgroundDue(false, true, true, start + 10000, start, 1439, start, 1439));
    /* stuck phase (minute start unknown, posted minute not moving): flush a dirty model at the cap */
    CHECK(!DisplayBackgroundDue(false, true, true, 89999, 0, 10, 0, 10));
    CHECK(DisplayBackgroundDue(false, true, true, 90000, 0, 10, 0, 10));
    CHECK(!DisplayBackgroundDue(false, true, false, 90000, 0, 10, 0, 10));

    /* Failed show, clock known: minute already matches, model still dirty. Due after the
     * backoff, not at the next rollover, and not on the very next 200 ms poll. */
    {
        U64 failedAt = 800000;
        U16 minuteNow = 10; /* minuteStart 0: no extrapolation, so this is the painted minute */
        CHECK(!DisplayBackgroundDue(
            true,
            true,
            true,
            failedAt + DISPLAY_FAIL_BACKOFF_MS - 1,
            failedAt,
            minuteNow,
            0,
            (int)minuteNow));
        CHECK(DisplayBackgroundDue(
            true,
            true,
            true,
            failedAt + DISPLAY_FAIL_BACKOFF_MS,
            failedAt,
            minuteNow,
            0,
            (int)minuteNow));
        /* nothing new to draw: a clean model is not a retry */
        CHECK(!DisplayBackgroundDue(
            true,
            true,
            false,
            failedAt + DISPLAY_FAIL_BACKOFF_MS,
            failedAt,
            minuteNow,
            0,
            (int)minuteNow));
        /* no attempt yet: the first dirty frame is not held for the backoff */
        CHECK(DisplayBackgroundDue(true, true, true, 200, 0, minuteNow, 0, (int)minuteNow));
    }

    /* Failed show, clock unknown: same backoff. Not every 200 ms poll, and not the 3 min slot. */
    {
        U64 failedAt = 800000;
        CHECK(!DisplayBackgroundDue(true, false, true, failedAt + DISPLAY_FAIL_BACKOFF_MS - 1, failedAt, 0, 0, -1));
        CHECK(DisplayBackgroundDue(true, false, true, failedAt + DISPLAY_FAIL_BACKOFF_MS, failedAt, 0, 0, -1));
        CHECK(!DisplayBackgroundDue(true, false, false, failedAt + DISPLAY_FAIL_BACKOFF_MS, failedAt, 0, 0, -1));
        CHECK(DisplayBackgroundDue(true, false, true, 200, 0, 0, 0, -1));
    }

    /* DST / resync: local minute jumps an hour; due immediately, not at the next rollover */
    U64 now = 5000000;
    CHECK(DisplayBackgroundDue(false, true, true, now, now - 5000, 4 * 60, now - 1000, 3 * 60));

    /* 20 partials (and even 40, two hours in) must not force a full flash */
    CHECK(!DisplayRefreshIsFull(false, false, 20, 20ull * 60000ull, 0));
    CHECK(!DisplayRefreshIsFull(false, false, 40, 40ull * 60000ull, 0));
    CHECK(!DisplayRefreshIsFull(false, false, 100, 2ull * 3600ull * 1000ull, 0));
    CHECK(!DisplayRefreshIsFull(false, false, 39, 3ull * 3600ull * 1000ull, 0));
    CHECK(DisplayRefreshIsFull(false, false, 40, 3ull * 3600ull * 1000ull, 0));
    CHECK(!DisplayRefreshIsFull(false, false, 0, 6ull * 3600ull * 1000ull - 1ull, 0));
    CHECK(DisplayRefreshIsFull(false, false, 0, 6ull * 3600ull * 1000ull, 0));
    CHECK(DisplayRefreshIsFull(true, false, 0, 0, 0));
    CHECK(DisplayRefreshIsFull(false, true, 0, 0, 0));
}

/** Run every test in this file; exit status 1 if any failed (tinytest DONE()). */
int main(void)
{
    RUN(Sht40CrcKnownVector);
    RUN(Sht40ParseValues);
    RUN(ButtonsDebounceAndRepeat);
    RUN(ButtonsModeShortLong);
    RUN(SensorFrozenDetection);
    RUN(ButtonsStuckAtBootIgnored);
    RUN(ButtonsStuckRuntimeRepeatStops);
    RUN(TempFormatting);
    RUN(RenderPreview);
    RUN(RenderPreviewCoolCelsiusFault);
    RUN(RenderPreviewBoosts);
    RUN(ClockOnStatusBar);
    RUN(ClockBannerClash);
    RUN(RefreshPolicy);
    DONE();
}
