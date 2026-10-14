/* ui.h - polls UP/DOWN/MODE every 10 ms and delivers debounced events. */
#pragma once
#include "buttons.h"
#include "esp_err.h"

/* kUiBtnCombo: UP and DOWN held together (fires kBtnEvPress once per hold). */
typedef enum { kUiBtnUp, kUiBtnDown, kUiBtnMode, kUiBtnCombo } UiButton;

/**
 * Button event callback. Called from the esp_timer task every BTN_TICK_MS tick that produces an
 * event; keep it short and non-blocking (e.g. post to a queue).
 */
typedef void (*UiEventCb)(UiButton which, BtnEvent ev);

/**
 * Configure the three buttons (active low, external 10 kΩ pull-ups) and the status LED (off),
 * then start a periodic 10 ms esp_timer that debounces the buttons and calls cb. In addition to
 * the per-button events, cb(kUiBtnCombo, kBtnEvPress) fires once when UP and DOWN are both
 * held.
 *
 * Returns the esp_timer create/start error, or ESP_OK.
 */
esp_err_t UiInit(UiEventCb cb);

/** Switch the status LED (active-low on IO8). */
void UiLed(bool on);

/** Returns true while an UP+DOWN combo is being held (UP/DOWN events should then be ignored). Safe from any task. */
bool UiComboActive(void);
