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
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Values intentionally equal the ZCL Thermostat SystemMode enum (0x001C). */
typedef enum {
    CTRL_MODE_OFF  = 0x00,
    CTRL_MODE_AUTO = 0x01,
    CTRL_MODE_COOL = 0x03,
    CTRL_MODE_HEAT = 0x04,
} ctrl_mode_t;

/* Values intentionally equal the ZCL Fan Control FanMode enum (0x0000). */
typedef enum {
    CTRL_FAN_ON   = 0x04,
    CTRL_FAN_AUTO = 0x05,
} ctrl_fan_t;

typedef struct {
    ctrl_mode_t mode;
    ctrl_fan_t  fan;
    int16_t     heat_sp;   /* occupied heating setpoint, c°C */
    int16_t     cool_sp;   /* occupied cooling setpoint, c°C */
} ctrl_settings_t;

typedef struct {
    int16_t  hysteresis;          /* c°C each side of the setpoint (0.3 °C)   */
    int16_t  min_sp, max_sp;      /* allowed setpoint range                    */
    int16_t  deadband;            /* min cool_sp - heat_sp (auto changeover)   */
    int16_t  temp_min, temp_max;  /* plausible room temp; outside -> failsafe  */
    uint32_t sensor_timeout_ms;   /* no valid reading for this long -> failsafe*/
    uint8_t  recover_samples;     /* good samples needed to leave failsafe     */
    uint32_t comp_min_off_ms;     /* compressor (Y) anti-short-cycle            */
    uint32_t comp_min_on_ms;
    uint32_t heat_min_off_ms;
    uint32_t heat_min_on_ms;
    uint32_t changeover_ms;       /* min gap between heat stop and cool start (and vice versa) */
    uint32_t fan_runon_heat_ms;   /* G stays on after W drops                  */
    uint32_t fan_runon_cool_ms;   /* G stays on after Y drops                  */
    bool     heat_drives_fan;     /* energise G with W (electric/hydro-air); false for gas furnace */
} ctrl_params_t;

/* Sensible defaults for a single-stage gas furnace + AC. */
#define CTRL_PARAMS_DEFAULT() {            \
    .hysteresis        = 30,               \
    .min_sp            = 1000,             \
    .max_sp            = 3200,             \
    .deadband          = 150,              \
    .temp_min          = 0,                \
    .temp_max          = 4500,             \
    .sensor_timeout_ms = 30000,            \
    .recover_samples   = 3,                \
    .comp_min_off_ms   = 5u * 60u * 1000u, \
    .comp_min_on_ms    = 3u * 60u * 1000u, \
    .heat_min_off_ms   = 60u * 1000u,      \
    .heat_min_on_ms    = 2u * 60u * 1000u, \
    .changeover_ms     = 5u * 60u * 1000u, \
    .fan_runon_heat_ms = 90u * 1000u,      \
    .fan_runon_cool_ms = 45u * 1000u,      \
    .heat_drives_fan   = false,            \
}

typedef enum {
    CTRL_FAULT_NONE = 0,
    CTRL_FAULT_SENSOR_TIMEOUT,  /* no valid reading recently        */
    CTRL_FAULT_TEMP_RANGE,      /* reading outside temp_min..temp_max */
} ctrl_fault_t;

typedef struct {
    bool    valid;      /* reading succeeded (CRC ok etc.) */
    int16_t temp;       /* c°C */
} ctrl_input_t;

typedef struct {
    bool w, y, g;               /* requested outputs (still pass through the interlock!) */
    ctrl_fault_t fault;         /* != NONE -> failsafe active, all outputs off */
    bool heat_demand, cool_demand; /* thermostat wants heat/cool (may be held off by timers) */
    bool waiting;               /* demand present but blocked by min-off / changeover */
} ctrl_output_t;

typedef struct {
    ctrl_params_t p;
    bool     w, y, g;
    bool     heat_call, cool_call;     /* latched hysteresis state */
    uint64_t w_on_at, w_off_at, y_on_at, y_off_at;
    bool     w_ran, y_ran;              /* changeover only applies after a real run */
    uint64_t fan_runon_until;
    uint64_t last_valid_at;
    int16_t  last_temp;
    bool     have_temp;
    ctrl_fault_t fault;
    uint8_t  good_streak;
} ctrl_t;

void ctrl_init(ctrl_t *c, const ctrl_params_t *p, uint64_t now_ms);

/* Clamp setpoints into range and enforce the deadband. When the two
 * setpoints collide, the one named by `keep_heat` wins and the other moves. */
void ctrl_sanitize_settings(const ctrl_params_t *p, ctrl_settings_t *s, bool keep_heat);

/* Feed a sensor sample (call on every read attempt, valid or not). */
void ctrl_feed_sensor(ctrl_t *c, const ctrl_input_t *in, uint64_t now_ms);

/* Run one control step; call at least once a second. */
ctrl_output_t ctrl_step(ctrl_t *c, const ctrl_settings_t *s, uint64_t now_ms);

/* Next mode for the MODE button: OFF -> HEAT -> COOL -> AUTO -> OFF. */
ctrl_mode_t ctrl_next_mode(ctrl_mode_t m);
bool ctrl_mode_valid(int v);

const char *ctrl_fault_str(ctrl_fault_t f);

#ifdef __cplusplus
}
#endif
