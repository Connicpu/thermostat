/* ui.h - polls UP/DOWN/MODE every 10 ms and delivers debounced events. */
#pragma once
#include "buttons.h"
#include "esp_err.h"

/* UI_BTN_COMBO: UP and DOWN held together (fires BTN_EV_PRESS once per hold). */
typedef enum { UI_BTN_UP, UI_BTN_DOWN, UI_BTN_MODE, UI_BTN_COMBO } ui_button_t;
typedef void (*ui_event_cb_t)(ui_button_t which, btn_event_t ev);

esp_err_t ui_init(ui_event_cb_t cb);
/* Status LED (active-low on IO8). */
void ui_led(bool on);
/* true while an UP+DOWN combo is being held (UP/DOWN events should be ignored). */
bool ui_combo_active(void);
