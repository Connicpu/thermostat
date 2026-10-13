/* sensor.h - SHT40 on I2C0 (SDA IO6, SCL IO7). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool     valid;
    int16_t  temp_c100;   /* calibrated (offset applied) */
    uint16_t rh_c100;
} sensor_reading_t;

esp_err_t sensor_init(void);
/* Blocking ~10 ms. On I2C/CRC error returns valid=false (and tries a soft reset). */
sensor_reading_t sensor_read(int16_t temp_offset_c100);
