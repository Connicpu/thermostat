#include "hw_watchdog.h"
#include "board.h"

/** Driver state: s_armed = HwWatchdogArm() was called; s_wdi = current WDI output level. */
static bool s_armed, s_wdi;

/** Configure WDI/~EN, watchdog disarmed. See hw_watchdog.h. */
void HwWatchdogInit(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_GPIO_WD_WDI) | (1ULL << BOARD_GPIO_WD_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_set_level(BOARD_GPIO_WD_EN, 1);   /* stay disarmed */
    gpio_set_level(BOARD_GPIO_WD_WDI, 0);
    gpio_config(&cfg);
    gpio_set_level(BOARD_GPIO_WD_EN, 1);
    gpio_sleep_sel_dis(BOARD_GPIO_WD_EN);
    gpio_sleep_sel_dis(BOARD_GPIO_WD_WDI);
}

/** Toggle WDI. See hw_watchdog.h. */
void HwWatchdogKick(void)
{
    s_wdi = !s_wdi;              /* STWD100 resets its timer on either edge */
    gpio_set_level(BOARD_GPIO_WD_WDI, s_wdi);
}

/** Kick, then drive ~EN low to arm. See hw_watchdog.h. */
void HwWatchdogArm(void)
{
    HwWatchdogKick();
    gpio_set_level(BOARD_GPIO_WD_EN, 0);
    s_armed = true;
}

/** @return whether the watchdog has been armed. See hw_watchdog.h. */
bool HwWatchdogArmed(void) { return s_armed; }
