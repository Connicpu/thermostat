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

/**
 * Accept a time read from the coordinator. Pure.
 * @param haveStatus  the TimeStatus attribute was returned
 * @param status      TimeStatus bits (only used when haveStatus)
 * @param secs        local or UTC time, seconds since 2000-01-01
 * @return true if secs is in ZB_TIME_MIN_PLAUSIBLE..ZB_TIME_MAX_PLAUSIBLE and, when TimeStatus was
 *         returned, it says Master or Synchronized (a coordinator with an unset clock reports neither)
 */
bool ZbTimeAcceptable(bool haveStatus, U8 status, U32 secs);

/**
 * Delay before the next Time read. Known: hourly resync. Unknown: fails 0 -> 1 min, 1 -> 2 min,
 * 2 -> 5 min, 3+ -> hourly.
 * @param fails  consecutive attempts without a usable answer
 * @param known  a time has already been accepted
 * @return delay in ms
 */
U32 ZbTimeNextDelayMs(U8 fails, bool known);
