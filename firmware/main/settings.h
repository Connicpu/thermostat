#pragma once
#include <stdbool.h>
#include "control.h"
#include "schedule.h"
typedef struct {
    /* mode + fan. heat_sp/cool_sp here are only a mirror of the current program
     * setpoints; the source of truth is `sched` (v5+). */
    ctrl_settings_t s;
    int8_t calib_tenths;    /* LocalTemperatureCalibration, 0.1 °C */
    /* boost *preferences* (what the next boost will use). An active boost is
     * deliberately NOT persisted: after a reboot the thermostat is back to normal. */
    int16_t  boost_sp;      /* c°C */
    uint16_t boost_minutes;
    uint8_t  boost_dir;     /* ctrl_boost_dir_t */
    int16_t  swing;         /* c°C each side of the setpoint, CTRL_SWING_MIN..MAX (v3+) */
    uint8_t  fahrenheit;    /* display units, 1 = °F (v4+; default from Kconfig) */
    sched_cfg_t sched;      /* day/night program + morning boost (v5+) */
} persist_t;
void settings_load(persist_t *p);          /* defaults if absent/corrupt */
bool settings_save(const persist_t *p);
void settings_erase(void);
