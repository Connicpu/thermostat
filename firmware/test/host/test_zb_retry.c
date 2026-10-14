/* Host tests for the Zigbee join/rejoin retry policy (radio failure handling). */
#include "tinytest.h"
#include "zb_retry.h"

#define MIN 60000u

static void join_never_succeeds_backs_off_and_never_gives_up(void)
{
    zb_retry_t r = {0};
    CHECK_EQ(zb_retry_failed(&r), 10000u);
    CHECK_EQ(zb_retry_failed(&r), 30000u);
    CHECK_EQ(zb_retry_failed(&r), 1 * MIN);
    CHECK_EQ(zb_retry_failed(&r), 2 * MIN);
    CHECK_EQ(zb_retry_failed(&r), 5 * MIN);
    for (int i = 0; i < 1000; i++) CHECK_EQ(zb_retry_failed(&r), 15 * MIN);   /* saturates, no overflow */
    CHECK_EQ(r.fails, 255);
}

static void stable_join_resets_backoff_after_loss(void)
{
    zb_retry_t r = {0};
    for (int i = 0; i < 6; i++) zb_retry_failed(&r);
    zb_retry_joined(&r, 1000);
    CHECK_EQ(r.fails, 6);                                       /* not reset on join itself */
    CHECK_EQ(zb_retry_lost(&r, 1000 + ZB_RETRY_STABLE_MS), 10000u);
    CHECK(!r.joined);
    CHECK_EQ(zb_retry_failed(&r), 30000u);
}

static void flapping_link_does_not_hammer(void)
{
    zb_retry_t r = {0};
    uint64_t t = 0;
    uint64_t total_attempts_in_hour = 0, elapsed = 0;
    uint32_t last = 0;
    /* join, lose the parent 30 s later, rejoin immediately succeeds, repeat */
    while (elapsed < 60u * MIN) {
        zb_retry_joined(&r, t);
        t += 30000; elapsed += 30000;
        last = zb_retry_lost(&r, t);
        t += last; elapsed += last;
        total_attempts_in_hour++;
    }
    CHECK(last == 15 * MIN);
    CHECK(total_attempts_in_hour <= 10);                         /* vs 120 with a fixed 10 s retry */
}

static void lost_without_join_counts_as_failure(void)
{
    zb_retry_t r = {0};
    CHECK_EQ(zb_retry_lost(&r, 5 * MIN), 10000u);               /* never joined: no reset */
    CHECK_EQ(zb_retry_lost(&r, 99 * MIN), 30000u);
}

static void stall_detection(void)
{
    CHECK(!zb_retry_stalled(1000, 1000));
    CHECK(!zb_retry_stalled(1000, 1000 + ZB_RETRY_STALL_MS - 1));
    CHECK(zb_retry_stalled(1000, 1000 + ZB_RETRY_STALL_MS));
    CHECK(!zb_retry_stalled(5000, 1000));                        /* heartbeat ahead of reader: not stalled */
}

int main(void)
{
    RUN(join_never_succeeds_backs_off_and_never_gives_up);
    RUN(stable_join_resets_backoff_after_loss);
    RUN(flapping_link_does_not_hammer);
    RUN(lost_without_join_counts_as_failure);
    RUN(stall_detection);
    DONE();
}
