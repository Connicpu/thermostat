/* zb_time.h - pure helpers for the Zigbee Time sync (host-tested in test/host/test_zb_time.c). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>

#define ZB_TIME_STATUS_MASTER       0x01 /* TimeStatus bit 0 */
#define ZB_TIME_STATUS_SYNCHRONIZED 0x02 /* TimeStatus bit 1 */
/* Plausible range, seconds since 2000-01-01 (Zigbee epoch): 2024-01-01 .. 2100-01-01 */
#define ZB_TIME_MIN_PLAUSIBLE 757382400u
#define ZB_TIME_MAX_PLAUSIBLE 3155760000u

/**
 * Accept a time read from the coordinator. Pure.
 *
 * `haveStatus` is the TimeStatus attribute was returned; `status` is TimeStatus bits (only used
 * when haveStatus); `secs` is local or UTC time, seconds since 2000-01-01.
 *
 * Returns true if secs is in ZB_TIME_MIN_PLAUSIBLE..ZB_TIME_MAX_PLAUSIBLE and, when TimeStatus
 * was returned, it says Master or Synchronized (a coordinator with an unset clock reports
 * neither).
 */
bool ZbTimeAcceptable(bool haveStatus, U8 status, U32 secs);

/**
 * Delay before the next Time read. Known: hourly resync. Unknown: fails 0 -> 1 min, 1 -> 2 min,
 * 2 -> 5 min, 3+ -> hourly.
 *
 * `fails` is consecutive attempts without a usable answer; `known` is a time has already been
 * accepted.
 *
 * Returns delay in ms.
 */
U32 ZbTimeNextDelayMs(U8 fails, bool known);

/**
 * Local Zigbee time from a Time-cluster read. Pure.
 *
 * When `haveLocal` is set, `localS` is the coordinator's LocalTime and is used as-is (TimeZone and
 * DstShift are not applied again). Otherwise, if `haveUtc` is set, the result is utcS + tzS + dstS
 * (the Time + TimeZone + DstShift fallback), wrapped to 32 bits. All three time values are seconds;
 * tzS and dstS are signed seconds.
 *
 * Writes the composed local seconds to `pLocalS`.
 *
 * Returns false when neither LocalTime nor Time was present.
 */
bool ZbTimeComposeLocal(bool haveLocal, U32 localS, bool haveUtc, U32 utcS, I32 tzS, I32 dstS, U32 *pLocalS);

/**
 * Extrapolate a synced local time to `nowMs`. Pure.
 *
 * `syncLocalS` is local seconds (Zigbee UTCTime style, not wrapped to a day) at `syncAtMs`
 * (monotonic ms). `pMsIntoSec` receives the milliseconds into the current second, 0..999, and may
 * be NULL. If `nowMs` is earlier than `syncAtMs`, elapsed time is treated as zero.
 *
 * Returns seconds since local midnight, 0..86399.
 */
U32 ZbTimeExtrapolate(U32 syncLocalS, U64 syncAtMs, U64 nowMs, U16 *pMsIntoSec);

/**
 * Monotonic start of the local minute that contains `nowMs`, from the same sync as
 * ZbTimeExtrapolate(). Pure.
 *
 * `pMinute` receives minutes after local midnight, 0..1439, and may be NULL.
 *
 * Returns that monotonic ms, or 0 when the minute began before monotonic 0 (the clock was already
 * part-way through a minute at boot).
 */
U64 ZbTimeMinuteStart(U32 syncLocalS, U64 syncAtMs, U64 nowMs, U16 *pMinute);
