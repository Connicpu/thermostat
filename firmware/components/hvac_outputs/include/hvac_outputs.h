/* hvac_outputs.h - GPIO driver for the three MOC3063 + BT136 triac channels.
 * All writes go through hvac_interlock(); nothing else in the firmware may
 * touch the DRV_W / DRV_Y / DRV_G GPIOs. */
#pragma once
#include "esp_err.h"
#include "hvac_interlock.h"

/* Readback verification (see hvac_outputs_set): pads are checked on the NEXT call,
 * once the last change is HVAC_SETTLE_US old; HVAC_MISMATCH_N consecutive mismatches
 * latch an output fault (all off). A latched readback/write fault is re-tested after
 * HVAC_RETEST_MS (longer than every min-off/changeover time), at most HVAC_RETEST_MAX
 * times per boot. Readback sees only the GPIO pad: a shorted or open triac, or a
 * wiring fault after the MOC3063, is NOT detected. */
#define HVAC_SETTLE_US   1000
#define HVAC_MISMATCH_N  2
#define HVAC_RETEST_MS   (10u * 60u * 1000u)
#define HVAC_RETEST_MAX  3

esp_err_t    hvac_outputs_init(void);           /* all off, pins as in/out, held through light sleep; error = pin fault latched */
/* Returns the pad levels read back. If a write fails or a pad does not follow,
 * an output fault is latched: every output is driven low from then on. */
hvac_state_t hvac_outputs_set(hvac_state_t req);
hvac_state_t hvac_outputs_get(void);
void         hvac_outputs_all_off(void);
uint32_t     hvac_outputs_interlock_faults(void);
uint8_t      hvac_outputs_drive_fault(void);   /* current fault mask: bit0 W, bit1 Y, bit2 G (clears on a passed re-test) */
