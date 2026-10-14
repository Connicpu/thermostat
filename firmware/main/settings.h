#pragma once
#include "core.h"
#include <stdbool.h>
#include "control.h"
#include "schedule.h"
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

/* pure codec (settings_codec.c, host-tested) */
void   SettingsDefaults(Persist *pP, bool fahrenheit);
bool   SettingsDecode(const void *pData, size_t n, Persist *pP);   /* false: older/unknown version or too short */
void   SettingsSanitize(Persist *pP, bool fahrenheitDefault);
size_t SettingsEncode(const Persist *pP, void *pBuf, size_t n);   /* 0 if buf too small */

/* NVS (settings.c) */
void SettingsLoad(Persist *pP);          /* defaults if absent/corrupt */
bool SettingsSave(const Persist *pP);
void SettingsErase(void);
