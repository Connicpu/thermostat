/* zb_retry.h - pure join/rejoin retry policy (host-tested in test/host/test_zb_retry.c).
 *
 * One counter of consecutive failures drives every (re)join attempt:
 * network steering that failed, a lost parent, or being asked to leave.
 * A join only "earns back" a fresh backoff once it has stayed up for
 * ZB_RETRY_STABLE_MS, so a flapping link (joins, drops 30 s later, ...)
 * keeps climbing the backoff instead of hammering the network every 10 s. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>

#define ZB_RETRY_STABLE_MS  (10u * 60u * 1000u)   /* joined this long = healthy again */
#define ZB_RETRY_STALL_MS   (60u * 1000u)          /* Zigbee task silent this long = stalled */

/** Join/rejoin backoff state. Zero-initialise; owned by the Zigbee task. */
typedef struct {
    U8  fails;      /* consecutive failed attempts / unstable joins */
    bool     joined;
    U64 joinedAt;  /* ms, valid while joined */
} ZbRetry;

/**
 * Returns the delay before the next attempt for the current failure count: 10 s, 30 s, 1 min, 2
 * min, 5 min, then every 15 min (never gives up).
 */
U32 ZbRetryDelayMs(const ZbRetry *pR);

/**
 * Joined (steering success or rejoin): remember when. Does not reset the backoff yet; that only
 * happens if the link then stays up for ZB_RETRY_STABLE_MS (see ZbRetryLost()).
 *
 * `nowMs` is monotonic time, ms.
 */
void     ZbRetryJoined(ZbRetry *pR, U64 nowMs);

/**
 * A join attempt failed: count it (saturating at 255) and mark not joined.
 *
 * Returns the delay before the next attempt (computed from the count before this failure), ms.
 */
U32 ZbRetryFailed(ZbRetry *pR);

/**
 * The link went away (parent lost, or asked to leave). Resets the backoff first if the join had
 * been stable for ZB_RETRY_STABLE_MS, then counts a failure, so a flapping link keeps climbing.
 *
 * `nowMs` is monotonic time, ms.
 *
 * Returns the delay before the rejoin, ms.
 */
U32 ZbRetryLost(ZbRetry *pR, U64 nowMs);

/**
 * Zigbee task heartbeat check.
 *
 * `lastAliveMs` is the last time the Zigbee task ran, ms; `nowMs` is monotonic time, ms.
 *
 * Returns true if the stack has not run for ZB_RETRY_STALL_MS (false if lastAliveMs is in the
 * future).
 */
bool     ZbRetryStalled(U64 lastAliveMs, U64 nowMs);
