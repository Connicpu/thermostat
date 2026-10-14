#pragma once
#include <stdbool.h>
#include "control.h"
#include "schedule.h"
typedef struct {
    /* mode + fan. heat_sp/cool_sp here are only a mirror of the current program
     * setpoints; the source of truth is `sched`. */
    ctrl_settings_t s;
    int8_t calib_tenths;    /* LocalTemperatureCalibration, 0.1 °C */
    /* boost *preferences* (what the next boost will use). An active boost is
     * deliberately NOT persisted: after a reboot the thermostat is back to normal. */
    int16_t  boost_sp;      /* c°C */
    uint16_t boost_minutes;
    uint8_t  boost_dir;     /* ctrl_boost_dir_t */
    int16_t  swing;         /* c°C each side of the setpoint, CTRL_SWING_MIN..MAX  */
    uint8_t  fahrenheit;    /* display units, 1 = °F (default from Kconfig) */
    sched_cfg_t sched;      /* day/night program + morning boost  */
} persist_t;
#include <stddef.h>
#define SETTINGS_VERSION 1           /* blob layout version; unknown versions load defaults */
#define SETTINGS_BLOB_MAX 256          /* read buffer; also bounds future blob growth */

/* pure codec (settings_codec.c, host-tested) */
void   settings_defaults(persist_t *p, bool fahrenheit);
bool   settings_decode(const void *data, size_t n, persist_t *p);   /* false: older/unknown version or too short */
void   settings_sanitize(persist_t *p, bool fahrenheit_default);
size_t settings_encode(const persist_t *p, void *buf, size_t n);   /* 0 if buf too small */

/* NVS (settings.c) */
void settings_load(persist_t *p);          /* defaults if absent/corrupt */
bool settings_save(const persist_t *p);
void settings_erase(void);
