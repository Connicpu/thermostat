#include "buttons.h"
#include <string.h>

void btn_init(btn_t *b, bool autorepeat)
{
    memset(b, 0, sizeof *b);
    b->autorepeat = autorepeat;
}

btn_event_t btn_tick(btn_t *b, bool raw)
{
    if (raw == b->raw_last) {
        if (b->raw_same_ms < 0xFFFF0000u) b->raw_same_ms += BTN_TICK_MS;
    } else {
        b->raw_last = raw;
        b->raw_same_ms = 0;
    }

    bool was = b->stable;
    if (b->raw_same_ms >= BTN_DEBOUNCE_MS) b->stable = raw;

    if (b->stable && !was) {                  /* press edge */
        b->held_ms = 0;
        b->hold_fired = b->long_fired = b->warn_fired = b->vlong_fired = false;
        b->next_repeat_ms = BTN_REPEAT_DELAY;
        return b->autorepeat ? BTN_EV_PRESS : BTN_EV_NONE;
    }
    if (!b->stable && was) {                  /* release edge */
        if (b->autorepeat) return BTN_EV_NONE;
        return b->hold_fired ? BTN_EV_RELEASE : BTN_EV_PRESS;
    }
    if (b->stable) {
        b->held_ms += BTN_TICK_MS;
        if (b->autorepeat) {
            if (b->held_ms >= b->next_repeat_ms) {
                b->next_repeat_ms += BTN_REPEAT_EVERY;
                return BTN_EV_REPEAT;
            }
        } else {
            /* one event per tick, in order; stages are far apart so none is ever late */
            if (!b->hold_fired && b->held_ms >= BTN_HOLD_MS)   { b->hold_fired = true;  return BTN_EV_HOLD; }
            if (!b->long_fired && b->held_ms >= BTN_LONG_MS)   { b->long_fired = true;  return BTN_EV_LONG; }
            if (!b->warn_fired && b->held_ms >= BTN_WARN_MS)   { b->warn_fired = true;  return BTN_EV_WARN; }
            if (!b->vlong_fired && b->held_ms >= BTN_VLONG_MS) { b->vlong_fired = true; return BTN_EV_VLONG; }
        }
    }
    return BTN_EV_NONE;
}
