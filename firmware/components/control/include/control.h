/*
 * control.h - hardware-independent thermostat control logic.
 *
 * No ESP-IDF / FreeRTOS dependencies: everything is driven by explicit
 * inputs and a monotonic millisecond clock, so it compiles and is unit
 * tested natively on the host (see test/host).
 *
 * Units: temperatures in centi-degrees Celsius (int16, same as ZCL),
 *        time in milliseconds since boot (uint64, never wraps in practice).
 */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Values intentionally equal the ZCL Thermostat SystemMode enum (0x001C).
 * There is deliberately no AUTO (heat/cool changeover) mode: the day/night
 * program schedule (schedule.h) replaced it. */
    typedef enum
    {
        kCtrlModeOff = 0x00,
        kCtrlModeCool = 0x03,
        kCtrlModeHeat = 0x04,
    } CtrlMode;

    /* Values intentionally equal the ZCL Fan Control FanMode enum (0x0000). */
    typedef enum
    {
        kCtrlFanOn = 0x04,
        kCtrlFanAuto = 0x05,
    } CtrlFan;

    /**
 * The user's normal (non-boost) operating settings, as set from Zigbee, the menu or the schedule.
 * Setpoints are in 0.01 °C and independent of each other (heat and cool never run in the same mode).
 */
    typedef struct
    {
        CtrlMode mode;
        CtrlFan fan;
        I16 heatSp; /* occupied heating setpoint, c°C */
        I16 coolSp; /* occupied cooling setpoint, c°C */
    } CtrlSettings;

    /**
 * Tuning and safety limits for the control loop. Times in ms, temperatures in 0.01 °C.
 * Fill with CTRL_PARAMS_DEFAULT(). Fixed at CtrlInit() except hysteresis (CtrlSetSwing()).
 */
    typedef struct
    {
        I16 hysteresis;       /* swing, c°C each side of the setpoint (user setting,
                                   * CTRL_SWING_MIN..MAX, default 0.5 °C)       */
        I16 minSp, maxSp;     /* allowed setpoint range                    */
        I16 tempMin, tempMax; /* plausible room temp; outside -> failsafe  */
        U32 sensorTimeoutMs;  /* no valid reading for this long -> failsafe*/
        U8 recoverSamples;    /* good samples needed to leave failsafe     */
        U32 compMinOffMs;     /* compressor (Y) anti-short-cycle            */
        U32 compMinOnMs;
        U32 heatMinOffMs;
        U32 heatMinOnMs;
        U32 changeoverMs;   /* min gap between heat stop and cool start (and vice versa) */
        U32 fanRunonHeatMs; /* G stays on after W drops                  */
        U32 fanRunonCoolMs; /* G stays on after Y drops                  */
        bool heatDrivesFan; /* energise G with W (electric/hydro-air); false for gas furnace */
    } CtrlParams;

/* Temperature swing (hysteresis), c°C each side of the setpoint. Heat turns on
 * at SP-swing and off at SP+swing; cooling is the mirror image. */
#define CTRL_SWING_MIN     10  /* 0.1 °C */
#define CTRL_SWING_MAX     100 /* 1.0 °C */
#define CTRL_SWING_DEFAULT 50  /* 0.5 °C */

/* Sensible defaults for a single-stage gas furnace + AC. */
#define CTRL_PARAMS_DEFAULT()             \
    {                                     \
        .hysteresis = CTRL_SWING_DEFAULT, \
        .minSp = 1000,                    \
        .maxSp = 3200,                    \
        .tempMin = 0,                     \
        .tempMax = 4500,                  \
        .sensorTimeoutMs = 30000,         \
        .recoverSamples = 3,              \
        .compMinOffMs = 5u * 60u * 1000u, \
        .compMinOnMs = 3u * 60u * 1000u,  \
        .heatMinOffMs = 60u * 1000u,      \
        .heatMinOnMs = 2u * 60u * 1000u,  \
        .changeoverMs = 5u * 60u * 1000u, \
        .fanRunonHeatMs = 90u * 1000u,    \
        .fanRunonCoolMs = 45u * 1000u,    \
        .heatDrivesFan = false,           \
    }

    typedef enum
    {
        kCtrlFaultNone = 0,
        kCtrlFaultSensorTimeout, /* no valid reading recently        */
        kCtrlFaultTempRange,     /* reading outside tempMin..tempMax */
    } CtrlFault;

    /** One sensor sample fed to CtrlFeedSensor(). */
    typedef struct
    {
        bool valid; /* reading succeeded (CRC ok etc.) */
        I16 temp;   /* c°C */
    } CtrlInput;

    /**
 * Result of one CtrlStep(). w/y/g are the *requested* outputs, which still have to pass
 * HvacInterlock() before they reach the relays.
 */
    typedef struct
    {
        bool w, y, g;                /* requested outputs (still pass through the interlock!) */
        CtrlFault fault;             /* != NONE -> failsafe active, all outputs off */
        bool heatDemand, coolDemand; /* thermostat wants heat/cool (may be held off by timers) */
        bool waiting;                /* demand present but blocked by min-off / changeover */
    } CtrlOutput;

    /* ---------------- boost (runtime only, never persisted) ---------------- */
    typedef enum
    {
        kBoostNone = 0,
        kBoostToTarget = 1,
        kBoostTimed = 2
    } CtrlBoostKind;
    /* Requested direction. Values match the Zigbee boost_mode attribute. */
    typedef enum
    {
        kBoostDirAuto = 0,
        kBoostDirHeat = 1,
        kBoostDirCool = 2
    } CtrlBoostDir;
    typedef enum
    {
        kBoostEndNone = 0,
        kBoostEndReached,
        kBoostEndExpired,
        kBoostEndCancelled,
        kBoostEndModeChanged,
        kBoostEndFault,
        kBoostEndLimit,
    } CtrlBoostEnd;
    typedef enum
    {
        kBoostOk = 0,
        kBoostErrArg,
        kBoostErrFault,
        kBoostErrAtTarget,
        kBoostErrNoDirection,
    } CtrlBoostResult;

