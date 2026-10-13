/* hvac_outputs.h - GPIO driver for the three MOC3063 + BT136 triac channels.
 * All writes go through hvac_interlock(); nothing else in the firmware may
 * touch the DRV_W / DRV_Y / DRV_G GPIOs. */
#pragma once
#include "esp_err.h"
#include "hvac_interlock.h"

esp_err_t    hvac_outputs_init(void);           /* all off, pins as outputs, held through light sleep */
hvac_state_t hvac_outputs_set(hvac_state_t req); /* returns what was actually driven */
hvac_state_t hvac_outputs_get(void);
void         hvac_outputs_all_off(void);
uint32_t     hvac_outputs_interlock_faults(void);
