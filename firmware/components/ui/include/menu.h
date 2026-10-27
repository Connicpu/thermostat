/* menu.h - on-device menu and MODE-hold state machine (pure C, host-tested).
 *
 * Main screen: UP/DOWN/UP+DOWN are passed back as kMaHomeKey (setpoint and
 * timed-boost handling stays in main.c). Short M opens the menu.
 * Holding M on the main screen:
 *    5 s  (kBtnEvLong)  -> "release to pair" banner. Releasing between 5 and
 *                           8 s starts pairing (kMaPair). Pairing is acted on
 *                           at release so going on to a reset never also
 *                           kicks off pairing.
 *    8 s  (kBtnEvWarn)  -> full-screen factory-reset warning.
 *   15 s  (kBtnEvVlong) -> kMaFactoryReset.
 *   Release between 8 and 15 s cancels and restores the screen (full refresh).
 * Menu: UP/DOWN move, M selects; editors: UP/DOWN change, M saves; 10 s with
 * no key anywhere in the menu returns to the main screen without saving.
 * Zigbee screen: hold M (1.5 s, kBtnEvHold) to pair. After any hold action
 * the rest of that M hold is swallowed until release.
 * Clear safe mode is listed only while safe mode is on, just before Back.
 * M opens a confirm screen (title and three lines, "Clear safe mode, restart?"): M again clears the boot-loop
 * counter and restarts, any other key returns to the list, and the 10 s timeout
 * leaves the menu without clearing. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "buttons.h"
#include "display_model.h"
#include "schedule.h"

#define MENU_TIMEOUT_MS       10000
#define MENU_BOOST_MIN_STEP   15
#define MENU_BOOST_MIN_MIN    15
#define MENU_BOOST_MIN_MAX    120
#define MENU_SCHED_TIME_STEP  15                      /* schedule start times: 15 min steps, wrap at midnight */
#define MENU_SCHED_BOOST_STEP 15                      /* morning boost duration: 0..240 min */
#define MENU_SCHED_OFS_STEP   10                      /* morning boost offset: 0.1 °C steps, 0..3.0 °C */
#define MENU_CALIB_STEP       10                      /* temperature offset: 0.1 °C, edited in 0.01 °C */
#define MENU_CALIB_MIN        (-25 * MENU_CALIB_STEP) /* -2.5 °C */
#define MENU_CALIB_MAX        (25 * MENU_CALIB_STEP)  /* +2.5 °C */

typedef enum
{
    kMenuKeyUp,
    kMenuKeyDown,
    kMenuKeyM,
    kMenuKeyCombo
} MenuKey;

typedef enum
{
    kMsHome = 0,
    kMsHoldPair,
    kMsHoldReset,
    kMsList,
    kMsEditMode,
    kMsEditFan,
    kMsBoost,
    kMsEditBoostSp,
    kMsEditBoostMin,
    kMsSched,
    kMsEditSched,
    kMsEditSwing,
    kMsEditOffset,
    kMsEditUnits,
    kMsZigbee,
    kMsInfo,
    kMsClearSafe,
} MenuState;

/* Main-list items (in order). */
typedef enum
{
    kMiMode,
    kMiFan,
    kMiBoost,
    kMiSched,
    kMiSwing,
    kMiOffset,
    kMiUnits,
    kMiZigbee,
    kMiInfo,
    kMiClearSafe, /* listed only while MenuCtx.safeMode is set */
    kMiBack,
    kMiCount
} MenuItem;
/* Boost-list items; kMbCancel is only present while a boost runs. */
typedef enum
{
    kMbTarget,
    kMbTimed,
    kMbCancel,
    kMbBack
} MenuBoostItem;

/**
 * Live data the menu shows and starts edits from. Filled by the app task (BuildMenuCtx() in
 * main.c) before every MenuOnKey()/MenuGetView() call; the menu never modifies it.
 */
typedef struct
{
    CtrlMode mode;
    CtrlFan fan;
    bool boostActive;
    I16 boostSp; /* c°C */
    U16 boostMinutes;
    I16 swing;      /* c°C */
    I16 calib;      /* c°C, LocalTemperatureCalibration (persisted in 0.1 °C) */
    SchedCfg sched; /* program schedule (Schedule submenu) */
    bool fahrenheit;
    LinkState link;
    const char *pFwVersion;
    bool rawValid;
    I16 rawTemp; /* c°C, uncalibrated sensor reading */
    U16 rawRh;   /* c%RH */
    U32 uptimeS;
    bool safeMode; /* boot-loop safe mode: outputs held off, banner on screen */
} MenuCtx;

