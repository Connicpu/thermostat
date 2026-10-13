#include "ui.h"
#include "board.h"
#include "esp_timer.h"

static btn_t s_btn[3];
static ui_event_cb_t s_cb;
static const gpio_num_t s_pin[3] = {BOARD_GPIO_BTN_UP, BOARD_GPIO_BTN_DOWN, BOARD_GPIO_BTN_MODE};

static void tick(void *arg)
{
    for (int i = 0; i < 3; i++) {
        btn_event_t ev = btn_tick(&s_btn[i], gpio_get_level(s_pin[i]) == 0);
        if (ev != BTN_EV_NONE && s_cb) s_cb((ui_button_t)i, ev);
    }
}

void ui_led(bool on) { gpio_set_level(BOARD_GPIO_LED, on ? 0 : 1); }

esp_err_t ui_init(ui_event_cb_t cb)
{
    s_cb = cb;
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << BOARD_GPIO_BTN_UP) | (1ULL << BOARD_GPIO_BTN_DOWN) | (1ULL << BOARD_GPIO_BTN_MODE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,  /* 10k on board */
    };
    gpio_config(&in);
    gpio_set_level(BOARD_GPIO_LED, 1);
    gpio_config_t led = {.pin_bit_mask = 1ULL << BOARD_GPIO_LED, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&led);
    gpio_set_level(BOARD_GPIO_LED, 1);

    btn_init(&s_btn[UI_BTN_UP], true);
    btn_init(&s_btn[UI_BTN_DOWN], true);
    btn_init(&s_btn[UI_BTN_MODE], false);

    /* The callback runs in the esp_timer task; keep handlers short. */
    const esp_timer_create_args_t a = {.callback = tick, .name = "buttons"};
    esp_timer_handle_t t;
    esp_err_t err = esp_timer_create(&a, &t);
    if (err == ESP_OK) err = esp_timer_start_periodic(t, BTN_TICK_MS * 1000);
    return err;
}
