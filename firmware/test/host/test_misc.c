/* Host tests: SHT40 decoding, button state machine, display rendering
 * (also writes a PBM preview of the screen to display_preview.pbm). */
#include "tinytest.h"
#include "sht40_parse.h"
#include "buttons.h"
#include "display_model.h"
#include <string.h>

static void sht40_crc_known_vector(void)
{
    const uint8_t d[2] = {0xBE, 0xEF};          /* Sensirion datasheet example: CRC 0x92 */
    CHECK_EQ(sht40_crc8(d, 2), 0x92);
}

static void sht40_parse_values(void)
{
    uint8_t f[6] = {0x66, 0x66, 0, 0x80, 0x00, 0};
    f[2] = sht40_crc8(f, 2); f[5] = sht40_crc8(f + 3, 2);
    int16_t t; uint16_t h;
    CHECK(sht40_parse(f, &t, &h));
    CHECK_EQ(t, 2500);                          /* -45 + 175*0.4 = 25.00 */
    CHECK_EQ(h, 5650);                          /* -6 + 125*0.5 = 56.50 */
    f[5] ^= 1;
    CHECK(!sht40_parse(f, &t, &h));
    uint8_t lo[6] = {0, 0, 0, 0, 0, 0};
    lo[2] = sht40_crc8(lo, 2); lo[5] = sht40_crc8(lo + 3, 2);
    CHECK(sht40_parse(lo, &t, &h));
    CHECK_EQ(t, -4500);
    CHECK_EQ(h, 0);                             /* clamped */
}

static int feed(btn_t *b, bool level, int ms, btn_event_t *evs, int max)
{
    int n = 0;
    for (int t = 0; t < ms; t += BTN_TICK_MS) {
        btn_event_t e = btn_tick(b, level);
        if (e && n < max) evs[n++] = e;
    }
    return n;
}

static void buttons_debounce_and_repeat(void)
{
    btn_t b; btn_event_t ev[64]; int n;
    btn_init(&b, true);
    /* contact bounce: 10 ms toggles must not produce events */
    for (int i = 0; i < 10; i++) { n = feed(&b, i & 1, 10, ev, 64); CHECK_EQ(n, 0); }
    n = feed(&b, false, 100, ev, 64); CHECK_EQ(n, 0);
    n = feed(&b, true, 100, ev, 64);  CHECK_EQ(n, 1); CHECK_EQ(ev[0], BTN_EV_PRESS);
    n = feed(&b, true, 1000, ev, 64); /* 600 ms delay then every 200 ms */
    CHECK(n >= 3 && n <= 4);
    CHECK_EQ(ev[0], BTN_EV_REPEAT);
    n = feed(&b, false, 100, ev, 64); CHECK_EQ(n, 0);
}

static void buttons_mode_short_long(void)
{
    btn_t b; btn_event_t ev[8]; int n;
    btn_init(&b, false);
    n = feed(&b, true, 500, ev, 8);  CHECK_EQ(n, 0);           /* nothing until release */
    n = feed(&b, false, 100, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], BTN_EV_PRESS);
    n = feed(&b, true, 3100, ev, 8); CHECK_EQ(n, 1); CHECK_EQ(ev[0], BTN_EV_LONG);
    n = feed(&b, false, 100, ev, 8); CHECK_EQ(n, 0);           /* no short press after long */
    n = feed(&b, true, 10100, ev, 8); CHECK_EQ(n, 2); CHECK_EQ(ev[0], BTN_EV_LONG); CHECK_EQ(ev[1], BTN_EV_VLONG);
    n = feed(&b, false, 100, ev, 8); CHECK_EQ(n, 0);
}

static void temp_formatting(void)
{
    char b[16];
    format_temp(b, sizeof b, 2150, false, true);  CHECK(!strcmp(b, "21.5"));
    format_temp(b, sizeof b, 2150, true, true);   CHECK(!strcmp(b, "70.7"));
    format_temp(b, sizeof b, 2150, true, false);  CHECK(!strcmp(b, "71"));
    format_temp(b, sizeof b, -50, false, true);   CHECK(!strcmp(b, "-0.5"));
    format_temp(b, sizeof b, 2000, true, false);  CHECK(!strcmp(b, "68"));
}

