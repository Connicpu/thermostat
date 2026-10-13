/* hw_watchdog.h - STWD100 external watchdog (tWD ~1.6 s typ, 1.12 s min). */
#pragma once
#include <stdbool.h>
void hw_watchdog_init(void);   /* pins configured, watchdog left DISARMED */
void hw_watchdog_arm(void);    /* drive ~EN low; must kick within tWD from here */
void hw_watchdog_kick(void);   /* toggle WDI */
bool hw_watchdog_armed(void);
