/*
 * schedule.h - day/night program schedule + morning heating boost.
 *
 * Pure C, no platform dependencies (host-tested in test/host/test_schedule.c).
 *
 * Heating and cooling each have their own two-period program:
 *   DAY   runs from  day_*_start  up to (not including) night_*_start
 *   NIGHT runs from night_*_start up to (not including)   day_*_start
 * Either period may cross midnight. If both start times are equal the
 * schedule is effectively disabled and DAY applies all the time.
 *
 * Morning boost (heat mode only): for `boost_min` minutes starting at
 * day_heat_start the heating setpoint is raised by `boost_offset`. The window is
 * cut short at night_heat_start (it never extends into the night period) and
 * never runs when day and night heating start are equal (program disabled).
 * boost_min = 0 or boost_offset = 0 disables it.
 *
 * Time unknown (no Zigbee Time sync yet since boot): the DAY setpoints are
 * used, no morning boost, and setpoint edits go to the DAY period.
 *
 * Times are minutes after local midnight (0..1439); temperatures c°C.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "control.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCHED_MIN_PER_DAY      1440
#define SCHED_BOOST_MAX_MIN    240      /* morning boost duration 0..240 min */
#define SCHED_BOOST_OFS_MAX    300      /* morning boost offset 0..3.0 °C */

typedef struct {
    uint16_t day_cool_start, night_cool_start;     /* minutes after midnight */
    int16_t  day_cool, night_cool;                 /* c°C */
    uint16_t day_heat_start, night_heat_start;
    int16_t  day_heat, night_heat;
    uint16_t boost_min;                            /* morning heat boost length, minutes */
    int16_t  boost_offset;                         /* c°C added to the heat setpoint */
} sched_cfg_t;

#define SCHED_CFG_DEFAULT() {                                  \
    .day_cool_start = 8 * 60,   .night_cool_start = 22 * 60,   \
    .day_cool = 2450,           .night_cool = 2400,            \
    .day_heat_start = 6 * 60,   .night_heat_start = 22 * 60,   \
    .day_heat = 2000,           .night_heat = 1800,            \
    .boost_min = 120,           .boost_offset = 50,            \
}

typedef enum { SCHED_DAY = 0, SCHED_NIGHT = 1 } sched_period_t;

/* Reported state (manufacturer attribute ZB_ATTR_SCHED_STATE). */
typedef enum {
    SCHED_STATE_TIME_UNKNOWN = 0,   /* holding the DAY setpoints */
    SCHED_STATE_DAY = 1,
    SCHED_STATE_NIGHT = 2,
    SCHED_STATE_DAY_BOOST = 3,      /* day + morning heat boost active */
} sched_state_t;

/* The 10 user knobs, in on-device menu order. */
typedef enum {
    SCHED_IT_DAY_HEAT_START = 0, SCHED_IT_DAY_HEAT, SCHED_IT_NIGHT_HEAT_START, SCHED_IT_NIGHT_HEAT,
    SCHED_IT_BOOST_MIN, SCHED_IT_BOOST_OFS,
    SCHED_IT_DAY_COOL_START, SCHED_IT_DAY_COOL, SCHED_IT_NIGHT_COOL_START, SCHED_IT_NIGHT_COOL,
    SCHED_IT_COUNT,
} sched_item_t;
typedef enum { SCHED_KIND_TIME, SCHED_KIND_TEMP, SCHED_KIND_MINUTES, SCHED_KIND_DELTA } sched_kind_t;
sched_kind_t sched_item_kind(sched_item_t it);
int32_t      sched_get(const sched_cfg_t *c, sched_item_t it);
void         sched_set(sched_cfg_t *c, sched_item_t it, int32_t v);   /* raw store; call sched_sanitize after */

/* Clamp everything into range (times 0..1439, temps min_sp..max_sp, boost limits). */
void sched_sanitize(const ctrl_params_t *p, sched_cfg_t *c);

/* Which period `minute` falls in for a day_start/night_start pair. */
sched_period_t sched_period(uint16_t day_start, uint16_t night_start, uint16_t minute);

/* Effective morning boost length in minutes (0 = off), after the clamps above. */
uint16_t sched_boost_length(const sched_cfg_t *c);
/* Morning boost window active at `minute` (ignores mode). */
bool sched_boost_window(const sched_cfg_t *c, uint16_t minute);

/* Current period for a mode (HEAT uses the heating program, everything else the
 * cooling program). Unknown time -> SCHED_DAY. */
sched_period_t sched_current(const sched_cfg_t *c, ctrl_mode_t mode, bool time_known, uint16_t minute);

/* The program setpoints for the current periods (no morning boost). This is
 * what the user sees and edits as "the" setpoint. */
void sched_program_sp(const sched_cfg_t *c, bool time_known, uint16_t minute, int16_t *heat, int16_t *cool);

/* Fill s->heat_sp / s->cool_sp with the setpoints the controller should use:
 * the program setpoints plus the morning boost (HEAT mode, time known, inside
 * the window; clamped to max_sp). Returns the state to report: the period of the
 * cooling program in COOL mode, of the heating program in HEAT and OFF. */
sched_state_t sched_apply(const ctrl_params_t *p, const sched_cfg_t *c, bool time_known, uint16_t minute,
                          ctrl_settings_t *s);

/* The user moved the main setpoint: store it in the CURRENT period of the
 * heating (`heat` true) or cooling program. Clamped. Returns true if changed. */
bool sched_edit_current(const ctrl_params_t *p, sched_cfg_t *c, bool heat, bool time_known, uint16_t minute,
                        int16_t sp);

#ifdef __cplusplus
}
#endif