static void write_pbm(const gfx_fb_t *fb, const char *name)
{
    FILE *f = fopen(name, "w");
    if (!f) return;
    fprintf(f, "P1\n%d %d\n", GFX_W, GFX_H);
    for (int y = 0; y < GFX_H; y++) { for (int x = 0; x < GFX_W; x++) fputc(gfx_get(fb, x, y) ? '1' : '0', f); fputc('\n', f); }
    fclose(f);
}

static void render_preview_auto_celsius_fault(void)
{
    static gfx_fb_t fb;
    display_model_t m;
    memset(&m, 0, sizeof m);
    m.temp_valid = false; m.rh_valid = false;
    m.s = (ctrl_settings_t){.mode = CTRL_MODE_AUTO, .fan = CTRL_FAN_ON, .heat_sp = 2050, .cool_sp = 2450};
    m.fault = CTRL_FAULT_SENSOR_TIMEOUT; m.link = LINK_PAIRING; m.fahrenheit = false;
    display_render(&fb, &m);
    /* nothing may be drawn past the right edge: last column must be clear except the status rule */
    int edge = 0;
    for (int y = 0; y < 100; y++) edge += gfx_get(&fb, GFX_W - 1, y);
    CHECK_EQ(edge, 0);
    write_pbm(&fb, "display_preview_auto_c.pbm");
}

static void render_preview_boosts(void)
{
    static gfx_fb_t fb;
    display_model_t m;
    memset(&m, 0, sizeof m);
    m.temp_valid = true; m.temp = 2050; m.rh_valid = true; m.rh = 4000;
    m.s = (ctrl_settings_t){.mode = CTRL_MODE_HEAT, .fan = CTRL_FAN_AUTO, .heat_sp = 2000, .cool_sp = 2450};
    m.w = true; m.link = LINK_JOINED; m.fahrenheit = true;
    m.boost = BOOST_TO_TARGET; m.boost_dir = CTRL_MODE_HEAT; m.boost_sp = 2222;   /* 72 F */
    display_render(&fb, &m);
    write_pbm(&fb, "display_preview_boost_target.pbm");
    CHECK(gfx_get(&fb, GFX_W - 2, 120));       /* inverted status bar */
    m.boost = BOOST_TIMED; m.boost_dir = CTRL_MODE_COOL; m.boost_min_left = 15; m.w = false; m.y = true; m.g = true;
    m.s.mode = CTRL_MODE_COOL;
    display_render(&fb, &m);
    write_pbm(&fb, "display_preview_boost_timed.pbm");
}

static void render_preview(void)
{
    static gfx_fb_t fb;
    display_model_t m;
    memset(&m, 0, sizeof m);
    m.temp_valid = true; m.temp = 2147; m.rh_valid = true; m.rh = 4630;
    m.s = (ctrl_settings_t){.mode = CTRL_MODE_HEAT, .fan = CTRL_FAN_AUTO, .heat_sp = 2111, .cool_sp = 2444};
    m.w = true; m.link = LINK_JOINED; m.fahrenheit = true;
    display_render(&fb, &m);
    int black = 0;
    for (int y = 0; y < GFX_H; y++) for (int x = 0; x < GFX_W; x++) black += gfx_get(&fb, x, y);
    CHECK(black > 500 && black < GFX_W * GFX_H / 2);
    write_pbm(&fb, "display_preview.pbm");
}

int main(void)
{
    RUN(sht40_crc_known_vector);
    RUN(sht40_parse_values);
    RUN(buttons_debounce_and_repeat);
    RUN(buttons_mode_short_long);
    RUN(temp_formatting);
    RUN(render_preview);
    RUN(render_preview_auto_celsius_fault);
    RUN(render_preview_boosts);
    DONE();
}
