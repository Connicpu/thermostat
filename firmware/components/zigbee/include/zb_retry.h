/* zb_retry.h - pure join/rejoin retry policy (host-tested in test/host/test_zb_retry.c).
 *
 * One counter of consecutive failures drives every (re)join attempt:
 * network steering that failed, a lost parent, or being asked to leave.
 * A join only "earns back" a fresh backoff once it has stayed up for
 * ZB_RETRY_STABLE_MS, so a flapping link (joins, drops 30 s later, ...)
 * keeps climbing the backoff instead of hammering the network every 10 s. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define ZB_RETRY_STABLE_MS  (10u * 60u * 1000u)   /* joined this long = healthy again */
#define ZB_RETRY_STALL_MS   (60u * 1000u)          /* Zigbee task silent this long = stalled */

typedef struct {
    uint8_t  fails;      /* consecutive failed attempts / unstable joins */
    bool     joined;
    uint64_t joined_at;  /* ms, valid while joined */
} zb_retry_t;

/* Delay before the next attempt for the current failure count:
 * 10 s, 30 s, 1 min, 2 min, 5 min, then every 15 min (never gives up). */
uint32_t zb_retry_delay_ms(const zb_retry_t *r);
/* Joined (steering success or rejoin): remember when. Does not reset the backoff yet. */
void     zb_retry_joined(zb_retry_t *r, uint64_t now_ms);
/* A join attempt failed. Returns the delay before the next one. */
uint32_t zb_retry_failed(zb_retry_t *r);
/* The link went away (parent lost / left). Resets the backoff if the join had been
 * stable, otherwise counts as a failure. Returns the delay before the rejoin. */
uint32_t zb_retry_lost(zb_retry_t *r, uint64_t now_ms);
/* Zigbee task heartbeat check: true if the stack has not run for ZB_RETRY_STALL_MS. */
bool     zb_retry_stalled(uint64_t last_alive_ms, uint64_t now_ms);
