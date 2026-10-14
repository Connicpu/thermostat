#include "hvac_outputs.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "hvac";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static hvac_state_t s_state;
static uint32_t s_faults;
static uint8_t s_drive_fault;   /* latched: bit0 W, bit1 Y, bit2 G - pad did not follow / pin unusable */

static const gpio_num_t PINS[3] = {BOARD_GPIO_DRV_W, BOARD_GPIO_DRV_Y, BOARD_GPIO_DRV_G};

/* returns a bitmask of pins whose write failed */
static uint8_t drive(hvac_state_t s)
{
    uint8_t bad = 0;
    const bool v[3] = {s.w, s.y, s.g};
    /* Drop first, raise after, so a W->Y swap can never overlap even for
     * one instruction. (The MOC3063 zero-cross opto + triac would hold
     * conduction until the next zero crossing anyway.) */
    for (int i = 0; i < 3; i++) if (!v[i] && gpio_set_level(PINS[i], 0) != ESP_OK) bad |= 1u << i;
    for (int i = 0; i < 3; i++) if (v[i] && gpio_set_level(PINS[i], 1) != ESP_OK) bad |= 1u << i;
    return bad;
}

/* pad levels as read back through the input buffer (pins are INPUT_OUTPUT) */
static hvac_state_t readback(void)
{
    return (hvac_state_t){.w = gpio_get_level(BOARD_GPIO_DRV_W) != 0, .y = gpio_get_level(BOARD_GPIO_DRV_Y) != 0,
                          .g = gpio_get_level(BOARD_GPIO_DRV_G) != 0};
}

esp_err_t hvac_outputs_init(void)
{
    const gpio_num_t *pins = PINS;
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < 3; i++) {
        gpio_set_level(pins[i], 0); /* latch 0 before enabling the driver */
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << pins[i],
            .mode = GPIO_MODE_INPUT_OUTPUT,       /* input buffer on: lets us read the pad back */
            .pull_down_en = GPIO_PULLDOWN_ENABLE, /* belt + braces with the 10k on board */
        };
        esp_err_t e = gpio_config(&cfg);
        if (e != ESP_OK) {
            /* No reboot loop: the 10k pull-down keeps this triac off; latch an output fault. */
            ESP_LOGE(TAG, "EVT output fault: config pin %d failed (%s)", pins[i], esp_err_to_name(e));
            s_drive_fault |= 1u << i;
            ret = e;
            continue;
        }
        gpio_set_level(pins[i], 0);
        /* Keep the normal (awake) pad config during auto light sleep, so
         * an output that is on stays on and one that is off stays off. */
        gpio_sleep_sel_dis(pins[i]);
    }
    s_state = (hvac_state_t){0};
    return ret;
}

hvac_state_t hvac_outputs_set(hvac_state_t req)
{
    bool fault = false;
    hvac_state_t ok = hvac_interlock(req, &fault);
    if (s_drive_fault) ok = (hvac_state_t){0};   /* latched output fault: hold everything off */
    portENTER_CRITICAL(&s_lock);
    uint8_t bad = drive(ok);
    hvac_state_t pad = readback();
    s_state = ok;
    if (fault) s_faults++;
    portEXIT_CRITICAL(&s_lock);
    if (fault) ESP_LOGE(TAG, "INTERLOCK: W+Y requested together - both dropped");
    bad |= (uint8_t)((pad.w != ok.w) | ((pad.y != ok.y) << 1) | ((pad.g != ok.g) << 2));
    if (bad) {
        if ((bad & ~s_drive_fault) != 0)
            ESP_LOGE(TAG, "EVT output fault: pads W=%d Y=%d G=%d, commanded W=%d Y=%d G=%d (mask 0x%x) - all off, latched",
                     pad.w, pad.y, pad.g, ok.w, ok.y, ok.g, bad);
        s_drive_fault |= bad;
        portENTER_CRITICAL(&s_lock);
        drive((hvac_state_t){0});        /* best effort: everything we can still drive goes low */
        s_state = (hvac_state_t){0};
        portEXIT_CRITICAL(&s_lock);
        pad = readback();
    }
    return pad;   /* what the pads really do (reported to Zigbee / display) */
}

uint8_t hvac_outputs_drive_fault(void) { return s_drive_fault; }

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
