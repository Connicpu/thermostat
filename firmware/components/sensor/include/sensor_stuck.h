/* sensor_stuck.h - detect a frozen SHT40 (host-tested in test/host/test_misc.c).
 *
 * A live SHT40 never returns the same temperature AND humidity for long:
 * its noise (~0.01 °C, ~0.1 %RH) flips the last digits of every reading.
 * Identical readings for SENSOR_STUCK_MS mean a latched sensor or a bus
 * returning stale data; the reading is then treated as invalid (failsafe). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_STUCK_MS (20u * 60u * 1000u)

typedef struct {
    bool     have;
    int16_t  t;
    uint16_t rh;
    uint64_t since_ms;
} sensor_stuck_t;

/* Feed every valid reading. Returns true while the value has been frozen for SENSOR_STUCK_MS. */
bool sensor_stuck_update(sensor_stuck_t *s, int16_t t_c100, uint16_t rh_c100, uint64_t now_ms);
