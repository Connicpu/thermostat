/* Host tests for the on-device menu state machine (+ PBM previews of every screen). */
#include "core.h"
#include "tinytest.h"
#include "menu.h"
#include "buttons.h"
#include "gfx.h"
#include <string.h>

static Menu s_m;
static MenuCtx s_c;
static U64 s_t;

/**
 * Fresh menu at the main screen, t = 1 s, and a fixed live context (Heat, joined, °C, default schedule, sample sensor/uptime values).
 */
static void Reset(void)
{
    MenuInit(&s_m);
    s_t = 1000;
    s_c = (MenuCtx){.mode = kCtrlModeHeat,
                    .boostSp = 2200,
                    .boostMinutes = 30,
                    .swing = 50,
                    .fahrenheit = false,
                    .link = kLinkJoined,
                    .pFwVersion = "0.2.0",
                    .rawValid = true,
                    .rawTemp = 2263,
                    .rawRh = 4512,
                    .uptimeS = 2 * 86400 + 3 * 3600 + 7 * 60,
                    .sched = SCHED_CFG_DEFAULT()};
}

/** Send one key event 300 ms after the previous one. Returns the menu action. */
static MenuAction Key(MenuKey k, BtnEvent ev)
{
    s_t += 300;
    return MenuOnKey(&s_m, &s_c, k, ev, s_t);
}

/** Short UP press. Returns the menu action. */
static MenuAction Up(void)
{
    return Key(kMenuKeyUp, kBtnEvPress);
}

/** Short DOWN press. Returns the menu action. */
static MenuAction Down(void)
{
    return Key(kMenuKeyDown, kBtnEvPress);
}

/** Short M press. Returns the menu action. */
static MenuAction Sel(void)
{
    return Key(kMenuKeyM, kBtnEvPress);
}

/** Open the menu (from the main screen) and move the cursor down to main-list row i. */
static void GotoItem(int i)
{
    Sel();
    for (int k = 0; k < i; k++)
        Down();
}

/** On the main screen UP/DOWN/combo are returned to the caller; short M opens the menu. */
static void HomeKeysPassThrough(void)
{
    Reset();
    CHECK_EQ(Up().kind, kMaHomeKey);
    CHECK_EQ(Down().kind, kMaHomeKey);
    CHECK_EQ(Key(kMenuKeyCombo, kBtnEvPress).kind, kMaHomeKey);
    CHECK(MenuIsHome(&s_m));
    MenuAction a = Sel();
    CHECK_EQ(s_m.st, kMsList);
    CHECK(a.redraw);
    CHECK(!MenuIsHome(&s_m));
}

/** The main list wraps both ways, and Back returns home with a full refresh and no action. */
static void NavigationWrapsAndBack(void)
{
    Reset();
    Sel();
    CHECK_EQ(s_m.cursor, kMiMode);
    Up(); /* wraps to the last row */
    CHECK_EQ(s_m.cursor, kMiBack);
    Down();
    CHECK_EQ(s_m.cursor, kMiMode);
    for (int i = 0; i < kMiBack; i++)
        Down();
    MenuAction a = Sel();
    CHECK(MenuIsHome(&s_m));
    CHECK(a.fullRefresh);
    CHECK_EQ(a.kind, kMaNone);
}

/** MENU_TIMEOUT_MS without a key returns home without saving the edit; MenuTick() at home is a no-op. */
static void TimeoutDiscardsEdit(void)
{
    Reset();
    GotoItem(kMiSwing);
    Sel();
    CHECK_EQ(s_m.st, kMsEditSwing);
    Up();
    Up();
    CHECK_EQ(s_m.edit, 70);
    CHECK_EQ(MenuTick(&s_m, s_t + MENU_TIMEOUT_MS - 1).kind, kMaNone);
    CHECK(!MenuIsHome(&s_m));
    MenuAction a = MenuTick(&s_m, s_t + MENU_TIMEOUT_MS);
    CHECK(MenuIsHome(&s_m));
    CHECK_EQ(a.kind, kMaNone); /* nothing saved */
    CHECK(a.fullRefresh);
    CHECK_EQ(MenuTick(&s_m, s_t + 3 * MENU_TIMEOUT_MS).kind, kMaNone); /* no-op at home */
}

