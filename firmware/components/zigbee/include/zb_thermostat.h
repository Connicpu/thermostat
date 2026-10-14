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
#include "display_model.h"   /* LinkState */
#include "zb_boost.h"

#define ZB_THERMOSTAT_EP 1

enum { kZbSetModeFan = 0, kZbSetHeatSp = 1, kZbSetCoolSp = 2 };

/**
 * Application callbacks for remote (Zigbee) writes and OTA events. All are called from the Zigbee
 * task, inside the stack's main loop: keep them short, don't block for long, and don't call
 * ZbThermostat* functions that take the stack lock (Pair, FactoryReset, Publish). Any may be NULL.
 */
typedef struct {
    /* A remote write changed settings. `what` says which field was written
     * (kZbSet*); only a setpoint write may change the program schedule. */
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

/**
 * Initial attribute values, from the persisted settings, applied when the stack starts.
 * calib is the LocalTemperatureCalibration in 0.1 °C; boostDir is a CtrlBoostDir value.
 */
typedef struct {
    CtrlSettings s;
    I8   calib;
    I16  boostSp;
    U16 boostMinutes;
    U8  boostDir;
    I16  swing;        /* c°C */
    SchedCfg sched;
} ZbThermostatInit;

/**
 * Local state pushed to the stack by ZbThermostatPublish(). Temperatures 0.01 °C, humidity
 * 0.01 %RH. Must stay plain data: publish compares it with memcmp() to skip no-op updates.
 */
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

/**
 * Start Zigbee: copy the callbacks and initial values, configure the native radio and create the
 * Zigbee task (6 KB stack, priority 5). That task builds the endpoint, starts the stack as a sleepy
 * end device, rejoins or starts pairing (network steering) and runs the stack main loop.
 * Failures never reboot: the thermostat keeps working locally and ZbThermostatLink() reports
 * kLinkFault.
 * @param pCbs   callbacks (copied)
 * @param pInit  initial attribute values (copied)
 * @return ESP_OK, or the platform-config / task-creation error (radio fault)
 */
esp_err_t    ZbThermostatStart(const ZbThermostatCbs *pCbs, const ZbThermostatInit *pInit);

/**
 * Push local state to the stack and report boost/schedule attribute changes to the coordinator.
 *
 * No-op before the stack runs, or when *pSt equals the previous call (unless a rejected remote
 * write must be undone). Takes the stack lock for at most 100 ms and skips the update if it
 * can't get it, so the control loop never blocks for long. Locally changed writable
 * manufacturer attributes (boost setpoint/duration, swing, schedule) only update the stack when
 * they differ from both the last published value and the current attribute, so a stale snapshot
 * can't undo a write that just came in from HA. Call from the app task.
 */
void         ZbThermostatPublish(const ZbThermostatState *pSt);

/**
 * @return the link state for display and HA: kLinkFault if the stack failed to start or the Zigbee
 * task has not run for ZB_RETRY_STALL_MS, else offline / pairing / joined / lost. Safe from any task.
 */
LinkState ZbThermostatLink(void);

/**
 * User asked to pair: if not joined, restart network steering with a fresh backoff.
 * No-op if the stack is not running or faulted; gives up (logged) if the stack lock is not
 * free within 1 s.
 */
void         ZbThermostatPair(void);

/**
 * Leave the network, erase the Zigbee NVRAM and reboot (esp_zb_factory_reset()). Returns only
 * if the stack is not running or its lock can't be taken within 1 s; the caller then restarts
 * by itself, and the network data is kept.
 */
void         ZbThermostatFactoryReset(void);

/**
 * Local time of day from the last Zigbee Time sync, extrapolated with the monotonic clock.
 * Safe from any task (spinlock).
 * @param pMinute  out: minutes after local midnight, 0..1439 (untouched when false is returned)
 * @return false until the first successful sync after boot
 */
bool         ZbThermostatLocalMinute(U16 *pMinute);
