/* buttons.h - pure debounce / long-press state machine (host-testable).
 * Feed raw samples every BTN_TICK_MS; it returns at most one event. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BTN_TICK_MS        10
#define BTN_DEBOUNCE_MS    30
#define BTN_REPEAT_DELAY   600
#define BTN_REPEAT_EVERY   200
#define BTN_LONG_MS        3000    /* MODE: pairing */
#define BTN_VLONG_MS       10000   /* MODE: factory reset */

typedef enum {
    BTN_EV_NONE = 0,
    BTN_EV_PRESS,     /* auto-repeat buttons: on press. others: on release if short */
    BTN_EV_REPEAT,    /* auto-repeat buttons while held */
    BTN_EV_LONG,      /* held >= BTN_LONG_MS (fires once, while still held) */
    BTN_EV_VLONG,     /* held >= BTN_VLONG_MS (fires once, while still held) */
} btn_event_t;

typedef struct {
    bool     autorepeat;
    bool     stable;        /* debounced level, true = pressed */
    bool     raw_last;
    uint32_t raw_same_ms;
    uint32_t held_ms;
    uint32_t next_repeat_ms;
    bool     long_fired, vlong_fired;
} btn_t;

void        btn_init(btn_t *b, bool autorepeat);
btn_event_t btn_tick(btn_t *b, bool raw_pressed);