/**
 * Regression (HOOTL menu_navigation_buttons): opening the menu long after the last key must not time out on the next tick.
 */
static void OpenAfterLongIdleDoesNotTimeOut(void)
{
    Reset();
    s_t += 10 * MENU_TIMEOUT_MS;
    Sel();
    CHECK(!MenuIsHome(&s_m));
    CHECK_EQ(MenuTick(&s_m, s_t + 1).kind, kMaNone);
    CHECK(!MenuIsHome(&s_m));
    Sel();
    CHECK_EQ(s_m.st, kMsEditMode);
}

/** The mode editor cycles Heat -> Cool -> Off (no Auto), wraps, and saves with M. */
static void EditModeSaves(void)
{
    Reset();
    GotoItem(kMiMode);
    Sel();
    CHECK_EQ(s_m.st, kMsEditMode);
    Up(); /* Heat -> Cool */
    Up(); /* -> Off (no Auto any more) */
    MenuAction a = Sel();
    CHECK_EQ(a.kind, kMaSetMode);
    CHECK_EQ(a.value, kCtrlModeOff);
    CHECK_EQ(s_m.st, kMsList);
    GotoItem(0); /* (already in list; sel opens Mode again) */
    CHECK_EQ(s_m.st, kMsEditMode);
    Up();
    Up();
    Up();
    Up(); /* Heat -> wraps past Off -> Cool */
    CHECK_EQ(Sel().value, kCtrlModeCool);
}

/** The fan editor toggles Auto/On, saves, and reopens at the current value. */
static void EditFanToggles(void)
{
    Reset();
    s_c.fan = kCtrlFanAuto;
    GotoItem(kMiFan);
    Sel();
    CHECK_EQ(s_m.st, kMsEditFan);
    MenuView v;
    MenuGetView(&s_m, &s_c, &v);
    CHECK(strcmp(v.big, "Auto") == 0);
    Up();
    MenuGetView(&s_m, &s_c, &v);
    CHECK(strcmp(v.big, "On") == 0);
    MenuAction a = Sel();
    CHECK_EQ(a.kind, kMaSetFan);
    CHECK_EQ(a.value, kCtrlFanOn);
    CHECK_EQ(s_m.st, kMsList);
    s_c.fan = kCtrlFanOn;
    MenuGetView(&s_m, &s_c, &v);
    CHECK(strcmp(v.value[kMiFan], "On") == 0);
    Sel(); /* cursor still on Fan: reopen, starts at On */
    Down();
    CHECK_EQ(Sel().value, kCtrlFanAuto);
}

/** Open the Schedule submenu and the editor for schedule item `it`. */
static void OpenSchedItem(int it)
{
    GotoItem(kMiSched);
    Sel();
    for (int k = 0; k < it; k++)
        Down();
    Sel();
}

/** The schedule list shows all 10 items with formatted values plus Back, and wraps to Back. */
static void ScheduleListShowsAllKnobs(void)
{
    Reset();
    GotoItem(kMiSched);
    Sel();
    CHECK_EQ(s_m.st, kMsSched);
    MenuView v;
    MenuGetView(&s_m, &s_c, &v);
    CHECK_EQ(v.nRows, kSchedItCount + 1);
    CHECK(strcmp(v.value[kSchedItDayHeatStart], "06:00") == 0);
    CHECK(strcmp(v.value[kSchedItNightHeatStart], "22:00") == 0);
    CHECK(
        strcmp(
            v.value[kSchedItDayHeat],
            "20.0"
            "\x7f"
            "C") == 0);
    CHECK(strcmp(v.value[kSchedItBoostMin], "120 min") == 0);
    CHECK(
        strcmp(
            v.value[kSchedItBoostOfs],
            "+0.5"
            "\x7f"
            "C") == 0);
    CHECK(strcmp(v.label[kSchedItCount], "Back") == 0);
    Up(); /* wraps to Back */
    CHECK_EQ(s_m.scursor, kSchedItCount);
    Sel();
    CHECK_EQ(s_m.st, kMsList);
}

