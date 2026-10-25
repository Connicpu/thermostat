#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"

/* Force the linker to keep this component. The bootloader hooks are weak. */
void bootloader_hooks_include(void) {}

/*
 * IO8 is the active-low status LED and a strap pin. Hold it on for 2 s so a
 * bench reset is obvious, then drive it high again before the app starts so a
 * later reset still samples the strap high.
 */
void bootloader_after_init(void)
{
    const uint32_t pin = 8;
    esp_rom_gpio_pad_select_gpio(pin);
    esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
    GPIO.enable_w1ts.val = 1u << pin;
    GPIO.out_w1tc.val = 1u << pin; /* on */
    esp_rom_delay_us(2000000);
    GPIO.out_w1ts.val = 1u << pin; /* off, strap-safe */
}
