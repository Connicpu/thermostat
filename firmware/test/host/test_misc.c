/* Host tests: SHT40 decoding, button state machine, display rendering
 * (also writes a PBM preview of the screen to display_preview.pbm). */
#include "core.h"
#include "tinytest.h"
#include "sensor_stuck.h"
#include "sht40_parse.h"
#include "buttons.h"
#include "display_model.h"
#include <string.h>

static void Sht40CrcKnownVector(void)
{
    const U8 d[2] = {0xBE, 0xEF};          /* Sensirion datasheet example: CRC 0x92 */
    CHECK_EQ(Sht40Crc8(d, 2), 0x92);
}

static void Sht40ParseValues(void)
{
    U8 f[6] = {0x66, 0x66, 0, 0x80, 0x00, 0};
    f[2] = Sht40Crc8(f, 2); f[5] = Sht40Crc8(f + 3, 2);
    I16 t; U16 h;
    CHECK(Sht40Parse(f, &t, &h));
    CHECK_EQ(t, 2500);                          /* -45 + 175*0.4 = 25.00 */
    CHECK_EQ(h, 5650);                          /* -6 + 125*0.5 = 56.50 */
    f[5] ^= 1;
    CHECK(!Sht40Parse(f, &t, &h));
    U8 lo[6] = {0, 0, 0, 0, 0, 0};
    lo[2] = Sht40Crc8(lo, 2); lo[5] = Sht40Crc8(lo + 3, 2);
    CHECK(Sht40Parse(lo, &t, &h));
    CHECK_EQ(t, -4500);
    CHECK_EQ(h, 0);                             /* clamped */
}

static int Feed(Btn *pB, bool level, int ms, BtnEvent *pEvs, int max)
{
    int n = 0;
    for (int t = 0; t < ms; t += BTN_TICK_MS) {
        BtnEvent e = BtnTick(pB, level);
        if (e && n < max) pEvs[n++] = e;
    }
    return n;
}

static void ButtonsDebounceAndRepeat(void)
{
    Btn b; BtnEvent ev[64]; int n;
    BtnInit(&b, true);
    /* contact bounce: 10 ms toggles must not produce events */
    for (int i = 0; i < 10; i++) { n = Feed(&b, i & 1, 10, ev, 64); CHECK_EQ(n, 0); }
    n = Feed(&b, false, 100, ev, 64); CHECK_EQ(n, 0);
    n = Feed(&b, true, 100, ev, 64);  CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvPress);
    n = Feed(&b, true, 1000, ev, 64); /* 600 ms delay then every 200 ms */
    CHECK(n >= 3 && n <= 4);
    CHECK_EQ(ev[0], kBtnEvRepeat);
    n = Feed(&b, false, 100, ev, 64); CHECK_EQ(n, 0);
}

/* A key held (stuck/shorted) at boot never fires; it works normally once released. */
static void ButtonsStuckAtBootIgnored(void)
{
    Btn b; BtnEvent ev[16]; int n;
    BtnInit(&b, false);
    n = Feed(&b, true, 20000, ev, 16); CHECK_EQ(n, 0);         /* no HOLD/LONG/WARN/VLONG (factory reset) */
    n = Feed(&b, false, 100, ev, 16);  CHECK_EQ(n, 0);         /* the release is not a PRESS either */
    n = Feed(&b, true, 100, ev, 16);   CHECK_EQ(n, 0);
    n = Feed(&b, false, 100, ev, 16);  CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvPress);
    BtnInit(&b, true);
    n = Feed(&b, true, 5000, ev, 16);  CHECK_EQ(n, 0);         /* UP held at boot: no setpoint change */
}

/* A key stuck during runtime: auto-repeat stops after BTN_STUCK_MS. */
static void ButtonsStuckRuntimeRepeatStops(void)
{
    Btn b; BtnEvent ev[512]; int n;
    BtnInit(&b, true);
    Feed(&b, false, 50, ev, 512);
    n = Feed(&b, true, BTN_STUCK_MS, ev, 512);
    CHECK(n > 100);
    CHECK(!b.stuck || n > 100);
    n = Feed(&b, true, 60000, ev, 512); CHECK_EQ(n, 0); CHECK(b.stuck);
    n = Feed(&b, false, 100, ev, 512); CHECK_EQ(n, 0);
    n = Feed(&b, true, 100, ev, 512);  CHECK_EQ(n, 1); CHECK(!b.stuck);   /* recovers after release */
}

static void SensorFrozenDetection(void)
{
    SensorStuck s = {0};
    U64 t = 0;
    for (; t < SENSOR_STUCK_MS; t += 2000) CHECK(!SensorStuckUpdate(&s, 2100, 4500, t));
    CHECK(SensorStuckUpdate(&s, 2100, 4500, t));                 /* 20 min identical: frozen */
    CHECK(!SensorStuckUpdate(&s, 2100, 4501, t + 2000));         /* any change: alive again */
    SensorStuck n = {0};                                        /* noisy RH keeps it alive */
    for (t = 0; t < 3 * SENSOR_STUCK_MS; t += 2000) CHECK(!SensorStuckUpdate(&n, 2100, (U16)(4500 + (t / 2000) % 2), t));
}