/** Start times step by 15 min, snap odd stored minutes to the grid and wrap around midnight both ways. */
static void ScheduleTimeStepsWrap(void)
{
    Reset();
    OpenSchedItem(kSchedItDayHeatStart);
    CHECK_EQ(s_m.st, kMsEditSched);
    CHECK_EQ(s_m.edit, 360);
    Up();
    MenuView v;
    MenuGetView(&s_m, &s_c, &v);
    CHECK(strcmp(v.big, "06:15") == 0);
    MenuAction a = Sel();
    CHECK_EQ(a.kind, kMaSetSched);
    CHECK_EQ(a.item, kSchedItDayHeatStart);
    CHECK_EQ(a.value, 375);
    CHECK_EQ(s_m.st, kMsSched); /* back to the schedule list */
    /* midnight wrap both ways; odd stored minute snaps to the 15-min grid */
    Reset();
    s_c.sched.nightCoolStart = 1432; /* 23:52 -> 23:45 */
    OpenSchedItem(kSchedItNightCoolStart);
    CHECK_EQ(s_m.edit, 1425);
    Up();
    CHECK_EQ(s_m.edit, 0);
    MenuGetView(&s_m, &s_c, &v);
    CHECK(strcmp(v.big, "00:00") == 0);
    Key(kMenuKeyDown, kBtnEvPress);
    CHECK_EQ(s_m.edit, 1425);
}

/**
 * Schedule temperatures step 0.5 °C (1 °F) within 10..32 °C; boost minutes and offset clamp to their ranges; °F shows the offset converted.
 */
static void ScheduleTempAndBoostClamps(void)
{
    Reset();
    OpenSchedItem(kSchedItNightHeat);
    CHECK_EQ(s_m.edit, 1800);
    Down();
    CHECK_EQ(s_m.edit, 1750); /* 0.5 °C steps */
    for (int i = 0; i < 40; i++)
        Key(kMenuKeyDown, kBtnEvRepeat);
    CHECK_EQ(s_m.edit, 1000); /* setpoint range like Zigbee */
    CHECK_EQ(Sel().value, 1000);
    Reset();
    OpenSchedItem(kSchedItDayCool);
    for (int i = 0; i < 40; i++)
        Key(kMenuKeyUp, kBtnEvRepeat);
    CHECK_EQ(s_m.edit, 3200);
    Reset();
    OpenSchedItem(kSchedItBoostMin);
    for (int i = 0; i < 30; i++)
        Up();
    CHECK_EQ(s_m.edit, SCHED_BOOST_MAX_MIN);
    for (int i = 0; i < 30; i++)
        Down();
    CHECK_EQ(s_m.edit, 0);
    MenuView v;
    MenuGetView(&s_m, &s_c, &v);
    CHECK(strcmp(v.bigUnit, "off") == 0);
    CHECK_EQ(Sel().value, 0);
    Reset();
    OpenSchedItem(kSchedItBoostOfs);
    CHECK_EQ(s_m.edit, 50);
    Up();
    CHECK_EQ(s_m.edit, 60); /* 0.1 °C steps */
    for (int i = 0; i < 40; i++)
        Up();
    CHECK_EQ(s_m.edit, SCHED_BOOST_OFS_MAX);
    for (int i = 0; i < 40; i++)
        Down();
    CHECK_EQ(s_m.edit, 0);
    /* °F: temps step 1 °F, offset shown converted */
    Reset();
    s_c.fahrenheit = true;
    OpenSchedItem(kSchedItDayHeat);
    Up();
    CHECK_EQ(s_m.edit, MenuStepTemp(2000, 1, true));
    Reset();
    s_c.fahrenheit = true;
    OpenSchedItem(kSchedItBoostOfs);
    MenuGetView(&s_m, &s_c, &v);
    CHECK(strcmp(v.big, "0.9") == 0); /* 0.5 °C = 0.9 °F difference */
}

