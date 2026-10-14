/* Host tests for the Zigbee join/rejoin retry policy (radio failure handling). */
#include "core.h"
#include "tinytest.h"
#include "zb_retry.h"

#define MIN 60000u

/**
 * Failed joins back off 10 s, 30 s, 1, 2, 5 min, then every 15 min forever; the counter saturates at 255.
 */
static void JoinNeverSucceedsBacksOffAndNeverGivesUp(void)
{
    ZbRetry r = {0};
    CHECK_EQ(ZbRetryFailed(&r), 10000u);
    CHECK_EQ(ZbRetryFailed(&r), 30000u);
    CHECK_EQ(ZbRetryFailed(&r), 1 * MIN);
    CHECK_EQ(ZbRetryFailed(&r), 2 * MIN);
    CHECK_EQ(ZbRetryFailed(&r), 5 * MIN);
    for (int i = 0; i < 1000; i++)
        CHECK_EQ(ZbRetryFailed(&r), 15 * MIN); /* saturates, no overflow */
    CHECK_EQ(r.fails, 255);
}

/** A join does not reset the backoff by itself; losing a link that was up for ZB_RETRY_STABLE_MS does. */
static void StableJoinResetsBackoffAfterLoss(void)
{
    ZbRetry r = {0};
    for (int i = 0; i < 6; i++)
        ZbRetryFailed(&r);
    ZbRetryJoined(&r, 1000);
    CHECK_EQ(r.fails, 6); /* not reset on join itself */
    CHECK_EQ(ZbRetryLost(&r, 1000 + ZB_RETRY_STABLE_MS), 10000u);
    CHECK(!r.joined);
    CHECK_EQ(ZbRetryFailed(&r), 30000u);
}

/** A link that drops 30 s after every join climbs to the 15 min backoff (at most 10 attempts in an hour). */
static void FlappingLinkDoesNotHammer(void)
{
    ZbRetry r = {0};
    U64 t = 0;
    U64 totalAttemptsInHour = 0, elapsed = 0;
    U32 last = 0;
    /* join, lose the parent 30 s later, rejoin immediately succeeds, repeat */
    while (elapsed < 60u * MIN)
    {
        ZbRetryJoined(&r, t);
        t += 30000;
        elapsed += 30000;
        last = ZbRetryLost(&r, t);
        t += last;
        elapsed += last;
        totalAttemptsInHour++;
    }
    CHECK(last == 15 * MIN);
    CHECK(totalAttemptsInHour <= 10); /* vs 120 with a fixed 10 s retry */
}

/** A loss without a preceding join counts as a plain failure. */
static void LostWithoutJoinCountsAsFailure(void)
{
    ZbRetry r = {0};
    CHECK_EQ(ZbRetryLost(&r, 5 * MIN), 10000u); /* never joined: no reset */
    CHECK_EQ(ZbRetryLost(&r, 99 * MIN), 30000u);
}

/**
 * ZbRetryStalled() fires at exactly ZB_RETRY_STALL_MS and never when the heartbeat is ahead of the reader.
 */
static void StallDetection(void)
{
    CHECK(!ZbRetryStalled(1000, 1000));
    CHECK(!ZbRetryStalled(1000, 1000 + ZB_RETRY_STALL_MS - 1));
    CHECK(ZbRetryStalled(1000, 1000 + ZB_RETRY_STALL_MS));
    CHECK(!ZbRetryStalled(5000, 1000)); /* heartbeat ahead of reader: not stalled */
}

/** Run every test in this file; exit status 1 if any failed (tinytest DONE()). */
int main(void)
{
    RUN(JoinNeverSucceedsBacksOffAndNeverGivesUp);
    RUN(StableJoinResetsBackoffAfterLoss);
    RUN(FlappingLinkDoesNotHammer);
    RUN(LostWithoutJoinCountsAsFailure);
    RUN(StallDetection);
    DONE();
}
