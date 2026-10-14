/* hw_watchdog.h - STWD100 external watchdog (tWD ~1.6 s typ, 1.12 s min). */
#pragma once
#include <stdbool.h>

/**
 * Configure the STWD100 pins (WDI and ~EN) as outputs, leaving the watchdog DISARMED.
 *
 * ~EN is driven high before and after gpio_config() so the pin never glitches low (armed),
 * and both pins are excluded from light-sleep GPIO reconfiguration so their levels hold while
 * the chip sleeps. Call once at boot, before HwWatchdogArm().
 */
void HwWatchdogInit(void);

/**
 * Arm the external watchdog: kick once, then drive ~EN low.
 *
 * From here HwWatchdogKick() must be called at least every tWD (1.12 s worst case) or the
 * STWD100 resets the chip. There is no disarm: only a reset clears it.
 */
void HwWatchdogArm(void);

/**
 * Kick the watchdog by toggling WDI (the STWD100 restarts its timer on either edge).
 *
 * Safe to call while disarmed. Not thread-safe: call from one task only (the app task: main loop and factory reset).
 */
void HwWatchdogKick(void);

/** @return true once HwWatchdogArm() has been called. */
bool HwWatchdogArmed(void);
