#include "hvac_outputs.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "hvac";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static hvac_state_t s_state;
static uint32_t s_faults;

static void drive(hvac_state_t s)
{
    /* Drop first, raise after, so a W->Y swap can never overlap even for
     * one instruction. (The MOC3063 zero-cross opto + triac would hold
     * conduction until the next zero crossing anyway.) */
    if (!s.w) gpio_set_level(BOARD_GPIO_DRV_W, 0);
    if (!s.y) gpio_set_level(BOARD_GPIO_DRV_Y, 0);
    if (!s.g) gpio_set_level(BOARD_GPIO_DRV_G, 0);
    if (s.w)  gpio_set_level(BOARD_GPIO_DRV_W, 1);
    if (s.y)  gpio_set_level(BOARD_GPIO_DRV_Y, 1);
    if (s.g)  gpio_set_level(BOARD_GPIO_DRV_G, 1);
}

esp_err_t hvac_outputs_init(void)
{
    const gpio_num_t pins[] = {BOARD_GPIO_DRV_W, BOARD_GPIO_DRV_Y, BOARD_GPIO_DRV_G};
    for (int i = 0; i < 3; i++) {
        gpio_set_level(pins[i], 0); /* latch 0 before enabling the driver */
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << pins[i],
            .mode = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_ENABLE, /* belt + braces with the 10k on board */
        };
        ESP_ERROR_CHECK(gpio_config(&cfg));
        gpio_set_level(pins[i], 0);
        /* Keep the normal (awake) pad config during auto light sleep, so
         * an output that is on stays on and one that is off stays off. */
        gpio_sleep_sel_dis(pins[i]);
    }
    s_state = (hvac_state_t){0};
    return ESP_OK;
}

hvac_state_t hvac_outputs_set(hvac_state_t req)
{
    bool fault = false;
    hvac_state_t ok = hvac_interlock(req, &fault);
    portENTER_CRITICAL(&s_lock);
    drive(ok);
    s_state = ok;
    if (fault) s_faults++;
    portEXIT_CRITICAL(&s_lock);
    if (fault) ESP_LOGE(TAG, "INTERLOCK: W+Y requested together - both dropped");
    return ok;
}

hvac_state_t hvac_outputs_get(void)
{
    portENTER_CRITICAL(&s_lock);
    hvac_state_t s = s_state;
    portEXIT_CRITICAL(&s_lock);
    return s;
}

void hvac_outputs_all_off(void)
{
    hvac_outputs_set((hvac_state_t){0});
}

uint32_t hvac_outputs_interlock_faults(void)
{
    return s_faults;
}