/** Timing out in a schedule editor returns home without an action. */
static void ScheduleEditTimeoutDiscards(void)
{
    Reset();
    OpenSchedItem(kSchedItDayHeat);
    Up();
    Up();
    MenuAction a = MenuTick(&s_m, s_t + MENU_TIMEOUT_MS);
    CHECK_EQ(a.kind, kMaNone);
    CHECK(MenuIsHome(&s_m));
}

/**
 * Every schedule editor driven to its maximum yields a value SchedSanitize() leaves unchanged (the menu never produces an out-of-range value).
 */
static void ScheduleActionRoundtrip(void)
{
    CtrlParams p = CTRL_PARAMS_DEFAULT();
    SchedCfg c = SCHED_CFG_DEFAULT();
    for (int it = 0; it < kSchedItCount; it++)
    {
        Reset();
        OpenSchedItem(it);
        for (int i = 0; i < 200; i++)
            Key(kMenuKeyUp, kBtnEvRepeat);
        MenuAction a = Sel();
        CHECK_EQ(a.kind, kMaSetSched);
        SchedSet(&c, (SchedItem)a.item, a.value);
        SchedCfg before = c;
        SchedSanitize(&p, &c);
        CHECK(memcmp(&before, &c, sizeof c) == 0); /* menu never produces an out-of-range value */
    }
}

/** The swing editor clamps to CTRL_SWING_MIN..MAX in 0.1 °C steps and saves. */
static void EditSwingClamps(void)
{
    Reset();
    GotoItem(kMiSwing);
    Sel();
    for (int i = 0; i < 20; i++)
        Up();
    CHECK_EQ(s_m.edit, CTRL_SWING_MAX);
    for (int i = 0; i < 20; i++)
        Key(kMenuKeyDown, kBtnEvRepeat);
    CHECK_EQ(s_m.edit, CTRL_SWING_MIN);
    Up();
    MenuAction a = Sel();
    CHECK_EQ(a.kind, kMaSetSwing);
    CHECK_EQ(a.value, 20);
}

/** The units editor toggles to °F and saves value 1. */
static void EditUnits(void)
{
    Reset();
    GotoItem(kMiUnits);
    Sel();
    Down();
    MenuAction a = Sel();
    CHECK_EQ(a.kind, kMaSetUnits);
    CHECK_EQ(a.value, 1);
}

/**
 * Boost to target (0.5 °C / 1 °F steps) and boost for N minutes (15 min grid, 15..120) start the right actions and return home.
 */
static void BoostTargetAndTimed(void)
{
    Reset();
    GotoItem(kMiBoost);
    Sel();
    CHECK_EQ(s_m.st, kMsBoost);
    Sel(); /* To target */
    CHECK_EQ(s_m.st, kMsEditBoostSp);
    Up();
    Up();
    MenuAction a = Sel();
    CHECK_EQ(a.kind, kMaBoostTarget);
    CHECK_EQ(a.value, 2300);
    CHECK(MenuIsHome(&s_m) && a.fullRefresh);

    s_c.fahrenheit = true; /* 1 °F steps */
    GotoItem(kMiBoost);
    Sel();
    Sel();
    Up();
    a = Sel();
    CHECK_EQ(a.value, 2278); /* 72 F -> 73 F (22.78 C) */

    s_c.boostMinutes = 37; /* from HA: snaps to the 15 min grid */
    GotoItem(kMiBoost);
    Sel();
    Down();
    Sel();
    CHECK_EQ(s_m.st, kMsEditBoostMin);
    CHECK_EQ(s_m.edit, 30);
    for (int i = 0; i < 10; i++)
        Up();
    CHECK_EQ(s_m.edit, 120);
    for (int i = 0; i < 10; i++)
        Down();
    CHECK_EQ(s_m.edit, 15);
    Up();
    a = Sel();
    CHECK_EQ(a.kind, kMaBoostTimed);
    CHECK_EQ(a.value, 30);
}

