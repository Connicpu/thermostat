/* sensor.h - SHT40 on I2C0 (SDA IO6, SCL IO7). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "sensor_cycle.h"

/**
 * Create the I2C0 master bus (SDA/SCL from board.h, external 4.7 kΩ pull-ups, 100 kHz), add the
 * SHT40 at 0x44 and soft-reset it. The soft reset is tried SENSOR_ATTEMPTS times, because the
 * sensor often does not ACK in the first moments after power-up. A reset that is ACKed is
 * followed by a 2 ms settle. Bus or device creation is not retried: that is the driver, not the
 * sensor.
 *
 * Returns ESP_OK once the device is on the bus (even if every soft reset NACKed; SensorRead()
 * keeps trying). Returns the error from bus or device creation, after which SensorRead() returns
 * invalid readings.
 */
esp_err_t SensorInit(void);

/**
 * Take one high-repeatability measurement, trying up to SENSOR_ATTEMPTS times about
 * SENSOR_RETRY_GAP_MS apart. Between attempts the bus is reset and the sensor is soft-reset;
 * an ACKed reset settles for SENSOR_RESET_SETTLE_MS before that gap. The cycle is valid if any
 * attempt succeeds. A failed cycle is logged at warning level ("I2C error" and/or "CRC error").
 * Call from one task only.
 *
 * Blocks for the 10 ms measurement wait on each attempt that gets a command through, plus the
 * gap between attempts. It does not loop past SENSOR_ATTEMPTS.
 *
 * `tempOffsetC100` is added to the temperature, in 0.01 °C (fixed + user calibration).
 *
 * Returns the reading; check .valid.
 */
SensorReading SensorRead(I16 tempOffsetC100);

/**
 * Copy the counters kept by SensorRead() and SensorInit(). The pure cycle updates whatever
 * SensorStats the caller passes; on the device that is the one this returns.
 *
 * `pOut` receives the copy.
 */
void SensorStatsGet(SensorStats *pOut);
