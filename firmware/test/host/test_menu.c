/* Host tests for the on-device menu state machine (+ PBM previews of every screen). */
#include "tinytest.h"
#include "menu.h"
#include "buttons.h"
#include "gfx.h"
#include <string.h>

static menu_t M;
static menu_ctx_t C;
static uint64_t T;

static void reset(void)
{
    menu_init(&M);
    T = 1000;
    C = (menu_ctx_t){.mode = CTRL_MODE_HEAT, .boost_sp = 2200, .boost_minutes = 30, .swing = 50,
                     .fahrenheit = false, .link = LINK_JOINED, .fw_version = "0.2.0",
                     .raw_valid = true, .raw_temp = 2263, .raw_rh = 4512, .uptime_s = 2 * 86400 + 3 * 3600 + 7 * 60};
}
static menu_action_t key(menu_key_t k, btn_event_t ev) { T += 300; return menu_key(&M, &C, k, ev, T); }
static menu_action_t up(void)   { return key(MENU_KEY_UP, BTN_EV_PRESS); }
static menu_action_t down(void) { return key(MENU_KEY_DOWN, BTN_EV_PRESS); }
static menu_action_t sel(void)  { return key(MENU_KEY_M, BTN_EV_PRESS); }
static void goto_item(int i) { sel(); for (int k = 0; k < i; k++) down(); }

static void home_keys_pass_through(void)
{
    reset();
    CHECK_EQ(up().kind, MA_HOME_KEY);
    CHECK_EQ(down().kind, MA_HOME_KEY);
    CHECK_EQ(key(MENU_KEY_COMBO, BTN_EV_PRESS).kind, MA_HOME_KEY);
    CHECK(menu_is_home(&M));
    menu_action_t a = sel();
    CHECK_EQ(M.st, MS_LIST);
    CHECK(a.redraw);
    CHECK(!menu_is_home(&M));
}

static void navigation_wraps_and_back(void)
{
    reset();
    sel();
    CHECK_EQ(M.cursor, MI_MODE);
    up();                                   /* wraps to the last row */
    CHECK_EQ(M.cursor, MI_BACK);
    down();
    CHECK_EQ(M.cursor, MI_MODE);
    for (int i = 0; i < MI_BACK; i++) down();
    menu_action_t a = sel();
    CHECK(menu_is_home(&M));
    CHECK(a.full_refresh);
    CHECK_EQ(a.kind, MA_NONE);
}

static void timeout_discards_edit(void)
{
    reset();
    goto_item(MI_SWING);
    sel();
    CHECK_EQ(M.st, MS_EDIT_SWING);
    up(); up();
    CHECK_EQ(M.edit, 70);
    CHECK_EQ(menu_tick(&M, T + MENU_TIMEOUT_MS - 1).kind, MA_NONE);
    CHECK(!menu_is_home(&M));
    menu_action_t a = menu_tick(&M, T + MENU_TIMEOUT_MS);
    CHECK(menu_is_home(&M));
    CHECK_EQ(a.kind, MA_NONE);              /* nothing saved */
    CHECK(a.full_refresh);
    CHECK_EQ(menu_tick(&M, T + 3 * MENU_TIMEOUT_MS).kind, MA_NONE);   /* no-op at home */
}

static void edit_mode_saves(void)
{
    reset();
    goto_item(MI_MODE);
    sel();
    CHECK_EQ(M.st, MS_EDIT_MODE);
    up();                                   /* Heat -> Cool */
    up();                                   /* -> Off (no Auto any more) */
    menu_action_t a = sel();
    CHECK_EQ(a.kind, MA_SET_MODE);
    CHECK_EQ(a.value, CTRL_MODE_OFF);
    CHECK_EQ(M.st, MS_LIST);
    goto_item(0); /* (already in list; sel opens Mode again) */
    CHECK_EQ(M.st, MS_EDIT_MODE);
    up(); up(); up(); up();                 /* Heat -> wraps past Off -> Cool */
    CHECK_EQ(sel().value, CTRL_MODE_COOL);
}

