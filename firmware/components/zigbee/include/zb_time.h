/* zb_time.h - pure helpers for the Zigbee Time sync (host-tested in test/host/test_zb_time.c). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>

#define ZB_TIME_STATUS_MASTER        0x01   /* TimeStatus bit 0 */
#define ZB_TIME_STATUS_SYNCHRONIZED  0x02   /* TimeStatus bit 1 */
/* Plausible range, seconds since 2000-01-01 (Zigbee epoch): 2024-01-01 .. 2100-01-01 */
#define ZB_TIME_MIN_PLAUSIBLE 757382400u
#define ZB_TIME_MAX_PLAUSIBLE 3155760000u

/* Accept a time read from the coordinator: the value (local or UTC seconds since 2000)
 * must lie in a plausible year range, and if TimeStatus was returned it must say
 * Master or Synchronized (a coordinator with an unset clock reports neither). */
bool ZbTimeAcceptable(bool haveStatus, U8 status, U32 secs);

/* Delay before the next Time read. `fails` = consecutive attempts without a usable
 * answer, `known` = we already have a time. Known: hourly resync. Unknown:
 * fails 0 -> 1 min, 1 -> 2 min, 2 -> 5 min, 3+ -> hourly. */
U32 ZbTimeNextDelayMs(U8 fails, bool known);
