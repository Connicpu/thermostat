/* display_model.h - everything the screen shows, and a pure renderer. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "control.h"
#include "gfx.h"

/* kLinkFault: radio/stack failed to start, or the Zigbee task stalled. Local control is unaffected. */
typedef enum { kLinkOffline = 0, kLinkPairing, kLinkJoined, kLinkLost, kLinkFault } LinkState;

/* What the on-device menu wants on screen (filled by MenuGetView(), see
 * components/ui/menu.h). kViewHome = normal screen. Plain text only, so the
 * whole model stays memcmp-able for change detection. */
typedef enum {
    kViewHome = 0,
    kViewHomeBanner,   /* normal screen, `hint` replaces the status bar (inverted) */
    kViewList,          /* title bar + scrolling rows, `sel` highlighted */
    kViewValue,         /* title bar + one big value being edited */
    kViewText,          /* title bar + read-only label/value rows */
    kViewWarning,       /* full-screen inverted warning */
} ViewKind;

#define VIEW_MAX_ROWS 12
typedef struct {
    ViewKind kind;
    char    title[20];
    char    hint[40];                     /* bottom line */
    U8 nRows, sel;
    char    label[VIEW_MAX_ROWS][28];
    char    value[VIEW_MAX_ROWS][16];
    char    big[12];                      /* kViewValue: value text */
    char    bigUnit[8];
    bool    bigIsText;                  /* letters: medium font instead of the digit-only big font */
} MenuView;

typedef struct {
    bool         tempValid;
    I16      temp;        /* c°C */
    bool         rhValid;
    U16     rh;          /* c%RH */
    CtrlSettings s;
    bool         w, y, g, waiting;
    CtrlFault fault;
    LinkState link;
    bool         fahrenheit;
    CtrlBoostKind boost;      /* kBoostNone = no boost line */
    CtrlMode  boostDir;       /* kCtrlModeHeat / kCtrlModeCool */
    I16      boostSp;        /* c°C (to-target) */
    U16     boostMinLeft;  /* minutes, rounded up (timed) */
    I16      swing;       /* c°C each side of the setpoint; 0 = not shown */
    U8      schedState; /* SchedState: 0 no clock (day), 1 day, 2 night, 3 day + morning boost */
    const char  *pBanner;
    MenuView  view;
    U8      fullEpoch;  /* bump to force a full (de-ghosting) refresh */      /* optional one-off message (e.g. "FACTORY RESET"), NULL = none */
} DisplayModel;

/* Temperature formatting helpers shared with the UI. */
int  TempToDisplay(I16 c100, bool fahrenheit, int *pTenths); /* returns integer part, tenths */
void FormatTemp(char *pOut, int n, I16 c100, bool fahrenheit, bool withTenths);

void DisplayRender(GfxFb *pFb, const DisplayModel *pM);
