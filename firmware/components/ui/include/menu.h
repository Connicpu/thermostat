/* menu.h - on-device menu and MODE-hold state machine (pure C, host-tested).
 *
 * Main screen: UP/DOWN/UP+DOWN are passed back as MA_HOME_KEY (setpoint and
 * timed-boost handling stays in main.c). Short M opens the menu.
 * Holding M on the main screen:
 *    5 s  (BTN_EV_LONG)  -> "release to pair" banner. Releasing between 5 and
 *                           8 s starts pairing (MA_PAIR). Pairing is acted on
 *                           at release so going on to a reset never also
 *                           kicks off pairing.
 *    8 s  (BTN_EV_WARN)  -> full-screen factory-reset warning.
 *   15 s  (BTN_EV_VLONG) -> MA_FACTORY_RESET.
 *   Release between 8 and 15 s cancels and restores the screen (full refresh).
 * Menu: UP/DOWN move, M selects; editors: UP/DOWN change, M saves; 10 s with
 * no key anywhere in the menu returns to the main screen without saving.
 * Zigbee screen: hold M (1.5 s, BTN_EV_HOLD) to pair. After any hold action
 * the rest of that M hold is swallowed until release. */
#pragma once
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

typedef enum { MENU_KEY_UP, MENU_KEY_DOWN, MENU_KEY_M, MENU_KEY_COMBO } menu_key_t;

typedef enum {
    MS_HOME = 0, MS_HOLD_PAIR, MS_HOLD_RESET,
    MS_LIST, MS_EDIT_MODE, MS_EDIT_FAN, MS_BOOST, MS_EDIT_BOOST_SP, MS_EDIT_BOOST_MIN,
    MS_SCHED, MS_EDIT_SCHED, MS_EDIT_SWING, MS_EDIT_UNITS, MS_ZIGBEE, MS_INFO,
} menu_state_t;

/* Main-list items (in order). */
typedef enum { MI_MODE, MI_FAN, MI_BOOST, MI_SCHED, MI_SWING, MI_UNITS, MI_ZIGBEE, MI_INFO, MI_BACK, MI_COUNT } menu_item_t;
/* Boost-list items; MB_CANCEL is only present while a boost runs. */
typedef enum { MB_TARGET, MB_TIMED, MB_CANCEL, MB_BACK } menu_boost_item_t;

/* Live data the menu shows / starts edits from. */
typedef struct {
    ctrl_mode_t  mode;
    ctrl_fan_t   fan;
    bool         boost_active;
    int16_t      boost_sp;        /* c°C */
    uint16_t     boost_minutes;
    int16_t      swing;           /* c°C */
    sched_cfg_t  sched;           /* program schedule (Schedule submenu) */
    bool         fahrenheit;
    link_state_t link;
    const char  *fw_version;
    bool         raw_valid;
    int16_t      raw_temp;        /* c°C, uncalibrated sensor reading */
    uint16_t     raw_rh;          /* c%RH */
    uint32_t     uptime_s;
} menu_ctx_t;

typedef enum {
    MA_NONE = 0,
    MA_HOME_KEY,       /* main screen key: caller handles (setpoint / boost combo) */
    MA_SET_MODE,       /* value = ctrl_mode_t */
    MA_SET_FAN,        /* value = ctrl_fan_t (CTRL_FAN_ON / CTRL_FAN_AUTO) */
    MA_BOOST_TARGET,   /* value = c°C */
    MA_BOOST_TIMED,    /* value = minutes */
    MA_BOOST_CANCEL,
    MA_SET_SWING,      /* value = c°C */
    MA_SET_SCHED,      /* item = sched_item_t, value = new raw value (already clamped) */
    MA_SET_UNITS,      /* value = 1 for °F */
    MA_PAIR,
    MA_FACTORY_RESET,
} menu_action_kind_t;

typedef struct {
    menu_action_kind_t kind;
    int32_t value;
    uint8_t item;        /* MA_SET_SCHED: sched_item_t */
    bool redraw;         /* screen changed: refresh promptly */
    bool full_refresh;   /* leaving the menu / warning: full refresh to clear ghosting */
} menu_action_t;

typedef struct {
    menu_state_t st;
    uint8_t  cursor;          /* main list */
    uint8_t  bcursor;         /* boost list (index into visible items) */
    uint8_t  scursor;         /* schedule list: sched_item_t, SCHED_IT_COUNT = Back */
    int32_t  edit;            /* value being edited */
    uint64_t last_key_ms;
    bool     swallow_m;       /* ignore M events until the current hold is released */
} menu_t;

void          menu_init(menu_t *m);
menu_action_t menu_key(menu_t *m, const menu_ctx_t *c, menu_key_t k, btn_event_t ev, uint64_t now_ms);
menu_action_t menu_tick(menu_t *m, uint64_t now_ms);   /* timeout */
bool          menu_is_home(const menu_t *m);           /* main screen showing (incl. hold banners) */
void          menu_view(const menu_t *m, const menu_ctx_t *c, menu_view_t *v);

/* Step a temperature one display unit (0.5 °C, or 1 °F), snapping to the grid. */
int16_t menu_step_temp(int16_t c100, int dir, bool fahrenheit);