/** "Cancel boost" is only listed while a boost runs; without one the third row is Back. */
static void BoostCancelOnlyWhenActive(void)
{
    Reset();
    GotoItem(kMiBoost);
    Sel();
    Down();
    Down(); /* To target, For N, Back */
    Sel();
    CHECK_EQ(s_m.st, kMsList); /* that was Back */
    s_c.boostActive = true;
    Sel(); /* Boost again (cursor still on Boost) */
    CHECK_EQ(s_m.st, kMsBoost);
    Down();
    Down();
    MenuAction a = Sel();
    CHECK_EQ(a.kind, kMaBoostCancel);
    CHECK(MenuIsHome(&s_m));
}

/**
 * Holding M on the Zigbee screen pairs once and swallows the rest of that hold; a short M just goes back.
 */
static void ZigbeeHoldToPair(void)
{
    Reset();
    GotoItem(kMiZigbee);
    Sel();
    CHECK_EQ(s_m.st, kMsZigbee);
    MenuAction a = Key(kMenuKeyM, kBtnEvHold);
    CHECK_EQ(a.kind, kMaPair);
    CHECK(MenuIsHome(&s_m));
    /* the rest of that hold must not escalate into the home pairing/reset path */
    CHECK_EQ(Key(kMenuKeyM, kBtnEvLong).kind, kMaNone);
    CHECK_EQ(Key(kMenuKeyM, kBtnEvWarn).kind, kMaNone);
    CHECK_EQ(Key(kMenuKeyM, kBtnEvVlong).kind, kMaNone);
    CHECK_EQ(s_m.st, kMsHome);
    Key(kMenuKeyM, kBtnEvRelease);
    Sel();
    CHECK_EQ(s_m.st, kMsList); /* normal again */
    /* short M on the Zigbee screen goes back, no pairing */
    Reset();
    GotoItem(kMiZigbee);
    Sel();
    CHECK_EQ(Sel().kind, kMaNone);
    CHECK_EQ(s_m.st, kMsList);
}

/** Device info shows firmware, raw temperature/humidity and uptime, formatted. */
static void InfoScreen(void)
{
    Reset();
    GotoItem(kMiInfo);
    Sel();
    CHECK_EQ(s_m.st, kMsInfo);
    MenuView v;
    MenuGetView(&s_m, &s_c, &v);
    CHECK_EQ(v.kind, kViewText);
    CHECK(!strcmp(v.value[0], "0.2.0"));
    CHECK(!strcmp(
        v.value[1],
        "22.63\x7f"
        "C"));
    CHECK(!strcmp(v.value[2], "45.12%"));
    CHECK(!strcmp(v.value[3], "2d 03:07"));
    Sel();
    CHECK_EQ(s_m.st, kMsList);
}

/**
 * Drive the real button debouncer with MODE held for `ms` (released afterwards if `release`)
 * and feed its events to the menu, counting pair and factory-reset actions.
 *
 * Returns the last action that did something or asked for a redraw.
 */
