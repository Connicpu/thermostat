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
 * the rest of that M hold is swallowed until release. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "buttons.h"
#include "display_model.h"
#include "schedule.h"

#define MENU_TIMEOUT_MS   10000
#define MENU_BOOST_MIN_STEP 15
#define MENU_BOOST_MIN_MIN  15
#define MENU_BOOST_MIN_MAX  120
#define MENU_SCHED_TIME_STEP 15     /* schedule start times: 15 min steps, wrap at midnight */
#define MENU_SCHED_BOOST_STEP 15    /* morning boost duration: 0..240 min */
#define MENU_SCHED_OFS_STEP  10     /* morning boost offset: 0.1 °C steps, 0..3.0 °C */

typedef enum { kMenuKeyUp, kMenuKeyDown, kMenuKeyM, kMenuKeyCombo } MenuKey;

typedef enum {
    kMsHome = 0, kMsHoldPair, kMsHoldReset,
    kMsList, kMsEditMode, kMsEditFan, kMsBoost, kMsEditBoostSp, kMsEditBoostMin,
    kMsSched, kMsEditSched, kMsEditSwing, kMsEditUnits, kMsZigbee, kMsInfo,
} MenuState;

/* Main-list items (in order). */
typedef enum { kMiMode, kMiFan, kMiBoost, kMiSched, kMiSwing, kMiUnits, kMiZigbee, kMiInfo, kMiBack, kMiCount } MenuItem;
/* Boost-list items; kMbCancel is only present while a boost runs. */
typedef enum { kMbTarget, kMbTimed, kMbCancel, kMbBack } MenuBoostItem;

/* Live data the menu shows / starts edits from. */
typedef struct {
    CtrlMode  mode;
    CtrlFan   fan;
    bool         boostActive;
    I16      boostSp;        /* c°C */
    U16     boostMinutes;
    I16      swing;           /* c°C */
    SchedCfg  sched;           /* program schedule (Schedule submenu) */
    bool         fahrenheit;
    LinkState link;
    const char  *pFwVersion;
    bool         rawValid;
    I16      rawTemp;        /* c°C, uncalibrated sensor reading */
    U16     rawRh;          /* c%RH */
    U32     uptimeS;
} MenuCtx;

typedef enum {
    kMaNone = 0,
    kMaHomeKey,       /* main screen key: caller handles (setpoint / boost combo) */
    kMaSetMode,       /* value = CtrlMode */
    kMaSetFan,        /* value = CtrlFan (kCtrlFanOn / kCtrlFanAuto) */
    kMaBoostTarget,   /* value = c°C */
    kMaBoostTimed,    /* value = minutes */
    kMaBoostCancel,
    kMaSetSwing,      /* value = c°C */
    kMaSetSched,      /* item = SchedItem, value = new raw value (already clamped) */
    kMaSetUnits,      /* value = 1 for °F */
    kMaPair,
    kMaFactoryReset,
} MenuActionKind;

typedef struct {
    MenuActionKind kind;
    I32 value;
    U8 item;        /* kMaSetSched: SchedItem */
    bool redraw;         /* screen changed: refresh promptly */
    bool fullRefresh;   /* leaving the menu / warning: full refresh to clear ghosting */
} MenuAction;

typedef struct {
    MenuState st;
    U8  cursor;          /* main list */
    U8  bcursor;         /* boost list (index into visible items) */
    U8  scursor;         /* schedule list: SchedItem, kSchedItCount = Back */
    I32  edit;            /* value being edited */
    U64 lastKeyMs;
    bool     swallowM;       /* ignore M events until the current hold is released */
} Menu;

void          MenuInit(Menu *pM);
MenuAction MenuOnKey(Menu *pM, const MenuCtx *pC, MenuKey k, BtnEvent ev, U64 nowMs);
MenuAction MenuTick(Menu *pM, U64 nowMs);   /* timeout */
bool          MenuIsHome(const Menu *pM);           /* main screen showing (incl. hold banners) */
void          MenuGetView(const Menu *pM, const MenuCtx *pC, MenuView *pV);

/* Step a temperature one display unit (0.5 °C, or 1 °F), snapping to the grid. */
I16 MenuStepTemp(I16 c100, int dir, bool fahrenheit);
