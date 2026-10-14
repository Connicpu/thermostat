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

/**
 * Stuck-sensor detector state. Zero-initialise before first use.
 * have: a reference reading has been stored. t (0.01 °C) and rh (0.01 %RH): the reference reading.
 * sinceMs: monotonic time in ms at which the reading last changed.
 */
typedef struct {
    bool     have;
    I16  t;
    U16 rh;
    U64 sinceMs;
} SensorStuck;

/**
 * Feed one valid reading into the stuck detector.
 *
 * A reading that differs in temperature or humidity from the stored one restarts the timer.
 * Pure function: only *pS is modified.
 *
 * `pS` is the detector state; `tC100` is the temperature, 0.01 °C; `rhC100` is the relative humidity, 0.01
 * %RH; `nowMs` is monotonic time, ms.
 *
 * Returns true while the identical value has persisted for at least SENSOR_STUCK_MS.
 */
bool SensorStuckUpdate(SensorStuck *pS, I16 tC100, U16 rhC100, U64 nowMs);
