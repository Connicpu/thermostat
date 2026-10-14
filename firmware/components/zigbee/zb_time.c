/* zb_time.c - see zb_time.h. Pure C. */
#include "zb_time.h"

bool zb_time_acceptable(bool have_status, uint8_t status, uint32_t secs)
{
    if (secs < ZB_TIME_MIN_PLAUSIBLE || secs >= ZB_TIME_MAX_PLAUSIBLE) return false;
    if (have_status && !(status & (ZB_TIME_STATUS_MASTER | ZB_TIME_STATUS_SYNCHRONIZED))) return false;
    return true;
}

uint32_t zb_time_next_delay_ms(uint8_t fails, bool known)
{
    static const uint32_t backoff[] = {60u * 1000u, 2u * 60u * 1000u, 5u * 60u * 1000u};
    const uint32_t hourly = 60u * 60u * 1000u;
    if (known) return hourly;
    if (fails == 0) return backoff[0];
    if (fails <= sizeof backoff / sizeof backoff[0]) return backoff[fails - 1];
    return hourly;
}
