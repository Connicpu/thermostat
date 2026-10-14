/* hw_watchdog.h - STWD100 external watchdog (tWD ~1.6 s typ, 1.12 s min). */
#pragma once
#include <stdbool.h>
void HwWatchdogInit(void);   /* pins configured, watchdog left DISARMED */
void HwWatchdogArm(void);    /* drive ~EN low; must kick within tWD from here */
void HwWatchdogKick(void);   /* toggle WDI */
bool HwWatchdogArmed(void);
