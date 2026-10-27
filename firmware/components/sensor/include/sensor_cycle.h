/* sensor_cycle.h - SHT40 measure/retry policy with the bus behind a function-pointer seam.
 * Pure C (host-tested). sensor.c supplies the ESP-IDF I2C calls. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * One SHT40 measurement.
 * valid: false on I2C or CRC error (the other fields are then 0).
 * tempC100: temperature in 0.01 °C, with the caller's offset already applied.
 * rhC100: relative humidity in 0.01 %RH (0..10000).
 */
typedef struct
{
    bool valid;
    I16 tempC100; /* calibrated (offset applied) */
    U16 rhC100;
} SensorReading;

#define SENSOR_CMD_MEAS        0xFD /* high repeatability, no clock stretch */
#define SENSOR_CMD_RESET       0x94
#define SENSOR_ATTEMPTS        3
#define SENSOR_MEAS_WAIT_MS    10 /* tMEAS high repeatability is 8.3 ms max */
#define SENSOR_RETRY_GAP_MS    20 /* between attempts, after the soft reset */
#define SENSOR_RESET_SETTLE_MS 2  /* datasheet soft-reset recovery, used when the reset is ACKed */
#define SENSOR_I2C_TIMEOUT_MS  50

/**
 * Running totals for one sensor. Zero-initialise. ok: cycles that returned a reading.
 * measErr: a measurement transmit or receive failed. resetErr: a soft reset NACKed, or a bus
 * reset returned failure. crcErr: frames the sensor returned with a bad CRC. retries: extra
 * attempts after the first. busResets: i2c bus resets, counted whether or not the reset call
 * succeeded. failRun: consecutive cycles that produced no reading. failRunMax: the longest such
 * run. Every counter saturates at 0xFFFFFFFF.
 */
typedef struct
{
    U32 ok;
    U32 measErr;
    U32 resetErr;
    U32 crcErr;
    U32 retries;
    U32 busResets;
    U32 failRun;
    U32 failRunMax;
} SensorStats;

/**
 * The I2C operations one measurement needs. tx/rx return 0 on success and nonzero on a
 * failed transfer. busReset recovers a wedged bus and returns 0 on success, nonzero on failure.
 * delayMs waits, and must not be long: the cycle calls it for SENSOR_MEAS_WAIT_MS per attempt
 * that is commanded, SENSOR_RESET_SETTLE_MS after an ACKed soft reset, and SENSOR_RETRY_GAP_MS
 * after every recovery. pCtx is passed through. The device driver ignores it (the bus handles
 * are file-scope); a host fake uses it to reach its script.
 */
typedef struct
{
    void *pCtx;
    int (*tx)(void *pCtx, const U8 *pBuf, size_t n);
    int (*rx)(void *pCtx, U8 *pBuf, size_t n);
    int (*busReset)(void *pCtx);
    void (*delayMs)(void *pCtx, U32 ms);
} SensorBus;

/**
 * Record one cycle that produced no reading. failRun grows by one, saturating at 0xFFFFFFFF,
 * and failRunMax keeps the longer of itself and failRun. Used by SensorCycle() and by
 * SensorRead() when the device was never created.
 *
 * `pSt` is the counters, updated in place.
 */
void SensorStatsNoteFail(SensorStats *pSt);

/**
 * Measure once, retrying a CRC or I2C failure up to SENSOR_ATTEMPTS times. Between attempts
 * the bus is reset and the sensor is soft-reset (command 0x94). An ACKed reset waits
 * SENSOR_RESET_SETTLE_MS, then every recovery waits SENSOR_RETRY_GAP_MS so the tries land
 * about 20 ms apart. The same recovery runs after the last failure so the next cycle does not
 * inherit a wedged bus. A cycle is valid if any attempt parses. The offset is added only then.
 * A failed bus reset counts as resetErr and the soft reset is still attempted.
 *
 * `pB` is the bus seam; `pSt` is the counters, updated in place; `tempOffsetC100` is added to a
 * valid temperature, in 0.01 °C.
 *
 * Returns the reading. .valid is false when every attempt failed; the other fields are then 0.
 */
SensorReading SensorCycle(const SensorBus *pB, SensorStats *pSt, I16 tempOffsetC100);

/**
 * Soft-reset the sensor up to SENSOR_ATTEMPTS times. A failed transfer resets the bus and waits
 * SENSOR_RETRY_GAP_MS before the next try. An ACK waits SENSOR_RESET_SETTLE_MS and stops.
 * Does not touch the failed-cycle run: this is bring-up, not a measurement.
 *
 * `pB` is the bus seam; `pSt` counts soft-reset NACKs, failed bus resets, and bus resets.
 *
 * Returns 0 if a reset was ACKed, and nonzero if every attempt failed.
 */
int SensorSoftReset(const SensorBus *pB, SensorStats *pSt);
