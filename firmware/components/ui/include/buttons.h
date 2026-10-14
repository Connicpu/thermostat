/* buttons.h - pure debounce / long-press state machine (host-testable).
 * Feed raw samples every BTN_TICK_MS; it returns at most one event. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>

#define BTN_TICK_MS      10
#define BTN_DEBOUNCE_MS  30
#define BTN_REPEAT_DELAY 600
#define BTN_REPEAT_EVERY 200
/* Non-repeating (MODE) button hold stages. A release before BTN_HOLD_MS is a
 * short PRESS; after that the release is reported as kBtnEvRelease so the
 * caller can act on the stage it reached (see components/ui/menu.h). */
#define BTN_HOLD_MS  1500  /* menu: "hold M to confirm" */
#define BTN_LONG_MS  5000  /* home: pairing (acted on at release, before WARN) */
#define BTN_WARN_MS  8000  /* home: factory-reset warning on screen */
#define BTN_VLONG_MS 15000 /* home: factory reset */
/* A button that is held at boot produces no events until it has been seen
 * released (a stuck/shorted key must never trigger a factory reset), and an
 * auto-repeat key stops repeating after BTN_STUCK_MS (no setpoint run-away). */
#define BTN_STUCK_MS 30000

typedef enum
{
    kBtnEvNone = 0,
    kBtnEvPress,   /* auto-repeat buttons: on press. others: on release if < BTN_HOLD_MS */
    kBtnEvRepeat,  /* auto-repeat buttons while held */
    kBtnEvLong,    /* held >= BTN_LONG_MS (fires once, while still held) */
    kBtnEvVlong,   /* held >= BTN_VLONG_MS (fires once, while still held) */
    kBtnEvHold,    /* held >= BTN_HOLD_MS (fires once, while still held) */
    kBtnEvWarn,    /* held >= BTN_WARN_MS (fires once, while still held) */
    kBtnEvRelease, /* released after kBtnEvHold fired (non-repeating buttons) */
} BtnEvent;

/**
 * Debounce / hold state of one button. Fields are internal; read only `stable` (debounced level,
 * true = pressed) from outside. Times are ms, counted in BTN_TICK_MS steps.
 */
typedef struct
{
    bool autorepeat;
    bool stable; /* debounced level, true = pressed */
    bool rawLast;
    U32 rawSameMs;
    U32 heldMs;
    U32 nextRepeatMs;
    bool holdFired, longFired, warnFired, vlongFired;
    bool armed; /* seen released (debounced) since init */
    bool stuck; /* held >= BTN_STUCK_MS */
} Btn;

/**
 * Reset a button. It stays silent until it has been seen released (debounced), so a key held or
 * shorted at boot never fires.
 *
 * `autorepeat` is true for UP/DOWN (press + repeat events), false for MODE (hold stages).
 */
void BtnInit(Btn *pB, bool autorepeat);

/**
 * Advance the state machine by one BTN_TICK_MS sample. Pure; returns at most one event per
 * tick.
 *
 * A level change is accepted after BTN_DEBOUNCE_MS of stable raw samples. Auto-repeat buttons:
 * kBtnEvPress on press, then kBtnEvRepeat after BTN_REPEAT_DELAY and every BTN_REPEAT_EVERY,
 * stopping once held for BTN_STUCK_MS. Other buttons: while held, kBtnEvHold, kBtnEvLong,
 * kBtnEvWarn and kBtnEvVlong fire once each at their thresholds. On release: kBtnEvPress if
 * released before BTN_HOLD_MS, else kBtnEvRelease.
 *
 * `rawPressed` is the raw (undebounced) level, true = pressed.
 *
 * Returns the event, or kBtnEvNone.
 */
BtnEvent BtnTick(Btn *pB, bool rawPressed);
