#pragma once
#include "core.h"
#include <stdbool.h>
#include "control.h"
#include "schedule.h"

/**
 * Everything that is persisted in NVS (one blob, see settings_codec.c for the compatibility rules:
 * fields may only be appended). Temperatures 0.01 °C. An active boost is deliberately not part of it.
 */
typedef struct {
    /* mode + fan. heatSp/coolSp here are only a mirror of the current program
     * setpoints; the source of truth is `sched`. */
    CtrlSettings s;
    I8 calibTenths;    /* LocalTemperatureCalibration, 0.1 °C */
    /* boost *preferences* (what the next boost will use). An active boost is
     * deliberately NOT persisted: after a reboot the thermostat is back to normal. */
    I16  boostSp;      /* c°C */
    U16 boostMinutes;
    U8  boostDir;     /* CtrlBoostDir */
    I16  swing;         /* c°C each side of the setpoint, CTRL_SWING_MIN..MAX  */
    U8  fahrenheit;    /* display units, 1 = °F (default from Kconfig) */
    SchedCfg sched;      /* day/night program + morning boost  */
} Persist;
#include <stddef.h>
#define SETTINGS_VERSION 1           /* blob layout version; unknown versions load defaults */
#define SETTINGS_BLOB_MAX 256          /* read buffer; also bounds future blob growth */

/**
 * Factory defaults: Off, fan Auto, heat 20.0 °C / cool 24.5 °C, boost 22.0 °C / 30 min / Auto,
 * default swing and schedule, no calibration.
 *
 * `fahrenheit` is the default display unit (from Kconfig).
 */
void   SettingsDefaults(Persist *pP, bool fahrenheit);

/**
 * Decode an NVS blob. Accepts the same or a newer layout (version >= SETTINGS_VERSION and at
 * least our size): the known prefix is copied and appended fields are ignored. Values are not
 * range checked; call SettingsSanitize() afterwards.
 *
 * Returns false (and *pP untouched) for an older/unknown version or a too-short blob.
 */
bool   SettingsDecode(const void *pData, size_t n, Persist *pP);

/**
 * Bring every field back into range: mode/fan/setpoints and the schedule are clamped, and other
 * out-of-range values (calibration beyond ±2.5 °C, boost setpoint/minutes/direction, units,
 * swing) are reset to their defaults.
 *
 * `fahrenheitDefault` is the units used when the stored flag is invalid.
 */
void   SettingsSanitize(Persist *pP, bool fahrenheitDefault);

/**
 * Encode settings as a blob of the current version (padding zeroed).
 *
 * Returns the blob size, or 0 if n is too small.
 */
size_t SettingsEncode(const Persist *pP, void *pBuf, size_t n);

/**
 * Load the settings from NVS (namespace "tstat", key "cfg3"): defaults if absent, unreadable or of
 * an unknown layout (logged), always sanitized. Uses a static buffer: call from one task (the app
 * task, also at boot).
 */
void SettingsLoad(Persist *pP);

/**
 * Encode and write the settings blob, then commit. Uses the same static buffer as
 * SettingsLoad().
 *
 * Returns true on success (failures are logged).
 */
bool SettingsSave(const Persist *pP);

/** Erase every key in the settings namespace (factory reset). Errors are ignored. */
void SettingsErase(void);
