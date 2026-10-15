/* zb_time.c - see zb_time.h. Pure C. */
#include "core.h"
#include "zb_time.h"

/** Range check, then the TimeStatus check. See zb_time.h. */
bool ZbTimeAcceptable(bool haveStatus, U8 status, U32 secs)
{
    if (secs < ZB_TIME_MIN_PLAUSIBLE || secs >= ZB_TIME_MAX_PLAUSIBLE)
        return false;
    if (haveStatus && !(status & (ZB_TIME_STATUS_MASTER | ZB_TIME_STATUS_SYNCHRONIZED)))
        return false;
    return true;
}

/** See zb_time.h. */
U32 ZbTimeNextDelayMs(U8 fails, bool known)
{
    static const U32 kBackoff[] = {60u * 1000u, 2u * 60u * 1000u, 5u * 60u * 1000u};
    const U32 hourly = 60u * 60u * 1000u;
    if (known)
        return hourly;
    if (fails < sizeof kBackoff / sizeof kBackoff[0])
        return kBackoff[fails];
    return hourly;
}

/** See zb_time.h. */
bool ZbTimeComposeLocal(bool haveLocal, U32 localS, bool haveUtc, U32 utcS, I32 tzS, I32 dstS, U32 *pLocalS)
{
    if (haveLocal)
    {
        *pLocalS = localS;
        return true;
    }
    if (!haveUtc)
        return false;
    *pLocalS = (U32)((I64)utcS + (I64)tzS + (I64)dstS);
    return true;
}

/** See zb_time.h. */
U32 ZbTimeExtrapolate(U32 syncLocalS, U64 syncAtMs, U64 nowMs, U16 *pMsIntoSec)
{
    U64 elapsed = nowMs >= syncAtMs ? nowMs - syncAtMs : 0;
    U64 totalMs = (U64)syncLocalS * 1000ULL + elapsed;
    if (pMsIntoSec)
        *pMsIntoSec = (U16)(totalMs % 1000ULL);
    return (U32)((totalMs / 1000ULL) % 86400ULL);
}

/** See zb_time.h. */
U64 ZbTimeMinuteStart(U32 syncLocalS, U64 syncAtMs, U64 nowMs, U16 *pMinute)
{
    U16 msIntoSec = 0;
    U32 sec = ZbTimeExtrapolate(syncLocalS, syncAtMs, nowMs, &msIntoSec);
    U32 msInto = (sec % 60u) * 1000u + msIntoSec;
    if (pMinute)
        *pMinute = (U16)(sec / 60u);
    if (nowMs < msInto)
        return 0;
    return nowMs - msInto;
}