typedef enum
{
    kMaNone = 0,
    kMaHomeKey,     /* main screen key: caller handles (setpoint / boost combo) */
    kMaSetMode,     /* value = CtrlMode */
    kMaSetFan,      /* value = CtrlFan (kCtrlFanOn / kCtrlFanAuto) */
    kMaBoostTarget, /* value = c°C */
    kMaBoostTimed,  /* value = minutes */
    kMaBoostCancel,
    kMaSetSwing,  /* value = c°C */
    kMaSetOffset, /* value = 0.1 °C, clamped to ±25 */
    kMaSetSched,  /* item = SchedItem, value = new raw value (already clamped) */
    kMaSetUnits,  /* value = 1 for °F */
    kMaPair,
    kMaFactoryReset,
    kMaClearSafeMode, /* clear the boot-loop counter and restart */
} MenuActionKind;

/**
 * What the caller should do after a key or tick. kind/value/item: the request (see
 * MenuActionKind). redraw: the screen changed, so post the display urgently. fullRefresh: the
 * menu or the warning was left, so force a full (de-ghosting) refresh.
 */
typedef struct
{
    MenuActionKind kind;
    I32 value;
    U8 item;          /* kMaSetSched: SchedItem */
    bool redraw;      /* screen changed: refresh promptly */
    bool fullRefresh; /* leaving the menu / warning: full refresh to clear ghosting */
} MenuAction;

/**
 * Menu state machine. Zero-initialise with MenuInit(). Owned by one task; not locked.
 * edit holds the value being edited, in the unit of the current editor (index, 0/1, 0.01 °C,
 * minutes); lastKeyMs is the time of the last key, for MENU_TIMEOUT_MS.
 */
typedef struct
{
    MenuState st;
    U8 cursor;  /* main list */
    U8 bcursor; /* boost list (index into visible items) */
    U8 scursor; /* schedule list: SchedItem, kSchedItCount = Back */
    I32 edit;   /* value being edited */
    U64 lastKeyMs;
    bool swallowM; /* ignore M events until the current hold is released */
} Menu;

/** Reset to the main screen. */
void MenuInit(Menu *pM);

/**
 * Handle one button event.
 *
 * Main screen (and the hold banners): UP/DOWN/combo are returned as kMaHomeKey for the caller.
 * M follows the hold sequence described at the top of this file. In the menu: UP/DOWN (press or
 * repeat) move or change, a short M press selects or saves, and M hold on the Zigbee screen
 * pairs. After a hold action, M events are swallowed until M is released. Every handled key
 * restarts the idle timeout.
 *
 * `pC` is the current live data (for opening editors and boost list contents); `k` is the key;
 * `ev` is the button event (buttons.h); `nowMs` is monotonic time, ms.
 *
 * Returns the action to perform (kMaNone with redraw set if only the screen changed).
 */
MenuAction MenuOnKey(Menu *pM, const MenuCtx *pC, MenuKey k, BtnEvent ev, U64 nowMs);

/**
 * Idle timeout: MENU_TIMEOUT_MS with no key anywhere in the menu returns to the main screen
 * without saving (full refresh). A key stamped later than nowMs is not idle; the caller may
 * have sampled the clock before reading the buttons. Call periodically.
 *
 * Returns the action (kMaNone; redraw/fullRefresh set when it timed out).
 */
MenuAction MenuTick(Menu *pM, U64 nowMs);

/** Returns true while the main screen is showing, including the pair/reset hold banners. */
bool MenuIsHome(const Menu *pM);

/**
 * Describe what should be on screen for the current state (pure). kViewHome on the main screen;
 * otherwise title, rows or the big value, and a hint line.
 *
 * `pV` is fully overwritten.
 */
void MenuGetView(const Menu *pM, const MenuCtx *pC, MenuView *pV);

/**
 * Step a temperature by one display unit, snapping to that unit's grid: 0.5 °C, or 1 °F
 * (converted back to the nearest 0.01 °C). dir = 0 just snaps.
 *
 * `c100` is the temperature, 0.01 °C; `dir` is +1, -1 or 0.
 *
 * Returns the new temperature, 0.01 °C (not range-clamped).
 */
I16 MenuStepTemp(I16 c100, int dir, bool fahrenheit);