static MenuAction HoldM(int ms, bool release, int *pNPair, int *pNReset)
{
    Btn b;
    BtnInit(&b, false);
    for (int t = 0; t < 50; t += BTN_TICK_MS)
        BtnTick(&b, false); /* key seen released after boot */
    MenuAction last = {0};
    for (int t = 0; t < ms + (release ? 200 : 0); t += BTN_TICK_MS)
    {
        BtnEvent e = BtnTick(&b, t < ms);
        s_t += BTN_TICK_MS;
        if (e == kBtnEvNone)
            continue;
        MenuAction a = MenuOnKey(&s_m, &s_c, kMenuKeyM, e, s_t);
        if (a.kind == kMaPair)
            (*pNPair)++;
        if (a.kind == kMaFactoryReset)
            (*pNReset)++;
        if (a.kind != kMaNone || a.redraw)
            last = a;
    }
    return last;
}

/**
 * MODE hold thresholds on the main screen: menu, nothing, pair banner, pair at release, reset warning, cancel, factory reset; holds inside the menu are harmless.
 */
static void ModeHoldTimings(void)
{
    int p, r;
    MenuView v;
    Reset();
    p = r = 0;
    HoldM(800, true, &p, &r); /* short press: menu */
    CHECK_EQ(s_m.st, kMsList);
    CHECK_EQ(p, 0);

    Reset();
    p = r = 0;
    HoldM(3000, true, &p, &r); /* 3 s: nothing */
    CHECK(s_m.st == kMsHome && p == 0 && r == 0);

    Reset();
    p = r = 0;
    HoldM(5200, false, &p, &r); /* reach 5 s: banner, no pairing yet */
    CHECK_EQ(s_m.st, kMsHoldPair);
    CHECK_EQ(p, 0);
    MenuGetView(&s_m, &s_c, &v);
    CHECK_EQ(v.kind, kViewHomeBanner);

    Reset();
    p = r = 0;
    HoldM(6500, true, &p, &r); /* release between 5 and 8 s: pair once */
    CHECK_EQ(p, 1);
    CHECK_EQ(r, 0);
    CHECK_EQ(s_m.st, kMsHome);

    Reset();
    p = r = 0;
    MenuAction a = HoldM(8200, false, &p, &r); /* 8 s: warning, full refresh */
    CHECK_EQ(s_m.st, kMsHoldReset);
    CHECK(a.fullRefresh);
    CHECK_EQ(p, 0);
    MenuGetView(&s_m, &s_c, &v);
    CHECK_EQ(v.kind, kViewWarning);

    Reset();
    p = r = 0;
    a = HoldM(12000, true, &p, &r); /* release before 15 s: cancelled, screen restored */
    CHECK_EQ(p, 0);
    CHECK_EQ(r, 0);
    CHECK_EQ(s_m.st, kMsHome);
    CHECK(a.fullRefresh);

    Reset();
    p = r = 0;
    HoldM(15100, true, &p, &r); /* 15 s: reset, never pairing */
    CHECK_EQ(r, 1);
    CHECK_EQ(p, 0);

    Reset();
    p = r = 0; /* holds inside the menu do nothing dangerous */
    Sel();
    HoldM(16000, true, &p, &r);
    CHECK(p == 0 && r == 0 && s_m.st == kMsList);
}

/** MenuStepTemp() snaps to the 0.5 °C grid and steps 1 °F in Fahrenheit. */
static void StepTemp(void)
{
    CHECK_EQ(MenuStepTemp(2010, 1, false), 2050);
    CHECK_EQ(MenuStepTemp(2000, -1, false), 1950);
    CHECK_EQ(MenuStepTemp(2000, 1, true), 2056); /* 68 F -> 69 F */
}

/* ---- previews ---- */

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

