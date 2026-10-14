/* zb_thermostat.h - Zigbee 3.0 HA thermostat (end device) on endpoint 1.
 *
 * Server clusters: Basic, Identify, Thermostat (0x0201), Fan Control
 * (0x0202), Temperature Measurement (0x0402), Relative Humidity (0x0405).
 * Client clusters: OTA Upgrade (0x0019), Time (0x000A; local time is read from
 * the coordinator on join and then hourly, see zb_thermostat_local_minute()).
 *
 * The Zigbee stack runs in its own task. Remote writes are delivered via
 * callbacks (from the Zigbee task); local state is pushed with
 * zb_thermostat_publish() from any task. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "control.h"
#include "schedule.h"
#include "display_model.h"   /* link_state_t */
#include "zb_boost.h"

#define ZB_THERMOSTAT_EP 1

enum { ZB_SET_MODE_FAN = 0, ZB_SET_HEAT_SP = 1, ZB_SET_COOL_SP = 2 };

typedef struct {
    /* A remote write changed settings. `what` says which field was written
     * (ZB_SET_*); only a setpoint write may change the program schedule. */
    void (*on_settings)(const ctrl_settings_t *s, uint8_t what);
    void (*on_calibration)(int8_t calib_tenths);       /* LocalTemperatureCalibration, 0.1 °C */
    void (*on_identify)(uint16_t seconds);
    void (*on_ota_progress)(int percent);               /* -1 = finished/aborted */
    void (*before_ota_reboot)(void);                     /* outputs off etc. */
    /* Boost cluster (0xFC57) writes. Called from the Zigbee task. */
    void (*on_boost_params)(int16_t sp, uint16_t minutes, uint8_t dir);
    void (*on_boost_command)(uint8_t cmd);                /* ZB_BOOST_CMD_* */
    void (*on_swing)(int16_t swing_c100);                 /* ZB_ATTR_SWING write (already clamped) */
    void (*on_sched)(const sched_cfg_t *c);               /* program schedule write (sanitized) */
} zb_thermostat_cbs_t;

typedef struct {
    ctrl_settings_t s;
    int8_t   calib;
    int16_t  boost_sp;
    uint16_t boost_minutes;
    uint8_t  boost_dir;
    int16_t  swing;        /* c°C */
    sched_cfg_t sched;
} zb_thermostat_init_t;

typedef struct {
    bool     temp_valid;
    int16_t  temp;         /* c°C, calibrated */
    bool     rh_valid;
    uint16_t rh;           /* c%RH */
    ctrl_settings_t s;
    bool     w, y, g;      /* actual outputs (ThermostatRunningState) */
    uint8_t  boost_state;          /* ZB_BOOST_STATE_* */
    uint16_t boost_remaining_min;
    uint8_t  boost_last_result;    /* ZB_BOOST_RESULT_* */
    /* Writable 0xFC57 settings; pushed (and reported) when changed locally, e.g. from the on-device menu. */
    int16_t  boost_sp;
    uint16_t boost_minutes;
    int16_t  swing;
    sched_cfg_t sched;
    uint8_t  sched_state;          /* sched_state_t */
} zb_thermostat_state_t;

esp_err_t    zb_thermostat_start(const zb_thermostat_cbs_t *cbs, const zb_thermostat_init_t *init);
void         zb_thermostat_publish(const zb_thermostat_state_t *st);
link_state_t zb_thermostat_link(void);
void         zb_thermostat_pair(void);           /* (re)start network steering if not joined */
void         zb_thermostat_factory_reset(void);  /* leave + erase Zigbee NVRAM + reboot */
/* Local time of day from the last Zigbee Time sync, extrapolated with the
 * monotonic clock. Returns false (and leaves *minute alone) until the first
 * successful sync after boot. */
bool         zb_thermostat_local_minute(uint16_t *minute);
