/* hvac_outputs.h - GPIO driver for the three MOC3063 + BT136 triac channels.
 * All writes go through HvacInterlock(); nothing else in the firmware may
 * touch the DRV_W / DRV_Y / DRV_G GPIOs. */
#pragma once
#include "core.h"
#include "esp_err.h"
#include "hvac_interlock.h"

/* Readback verification (see HvacOutputsSet): pads are checked on the NEXT call,
 * once the last change is HVAC_SETTLE_US old; HVAC_MISMATCH_N consecutive mismatches
 * latch an output fault (all off). A latched readback/write fault is re-tested after
 * HVAC_RETEST_MS (longer than every min-off/changeover time), at most HVAC_RETEST_MAX
 * times per boot. Readback sees only the GPIO pad: a shorted or open triac, or a
 * wiring fault after the MOC3063, is NOT detected. */
#define HVAC_SETTLE_US   1000
#define HVAC_MISMATCH_N  2
#define HVAC_RETEST_MS   (10u * 60u * 1000u)
#define HVAC_RETEST_MAX  3

/**
 * Configure DRV_W/Y/G as input+output (so the pads can be read back) with pull-downs, latched
 * low before the driver is enabled, and kept in their awake configuration through auto light
 * sleep. A pin that fails to configure is latched as a permanent output fault (the on-board 10
 * kΩ pull-down keeps that triac off); there is no reboot loop.
 *
 * Returns ESP_OK, or the last gpio_config() error.
 */
esp_err_t    HvacOutputsInit(void);

/**
 * Drive the outputs. The only path to the DRV_* GPIOs.
 *
 * 1. Verifies the PREVIOUS command once it is HVAC_SETTLE_US old; HVAC_MISMATCH_N consecutive
 * readback mismatches on a pin latch an output fault. 2. A latched fault is re-tested after
 * HVAC_RETEST_MS while the pads read all-off, at most HVAC_RETEST_MAX times per boot. 3. Passes
 * req through HvacInterlock() (a W+Y request counts an interlock fault), forces everything off
 * while a fault is latched, and drives the pins (drop first, then G, W, Y; never Y without G).
 * A failed GPIO write latches a fault and drives everything low. Threading: the pin writes and
 * the commanded state are updated under a spinlock, but the readback/re-test bookkeeping is
 * not. Call from the app task; the one exception is HvacOutputsAllOff() from the Zigbee task
 * just before a reboot (ZbBeforeReboot in main.c).
 *
 * Requested outputs.
 *
 * Returns the state actually commanded (after the interlock and fault hold), not the pad
 * readback.
 */
HvacState HvacOutputsSet(HvacState req);

/** Returns the last commanded output state (spinlock-protected copy; safe from any task). */
HvacState HvacOutputsGet(void);

/** Command all outputs off through HvacOutputsSet() (so the normal verification still runs). */
void         HvacOutputsAllOff(void);

/**
 * Returns how many times HvacInterlock() has rejected a W+Y request since boot. Non-zero makes
 * the main loop unhealthy, so the watchdogs stop being kicked.
 */
U32     HvacOutputsInterlockFaults(void);

/**
 * Returns the current output fault mask: bit0 W, bit1 Y, bit2 G. Readback/write faults clear on
 * a passed re-test; pin configuration faults last until reboot.
 */
U8      HvacOutputsDriveFault(void);
