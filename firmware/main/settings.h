#pragma once
#include <stdbool.h>
#include "control.h"
typedef struct {
    ctrl_settings_t s;
    int8_t calib_tenths;    /* LocalTemperatureCalibration, 0.1 °C */
} persist_t;
void settings_load(persist_t *p);          /* defaults if absent/corrupt */
bool settings_save(const persist_t *p);
void settings_erase(void);
