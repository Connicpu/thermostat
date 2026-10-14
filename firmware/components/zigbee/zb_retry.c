/* zb_retry.c - see zb_retry.h. Pure C. */
#include "zb_retry.h"

uint32_t zb_retry_delay_ms(const zb_retry_t *r)
{
    static const uint32_t table[] = {10000u, 30000u, 60000u, 120000u, 300000u};
    if (r->fails < sizeof table / sizeof table[0]) return table[r->fails];
    return 15u * 60u * 1000u;
}

void zb_retry_joined(zb_retry_t *r, uint64_t now_ms)
{
    r->joined = true;
    r->joined_at = now_ms;
}

uint32_t zb_retry_failed(zb_retry_t *r)
{
    uint32_t d = zb_retry_delay_ms(r);
    r->joined = false;
    if (r->fails < 255) r->fails++;
    return d;
}

uint32_t zb_retry_lost(zb_retry_t *r, uint64_t now_ms)
{
    if (r->joined && now_ms - r->joined_at >= ZB_RETRY_STABLE_MS) r->fails = 0;
    return zb_retry_failed(r);
}

bool zb_retry_stalled(uint64_t last_alive_ms, uint64_t now_ms)
{
    return now_ms > last_alive_ms && now_ms - last_alive_ms >= ZB_RETRY_STALL_MS;
}
