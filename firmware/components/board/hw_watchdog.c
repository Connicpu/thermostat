#include "hw_watchdog.h"
#include "board.h"

static bool s_armed, s_wdi;

void hw_watchdog_init(void)
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

void hw_watchdog_kick(void)
{
    s_wdi = !s_wdi;              /* STWD100 resets its timer on either edge */
    gpio_set_level(BOARD_GPIO_WD_WDI, s_wdi);
}

void hw_watchdog_arm(void)
{
    hw_watchdog_kick();
    gpio_set_level(BOARD_GPIO_WD_EN, 0);
    s_armed = true;
}

bool hw_watchdog_armed(void) { return s_armed; }
