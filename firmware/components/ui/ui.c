#include "ui.h"
#include "board.h"
#include "esp_timer.h"

static Btn s_btn[3];
static UiEventCb s_cb;
static volatile bool s_combo;
static const gpio_num_t kPin[3] = {BOARD_GPIO_BTN_UP, BOARD_GPIO_BTN_DOWN, BOARD_GPIO_BTN_MODE};

/**
 * esp_timer callback, every BTN_TICK_MS: sample the three buttons (low = pressed), deliver their
 * events, then detect the UP+DOWN combo (fires once; reset when both are released).
 */
static void Tick(void *pArg)
{
    for (int i = 0; i < 3; i++)
    {
        BtnEvent ev = BtnTick(&s_btn[i], gpio_get_level(kPin[i]) == 0);
        if (ev != kBtnEvNone && s_cb)
            s_cb((UiButton)i, ev);
    }
    bool up = s_btn[kUiBtnUp].stable, down = s_btn[kUiBtnDown].stable;
    if (up && down && !s_combo)
    {
        s_combo = true;
        if (s_cb)
            s_cb(kUiBtnCombo, kBtnEvPress);
    }
    else if (!up && !down)
    {
        s_combo = false;
    }
}

/** See ui.h. */
void UiLed(bool on)
{
    gpio_set_level(BOARD_GPIO_LED, on ? 0 : 1);
}

/** See ui.h. */
esp_err_t UiInit(UiEventCb cb)
{
    s_cb = cb;
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << BOARD_GPIO_BTN_UP) | (1ULL << BOARD_GPIO_BTN_DOWN) | (1ULL << BOARD_GPIO_BTN_MODE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, /* 10k on board */
    };
    gpio_config(&in);
    gpio_set_level(BOARD_GPIO_LED, 1);
    gpio_config_t led = {.pin_bit_mask = 1ULL << BOARD_GPIO_LED, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&led);
    gpio_set_level(BOARD_GPIO_LED, 1);

    BtnInit(&s_btn[kUiBtnUp], true);
    BtnInit(&s_btn[kUiBtnDown], true);
    BtnInit(&s_btn[kUiBtnMode], false);

    /* The callback runs in the esp_timer task; keep handlers short. */
    const esp_timer_create_args_t a = {.callback = Tick, .name = "buttons"};
    esp_timer_handle_t t;
    esp_err_t err = esp_timer_create(&a, &t);
    if (err == ESP_OK)
        err = esp_timer_start_periodic(t, BTN_TICK_MS * 1000);
    return err;
}

/** See ui.h. */
bool UiComboActive(void)
{
    return s_combo;
}