static void edit_fan_toggles(void)
{
    reset();
    C.fan = CTRL_FAN_AUTO;
    goto_item(MI_FAN);
    sel();
    CHECK_EQ(M.st, MS_EDIT_FAN);
    menu_view_t v;
    menu_view(&M, &C, &v);
    CHECK(strcmp(v.big, "Auto") == 0);
    up();
    menu_view(&M, &C, &v);
    CHECK(strcmp(v.big, "On") == 0);
    menu_action_t a = sel();
    CHECK_EQ(a.kind, MA_SET_FAN);
    CHECK_EQ(a.value, CTRL_FAN_ON);
    CHECK_EQ(M.st, MS_LIST);
    C.fan = CTRL_FAN_ON;
    menu_view(&M, &C, &v);
    CHECK(strcmp(v.value[MI_FAN], "On") == 0);
    sel();                                  /* cursor still on Fan: reopen, starts at On */
    down();
    CHECK_EQ(sel().value, CTRL_FAN_AUTO);
}

static void edit_swing_clamps(void)
{
    reset();
    goto_item(MI_SWING); sel();
    for (int i = 0; i < 20; i++) up();
    CHECK_EQ(M.edit, CTRL_SWING_MAX);
    for (int i = 0; i < 20; i++) key(MENU_KEY_DOWN, BTN_EV_REPEAT);
    CHECK_EQ(M.edit, CTRL_SWING_MIN);
    up();
    menu_action_t a = sel();
    CHECK_EQ(a.kind, MA_SET_SWING);
    CHECK_EQ(a.value, 20);
}

static void edit_units(void)
{
    reset();
    goto_item(MI_UNITS); sel();
    down();
    menu_action_t a = sel();
    CHECK_EQ(a.kind, MA_SET_UNITS);
    CHECK_EQ(a.value, 1);
}

static void boost_target_and_timed(void)
{
    reset();
    goto_item(MI_BOOST); sel();
    CHECK_EQ(M.st, MS_BOOST);
    sel();                                  /* To target */
    CHECK_EQ(M.st, MS_EDIT_BOOST_SP);
    up(); up();
    menu_action_t a = sel();
    CHECK_EQ(a.kind, MA_BOOST_TARGET);
    CHECK_EQ(a.value, 2300);
    CHECK(menu_is_home(&M) && a.full_refresh);

    C.fahrenheit = true;                    /* 1 °F steps */
    goto_item(MI_BOOST); sel(); sel();
    up();
    a = sel();
    CHECK_EQ(a.value, 2278);                /* 72 F -> 73 F (22.78 C) */

    C.boost_minutes = 37;                   /* from HA: snaps to the 15 min grid */
    goto_item(MI_BOOST); sel(); down(); sel();
    CHECK_EQ(M.st, MS_EDIT_BOOST_MIN);
    CHECK_EQ(M.edit, 30);
    for (int i = 0; i < 10; i++) up();
    CHECK_EQ(M.edit, 120);
    for (int i = 0; i < 10; i++) down();
    CHECK_EQ(M.edit, 15);
    up();
    a = sel();
    CHECK_EQ(a.kind, MA_BOOST_TIMED);
    CHECK_EQ(a.value, 30);
}

static void boost_cancel_only_when_active(void)
{
    reset();
    goto_item(MI_BOOST); sel();
    down(); down();                         /* To target, For N, Back */
    sel();
    CHECK_EQ(M.st, MS_LIST);                /* that was Back */
    C.boost_active = true;
    sel();                                  /* Boost again (cursor still on Boost) */
    CHECK_EQ(M.st, MS_BOOST);
    down(); down();
    menu_action_t a = sel();
    CHECK_EQ(a.kind, MA_BOOST_CANCEL);
    CHECK(menu_is_home(&M));
}

