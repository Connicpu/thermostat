/* zb_retry.c - see zb_retry.h. Pure C. */
#include "core.h"
#include "zb_retry.h"

/** Backoff table lookup. See zb_retry.h. */
U32 ZbRetryDelayMs(const ZbRetry *pR)
{
    static const U32 kTable[] = {10000u, 30000u, 60000u, 120000u, 300000u};
    if (pR->fails < sizeof kTable / sizeof kTable[0])
        return kTable[pR->fails];
    return 15u * 60u * 1000u;
}

/** See zb_retry.h. */
void ZbRetryJoined(ZbRetry *pR, U64 nowMs)
{
    pR->joined = true;
    pR->joinedAt = nowMs;
}

/** See zb_retry.h. */
U32 ZbRetryFailed(ZbRetry *pR)
{
    U32 d = ZbRetryDelayMs(pR);
    pR->joined = false;
    if (pR->fails < 255)
        pR->fails++;
    return d;
}

/** Reset if the join was stable, then count a failure. See zb_retry.h. */
U32 ZbRetryLost(ZbRetry *pR, U64 nowMs)
{
    if (pR->joined && nowMs - pR->joinedAt >= ZB_RETRY_STABLE_MS)
        pR->fails = 0;
    return ZbRetryFailed(pR);
}

/** See zb_retry.h. */
bool ZbRetryStalled(U64 lastAliveMs, U64 nowMs)
{
    return nowMs > lastAliveMs && nowMs - lastAliveMs >= ZB_RETRY_STALL_MS;
}
