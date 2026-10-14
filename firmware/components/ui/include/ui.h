/* ui.h - polls UP/DOWN/MODE every 10 ms and delivers debounced events. */
#pragma once
#include "buttons.h"
#include "esp_err.h"

/* kUiBtnCombo: UP and DOWN held together (fires kBtnEvPress once per hold). */
typedef enum { kUiBtnUp, kUiBtnDown, kUiBtnMode, kUiBtnCombo } UiButton;
typedef void (*UiEventCb)(UiButton which, BtnEvent ev);

esp_err_t UiInit(UiEventCb cb);
/* Status LED (active-low on IO8). */
void UiLed(bool on);
/* true while an UP+DOWN combo is being held (UP/DOWN events should be ignored). */
bool UiComboActive(void);