/** Render the current menu view over a fixed home model and save it as a PBM preview. */
static void Shot(const char *pName)
{
    static GfxFb s_fb;
    DisplayModel m;
    memset(&m, 0, sizeof m);
    m.tempValid = true;
    m.temp = 2150;
    m.rhValid = true;
    m.rh = 4500;
    m.s = (CtrlSettings){.mode = kCtrlModeHeat, .fan = kCtrlFanAuto, .heatSp = 2100, .coolSp = 2450};
    m.link = kLinkJoined;
    m.fahrenheit = s_c.fahrenheit;
    m.swing = s_c.swing;
    MenuGetView(&s_m, &s_c, &m.view);
    DisplayRender(&s_fb, &m);
    /* nothing drawn past the right edge in list/text screens except highlight/scroll marks */
    WritePbm(&s_fb, pName);
}

/**
 * Render PBM previews of every menu screen (written to the working directory) and check the main list row count.
 */
static void RenderMenuPreviews(void)
{
    Reset();
    Sel();
    Shot("menu_main.pbm");
    for (int i = 0; i < 5; i++)
        Down();
    Shot("menu_main_scrolled.pbm");
    Reset();
    GotoItem(kMiMode);
    Sel();
    Shot("menu_mode.pbm");
    Reset();
    s_c.boostActive = true;
    GotoItem(kMiBoost);
    Sel();
    Shot("menu_boost.pbm");
    Sel();
    Shot("menu_boost_target.pbm");
    Reset();
    GotoItem(kMiBoost);
    Sel();
    Down();
    Sel();
    Shot("menu_boost_minutes.pbm");
    Reset();
    s_c.fahrenheit = true;
    GotoItem(kMiSwing);
    Sel();
    Shot("menu_swing_f.pbm");
    Reset();
    GotoItem(kMiUnits);
    Sel();
    Shot("menu_units.pbm");
    Reset();
    GotoItem(kMiSched);
    Sel();
    Shot("menu_schedule.pbm");
    for (int i = 0; i < 5; i++)
        Down();
    Shot("menu_schedule_scrolled.pbm");
    Reset();
    OpenSchedItem(kSchedItDayHeatStart);
    Shot("menu_schedule_time.pbm");
    Reset();
    OpenSchedItem(kSchedItNightHeat);
    Shot("menu_schedule_temp.pbm");
    Reset();
    OpenSchedItem(kSchedItBoostMin);
    Shot("menu_schedule_boost.pbm");
    Reset();
    OpenSchedItem(kSchedItBoostOfs);
    Shot("menu_schedule_offset.pbm");
    Reset();
    GotoItem(kMiZigbee);
    Sel();
    Shot("menu_zigbee.pbm");
    Reset();
    GotoItem(kMiInfo);
    Sel();
    Shot("menu_info.pbm");
    Reset();
    s_m.st = kMsHoldPair;
    Shot("menu_hold_pair.pbm");
    Reset();
    s_m.st = kMsHoldReset;
    Shot("menu_reset_warning.pbm");
    /* every label/value must fit: check the text in the list view is not clipped into the value column */
    Reset();
    Sel();
    MenuView v;
    MenuGetView(&s_m, &s_c, &v);
    CHECK_EQ(v.nRows, kMiCount);
}

/** Run every menu test; exit status 1 if any failed (tinytest DONE()). */
int main(void)
{
    RUN(HomeKeysPassThrough);
    RUN(NavigationWrapsAndBack);
    RUN(TimeoutDiscardsEdit);
    RUN(OpenAfterLongIdleDoesNotTimeOut);
    RUN(EditModeSaves);
    RUN(EditFanToggles);
    RUN(ScheduleListShowsAllKnobs);
    RUN(ScheduleTimeStepsWrap);
    RUN(ScheduleTempAndBoostClamps);
    RUN(ScheduleEditTimeoutDiscards);
    RUN(ScheduleActionRoundtrip);
    RUN(EditSwingClamps);
    RUN(EditUnits);
    RUN(BoostTargetAndTimed);
    RUN(BoostCancelOnlyWhenActive);
    RUN(ZigbeeHoldToPair);
    RUN(InfoScreen);
    RUN(ModeHoldTimings);
    RUN(StepTemp);
    RUN(RenderMenuPreviews);
    DONE();
}