static void zigbee_hold_to_pair(void)
{
    reset();
    goto_item(MI_ZIGBEE); sel();
    CHECK_EQ(M.st, MS_ZIGBEE);
    menu_action_t a = key(MENU_KEY_M, BTN_EV_HOLD);
    CHECK_EQ(a.kind, MA_PAIR);
    CHECK(menu_is_home(&M));
    /* the rest of that hold must not escalate into the home pairing/reset path */
    CHECK_EQ(key(MENU_KEY_M, BTN_EV_LONG).kind, MA_NONE);
    CHECK_EQ(key(MENU_KEY_M, BTN_EV_WARN).kind, MA_NONE);
    CHECK_EQ(key(MENU_KEY_M, BTN_EV_VLONG).kind, MA_NONE);
    CHECK_EQ(M.st, MS_HOME);
    key(MENU_KEY_M, BTN_EV_RELEASE);
    sel();
    CHECK_EQ(M.st, MS_LIST);                /* normal again */
    /* short M on the Zigbee screen goes back, no pairing */
    reset(); goto_item(MI_ZIGBEE); sel();
    CHECK_EQ(sel().kind, MA_NONE);
    CHECK_EQ(M.st, MS_LIST);
}

static void info_screen(void)
{
    reset();
    goto_item(MI_INFO); sel();
    CHECK_EQ(M.st, MS_INFO);
    menu_view_t v; menu_view(&M, &C, &v);
    CHECK_EQ(v.kind, VIEW_TEXT);
    CHECK(!strcmp(v.value[0], "0.2.0"));
    CHECK(!strcmp(v.value[1], "22.63\x7f" "C"));
    CHECK(!strcmp(v.value[2], "45.12%"));
    CHECK(!strcmp(v.value[3], "2d 03:07"));
    sel();
    CHECK_EQ(M.st, MS_LIST);
}

/* Drive the real button debouncer with a held MODE key and feed its events to the menu. */
static menu_action_t hold_m(int ms, bool release, int *n_pair, int *n_reset)
{
    btn_t b; btn_init(&b, false);
    menu_action_t last = {0};
    for (int t = 0; t < ms + (release ? 200 : 0); t += BTN_TICK_MS) {
        btn_event_t e = btn_tick(&b, t < ms);
        T += BTN_TICK_MS;
        if (e == BTN_EV_NONE) continue;
        menu_action_t a = menu_key(&M, &C, MENU_KEY_M, e, T);
        if (a.kind == MA_PAIR) (*n_pair)++;
        if (a.kind == MA_FACTORY_RESET) (*n_reset)++;
        if (a.kind != MA_NONE || a.redraw) last = a;
    }
    return last;
}

static void mode_hold_timings(void)
{
    int p, r;
    menu_view_t v;
    reset(); p = r = 0;
    hold_m(800, true, &p, &r);              /* short press: menu */
    CHECK_EQ(M.st, MS_LIST); CHECK_EQ(p, 0);

    reset(); p = r = 0;
    hold_m(3000, true, &p, &r);             /* 3 s: nothing */
    CHECK(M.st == MS_HOME && p == 0 && r == 0);

    reset(); p = r = 0;
    hold_m(5200, false, &p, &r);            /* reach 5 s: banner, no pairing yet */
    CHECK_EQ(M.st, MS_HOLD_PAIR); CHECK_EQ(p, 0);
    menu_view(&M, &C, &v); CHECK_EQ(v.kind, VIEW_HOME_BANNER);

    reset(); p = r = 0;
    hold_m(6500, true, &p, &r);             /* release between 5 and 8 s: pair once */
    CHECK_EQ(p, 1); CHECK_EQ(r, 0); CHECK_EQ(M.st, MS_HOME);

    reset(); p = r = 0;
    menu_action_t a = hold_m(8200, false, &p, &r);   /* 8 s: warning, full refresh */
    CHECK_EQ(M.st, MS_HOLD_RESET); CHECK(a.full_refresh); CHECK_EQ(p, 0);
    menu_view(&M, &C, &v); CHECK_EQ(v.kind, VIEW_WARNING);

    reset(); p = r = 0;
    a = hold_m(12000, true, &p, &r);        /* release before 15 s: cancelled, screen restored */
    CHECK_EQ(p, 0); CHECK_EQ(r, 0); CHECK_EQ(M.st, MS_HOME); CHECK(a.full_refresh);

    reset(); p = r = 0;
    hold_m(15100, true, &p, &r);            /* 15 s: reset, never pairing */
    CHECK_EQ(r, 1); CHECK_EQ(p, 0);

    reset(); p = r = 0;                     /* holds inside the menu do nothing dangerous */
    sel();
    hold_m(16000, true, &p, &r);
    CHECK(p == 0 && r == 0 && M.st == MS_LIST);
}

