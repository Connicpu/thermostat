/* zb_thermostat.h - Zigbee 3.0 HA thermostat (end device) on endpoint 1.
 *
 * Server clusters: Basic, Identify, Thermostat (0x0201), Fan Control
 * (0x0202), Temperature Measurement (0x0402), Relative Humidity (0x0405).
 * Client clusters: OTA Upgrade (0x0019), Time (0x000A; local time is read from
 * the coordinator on join and then hourly, see ZbThermostatLocalMinute()).
 *
 * The Zigbee stack runs in its own task. Remote writes are delivered via
 * callbacks (from the Zigbee task); local state is pushed with
 * ZbThermostatPublish() from any task. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "control.h"
#include "schedule.h"
#include "display_model.h"   /* link_state_t */
#include "zb_boost.h"

#define ZB_THERMOSTAT_EP 1

enum { kZbSetModeFan = 0, kZbSetHeatSp = 1, kZbSetCoolSp = 2 };

typedef struct {
    /* A remote write changed settings. `what` says which field was written
     * (ZB_SET_*); only a setpoint write may change the program schedule. */
    void (*onSettings)(const CtrlSettings *pS, U8 what);
    void (*onCalibration)(I8 calibTenths);       /* LocalTemperatureCalibration, 0.1 °C */
    void (*onIdentify)(U16 seconds);
    void (*onOtaProgress)(int percent);               /* -1 = finished/aborted */
    void (*beforeOtaReboot)(void);                     /* outputs off etc. */
    /* Boost cluster (0xFC57) writes. Called from the Zigbee task. */
    void (*onBoostParams)(I16 sp, U16 minutes, U8 dir);
    void (*onBoostCommand)(U8 cmd);                /* kZbBoostCmd* */
    void (*onSwing)(I16 swingC100);                 /* kZbAttrSwing write (already clamped) */
    void (*onSched)(const SchedCfg *pC);               /* program schedule write (sanitized) */
} ZbThermostatCbs;

typedef struct {
    CtrlSettings s;
    I8   calib;
    I16  boostSp;
    U16 boostMinutes;
    U8  boostDir;
    I16  swing;        /* c°C */
    SchedCfg sched;
} ZbThermostatInit;

typedef struct {
    bool     tempValid;
    I16  temp;         /* c°C, calibrated */
    bool     rhValid;
    U16 rh;           /* c%RH */
    CtrlSettings s;
    bool     w, y, g;      /* actual outputs (ThermostatRunningState) */
    U8  boostState;          /* kZbBoostState* */
    U16 boostRemainingMin;
    U8  boostLastResult;    /* kZbBoostResult* */
    /* Writable 0xFC57 settings; pushed (and reported) when changed locally, e.g. from the on-device menu. */
    I16  boostSp;
    U16 boostMinutes;
    I16  swing;
    SchedCfg sched;
    U8  schedState;          /* SchedState */
} ZbThermostatState;

esp_err_t    ZbThermostatStart(const ZbThermostatCbs *pCbs, const ZbThermostatInit *pInit);
void         ZbThermostatPublish(const ZbThermostatState *pSt);
LinkState ZbThermostatLink(void);
void         ZbThermostatPair(void);           /* (re)start network steering if not joined */
void         ZbThermostatFactoryReset(void);  /* leave + erase Zigbee NVRAM + reboot */
/* Local time of day from the last Zigbee Time sync, extrapolated with the
 * monotonic clock. Returns false (and leaves *minute alone) until the first
 * successful sync after boot. */
bool         ZbThermostatLocalMinute(U16 *pMinute);
