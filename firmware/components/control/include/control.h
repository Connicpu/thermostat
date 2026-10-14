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
extern "C" {
#endif

/* Values intentionally equal the ZCL Thermostat SystemMode enum (0x001C).
 * There is deliberately no AUTO (heat/cool changeover) mode: the day/night
 * program schedule (schedule.h) replaced it. */
typedef enum {
    kCtrlModeOff  = 0x00,
    kCtrlModeCool = 0x03,
    kCtrlModeHeat = 0x04,
} CtrlMode;

/* Values intentionally equal the ZCL Fan Control FanMode enum (0x0000). */
typedef enum {
    kCtrlFanOn   = 0x04,
    kCtrlFanAuto = 0x05,
} CtrlFan;

typedef struct {
    CtrlMode mode;
    CtrlFan  fan;
    I16     heatSp;   /* occupied heating setpoint, c°C */
    I16     coolSp;   /* occupied cooling setpoint, c°C */
} CtrlSettings;

typedef struct {
    I16  hysteresis;          /* swing, c°C each side of the setpoint (user setting,
                                   * CTRL_SWING_MIN..MAX, default 0.5 °C)       */
    I16  minSp, maxSp;      /* allowed setpoint range                    */
    I16  tempMin, tempMax;  /* plausible room temp; outside -> failsafe  */
    U32 sensorTimeoutMs;   /* no valid reading for this long -> failsafe*/
    U8  recoverSamples;     /* good samples needed to leave failsafe     */
    U32 compMinOffMs;     /* compressor (Y) anti-short-cycle            */
    U32 compMinOnMs;
    U32 heatMinOffMs;
    U32 heatMinOnMs;
    U32 changeoverMs;       /* min gap between heat stop and cool start (and vice versa) */
    U32 fanRunonHeatMs;   /* G stays on after W drops                  */
    U32 fanRunonCoolMs;   /* G stays on after Y drops                  */
    bool     heatDrivesFan;     /* energise G with W (electric/hydro-air); false for gas furnace */
} CtrlParams;

/* Temperature swing (hysteresis), c°C each side of the setpoint. Heat turns on
 * at SP-swing and off at SP+swing; cooling is the mirror image. */
#define CTRL_SWING_MIN      10      /* 0.1 °C */
#define CTRL_SWING_MAX      100     /* 1.0 °C */
#define CTRL_SWING_DEFAULT  50      /* 0.5 °C */

/* Sensible defaults for a single-stage gas furnace + AC. */
#define CTRL_PARAMS_DEFAULT() {            \
    .hysteresis        = CTRL_SWING_DEFAULT,             \
    .minSp            = 1000,             \
    .maxSp            = 3200,             \
    .tempMin          = 0,                \
    .tempMax          = 4500,             \
    .sensorTimeoutMs = 30000,            \
    .recoverSamples   = 3,                \
    .compMinOffMs   = 5u * 60u * 1000u, \
    .compMinOnMs    = 3u * 60u * 1000u, \
    .heatMinOffMs   = 60u * 1000u,      \
    .heatMinOnMs    = 2u * 60u * 1000u, \
    .changeoverMs     = 5u * 60u * 1000u, \
    .fanRunonHeatMs = 90u * 1000u,      \
    .fanRunonCoolMs = 45u * 1000u,      \
    .heatDrivesFan   = false,            \
}

typedef enum {
    kCtrlFaultNone = 0,
    kCtrlFaultSensorTimeout,  /* no valid reading recently        */
    kCtrlFaultTempRange,      /* reading outside tempMin..tempMax */
} CtrlFault;

typedef struct {
    bool    valid;      /* reading succeeded (CRC ok etc.) */
    I16 temp;       /* c°C */
} CtrlInput;

typedef struct {
    bool w, y, g;               /* requested outputs (still pass through the interlock!) */
    CtrlFault fault;         /* != NONE -> failsafe active, all outputs off */
    bool heatDemand, coolDemand; /* thermostat wants heat/cool (may be held off by timers) */
    bool waiting;               /* demand present but blocked by min-off / changeover */
} CtrlOutput;

