#include "buttons.h"
#include <string.h>

/** See buttons.h. */
void BtnInit(Btn *pB, bool autorepeat)
{
    memset(pB, 0, sizeof *pB);
    pB->autorepeat = autorepeat;
}

/** Debounce, then press/repeat or hold-stage events. See buttons.h. */
BtnEvent BtnTick(Btn *pB, bool raw)
{
    if (raw == pB->rawLast)
    {
        if (pB->rawSameMs < 0xFFFF0000u)
            pB->rawSameMs += BTN_TICK_MS;
    }
    else
    {
        pB->rawLast = raw;
        pB->rawSameMs = 0;
    }

    bool was = pB->stable;
    if (pB->rawSameMs >= BTN_DEBOUNCE_MS)
        pB->stable = raw;
    if (!pB->armed)
    { /* ignore a key held since boot until it is released */
        if (pB->rawSameMs >= BTN_DEBOUNCE_MS && !raw)
            pB->armed = true;
        return kBtnEvNone;
    }

    if (pB->stable && !was)
    { /* press edge */
        pB->heldMs = 0;
        pB->stuck = false;
        pB->holdFired = pB->longFired = pB->warnFired = pB->vlongFired = false;
        pB->nextRepeatMs = BTN_REPEAT_DELAY;
        return pB->autorepeat ? kBtnEvPress : kBtnEvNone;
    }
    if (!pB->stable && was)
    { /* release edge */
        if (pB->autorepeat)
            return kBtnEvNone;
        return pB->holdFired ? kBtnEvRelease : kBtnEvPress;
    }
    if (pB->stable)
    {
        if (pB->heldMs < 0xFFFF0000u)
            pB->heldMs += BTN_TICK_MS;
        if (pB->heldMs >= BTN_STUCK_MS)
            pB->stuck = true;
        if (pB->autorepeat)
        {
            if (pB->stuck)
                return kBtnEvNone;
            if (pB->heldMs >= pB->nextRepeatMs)
            {
                pB->nextRepeatMs += BTN_REPEAT_EVERY;
                return kBtnEvRepeat;
            }
        }
        else
        {
            /* one event per tick, in order; stages are far apart so none is ever late */
            if (!pB->holdFired && pB->heldMs >= BTN_HOLD_MS)
            {
                pB->holdFired = true;
                return kBtnEvHold;
            }
            if (!pB->longFired && pB->heldMs >= BTN_LONG_MS)
            {
                pB->longFired = true;
                return kBtnEvLong;
            }
            if (!pB->warnFired && pB->heldMs >= BTN_WARN_MS)
            {
                pB->warnFired = true;
                return kBtnEvWarn;
            }
            if (!pB->vlongFired && pB->heldMs >= BTN_VLONG_MS)
            {
                pB->vlongFired = true;
                return kBtnEvVlong;
            }
        }
    }
    return kBtnEvNone;
}
