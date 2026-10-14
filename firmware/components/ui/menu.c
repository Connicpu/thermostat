/* menu.c - see menu.h. Pure C, no platform dependencies. */
#include "core.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>

#define SP_MIN 1000
#define SP_MAX 3200

static const CtrlMode kModes[] = {kCtrlModeOff, kCtrlModeHeat, kCtrlModeCool};
#define N_MODES ((int)(sizeof kModes / sizeof kModes[0]))

/** Returns v clamped into lo..hi. */
static I32 Clamp32(I32 v, I32 lo, I32 hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/** Snap to the °F or 0.5 °C grid, then step. See menu.h. */
I16 MenuStepTemp(I16 sp, int dir, bool fahrenheit)
{
    if (fahrenheit)
    {
        I32 f10 = (I32)sp * 9 / 5 + 3200;                     /* c°F */
        I32 f = (f10 >= 0 ? f10 + 50 : f10 - 50) / 100 + dir; /* whole °F */
        return (I16)(((f * 100 - 3200) * 5 + (f >= 32 ? 4 : -4)) / 9);
    }
    I32 half = (sp + (sp >= 0 ? 25 : -25)) / 50 + dir;
    return (I16)(half * 50);
}

/** See menu.h. */
void MenuInit(Menu *pM)
{
    memset(pM, 0, sizeof *pM);
}

/** See menu.h. */
bool MenuIsHome(const Menu *pM)
{
    return pM->st <= kMsHoldReset;
}

/** Returns the number of rows in the boost list (Cancel is only shown while a boost runs). */
static U8 BoostCount(const MenuCtx *pC)
{
    return pC->boostActive ? 4 : 3;
}

/** Returns the boost-list item at visible row i (rows past Timed are Back when no boost runs). */
static MenuBoostItem BoostItem(const MenuCtx *pC, U8 i)
{
    if (!pC->boostActive && i >= kMbCancel)
        return kMbBack;
    return (MenuBoostItem)i;
}

/** Returns an action of kind k with value v that also asks for a redraw. */
static MenuAction Act(MenuActionKind k, I32 v)
{
    return (MenuAction){.kind = k, .value = v, .redraw = true};
}

/** Returns a redraw-only action. */
static MenuAction Redraw(void)
{
    return Act(kMaNone, 0);
}

/** Return to the main screen. Returns action k/v with redraw and a full refresh (clears menu ghosting). */
static MenuAction GoHome(Menu *pM, MenuActionKind k, I32 v)
{
    pM->st = kMsHome;
    MenuAction a = Act(k, v);
    a.fullRefresh = true;
    return a;
}

/** Returns the index of md in kModes (0 = Off if unknown). */
static int ModeIndex(CtrlMode md)
{
    for (int i = 0; i < N_MODES; i++)
        if (kModes[i] == md)
            return i;
    return 0;
}

/**
 * Open the main-list item under the cursor: switch to its screen and load the editor's start value from pC.
 */
static void OpenItem(Menu *pM, const MenuCtx *pC)
{
    switch ((MenuItem)pM->cursor)
    {
        case kMiMode:
            pM->st = kMsEditMode;
            pM->edit = ModeIndex(pC->mode);
            break;
        case kMiFan:
            pM->st = kMsEditFan;
            pM->edit = pC->fan == kCtrlFanOn;
            break;
        case kMiBoost:
            pM->st = kMsBoost;
            pM->bcursor = 0;
            break;
        case kMiSched:
            pM->st = kMsSched;
            pM->scursor = 0;
            break;
        case kMiSwing:
            pM->st = kMsEditSwing;
            pM->edit = Clamp32(pC->swing, CTRL_SWING_MIN, CTRL_SWING_MAX);
            break;
        case kMiUnits:
            pM->st = kMsEditUnits;
            pM->edit = pC->fahrenheit;
            break;
        case kMiZigbee:
            pM->st = kMsZigbee;
            break;
        case kMiInfo:
            pM->st = kMsInfo;
            break;
        default:
            break;
    }
}

/* ---------------- schedule editor ---------------- */

static const char *const kSchedLabel[kSchedItCount] = {
    "Heat day start",  "Heat day temp",  "Heat night start", "Heat night temp",  "Morning boost time",
    "Morning boost +", "Cool day start", "Cool day temp",    "Cool night start", "Cool night temp",
};
static const char *const kSchedTitle[kSchedItCount] = {
    "Heat day from",   "Heat day",      "Heat night from", "Heat night",      "Morning boost",
    "Morning boost +", "Cool day from", "Cool day",        "Cool night from", "Cool night",
};

/**
 * Returns the editor start value for a schedule item, snapped to its step: times to 15 min
 * (below midnight), boost minutes to 15, offsets to 0.1 °C, temperatures to the display grid
 * within SP_MIN..SP_MAX.
 */
static I32 SchedEditStart(const MenuCtx *pC, SchedItem it)
{
    I32 v = SchedGet(&pC->sched, it);
    switch (SchedItemKind(it))
    {
        case kSchedKindTime:
            return Clamp32((v + MENU_SCHED_TIME_STEP / 2) / MENU_SCHED_TIME_STEP * MENU_SCHED_TIME_STEP %
                               SCHED_MIN_PER_DAY,
                           0, SCHED_MIN_PER_DAY - MENU_SCHED_TIME_STEP);
        case kSchedKindMinutes:
            return Clamp32((v + MENU_SCHED_BOOST_STEP / 2) / MENU_SCHED_BOOST_STEP * MENU_SCHED_BOOST_STEP, 0,
                           SCHED_BOOST_MAX_MIN);
        case kSchedKindDelta:
            return Clamp32((v + MENU_SCHED_OFS_STEP / 2) / MENU_SCHED_OFS_STEP * MENU_SCHED_OFS_STEP, 0,
                           SCHED_BOOST_OFS_MAX);
        default:
            return Clamp32(MenuStepTemp((I16)v, 0, pC->fahrenheit), SP_MIN, SP_MAX);
    }
}

/**
 * Returns v stepped by one unit in direction dir: times wrap around midnight, minutes and
 * offsets are clamped, and a temperature that would leave SP_MIN..SP_MAX keeps its old value.
 */
static I32 SchedEditStep(const MenuCtx *pC, SchedItem it, I32 v, int dir)
{
    switch (SchedItemKind(it))
    {
        case kSchedKindTime: /* wraps around midnight */
            return (v + dir * MENU_SCHED_TIME_STEP + SCHED_MIN_PER_DAY) % SCHED_MIN_PER_DAY;
        case kSchedKindMinutes:
            return Clamp32(v + dir * MENU_SCHED_BOOST_STEP, 0, SCHED_BOOST_MAX_MIN);
        case kSchedKindDelta:
            return Clamp32(v + dir * MENU_SCHED_OFS_STEP, 0, SCHED_BOOST_OFS_MAX);
        default:
        {
            I32 n = MenuStepTemp((I16)v, dir, pC->fahrenheit);
            return (n >= SP_MIN && n <= SP_MAX) ? n : v;
        }
    }
}

/** Format minutes after midnight as "HH:MM". */
static void FmtHhmm(char *pO, int n, I32 min)
{
    snprintf(pO, n, "%02d:%02d", (int)(min / 60 % 24), (int)(min % 60));
}

static void FmtSwing(char *pO, int n, I32 sw, bool f);

/** Format a schedule item's value with its unit (time, "N min"/"off", +offset or temperature). */
static void FmtSched(char *pO, int n, SchedItem it, I32 v, bool f)
{
    const char *pU = f ? "\x7f"
                         "F"
                       : "\x7f"
                         "C";
    char b[12];
    switch (SchedItemKind(it))
    {
        case kSchedKindTime:
            FmtHhmm(pO, n, v);
            break;
        case kSchedKindMinutes:
            if (v)
                snprintf(pO, n, "%ld min", (long)v);
            else
                snprintf(pO, n, "off");
            break;
        case kSchedKindDelta:
            FmtSwing(b, sizeof b, v, f);
            snprintf(pO, n, "+%s%s", b, pU);
            break;
        default:
            FormatTemp(b, sizeof b, (I16)v, f, !f);
            snprintf(pO, n, "%s%s", b, pU);
            break;
    }
}

/**
 * Main-screen key handling: UP/DOWN/combo go back to the caller (kMaHomeKey); M opens the menu
 * (short press) or runs the hold sequence (pair banner, reset warning, factory reset).
 */
static MenuAction KeyHome(Menu *pM, MenuKey k, BtnEvent ev)
{
    if (k != kMenuKeyM)
        return (MenuAction){.kind = kMaHomeKey};
    switch (pM->st)
    {
        case kMsHome:
            if (ev == kBtnEvPress)
            {
                pM->st = kMsList;
                pM->cursor = 0;
                return Redraw();
            }
            if (ev == kBtnEvLong)
            {
                pM->st = kMsHoldPair;
                return Redraw();
            }
            break;
        case kMsHoldPair:
            if (ev == kBtnEvRelease)
            {
                pM->st = kMsHome;
                return Act(kMaPair, 0);
            }
            if (ev == kBtnEvWarn)
            {
                pM->st = kMsHoldReset;
                MenuAction a = Redraw();
                a.fullRefresh = true;
                return a;
            }
            break;
        case kMsHoldReset:
            if (ev == kBtnEvRelease)
                return GoHome(pM, kMaNone, 0); /* cancelled */
            if (ev == kBtnEvVlong)
            {
                pM->swallowM = true;
                pM->st = kMsHome;
                return Act(kMaFactoryReset, 0);
            }
            break;
        default:
            break;
    }
    return (MenuAction){0};
}

/** Swallow, stamp the idle timer, then dispatch by state. See menu.h. */
MenuAction MenuOnKey(Menu *pM, const MenuCtx *pC, MenuKey k, BtnEvent ev, U64 now)
{
    if (k == kMenuKeyM && pM->swallowM)
    {
        if (ev == kBtnEvRelease || ev == kBtnEvPress)
            pM->swallowM = false;
        return (MenuAction){0};
    }
    /* stamp first: opening the menu from the main screen must start the idle
     * timeout now, not from the last key pressed inside a menu long ago */
    pM->lastKeyMs = now;
    if (MenuIsHome(pM))
        return KeyHome(pM, k, ev);
    if (k == kMenuKeyCombo)
        return (MenuAction){0};
    const bool updown = (k == kMenuKeyUp || k == kMenuKeyDown) && (ev == kBtnEvPress || ev == kBtnEvRepeat);
    const int dir = k == kMenuKeyUp ? 1 : -1;
    const bool sel = k == kMenuKeyM && ev == kBtnEvPress;
    if (!updown && !sel && !(k == kMenuKeyM && ev == kBtnEvHold))
        return (MenuAction){0};

    switch (pM->st)
    {
        case kMsList:
            if (updown)
            {
                pM->cursor = (U8)((pM->cursor + kMiCount - dir) % kMiCount);
                return Redraw();
            } /* UP = previous row */
            if (sel)
            {
                if (pM->cursor == kMiBack)
                    return GoHome(pM, kMaNone, 0);
                OpenItem(pM, pC);
                return Redraw();
            }
            break;
        case kMsEditMode:
            if (updown)
            {
                pM->edit = (pM->edit + N_MODES + dir) % N_MODES;
                return Redraw();
            }
            if (sel)
            {
                pM->st = kMsList;
                return Act(kMaSetMode, kModes[pM->edit]);
            }
            break;
        case kMsSched:
        {
            const U8 n = kSchedItCount + 1; /* + Back */
            if (updown)
            {
                pM->scursor = (U8)((pM->scursor + n - dir) % n);
                return Redraw();
            }
            if (sel)
            {
                if (pM->scursor >= kSchedItCount)
                {
                    pM->st = kMsList;
                    return Redraw();
                }
                pM->st = kMsEditSched;
                pM->edit = SchedEditStart(pC, (SchedItem)pM->scursor);
                return Redraw();
            }
            break;
        }
        case kMsEditSched:
            if (updown)
            {
                pM->edit = SchedEditStep(pC, (SchedItem)pM->scursor, pM->edit, dir);
                return Redraw();
            }
            if (sel)
            {
                pM->st = kMsSched;
                MenuAction a = Act(kMaSetSched, pM->edit);
                a.item = pM->scursor;
                return a;
            }
            break;
        case kMsEditFan:
            if (updown)
            {
                pM->edit = !pM->edit;
                return Redraw();
            }
            if (sel)
            {
                pM->st = kMsList;
                return Act(kMaSetFan, pM->edit ? kCtrlFanOn : kCtrlFanAuto);
            }
            break;
        case kMsEditSwing:
            if (updown)
            {
                pM->edit = Clamp32(pM->edit + dir * 10, CTRL_SWING_MIN, CTRL_SWING_MAX);
                return Redraw();
            }
            if (sel)
            {
                pM->st = kMsList;
                return Act(kMaSetSwing, pM->edit);
            }
            break;
        case kMsEditUnits:
            if (updown)
            {
                pM->edit = !pM->edit;
                return Redraw();
            }
            if (sel)
            {
                pM->st = kMsList;
                return Act(kMaSetUnits, pM->edit);
            }
            break;
        case kMsBoost:
        {
            U8 n = BoostCount(pC);
            if (pM->bcursor >= n)
                pM->bcursor = (U8)(n - 1);
            if (updown)
            {
                pM->bcursor = (U8)((pM->bcursor + n - dir) % n);
                return Redraw();
            }
            if (sel)
            {
                switch (BoostItem(pC, pM->bcursor))
                {
                    case kMbTarget:
                        pM->st = kMsEditBoostSp;
                        pM->edit = Clamp32(MenuStepTemp(pC->boostSp, 0, pC->fahrenheit), SP_MIN, SP_MAX);
                        return Redraw();
                    case kMbTimed:
                    {
                        I32 v =
                            (pC->boostMinutes + MENU_BOOST_MIN_STEP / 2) / MENU_BOOST_MIN_STEP * MENU_BOOST_MIN_STEP;
                        pM->edit = Clamp32(v, MENU_BOOST_MIN_MIN, MENU_BOOST_MIN_MAX);
                        pM->st = kMsEditBoostMin;
                        return Redraw();
                    }
                    case kMbCancel:
                        return GoHome(pM, kMaBoostCancel, 0);
                    default:
                        pM->st = kMsList;
                        return Redraw();
                }
            }
            break;
        }
        case kMsEditBoostSp:
            if (updown)
            {
                I32 v = MenuStepTemp((I16)pM->edit, dir, pC->fahrenheit);
                if (v >= SP_MIN && v <= SP_MAX)
                    pM->edit = v;
                return Redraw();
            }
            if (sel)
                return GoHome(pM, kMaBoostTarget, pM->edit);
            break;
        case kMsEditBoostMin:
            if (updown)
            {
                pM->edit = Clamp32(pM->edit + dir * MENU_BOOST_MIN_STEP, MENU_BOOST_MIN_MIN, MENU_BOOST_MIN_MAX);
                return Redraw();
            }
            if (sel)
                return GoHome(pM, kMaBoostTimed, pM->edit);
            break;
        case kMsZigbee:
            if (k == kMenuKeyM && ev == kBtnEvHold)
            {
                pM->swallowM = true;
                return GoHome(pM, kMaPair, 0);
            }
            if (sel)
            {
                pM->st = kMsList;
                return Redraw();
            }
            break;
        case kMsInfo:
            if (sel)
            {
                pM->st = kMsList;
                return Redraw();
            }
            break;
        default:
            break;
    }
    return (MenuAction){0};
}

/** See menu.h. */
MenuAction MenuTick(Menu *pM, U64 now)
{
    if (!MenuIsHome(pM) && now - pM->lastKeyMs >= MENU_TIMEOUT_MS)
        return GoHome(pM, kMaNone, 0);
    return (MenuAction){0};
}

/* ---------------- view ---------------- */

/**
 * Format a temperature *difference* (swing, boost offset; 0.01 °C) in tenths of the display unit,
 * "0.5". °F = °C x 1.8 with no 32° offset.
 */
static void FmtSwing(char *pO, int n, I32 sw, bool f)
{
    int d = f ? (int)((sw * 18 + 50) / 100) : (int)((sw + 5) / 10); /* tenths of the display unit */
    snprintf(pO, n, "%d.%d", d / 10, d % 10);
}

/** Returns the mode name used in the menu ("Heat", "Cool", "Off"). */
static const char *ModeName(CtrlMode md)
{
    switch (md)
    {
        case kCtrlModeHeat:
            return "Heat";
        case kCtrlModeCool:
            return "Cool";
        default:
            return "Off";
    }
}

/** Returns the Zigbee link state as shown in the menu. */
static const char *LinkName(LinkState l)
{
    switch (l)
    {
        case kLinkJoined:
            return "connected";
        case kLinkPairing:
            return "pairing";
        case kLinkLost:
            return "lost parent";
        case kLinkFault:
            return "radio fault";
        default:
            return "not paired";
    }
}

/** Append a label/value row to the view (value may be NULL); ignored when VIEW_MAX_ROWS are used. */
static void Row(MenuView *pV, const char *pL, const char *pVal)
{
    if (pV->nRows >= VIEW_MAX_ROWS)
        return;
    snprintf(pV->label[pV->nRows], sizeof pV->label[0], "%s", pL);
    snprintf(pV->value[pV->nRows], sizeof pV->value[0], "%s", pVal ? pVal : "");
    pV->nRows++;
}

/** Fill the view for the current state. See menu.h. */
void MenuGetView(const Menu *pM, const MenuCtx *pC, MenuView *pV)
{
    memset(pV, 0, sizeof *pV);
    const bool f = pC->fahrenheit;
    const char *pU = f ? "\x7f"
                         "F"
                       : "\x7f"
                         "C";
    char b[16];
    switch (pM->st)
    {
        case kMsHome:
            pV->kind = kViewHome;
            return;
        case kMsHoldPair:
            pV->kind = kViewHomeBanner;
            snprintf(pV->hint, sizeof pV->hint, "Let go: pair  |  hold on: reset");
            return;
        case kMsHoldReset:
            pV->kind = kViewWarning;
            snprintf(pV->title, sizeof pV->title, "FACTORY RESET");
            Row(pV, "Keep holding M to reset", NULL);
            Row(pV, "Release to cancel", NULL);
            Row(pV, "Erases settings + Zigbee", NULL);
            return;
        case kMsList:
            pV->kind = kViewList;
            snprintf(pV->title, sizeof pV->title, "Menu");
            Row(pV, "Mode", ModeName(pC->mode));
            Row(pV, "Fan", pC->fan == kCtrlFanOn ? "On" : "Auto");
            Row(pV, "Boost", pC->boostActive ? "running" : "");
            Row(pV, "Schedule", "");
            FmtSwing(b, sizeof b, pC->swing, f);
            {
                char s2[32];
                snprintf(s2, sizeof s2, "+/-%s%s", b, pU);
                Row(pV, "Swing", s2);
            }
            Row(pV, "Units", pU);
            Row(pV, "Zigbee", LinkName(pC->link));
            Row(pV, "Device info", "");
            Row(pV, "Back", "");
            pV->sel = pM->cursor;
            break;
        case kMsBoost:
            pV->kind = kViewList;
            snprintf(pV->title, sizeof pV->title, "Boost");
            for (U8 i = 0; i < BoostCount(pC); i++)
            {
                switch (BoostItem(pC, i))
                {
                    case kMbTarget:
                        FormatTemp(b, sizeof b, pC->boostSp, f, !f);
                        strcat(b, pU);
                        Row(pV, "To target", b);
                        break;
                    case kMbTimed:
                        Row(pV, "For N minutes", "");
                        break;
                    case kMbCancel:
                        Row(pV, "Cancel boost", "");
                        break;
                    default:
                        Row(pV, "Back", "");
                        break;
                }
            }
            pV->sel = pM->bcursor < pV->nRows ? pM->bcursor : (U8)(pV->nRows - 1);
            break;
        case kMsEditMode:
            pV->kind = kViewValue;
            pV->bigIsText = true;
            snprintf(pV->title, sizeof pV->title, "Mode");
            snprintf(pV->big, sizeof pV->big, "%s", ModeName(kModes[pM->edit]));
            break;
        case kMsSched:
            pV->kind = kViewList;
            snprintf(pV->title, sizeof pV->title, "Schedule");
            for (int i = 0; i < kSchedItCount; i++)
            {
                char val[16];
                FmtSched(val, sizeof val, (SchedItem)i, SchedGet(&pC->sched, (SchedItem)i), f);
                Row(pV, kSchedLabel[i], val);
            }
            Row(pV, "Back", "");
            pV->sel = pM->scursor;
            break;
        case kMsEditSched:
        {
            SchedItem it = (SchedItem)pM->scursor;
            pV->kind = kViewValue;
            snprintf(pV->title, sizeof pV->title, "%s", kSchedTitle[it]);
            switch (SchedItemKind(it))
            {
                case kSchedKindTime:
                    FmtHhmm(pV->big, sizeof pV->big, pM->edit);
                    pV->bigIsText = true;
                    break; /* big font has no colon */
                case kSchedKindMinutes:
                    snprintf(pV->big, sizeof pV->big, "%ld", (long)pM->edit);
                    snprintf(pV->bigUnit, sizeof pV->bigUnit, "%s", pM->edit ? "min" : "off");
                    break;
                case kSchedKindDelta:
                    FmtSwing(pV->big, sizeof pV->big, pM->edit, f);
                    snprintf(pV->bigUnit, sizeof pV->bigUnit, "%s", pU);
                    break;
                default:
                    FormatTemp(pV->big, sizeof pV->big, (I16)pM->edit, f, !f);
                    snprintf(pV->bigUnit, sizeof pV->bigUnit, "%s", pU);
                    break;
            }
            break;
        }
        case kMsEditFan:
            pV->kind = kViewValue;
            pV->bigIsText = true;
            snprintf(pV->title, sizeof pV->title, "Fan");
            snprintf(pV->big, sizeof pV->big, "%s", pM->edit ? "On" : "Auto");
            break;
        case kMsEditSwing:
            pV->kind = kViewValue;
            snprintf(pV->title, sizeof pV->title, "Swing (+/-)");
            FmtSwing(pV->big, sizeof pV->big, pM->edit, f);
            snprintf(pV->bigUnit, sizeof pV->bigUnit, "%s", pU);
            break;
        case kMsEditUnits:
            pV->kind = kViewValue;
            snprintf(pV->title, sizeof pV->title, "Units");
            snprintf(pV->big, sizeof pV->big, "%s",
                     pM->edit ? "\x7f"
                                "F"
                              : "\x7f"
                                "C");
            break;
        case kMsEditBoostSp:
            pV->kind = kViewValue;
            snprintf(pV->title, sizeof pV->title, "Boost to");
            FormatTemp(pV->big, sizeof pV->big, (I16)pM->edit, f, !f);
            snprintf(pV->bigUnit, sizeof pV->bigUnit, "%s", pU);
            snprintf(pV->hint, sizeof pV->hint, "Up/Down: change   M: start");
            return;
        case kMsEditBoostMin:
            pV->kind = kViewValue;
            snprintf(pV->title, sizeof pV->title, "Boost for");
            snprintf(pV->big, sizeof pV->big, "%ld", (long)pM->edit);
            snprintf(pV->bigUnit, sizeof pV->bigUnit, "min");
            snprintf(pV->hint, sizeof pV->hint, "Up/Down: change   M: start");
            return;
        case kMsZigbee:
            pV->kind = kViewText;
            snprintf(pV->title, sizeof pV->title, "Zigbee");
            Row(pV, "Status", LinkName(pC->link));
            Row(pV, "Hold M: pair", "");
            snprintf(pV->hint, sizeof pV->hint, "M: back");
            return;
        case kMsInfo:
        {
            pV->kind = kViewText;
            snprintf(pV->title, sizeof pV->title, "Device info");
            Row(pV, "Firmware", pC->pFwVersion);
            if (pC->rawValid)
            {
                int t = pC->rawTemp, a = t < 0 ? -t : t;
                snprintf(b, sizeof b,
                         "%s%d.%02d"
                         "\x7f"
                         "C",
                         t < 0 ? "-" : "", a / 100, a % 100);
                Row(pV, "Raw temp", b);
                snprintf(b, sizeof b, "%u.%02u%%", (unsigned)(pC->rawRh / 100), (unsigned)(pC->rawRh % 100));
                Row(pV, "Raw RH", b);
            }
            else
            {
                Row(pV, "Raw temp", "--");
                Row(pV, "Raw RH", "--");
            }
            U32 s = pC->uptimeS;
            snprintf(b, sizeof b, "%lud %02lu:%02lu", (unsigned long)(s / 86400), (unsigned long)(s / 3600 % 24),
                     (unsigned long)(s / 60 % 60));
            Row(pV, "Uptime", b);
            snprintf(pV->hint, sizeof pV->hint, "M: back");
            return;
        }
    }
    if (!pV->hint[0])
        snprintf(pV->hint, sizeof pV->hint,
                 pV->kind == kViewList ? "Up/Down: move   M: select" : "Up/Down: change   M: save");
}
