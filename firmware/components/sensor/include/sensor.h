/* sensor.h - SHT40 on I2C0 (SDA IO6, SCL IO7). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

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

/**
 * Create the I2C0 master bus (SDA/SCL from board.h, external 4.7 kΩ pull-ups, 100 kHz), add the
 * SHT40 at 0x44 and send it a soft reset (then wait 2 ms).
 *
 * Returns ESP_OK, or the error from bus/device creation (SensorRead() then returns invalid
 * readings).
 */
esp_err_t SensorInit(void);

/**
 * Take one high-repeatability measurement. Blocks ~10 ms (vTaskDelay; tMEAS is 8.3 ms max).
 *
 * On an I2C error the bus is reset and the sensor soft-reset, and valid=false is returned. On a
 * CRC error valid=false is returned (logged at warning level). Call from one task only.
 *
 * `tempOffsetC100` is added to the temperature, in 0.01 °C (fixed + user calibration).
 *
 * Returns the reading; check .valid.
 */
SensorReading SensorRead(I16 tempOffsetC100);
