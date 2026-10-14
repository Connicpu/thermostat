/* display_model.h - everything the screen shows, and a pure renderer. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "control.h"
#include "gfx.h"

typedef enum { LINK_OFFLINE = 0, LINK_PAIRING, LINK_JOINED, LINK_LOST } link_state_t;

/* What the on-device menu wants on screen (filled by menu_view(), see
 * components/ui/menu.h). VIEW_HOME = normal screen. Plain text only, so the
 * whole model stays memcmp-able for change detection. */
typedef enum {
    VIEW_HOME = 0,
    VIEW_HOME_BANNER,   /* normal screen, `hint` replaces the status bar (inverted) */
    VIEW_LIST,          /* title bar + scrolling rows, `sel` highlighted */
    VIEW_VALUE,         /* title bar + one big value being edited */
    VIEW_TEXT,          /* title bar + read-only label/value rows */
    VIEW_WARNING,       /* full-screen inverted warning */
} view_kind_t;

#define VIEW_MAX_ROWS 8
typedef struct {
    view_kind_t kind;
    char    title[20];
    char    hint[40];                     /* bottom line */
    uint8_t n_rows, sel;
    char    label[VIEW_MAX_ROWS][28];
    char    value[VIEW_MAX_ROWS][16];
    char    big[12];                      /* VIEW_VALUE: value text */
    char    big_unit[8];
    bool    big_is_text;                  /* letters: medium font instead of the digit-only big font */
} menu_view_t;

typedef struct {
    bool         temp_valid;
    int16_t      temp;        /* c°C */
    bool         rh_valid;
    uint16_t     rh;          /* c%RH */
    ctrl_settings_t s;
    bool         w, y, g, waiting;
    ctrl_fault_t fault;
    link_state_t link;
    bool         fahrenheit;
    ctrl_boost_kind_t boost;      /* BOOST_NONE = no boost line */
    ctrl_mode_t  boost_dir;       /* CTRL_MODE_HEAT / CTRL_MODE_COOL */
    int16_t      boost_sp;        /* c°C (to-target) */
    uint16_t     boost_min_left;  /* minutes, rounded up (timed) */
    int16_t      swing;       /* c°C each side of the setpoint; 0 = not shown */
    uint8_t      sched_state; /* sched_state_t: 0 no clock (day), 1 day, 2 night, 3 day + morning boost */
    const char  *banner;
    menu_view_t  view;
    uint8_t      full_epoch;  /* bump to force a full (de-ghosting) refresh */      /* optional one-off message (e.g. "FACTORY RESET"), NULL = none */
} display_model_t;

/* Temperature formatting helpers shared with the UI. */
int  temp_to_display(int16_t c100, bool fahrenheit, int *tenths); /* returns integer part, tenths */
void format_temp(char *out, int n, int16_t c100, bool fahrenheit, bool with_tenths);

void display_render(gfx_fb_t *fb, const display_model_t *m);
