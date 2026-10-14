/* Host tests for the Zigbee Time sync helpers. */
#include "core.h"
#include "tinytest.h"
#include "zb_time.h"

#define Y2026 ((U32)26 * 365u * 86400u + 7u * 86400u)   /* ~2026-01-01 since 2000 */

/** ZbTimeAcceptable(): year range check, and TimeStatus must say Master or Synchronized when present. */
static void PlausibilityAndStatus(void)
{
    CHECK(ZbTimeAcceptable(false, 0, Y2026));                          /* no status attr: year check only */
    CHECK(ZbTimeAcceptable(true, ZB_TIME_STATUS_MASTER, Y2026));
    CHECK(ZbTimeAcceptable(true, ZB_TIME_STATUS_SYNCHRONIZED, Y2026));
    CHECK(ZbTimeAcceptable(true, 0x05, Y2026));                        /* zigpy: Master | Master_for_Zone_and_DST */
    CHECK(!ZbTimeAcceptable(true, 0x00, Y2026));                       /* unset clock */
    CHECK(!ZbTimeAcceptable(true, 0x04, Y2026));                       /* only zone/DST master */
    CHECK(!ZbTimeAcceptable(false, 0, 0));
    CHECK(!ZbTimeAcceptable(false, 0, 3600));                          /* 2000-01-01 01:00: never set */
    CHECK(!ZbTimeAcceptable(false, 0, ZB_TIME_MIN_PLAUSIBLE - 1));
    CHECK(ZbTimeAcceptable(false, 0, ZB_TIME_MIN_PLAUSIBLE));
    CHECK(!ZbTimeAcceptable(true, 0x03, 0xFFFFFFFFu));
}

/** ZbTimeNextDelayMs(): 1, 2, 5 min, then hourly while unknown; hourly once known. */
static void RetryBackoff(void)
{
    CHECK_EQ(ZbTimeNextDelayMs(0, false), 60000u);
    CHECK_EQ(ZbTimeNextDelayMs(1, false), 120000u);
    CHECK_EQ(ZbTimeNextDelayMs(2, false), 300000u);
    CHECK_EQ(ZbTimeNextDelayMs(3, false), 3600000u);
    CHECK_EQ(ZbTimeNextDelayMs(4, false), 3600000u);
    CHECK_EQ(ZbTimeNextDelayMs(255, false), 3600000u);
    CHECK_EQ(ZbTimeNextDelayMs(0, true), 3600000u);
    CHECK_EQ(ZbTimeNextDelayMs(9, true), 3600000u);
}

/** Run every test in this file; exit status 1 if any failed (tinytest DONE()). */
int main(void)
{
    RUN(PlausibilityAndStatus);
    RUN(RetryBackoff);
    DONE();
}
