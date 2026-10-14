/* sensor_stuck.c - see sensor_stuck.h. Pure C. */
#include "sensor_stuck.h"

bool sensor_stuck_update(sensor_stuck_t *s, int16_t t, uint16_t rh, uint64_t now)
{
    if (!s->have || s->t != t || s->rh != rh) {
        s->have = true;
        s->t = t;
        s->rh = rh;
        s->since_ms = now;
        return false;
    }
    return now - s->since_ms >= SENSOR_STUCK_MS;
}
