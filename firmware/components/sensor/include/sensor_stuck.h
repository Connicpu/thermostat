/* sensor_stuck.h - detect a frozen SHT40 (host-tested in test/host/test_misc.c).
 *
 * A live SHT40 never returns the same temperature AND humidity for long:
 * its noise (~0.01 °C, ~0.1 %RH) flips the last digits of every reading.
 * Identical readings for SENSOR_STUCK_MS mean a latched sensor or a bus
 * returning stale data; the reading is then treated as invalid (failsafe). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_STUCK_MS (20u * 60u * 1000u)

typedef struct {
    bool     have;
    I16  t;
    U16 rh;
    U64 sinceMs;
} SensorStuck;

/* Feed every valid reading. Returns true while the value has been frozen for SENSOR_STUCK_MS. */
bool SensorStuckUpdate(SensorStuck *pS, I16 tC100, U16 rhC100, U64 nowMs);
