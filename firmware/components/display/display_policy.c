/* display_policy.c - see display_policy.h. Pure C, no panel or FreeRTOS calls. */
#include "core.h"
#include "display_policy.h"

/** See display_policy.h. */
U16 DisplayClockMinute(U16 localMinute, U64 minuteStartMs, U64 nowMs)
{
    U32 base = (U32)localMinute % 1440u;
    if (minuteStartMs == 0 || nowMs <= minuteStartMs)
        return (U16)base;
    U64 add = (nowMs - minuteStartMs) / (U64)DISPLAY_BG_KNOWN_MS;
    return (U16)((base + add) % 1440u);
}

/** See display_policy.h. */
bool DisplayBackgroundDue(
    bool first,
    bool clockKnown,
    bool dirty,
    U64 nowMs,
    U64 lastRefreshMs,
    U16 localMinute,
    U64 minuteStartMs,
    int lastPaintedMinute)
{
    if (clockKnown)
    {
        /* Failed show: the minute was not recorded, and the model is dirty again.
         * Due after a few seconds even when the minute already matches what was
         * painted earlier. No attempt yet (lastRefresh 0) paints immediately.
         * Inside the backoff, return here so a minute mismatch cannot spin. */
        if (first && dirty)
        {
            if (lastRefreshMs == 0 || nowMs - lastRefreshMs >= DISPLAY_FAIL_BACKOFF_MS)
                return true;
            return false;
        }
        U16 minuteNow = DisplayClockMinute(localMinute, minuteStartMs, nowMs);
        /* Rollover or a step (DST / resync): due at that minute, not lastRefresh + 60 s. */
        if ((int)minuteNow != lastPaintedMinute)
            return true;
        if (dirty && nowMs - lastRefreshMs >= DISPLAY_BG_CAP_MS)
            return true;
        return false;
    }
    /* Same failure gate as the clock-known path: a dead panel must not retry every loop. */
    if (first)
    {
        if (!dirty)
            return false;
        return lastRefreshMs == 0 || nowMs - lastRefreshMs >= DISPLAY_FAIL_BACKOFF_MS;
    }
    return dirty && nowMs - lastRefreshMs >= DISPLAY_BG_UNKNOWN_MS;
}

/** See display_policy.h. */
bool DisplayRefreshIsFull(bool first, bool epochChanged, int partials, U64 nowMs, U64 lastFullMs)
{
    if (first || epochChanged)
        return true;
    U64 age = nowMs - lastFullMs;
    if (age >= DISPLAY_FULL_CAP_MS)
        return true;
    return age >= DISPLAY_FULL_GAP_MS && partials >= DISPLAY_FULL_PARTIALS;
}
