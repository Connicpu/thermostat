/* hvac_outputs.h - GPIO driver for the three MOC3063 + BT136 triac channels.
 * All writes go through hvac_interlock(); nothing else in the firmware may
 * touch the DRV_W / DRV_Y / DRV_G GPIOs. */
#pragma once
#include "esp_err.h"
#include "hvac_interlock.h"

esp_err_t    hvac_outputs_init(void);           /* all off, pins as in/out, held through light sleep; error = pin fault latched */
/* Returns the pad levels read back. If a write fails or a pad does not follow,
 * an output fault is latched: every output is driven low from then on. */
hvac_state_t hvac_outputs_set(hvac_state_t req);
hvac_state_t hvac_outputs_get(void);
void         hvac_outputs_all_off(void);
uint32_t     hvac_outputs_interlock_faults(void);
uint8_t      hvac_outputs_drive_fault(void);   /* latched mask: bit0 W, bit1 Y, bit2 G */