#define BOOST_TIMED_MIN_MIN  1
#define BOOST_TIMED_MAX_MIN  120 /* timed boost: 1..120 min */
#define BOOST_TARGET_MAX_MIN 180 /* boost-to-target gives up after 3 h */

    /**
 * An active boost: a temporary override of what the thermostat asks for (runtime only, never persisted).
 * kind == kBoostNone means no boost is running; lastEnd then tells why the last one ended.
 */
    typedef struct
    {
        CtrlBoostKind kind;
        CtrlMode dir;      /* resolved: kCtrlModeHeat or kCtrlModeCool */
        I16 sp;            /* target (kBoostToTarget) */
        U64 endAt;         /* hard deadline */
        CtrlMode baseMode; /* system mode when started; changing it cancels */
        CtrlBoostEnd lastEnd;
    } CtrlBoost;

    /**
 * Snapshot of the boost for reporting (Zigbee attributes, display). Built by CtrlGetBoostStatus().
 * remainingS: seconds until the hard deadline, rounded up; 0 when no boost is running.
 */
    typedef struct
    {
        CtrlBoostKind kind; /* kBoostNone if inactive */
        CtrlMode dir;
        I16 sp;
        U32 remainingS;
        CtrlBoostEnd lastEnd;
    } CtrlBoostStatus;

    /**
 * Complete control-loop state. Owned by one task; none of the Ctrl* functions lock.
 * Initialise with CtrlInit(). Timestamps (*At, *Until) are monotonic ms.
 */
    typedef struct
    {
        CtrlParams p;
        CtrlBoost boost;
        bool softStop; /* boost just ended: running stage still honours its min on-time */
        bool w, y, g;
        bool heatCall, coolCall; /* latched hysteresis state */
        U64 wOnAt, wOffAt, yOnAt, yOffAt;
        bool wRan, yRan; /* changeover only applies after a real run */
        U64 fanRunonUntil;
        U64 lastValidAt;
        I16 lastTemp;
        bool haveTemp;
        CtrlFault fault;
        U8 goodStreak;
    } Ctrl;

    /**
 * Reset the controller.
 *
 * Every output starts off, with wOffAt/yOffAt = nowMs, so after a power blip the compressor and
 * the heat still get their full minimum off-time. The controller starts in failsafe
 * (kCtrlFaultSensorTimeout) until pP->recoverSamples good readings have arrived. The swing is
 * clamped into CTRL_SWING_MIN..MAX.
 *
 * `pC` is the state to initialise (fully overwritten); `pP` is the parameters, copied; `nowMs` is
 * monotonic time, ms.
 */
    void CtrlInit(Ctrl *pC, const CtrlParams *pP, U64 nowMs);

    /**
 * Make settings safe to run: an unknown mode becomes kCtrlModeOff, an unknown fan mode becomes
 * kCtrlFanAuto, and both setpoints are clamped into pP->minSp..maxSp. There is no heat/cool
 * deadband, because heating and cooling never run in the same mode.
 *
 * `pP` is the limits; `pS` is the settings to fix in place; `keepHeat` is ignored (kept for API
 * compatibility).
 */
    void CtrlSanitizeSettings(const CtrlParams *pP, CtrlSettings *pS, bool keepHeat);

    /**
 * Feed one sensor read attempt, valid or not. Call on every attempt.
 *
 * An invalid reading only resets the recovery streak. A dead sensor is detected by the timeout
 * in CtrlStep(). A reading outside tempMin..tempMax raises kCtrlFaultTempRange. A plausible
 * reading refreshes the timeout, and while in failsafe it counts towards the recoverSamples
 * consecutive good readings needed to clear the fault.
 *
 * `pC` is the controller; `pIn` is the sample (temp in 0.01 °C); `nowMs` is monotonic time, ms.
 */
    void CtrlFeedSensor(Ctrl *pC, const CtrlInput *pIn, U64 nowMs);

    /**
 * Run one control step. Call at least once a second.
 *
 * In order: - failsafe: no valid reading for sensorTimeoutMs, or any fault, turns W, Y and G
 * off immediately (no fan run-on), drops the latched calls and ends a running boost
 * (kBoostEndFault); - boost: checks the end conditions (mode changed, deadline, target reached,
 * timed-boost comfort cap at maxSp/minSp), otherwise forces the boost's direction; -
 * hysteresis: latched heat/cool calls with +/- p.hysteresis around the setpoint; - W and Y:
 * minimum on/off times and the changeover gap (only after that stage actually ran). After a
 * boost ends or is displaced (softStop), the running stage still finishes its minimum on-time;
 * - G: on when the fan mode is On, with Y, with W if heatDrivesFan, and during the run-on after
 * W/Y.
 *
 * `pC` is the controller; `pS` is the current settings (sanitised copy is used; *pS is not modified);
 * `nowMs` is monotonic time, ms.
 *
 * Returns requested outputs plus demand/waiting/fault flags.
 */
    CtrlOutput CtrlStep(Ctrl *pC, const CtrlSettings *pS, U64 nowMs);

    /** Returns the next system mode for the mode button: Off -> Heat -> Cool -> Off (unknown -> Off). */
    CtrlMode CtrlNextMode(CtrlMode m);

    /** Returns true if v is one of the supported CtrlMode values (Off, Cool, Heat). */
    bool CtrlModeValid(int v);

    /** Returns a short upper-case display string for a fault ("OK", "SENSOR FAIL", "TEMP RANGE", "?"). */
    const char *CtrlFaultStr(CtrlFault f);

    /**
 * Start a boost. A running boost is replaced; a change of direction drops the old latched call.
 *
 * kBoostToTarget: sp is clamped to minSp..maxSp. kBoostDirAuto heats if the room is at least
 * one swing below sp and cools if at least one swing above, otherwise the target is already
 * reached. The boost ends when the room reaches sp, after BOOST_TARGET_MAX_MIN, or when the
 * system mode changes. kBoostTimed: runs `minutes` (BOOST_TIMED_MIN_MIN..MAX_MIN).
 * kBoostDirAuto follows the system mode, and in Off it follows the room relative to the
 * heat/cool setpoints. It stops early at maxSp (heat) / minSp (cool). All safety rules
 * (failsafe, min times, changeover, interlock) still apply: a boost only changes what the
 * thermostat asks for.
 *
 * `pC` is the controller; `pS` is the current normal settings (direction for Auto, base mode for change
 * detection); `kind` is kBoostToTarget or kBoostTimed; `dir` is kBoostDirAuto/Heat/Cool; `sp`
 * is the target, 0.01 °C (kBoostToTarget only); `minutes` is the duration (kBoostTimed only); `nowMs`
 * is monotonic time, ms.
 *
 * Returns kBoostOk, kBoostErrFault (failsafe or no reading yet), kBoostErrArg,
 * kBoostErrAtTarget or kBoostErrNoDirection.
 */
    CtrlBoostResult CtrlBoostStart(Ctrl *pC, const CtrlSettings *pS, CtrlBoostKind kind, CtrlBoostDir dir, I16 sp,
                                   U16 minutes, U64 nowMs);

    /**
 * Cancel a running boost (no-op if none): drops its latched call and records kBoostEndCancelled.
 * The running stage still finishes its minimum on-time.
 */
    void CtrlBoostCancel(Ctrl *pC);

    /** Returns swing (0.01 °C) clamped into CTRL_SWING_MIN..CTRL_SWING_MAX. */
    I16 CtrlClampSwing(I32 swing);

    /**
 * Change the swing at runtime (clamped). Latched heat/cool calls are kept, so a running stage
 * simply finishes against the new off threshold.
 *
 * `swing` is the band each side, in 0.01 °C, of the setpoint.
 */
    void CtrlSetSwing(Ctrl *pC, I16 swing);

    /**
 * `nowMs` is monotonic time, ms.
 *
 * Returns the current boost state for reporting.
 */
    CtrlBoostStatus CtrlGetBoostStatus(const Ctrl *pC, U64 nowMs);

#ifdef __cplusplus
}
#endif
