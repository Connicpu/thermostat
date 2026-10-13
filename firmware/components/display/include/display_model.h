/* display_model.h - everything the screen shows, and a pure renderer. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "control.h"
#include "gfx.h"

typedef enum { LINK_OFFLINE = 0, LINK_PAIRING, LINK_JOINED, LINK_LOST } link_state_t;

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
    const char  *banner;      /* optional one-off message (e.g. "FACTORY RESET"), NULL = none */
} display_model_t;

/* Temperature formatting helpers shared with the UI. */
int  temp_to_display(int16_t c100, bool fahrenheit, int *tenths); /* returns integer part, tenths */
void format_temp(char *out, int n, int16_t c100, bool fahrenheit, bool with_tenths);

void display_render(gfx_fb_t *fb, const display_model_t *m);
