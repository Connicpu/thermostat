/* sensor.h - SHT40 on I2C0 (SDA IO6, SCL IO7). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool     valid;
    I16  tempC100;   /* calibrated (offset applied) */
    U16 rhC100;
} SensorReading;

esp_err_t SensorInit(void);
/* Blocking ~10 ms. On I2C/CRC error returns valid=false (and tries a soft reset). */
SensorReading SensorRead(I16 tempOffsetC100);
