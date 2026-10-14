/* buttons.h - pure debounce / long-press state machine (host-testable).
 * Feed raw samples every BTN_TICK_MS; it returns at most one event. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BTN_TICK_MS        10
#define BTN_DEBOUNCE_MS    30
#define BTN_REPEAT_DELAY   600
#define BTN_REPEAT_EVERY   200
/* Non-repeating (MODE) button hold stages. A release before BTN_HOLD_MS is a
 * short PRESS; after that the release is reported as BTN_EV_RELEASE so the
 * caller can act on the stage it reached (see components/ui/menu.h). */
#define BTN_HOLD_MS        1500    /* menu: "hold M to confirm" */
#define BTN_LONG_MS        5000    /* home: pairing (acted on at release, before WARN) */
#define BTN_WARN_MS        8000    /* home: factory-reset warning on screen */
#define BTN_VLONG_MS       15000   /* home: factory reset */
/* A button that is held at boot produces no events until it has been seen
 * released (a stuck/shorted key must never trigger a factory reset), and an
 * auto-repeat key stops repeating after BTN_STUCK_MS (no setpoint run-away). */
#define BTN_STUCK_MS       30000

typedef enum {
    BTN_EV_NONE = 0,
    BTN_EV_PRESS,     /* auto-repeat buttons: on press. others: on release if < BTN_HOLD_MS */
    BTN_EV_REPEAT,    /* auto-repeat buttons while held */
    BTN_EV_LONG,      /* held >= BTN_LONG_MS (fires once, while still held) */
    BTN_EV_VLONG,     /* held >= BTN_VLONG_MS (fires once, while still held) */
    BTN_EV_HOLD,      /* held >= BTN_HOLD_MS (fires once, while still held) */
    BTN_EV_WARN,      /* held >= BTN_WARN_MS (fires once, while still held) */
    BTN_EV_RELEASE,   /* released after BTN_EV_HOLD fired (non-repeating buttons) */
} btn_event_t;

typedef struct {
    bool     autorepeat;
    bool     stable;        /* debounced level, true = pressed */
    bool     raw_last;
    uint32_t raw_same_ms;
    uint32_t held_ms;
    uint32_t next_repeat_ms;
    bool     hold_fired, long_fired, warn_fired, vlong_fired;
    bool     armed;         /* seen released (debounced) since init */
    bool     stuck;         /* held >= BTN_STUCK_MS */
} btn_t;

void        btn_init(btn_t *b, bool autorepeat);
btn_event_t btn_tick(btn_t *b, bool raw_pressed);