static void step_temp(void)
{
    CHECK_EQ(menu_step_temp(2010, 1, false), 2050);
    CHECK_EQ(menu_step_temp(2000, -1, false), 1950);
    CHECK_EQ(menu_step_temp(2000, 1, true), 2056);      /* 68 F -> 69 F */
}

/* ---- previews ---- */
static void write_pbm(const gfx_fb_t *fb, const char *name)
{
    FILE *f = fopen(name, "w");
    if (!f) return;
    fprintf(f, "P1\n%d %d\n", GFX_W, GFX_H);
    for (int y = 0; y < GFX_H; y++) { for (int x = 0; x < GFX_W; x++) fputc(gfx_get(fb, x, y) ? '1' : '0', f); fputc('\n', f); }
    fclose(f);
}

static void shot(const char *name)
{
    static gfx_fb_t fb;
    display_model_t m;
    memset(&m, 0, sizeof m);
    m.temp_valid = true; m.temp = 2150; m.rh_valid = true; m.rh = 4500;
    m.s = (ctrl_settings_t){.mode = CTRL_MODE_HEAT, .fan = CTRL_FAN_AUTO, .heat_sp = 2100, .cool_sp = 2450};
    m.link = LINK_JOINED; m.fahrenheit = C.fahrenheit; m.swing = C.swing;
    menu_view(&M, &C, &m.view);
    display_render(&fb, &m);
    /* nothing drawn past the right edge in list/text screens except highlight/scroll marks */
    write_pbm(&fb, name);
}

static void render_menu_previews(void)
{
    reset();
    sel();                                  shot("menu_main.pbm");
    for (int i = 0; i < 5; i++) down();
    shot("menu_main_scrolled.pbm");
    reset(); goto_item(MI_MODE); sel();     shot("menu_mode.pbm");
    reset(); C.boost_active = true; goto_item(MI_BOOST); sel(); shot("menu_boost.pbm");
    sel();                                  shot("menu_boost_target.pbm");
    reset(); goto_item(MI_BOOST); sel(); down(); sel(); shot("menu_boost_minutes.pbm");
    reset(); C.fahrenheit = true; goto_item(MI_SWING); sel(); shot("menu_swing_f.pbm");
    reset(); goto_item(MI_UNITS); sel();    shot("menu_units.pbm");
    reset(); goto_item(MI_ZIGBEE); sel();   shot("menu_zigbee.pbm");
    reset(); goto_item(MI_INFO); sel();     shot("menu_info.pbm");
    reset(); M.st = MS_HOLD_PAIR;           shot("menu_hold_pair.pbm");
    reset(); M.st = MS_HOLD_RESET;          shot("menu_reset_warning.pbm");
    /* every label/value must fit: check the text in the list view is not clipped into the value column */
    reset(); sel();
    menu_view_t v; menu_view(&M, &C, &v);
    CHECK_EQ(v.n_rows, MI_COUNT);
}

int main(void)
{
    RUN(home_keys_pass_through);
    RUN(navigation_wraps_and_back);
    RUN(timeout_discards_edit);
    RUN(edit_mode_saves);
    RUN(edit_fan_toggles);
    RUN(edit_swing_clamps);
    RUN(edit_units);
    RUN(boost_target_and_timed);
    RUN(boost_cancel_only_when_active);
    RUN(zigbee_hold_to_pair);
    RUN(info_screen);
    RUN(mode_hold_timings);
    RUN(step_temp);
    RUN(render_menu_previews);
    DONE();
}
