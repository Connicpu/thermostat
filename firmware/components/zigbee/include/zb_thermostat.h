/* zb_thermostat.h - Zigbee 3.0 HA thermostat (end device) on endpoint 1.
 *
 * Server clusters: Basic, Identify, Thermostat (0x0201), Fan Control
 * (0x0202), Temperature Measurement (0x0402), Relative Humidity (0x0405).
 * Client clusters: OTA Upgrade (0x0019).
 *
 * The Zigbee stack runs in its own task. Remote writes are delivered via
 * callbacks (from the Zigbee task); local state is pushed with
 * zb_thermostat_publish() from any task. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "control.h"
#include "display_model.h"   /* link_state_t */
#include "zb_boost.h"

#define ZB_THERMOSTAT_EP 1

typedef struct {
    /* A remote write changed settings. `keep_heat`: heat setpoint was the one written. */
    void (*on_settings)(const ctrl_settings_t *s, bool keep_heat);
    void (*on_calibration)(int8_t calib_tenths);       /* LocalTemperatureCalibration, 0.1 °C */
    void (*on_identify)(uint16_t seconds);
    void (*on_ota_progress)(int percent);               /* -1 = finished/aborted */
    void (*before_ota_reboot)(void);                     /* outputs off etc. */
    /* Boost cluster (0xFC57) writes. Called from the Zigbee task. */
    void (*on_boost_params)(int16_t sp, uint16_t minutes, uint8_t dir);
    void (*on_boost_command)(uint8_t cmd);                /* ZB_BOOST_CMD_* */
} zb_thermostat_cbs_t;

typedef struct {
    ctrl_settings_t s;
    int8_t   calib;
    int16_t  boost_sp;
    uint16_t boost_minutes;
    uint8_t  boost_dir;
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
} zb_thermostat_state_t;

esp_err_t    zb_thermostat_start(const zb_thermostat_cbs_t *cbs, const zb_thermostat_init_t *init);
void         zb_thermostat_publish(const zb_thermostat_state_t *st);
link_state_t zb_thermostat_link(void);
void         zb_thermostat_pair(void);           /* (re)start network steering if not joined */
void         zb_thermostat_factory_reset(void);  /* leave + erase Zigbee NVRAM + reboot */
