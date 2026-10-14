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

/**
 * The user's day/night program and morning boost. Times are minutes after local midnight
 * (0..1439); temperatures in 0.01 °C. Persisted as part of Persist; fill with SCHED_CFG_DEFAULT()
 * and keep sane with SchedSanitize().
 */
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

/**
 * @return how a schedule item is displayed and stepped (time of day, temperature, minutes, temperature delta).
 */
SchedKind SchedItemKind(SchedItem it);

/**
 * @return the raw value of one schedule item (minutes or 0.01 °C, by SchedItemKind()); 0 for an
 * unknown item.
 */
I32      SchedGet(const SchedCfg *pC, SchedItem it);

/**
 * Store a raw value into one schedule item, saturated to the field's U16/I16 range only.
 * Call SchedSanitize() afterwards to apply the real limits. Unknown items are ignored.
 */
void         SchedSet(SchedCfg *pC, SchedItem it, I32 v);   /* raw store; call SchedSanitize after */

/**
 * Clamp everything into range: start times to 0..1439, temperatures to pP->minSp..maxSp,
 * boostMin to 0..SCHED_BOOST_MAX_MIN and boostOffset to 0..SCHED_BOOST_OFS_MAX.
 */
void SchedSanitize(const CtrlParams *pP, SchedCfg *pC);

/**
 * Which period `minute` falls in for one start-time pair. DAY is [dayStart, nightStart) and
 * either period may cross midnight; equal starts mean the program is disabled (always DAY).
 * @param minute  minutes after midnight (taken modulo 1440)
 */
SchedPeriod SchedPeriodAt(U16 dayStart, U16 nightStart, U16 minute);

/**
 * @return the effective morning boost length in minutes: boostMin, cut short at nightHeatStart;
 * 0 if boostMin or boostOffset is 0 or the heating program is disabled (equal starts).
 */
U16 SchedBoostLength(const SchedCfg *pC);

/**
 * @param minute  minutes after midnight
 * @return true if `minute` is inside the morning boost window that starts at dayHeatStart
 * (ignores the system mode and whether the time is known).
 */
bool SchedBoostWindow(const SchedCfg *pC, U16 minute);

/**
 * @return the current period for a mode: HEAT uses the heating program, every other mode the
 * cooling program; kSchedDay when the time is unknown.
 */
SchedPeriod SchedCurrent(const SchedCfg *pC, CtrlMode mode, bool timeKnown, U16 minute);

/**
 * The program setpoints for the current periods, without the morning boost: what the user sees
 * and edits as "the" setpoint. The heat setpoint follows the heating program and the cool
 * setpoint the cooling program, independently.
 * @param pHeat  out: heating setpoint, 0.01 °C
 * @param pCool  out: cooling setpoint, 0.01 °C
 */
void SchedProgramSp(const SchedCfg *pC, bool timeKnown, U16 minute, I16 *pHeat, I16 *pCool);

/**
 * Fill pS->heatSp / pS->coolSp with the setpoints the controller should use: the program
 * setpoints, plus boostOffset on the heat setpoint (clamped to maxSp) in HEAT mode inside the
 * morning window when the time is known. Only the setpoints in *pS are written.
 * @return the state to report: kSchedStateTimeUnknown, kSchedStateDayBoost, or the period of the
 * cooling program in COOL mode and of the heating program in HEAT and OFF.
 */
SchedState SchedApply(const CtrlParams *pP, const SchedCfg *pC, bool timeKnown, U16 minute,
                          CtrlSettings *pS);

/**
 * The user moved the main setpoint: store it (clamped) in the CURRENT period of the heating
 * (`heat` true) or cooling program; DAY when the time is unknown.
 * @return true if the stored value changed
 */
bool SchedEditCurrent(const CtrlParams *pP, SchedCfg *pC, bool heat, bool timeKnown, U16 minute,
                        I16 sp);

#ifdef __cplusplus
}
#endif
