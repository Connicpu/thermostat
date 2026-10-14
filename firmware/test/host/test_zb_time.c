/* Host tests for the Zigbee Time sync helpers. */
#include "tinytest.h"
#include "zb_time.h"

#define Y2026 ((uint32_t)26 * 365u * 86400u + 7u * 86400u)   /* ~2026-01-01 since 2000 */

static void plausibility_and_status(void)
{
    CHECK(zb_time_acceptable(false, 0, Y2026));                          /* no status attr: year check only */
    CHECK(zb_time_acceptable(true, ZB_TIME_STATUS_MASTER, Y2026));
    CHECK(zb_time_acceptable(true, ZB_TIME_STATUS_SYNCHRONIZED, Y2026));
    CHECK(zb_time_acceptable(true, 0x05, Y2026));                        /* zigpy: Master | Master_for_Zone_and_DST */
    CHECK(!zb_time_acceptable(true, 0x00, Y2026));                       /* unset clock */
    CHECK(!zb_time_acceptable(true, 0x04, Y2026));                       /* only zone/DST master */
    CHECK(!zb_time_acceptable(false, 0, 0));
    CHECK(!zb_time_acceptable(false, 0, 3600));                          /* 2000-01-01 01:00: never set */
    CHECK(!zb_time_acceptable(false, 0, ZB_TIME_MIN_PLAUSIBLE - 1));
    CHECK(zb_time_acceptable(false, 0, ZB_TIME_MIN_PLAUSIBLE));
    CHECK(!zb_time_acceptable(true, 0x03, 0xFFFFFFFFu));
}

static void retry_backoff(void)
{
    CHECK_EQ(zb_time_next_delay_ms(0, false), 60000u);
    CHECK_EQ(zb_time_next_delay_ms(1, false), 60000u);
    CHECK_EQ(zb_time_next_delay_ms(2, false), 120000u);
    CHECK_EQ(zb_time_next_delay_ms(3, false), 300000u);
    CHECK_EQ(zb_time_next_delay_ms(4, false), 3600000u);
    CHECK_EQ(zb_time_next_delay_ms(255, false), 3600000u);
    CHECK_EQ(zb_time_next_delay_ms(0, true), 3600000u);
    CHECK_EQ(zb_time_next_delay_ms(9, true), 3600000u);
}

int main(void)
{
    RUN(plausibility_and_status);
    RUN(retry_backoff);
    DONE();
}
