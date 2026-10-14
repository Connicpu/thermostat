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
 * Morning boost (heat mode only): for `boostMin` minutes starting at
 * dayHeatStart the heating setpoint is raised by `boostOffset`. The window is
 * cut short at nightHeatStart (it never extends into the night period) and
 * never runs when day and night heating start are equal (program disabled).
 * boostMin = 0 or boostOffset = 0 disables it.
 *
 * Time unknown (no Zigbee Time sync yet since boot): the DAY setpoints are
 * used, no morning boost, and setpoint edits go to the DAY period.
 *
 * Times are minutes after local midnight (0..1439); temperatures c°C.
 */
#pragma once
#include "core.h"
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
    U16 dayCoolStart, nightCoolStart;     /* minutes after midnight */
    I16  dayCool, nightCool;                 /* c°C */
    U16 dayHeatStart, nightHeatStart;
    I16  dayHeat, nightHeat;
    U16 boostMin;                            /* morning heat boost length, minutes */
    I16  boostOffset;                         /* c°C added to the heat setpoint */
} SchedCfg;

#define SCHED_CFG_DEFAULT() {                                  \
    .dayCoolStart = 8 * 60,   .nightCoolStart = 22 * 60,   \
    .dayCool = 2450,           .nightCool = 2400,            \
    .dayHeatStart = 6 * 60,   .nightHeatStart = 22 * 60,   \
    .dayHeat = 2000,           .nightHeat = 1800,            \
    .boostMin = 120,           .boostOffset = 50,            \
}

typedef enum { kSchedDay = 0, kSchedNight = 1 } SchedPeriod;

/* Reported state (manufacturer attribute kZbAttrSchedState). */
typedef enum {
    kSchedStateTimeUnknown = 0,   /* holding the DAY setpoints */
    kSchedStateDay = 1,
    kSchedStateNight = 2,
    kSchedStateDayBoost = 3,      /* day + morning heat boost active */
} SchedState;

/* The 10 user knobs, in on-device menu order. */
typedef enum {
    kSchedItDayHeatStart = 0, kSchedItDayHeat, kSchedItNightHeatStart, kSchedItNightHeat,
    kSchedItBoostMin, kSchedItBoostOfs,
    kSchedItDayCoolStart, kSchedItDayCool, kSchedItNightCoolStart, kSchedItNightCool,
    kSchedItCount,
} SchedItem;
typedef enum { kSchedKindTime, kSchedKindTemp, kSchedKindMinutes, kSchedKindDelta } SchedKind;
SchedKind SchedItemKind(SchedItem it);
I32      SchedGet(const SchedCfg *pC, SchedItem it);
void         SchedSet(SchedCfg *pC, SchedItem it, I32 v);   /* raw store; call SchedSanitize after */

/* Clamp everything into range (times 0..1439, temps minSp..maxSp, boost limits). */
void SchedSanitize(const CtrlParams *pP, SchedCfg *pC);

/* Which period `minute` falls in for a day_start/night_start pair. */
SchedPeriod SchedPeriodAt(U16 dayStart, U16 nightStart, U16 minute);

/* Effective morning boost length in minutes (0 = off), after the clamps above. */
U16 SchedBoostLength(const SchedCfg *pC);
/* Morning boost window active at `minute` (ignores mode). */
bool SchedBoostWindow(const SchedCfg *pC, U16 minute);

/* Current period for a mode (HEAT uses the heating program, everything else the
 * cooling program). Unknown time -> kSchedDay. */
SchedPeriod SchedCurrent(const SchedCfg *pC, CtrlMode mode, bool timeKnown, U16 minute);

/* The program setpoints for the current periods (no morning boost). This is
 * what the user sees and edits as "the" setpoint. */
void SchedProgramSp(const SchedCfg *pC, bool timeKnown, U16 minute, I16 *pHeat, I16 *pCool);

/* Fill s->heatSp / s->coolSp with the setpoints the controller should use:
 * the program setpoints plus the morning boost (HEAT mode, time known, inside
 * the window; clamped to maxSp). Returns the state to report: the period of the
 * cooling program in COOL mode, of the heating program in HEAT and OFF. */
SchedState SchedApply(const CtrlParams *pP, const SchedCfg *pC, bool timeKnown, U16 minute,
                          CtrlSettings *pS);

/* The user moved the main setpoint: store it in the CURRENT period of the
 * heating (`heat` true) or cooling program. Clamped. Returns true if changed. */
bool SchedEditCurrent(const CtrlParams *pP, SchedCfg *pC, bool heat, bool timeKnown, U16 minute,
                        I16 sp);

#ifdef __cplusplus
}
#endif
