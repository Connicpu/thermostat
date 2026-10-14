/* zb_time.c - see zb_time.h. Pure C. */
#include "core.h"
#include "zb_time.h"

/** Range check, then the TimeStatus check. See zb_time.h. */
bool ZbTimeAcceptable(bool haveStatus, U8 status, U32 secs)
{
    if (secs < ZB_TIME_MIN_PLAUSIBLE || secs >= ZB_TIME_MAX_PLAUSIBLE) return false;
    if (haveStatus && !(status & (ZB_TIME_STATUS_MASTER | ZB_TIME_STATUS_SYNCHRONIZED))) return false;
    return true;
}

/** See zb_time.h. */
U32 ZbTimeNextDelayMs(U8 fails, bool known)
{
    static const U32 kBackoff[] = {60u * 1000u, 2u * 60u * 1000u, 5u * 60u * 1000u};
    const U32 hourly = 60u * 60u * 1000u;
    if (known) return hourly;
    if (fails < sizeof kBackoff / sizeof kBackoff[0]) return kBackoff[fails];
    return hourly;
}