/* ---------------- boost (runtime only, never persisted) ---------------- */
typedef enum { kBoostNone = 0, kBoostToTarget = 1, kBoostTimed = 2 } CtrlBoostKind;
/* Requested direction. Values match the Zigbee boost_mode attribute. */
typedef enum { kBoostDirAuto = 0, kBoostDirHeat = 1, kBoostDirCool = 2 } CtrlBoostDir;
typedef enum {
    kBoostEndNone = 0, kBoostEndReached, kBoostEndExpired, kBoostEndCancelled,
    kBoostEndModeChanged, kBoostEndFault, kBoostEndLimit,
} CtrlBoostEnd;
typedef enum {
    kBoostOk = 0, kBoostErrArg, kBoostErrFault, kBoostErrAtTarget, kBoostErrNoDirection,
} CtrlBoostResult;

#define BOOST_TIMED_MIN_MIN     1
#define BOOST_TIMED_MAX_MIN     120     /* timed boost: 1..120 min */
#define BOOST_TARGET_MAX_MIN    180     /* boost-to-target gives up after 3 h */

typedef struct {
    CtrlBoostKind kind;
    CtrlMode dir;          /* resolved: kCtrlModeHeat or kCtrlModeCool */
    I16     sp;           /* target (kBoostToTarget) */
    U64    endAt;       /* hard deadline */
    CtrlMode baseMode;    /* system mode when started; changing it cancels */
    CtrlBoostEnd lastEnd;
} CtrlBoost;

typedef struct {
    CtrlBoostKind kind;   /* kBoostNone if inactive */
    CtrlMode dir;
    I16     sp;
    U32    remainingS;
    CtrlBoostEnd lastEnd;
} CtrlBoostStatus;

typedef struct {
    CtrlParams p;
    CtrlBoost boost;
    bool     softStop;   /* boost just ended: running stage still honours its min on-time */
    bool     w, y, g;
    bool     heatCall, coolCall;     /* latched hysteresis state */
    U64 wOnAt, wOffAt, yOnAt, yOffAt;
    bool     wRan, yRan;              /* changeover only applies after a real run */
    U64 fanRunonUntil;
    U64 lastValidAt;
    I16  lastTemp;
    bool     haveTemp;
    CtrlFault fault;
    U8  goodStreak;
} Ctrl;

void CtrlInit(Ctrl *pC, const CtrlParams *pP, U64 nowMs);

/* Validate mode/fan and clamp both setpoints into minSp..maxSp. Heating and
 * cooling never run from the same mode, so the two setpoints are independent
 * (no deadband). `keep_heat` is kept for API compatibility and ignored. */
void CtrlSanitizeSettings(const CtrlParams *pP, CtrlSettings *pS, bool keepHeat);

/* Feed a sensor sample (call on every read attempt, valid or not). */
void CtrlFeedSensor(Ctrl *pC, const CtrlInput *pIn, U64 nowMs);

/* Run one control step; call at least once a second. */
CtrlOutput CtrlStep(Ctrl *pC, const CtrlSettings *pS, U64 nowMs);

/* Next mode: OFF -> HEAT -> COOL -> OFF. */
CtrlMode CtrlNextMode(CtrlMode m);
bool CtrlModeValid(int v);

const char *CtrlFaultStr(CtrlFault f);

/* Start a boost. `s` = current normal settings (used for the AUTO boost direction of a
 * timed boost and for detecting later mode changes). kBoostToTarget uses
 * `sp` (clamped to minSp..maxSp) and ends when reached or after
 * BOOST_TARGET_MAX_MIN. kBoostTimed runs `minutes` (1..120). Starting a new
 * boost replaces a running one. All safety rules still apply: the boost only
 * changes what the thermostat *asks for*. */
CtrlBoostResult CtrlBoostStart(Ctrl *pC, const CtrlSettings *pS, CtrlBoostKind kind,
                                     CtrlBoostDir dir, I16 sp, U16 minutes, U64 nowMs);
void CtrlBoostCancel(Ctrl *pC);

/* Clamp a swing value into CTRL_SWING_MIN..MAX. */
I16 CtrlClampSwing(I32 swing);
/* Change the swing at runtime (clamped). Latched heat/cool calls are kept, so
 * a running stage simply finishes against the new off threshold. */
void CtrlSetSwing(Ctrl *pC, I16 swing);
CtrlBoostStatus CtrlGetBoostStatus(const Ctrl *pC, U64 nowMs);

#ifdef __cplusplus
}
#endif
