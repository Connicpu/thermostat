/* display_model.h - everything the screen shows, and a pure renderer. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "control.h"
#include "gfx.h"

/* kLinkFault: radio/stack failed to start, or the Zigbee task stalled. Local control is unaffected. */
typedef enum
{
    kLinkOffline = 0,
    kLinkPairing,
    kLinkJoined,
    kLinkLost,
    kLinkFault
} LinkState;

/* What the on-device menu wants on screen (filled by MenuGetView(), see
 * components/ui/menu.h). kViewHome = normal screen. Plain text only, so the
 * whole model stays memcmp-able for change detection. */
typedef enum
{
    kViewHome = 0,
    kViewHomeBanner, /* normal screen, `hint` replaces the status bar (inverted) */
    kViewList,       /* title bar + scrolling rows, `sel` highlighted */
    kViewValue,      /* title bar + one big value being edited */
    kViewText,       /* title bar + read-only label/value rows */
    kViewWarning,    /* full-screen inverted warning */
} ViewKind;

#define VIEW_MAX_ROWS 12

/**
 * What the on-device menu wants on screen (filled by MenuGetView(), see components/ui/menu.h).
 * kViewHome = normal screen. Plain text only, so the whole model stays memcmp-able for change
 * detection. label/value: one row each (value right-aligned, may be empty).
 */
typedef struct
{
    ViewKind kind;
    char title[20];
    char hint[40]; /* bottom line */
    U8 nRows, sel;
    char label[VIEW_MAX_ROWS][28];
    char value[VIEW_MAX_ROWS][16];
    char big[12]; /* kViewValue: value text */
    char bigUnit[8];
    bool bigIsText; /* letters: medium font instead of the digit-only big font */
} MenuView;

/**
 * Everything the screen shows. Built by the app task, posted with DisplayPost(), rendered by
 * DisplayRender(). Must stay plain data (no pointers into mutable buffers apart from the constant
 * pBanner string) because changes are detected with memcmp(); zero-initialise before filling it.
 */
typedef struct
{
    bool tempValid;
    I16 temp; /* c°C */
    bool rhValid;
    U16 rh; /* c%RH */
    CtrlSettings s;
    bool w, y, g, waiting;
    CtrlFault fault;
    LinkState link;
    bool fahrenheit;
    CtrlBoostKind boost; /* kBoostNone = no boost line */
    CtrlMode boostDir;   /* kCtrlModeHeat / kCtrlModeCool */
    I16 boostSp;         /* c°C (to-target) */
    U16 boostMinLeft;    /* minutes, rounded up (timed) */
    I16 swing;           /* c°C each side of the setpoint; 0 = not shown */
    U8 schedState;       /* SchedState: 0 no clock (day), 1 day, 2 night, 3 day + morning boost */
    const char *pBanner; /* optional one-off message (e.g. "FACTORY RESET"), NULL = none */
    bool clockKnown;     /* Zigbee local time synced: status bar shows 24h HH:MM */
    U16 localMinute;     /* minutes after local midnight, 0..1439; ignored when !clockKnown */
    U64 minuteStartMs;   /* monotonic ms this local minute began; 0 if unknown or before boot */
    MenuView view;
    U8 fullEpoch; /* bump to force a full (de-ghosting) refresh */
} DisplayModel;

/**
 * Format 0.01 °C in the display unit into pOut (at most n bytes, NUL-terminated): "21.5" /
 * "-3.0" with tenths, or the value rounded to a whole degree without. No unit or degree sign.
 */
void FormatTemp(char *pOut, int n, I16 c100, bool fahrenheit, bool withTenths);

/**
 * Format minutes after local midnight as 24-hour "HH:MM" into pOut (at most n bytes,
 * NUL-terminated). Minute 0 is "00:00", minute 1439 is "23:59"; values wrap at 1440.
 */
void FormatHhmm(char *pOut, int n, U16 minuteOfDay);

/**
 * Render a model into a frame buffer: a menu view (list/value/text/warning) when one is open,
 * else the home screen (room temperature and humidity, swing, program period, mode, setpoint,
 * running state, and a bottom bar with the menu hint, banner, boost or Zigbee link state).
 * When clockKnown, the status bar also shows 24-hour HH:MM, right-aligned, unless that time
 * would meet the left-hand text (a long banner or fault line); the left-hand text is kept and
 * the time is dropped. Unknown clock draws no time. Pure: draws into *pFb only.
 */
void DisplayRender(GfxFb *pFb, const DisplayModel *pM);
