/* display_policy.h - pure refresh decisions for the e-paper task.
 * Host-tested. The display task is the only caller on device; it does not add a second
 * clock-only refresh path. */
#pragma once
#include "core.h"
#include <stdbool.h>

/* Background slot. One interval for the clock and for sensor/link/running changes. */
#define DISPLAY_BG_UNKNOWN_MS   (3u * 60u * 1000u) /* clock not synced yet */
#define DISPLAY_BG_KNOWN_MS     (60u * 1000u)      /* one local minute */
#define DISPLAY_BG_CAP_MS       (90u * 1000u)      /* stuck phase: still flush a dirty model */
#define DISPLAY_FAIL_BACKOFF_MS (5u * 1000u)       /* retry a failed show; not every 200 ms loop */

/* De-ghost. A full flash runs when BOTH the gap and the partial count have been reached
 * (the later of the two), or when the 6 h cap has elapsed. Once-a-minute partials therefore
 * do not full-flash every 20 updates. Good Display's note of a full refresh after 5 partials
 * is intentionally not followed; see the firmware README. */
#define DISPLAY_FULL_GAP_MS   (3ULL * 3600ULL * 1000ULL)
#define DISPLAY_FULL_PARTIALS 40
#define DISPLAY_FULL_CAP_MS   (6ULL * 3600ULL * 1000ULL)

/* App-task health check. The display task must loop at least this often. */
#define DISPLAY_STALL_MS (30u * 1000u)

/**
 * Local minute to draw at `nowMs`.
 *
 * `localMinute` and `minuteStartMs` are the sample in the display model (minutes after local
 * midnight, and the monotonic ms that minute began). Whole minutes elapsed since `minuteStartMs`
 * are added, so a refresh that lands on the rollover shows the new minute even if the app task
 * has not posted it yet. `minuteStartMs` of 0 means the minute began before boot: no
 * extrapolation, the posted minute is used as-is. The result wraps at midnight.
 *
 * Returns minutes after local midnight, 0..1439.
 */
U16 DisplayClockMinute(U16 localMinute, U64 minuteStartMs, U64 nowMs);

/**
 * Whether a non-urgent background refresh is due. Urgent button refreshes are decided by the
 * display task and are not this function.
 *
 * `first` is no successful refresh yet (or the previous one failed and must be retried in full);
 * a first refresh runs only when `dirty` is set, so boot does not paint an empty model.
 * `clockKnown` selects the interval: unknown keeps a 3 minute slot and requires `dirty`; known
 * refreshes when DisplayClockMinute() differs from `lastPaintedMinute` (the next local-minute
 * rollover, or a step such as DST), even if the model is not otherwise dirty. `lastPaintedMinute`
 * is -1 until a minute has actually been shown. A failed show does not record it.
 * `first && dirty` is due immediately if `lastRefreshMs` is 0 (no attempt yet) and otherwise only
 * after DISPLAY_FAIL_BACKOFF_MS. That gate applies whether or not the clock is known, so a failed
 * show retries in a few seconds instead of the next rollover or the next loop. When the clock is
 * known it also covers a painted minute that already matches.
 * Sensor changes while the minute is unchanged ride along to that rollover. If the extrapolated
 * minute never advances, a dirty model is still flushed after DISPLAY_BG_CAP_MS.
 *
 * Returns true if the display task should render and push a frame.
 */
bool DisplayBackgroundDue(
    bool first,
    bool clockKnown,
    bool dirty,
    U64 nowMs,
    U64 lastRefreshMs,
    U16 localMinute,
    U64 minuteStartMs,
    int lastPaintedMinute);

/**
 * Whether this refresh should be a full (de-ghosting) flash.
 *
 * `first` covers boot and a retry after a failed refresh. `epochChanged` is DisplayModel.fullEpoch
 * (leaving the menu). Otherwise a full refresh waits until DISPLAY_FULL_GAP_MS has passed AND at
 * least DISPLAY_FULL_PARTIALS partials have been shown since `lastFullMs`, or until
 * DISPLAY_FULL_CAP_MS has passed regardless of the partial count.
 *
 * Returns true for a full refresh, false for a partial.
 */
bool DisplayRefreshIsFull(bool first, bool epochChanged, int partials, U64 nowMs, U64 lastFullMs);

/**
 * True when the display task has been silent for DISPLAY_STALL_MS.
 *
 * `aliveMs` is the display task's last loop time and `nowMs` is the app task's sample from the
 * start of its loop. SensorRead() runs after that sample, and a failing read takes long enough
 * for the display task to stamp its heartbeat in between, so `aliveMs` can be ahead of `nowMs`.
 * Unsigned subtraction would wrap and look like a stall of millions of milliseconds. A heartbeat
 * in the future is not a stall: the gap counts only when nowMs is strictly later than aliveMs and
 * the difference is at least DISPLAY_STALL_MS.
 *
 * Returns true when the display task should be reported stalled.
 */
bool DisplayHeartbeatStalled(U64 aliveMs, U64 nowMs);
