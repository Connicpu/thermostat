/* sensor_stuck.c - see sensor_stuck.h. Pure C. */
#include "core.h"
#include "sensor_stuck.h"

/** Restart the timer on any change; report frozen after SENSOR_STUCK_MS. See sensor_stuck.h. */
bool SensorStuckUpdate(SensorStuck *pS, I16 t, U16 rh, U64 now)
{
    if (!pS->have || pS->t != t || pS->rh != rh)
    {
        pS->have = true;
        pS->t = t;
        pS->rh = rh;
        pS->sinceMs = now;
        return false;
    }
    return now - pS->sinceMs >= SENSOR_STUCK_MS;
}
