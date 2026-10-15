/* Host tests for the Zigbee Time sync helpers. */
#include "core.h"
#include "tinytest.h"
#include "zb_time.h"

#define Y2026 ((U32)26 * 365u * 86400u + 7u * 86400u) /* ~2026-01-01 since 2000 */

/** ZbTimeAcceptable(): year range check, and TimeStatus must say Master or Synchronized when present. */
static void PlausibilityAndStatus(void)
{
    CHECK(ZbTimeAcceptable(false, 0, Y2026)); /* no status attr: year check only */
    CHECK(ZbTimeAcceptable(true, ZB_TIME_STATUS_MASTER, Y2026));
    CHECK(ZbTimeAcceptable(true, ZB_TIME_STATUS_SYNCHRONIZED, Y2026));
    CHECK(ZbTimeAcceptable(true, 0x05, Y2026));  /* zigpy: Master | Master_for_Zone_and_DST */
    CHECK(!ZbTimeAcceptable(true, 0x00, Y2026)); /* unset clock */
    CHECK(!ZbTimeAcceptable(true, 0x04, Y2026)); /* only zone/DST master */
    CHECK(!ZbTimeAcceptable(false, 0, 0));
    CHECK(!ZbTimeAcceptable(false, 0, 3600)); /* 2000-01-01 01:00: never set */
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

#define DAY0 ((U32)9497u * 86400u) /* 2026-01-01 00:00, seconds since 2000-01-01 */

/** LocalTime is used as-is. Without it, Time + TimeZone + DstShift is the local time, including a +1 h DST step and a step that crosses midnight. */
static void DstAndLocalTime(void)
{
    U32 local = 0;
    U32 utc0130 = DAY0 + 90u * 60u; /* 01:30 */
    CHECK(ZbTimeComposeLocal(true, utc0130, true, utc0130, 0, 3600, &local));
    CHECK_EQ(local, utc0130); /* LocalTime wins: DstShift is not applied twice */
    CHECK(ZbTimeComposeLocal(false, 0, true, utc0130, 0, 0, &local));
    CHECK_EQ(local % 86400u, 90u * 60u);
    CHECK(ZbTimeComposeLocal(false, 0, true, utc0130, 0, 3600, &local));
    CHECK_EQ(local % 86400u, 150u * 60u); /* 01:30 + 1 h DST -> 02:30 */
    U32 utc2330 = DAY0 + 23u * 3600u + 30u * 60u;
    CHECK(ZbTimeComposeLocal(false, 0, true, utc2330, 0, 3600, &local));
    CHECK_EQ(local % 86400u, 30u * 60u); /* 23:30 + 1 h DST -> 00:30 the next day */
    CHECK_EQ(local / 86400u, utc2330 / 86400u + 1u);
    CHECK(!ZbTimeComposeLocal(false, 0, false, 0, 0, 3600, &local));
    /* negative zone still wraps to a U32 the acceptability check can reject */
    CHECK(ZbTimeComposeLocal(false, 0, true, 1000, -5000, 0, &local));
    CHECK(ZbTimeAcceptable(false, 0, local) == false);
}

/** Extrapolating 23:59:30 by 30 s and by 90 s crosses midnight; the minute start is monotonic. */
static void ExtrapolateAcrossMidnight(void)
{
    U32 sync = 23u * 3600u + 59u * 60u + 30u; /* 23:59:30 at monotonic t = 1000 */
    U16 ms = 99;
    U16 minute = 99;
    CHECK_EQ(ZbTimeExtrapolate(sync, 1000, 1000, &ms), sync);
    CHECK_EQ(ms, 0);
    CHECK_EQ(ZbTimeExtrapolate(sync, 1000, 1000 + 29500, &ms), 86399u); /* 23:59:59.500 */
    CHECK_EQ(ms, 500);
    CHECK_EQ(ZbTimeExtrapolate(sync, 1000, 1000 + 30000, &ms), 0u); /* 00:00:00.000 */
    CHECK_EQ(ms, 0);
    CHECK_EQ(ZbTimeExtrapolate(sync, 1000, 1000 + 30500, &ms), 0u); /* 00:00:00.500 */
    CHECK_EQ(ms, 500);
    CHECK_EQ(ZbTimeExtrapolate(sync, 1000, 1000 + 90000, &ms), 60u); /* 00:01:00 */
    CHECK_EQ(ms, 0);
    /* 30 s before the sync the minute had already started, which is before monotonic 0 */
    CHECK_EQ(ZbTimeMinuteStart(sync, 1000, 1000, &minute), 0u);
    CHECK_EQ(minute, 23 * 60 + 59);
    U64 atMidnight = 1000 + 30000;
    CHECK_EQ(ZbTimeMinuteStart(sync, 1000, atMidnight, &minute), atMidnight);
    CHECK_EQ(minute, 0);
    CHECK_EQ(ZbTimeMinuteStart(sync, 1000, 1000 + 90000, &minute), 1000u + 90000u);
    CHECK_EQ(minute, 1);
    /* a backwards now does not invent a negative elapsed time */
    CHECK_EQ(ZbTimeExtrapolate(sync, 5000, 1000, &ms), sync);
}

/** Run every test in this file; exit status 1 if any failed (tinytest DONE()). */
int main(void)
{
    RUN(PlausibilityAndStatus);
    RUN(RetryBackoff);
    RUN(DstAndLocalTime);
    RUN(ExtrapolateAcrossMidnight);
    DONE();
}