static void ButtonsModeShortLong(void)
{
    Btn b; BtnEvent ev[8]; int n;
    BtnInit(&b, false);
    n = Feed(&b, false, 50, ev, 8);  CHECK_EQ(n, 0);           /* arm: released after boot */
    n = Feed(&b, true, 500, ev, 8);  CHECK_EQ(n, 0);           /* nothing until release */
    n = Feed(&b, false, 100, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvPress);
    n = Feed(&b, true, 1600, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvHold);
    n = Feed(&b, false, 100, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvRelease); /* no short press after hold */
    n = Feed(&b, true, 5100, ev, 8); CHECK_EQ(n, 2); CHECK_EQ(ev[0], kBtnEvHold); CHECK_EQ(ev[1], kBtnEvLong);
    n = Feed(&b, true, 2850, ev, 8); CHECK_EQ(n, 0);                       /* ~7.9 s held: no warning yet */
    n = Feed(&b, true, 100, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvWarn);
    n = Feed(&b, true, 6900, ev, 8); CHECK_EQ(n, 0);
    n = Feed(&b, true, 100, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvVlong);
    n = Feed(&b, false, 100, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], kBtnEvRelease);
}

static void TempFormatting(void)
{
    char b[16];
    FormatTemp(b, sizeof b, 2150, false, true);  CHECK(!strcmp(b, "21.5"));
    FormatTemp(b, sizeof b, 2150, true, true);   CHECK(!strcmp(b, "70.7"));
    FormatTemp(b, sizeof b, 2150, true, false);  CHECK(!strcmp(b, "71"));
    FormatTemp(b, sizeof b, -50, false, true);   CHECK(!strcmp(b, "-0.5"));
    FormatTemp(b, sizeof b, 2000, true, false);  CHECK(!strcmp(b, "68"));
}

static void WritePbm(const GfxFb *pFb, const char *pName)
{
    FILE *pF = fopen(pName, "w");
    if (!pF) return;
    fprintf(pF, "P1\n%d %d\n", GFX_W, GFX_H);
    for (int y = 0; y < GFX_H; y++) { for (int x = 0; x < GFX_W; x++) fputc(GfxGet(pFb, x, y) ? '1' : '0', pF); fputc('\n', pF); }
    fclose(pF);
}

static void RenderPreviewCoolCelsiusFault(void)
{
    static GfxFb s_fb;
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = false; m.rhValid = false;
    m.s = (CtrlSettings){.mode = kCtrlModeCool, .fan = kCtrlFanOn, .heatSp = 2050, .coolSp = 2450};
    m.fault = kCtrlFaultSensorTimeout; m.link = kLinkPairing; m.fahrenheit = false; m.swing = 100;
    m.schedState = 0;                       /* no clock yet: holding the day program */
    DisplayRender(&s_fb, &m);
    /* nothing may be drawn past the right edge: last column must be clear except the status rule */
    int edge = 0;
    for (int y = 0; y < 100; y++) edge += GfxGet(&s_fb, GFX_W - 1, y);
    CHECK_EQ(edge, 0);
    WritePbm(&s_fb, "display_preview_cool_c.pbm");
}

static void RenderPreviewBoosts(void)
{
    static GfxFb s_fb;
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = true; m.temp = 2050; m.rhValid = true; m.rh = 4000;
    m.s = (CtrlSettings){.mode = kCtrlModeHeat, .fan = kCtrlFanAuto, .heatSp = 2000, .coolSp = 2450};
    m.w = true; m.link = kLinkJoined; m.fahrenheit = true;
    m.boost = kBoostToTarget; m.boostDir = kCtrlModeHeat; m.boostSp = 2222;   /* 72 F */
    DisplayRender(&s_fb, &m);
    WritePbm(&s_fb, "display_preview_boost_target.pbm");
    CHECK(GfxGet(&s_fb, GFX_W - 2, 120));       /* inverted status bar */
    m.boost = kBoostTimed; m.boostDir = kCtrlModeCool; m.boostMinLeft = 15; m.w = false; m.y = true; m.g = true;
    m.s.mode = kCtrlModeCool;
    DisplayRender(&s_fb, &m);
    WritePbm(&s_fb, "display_preview_boost_timed.pbm");
}

static void RenderPreview(void)
{
    static GfxFb s_fb;
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = true; m.temp = 2147; m.rhValid = true; m.rh = 4630;
    m.s = (CtrlSettings){.mode = kCtrlModeHeat, .fan = kCtrlFanAuto, .heatSp = 2111, .coolSp = 2444};
    m.w = true; m.link = kLinkJoined; m.fahrenheit = true; m.swing = 50;   /* +/-0.9 F */
    DisplayRender(&s_fb, &m);
    int black = 0;
    for (int y = 0; y < GFX_H; y++) for (int x = 0; x < GFX_W; x++) black += GfxGet(&s_fb, x, y);
    CHECK(black > 500 && black < GFX_W * GFX_H / 2);
    WritePbm(&s_fb, "display_preview.pbm");
}

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
    DONE();
}
