/* core.h - project-wide basic types (Connie's conventions, see README "Code style").
 * Use these instead of <stdint.h> names / float / double in our own code; vendor
 * (ESP-IDF, esp-zigbee, FreeRTOS) APIs keep their own types at the call site. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if __has_include("esp_timer.h")
#include "esp_timer.h" /* NowMs(); absent in the pure-logic host tests */
#endif

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef float F32;
typedef double F64;

_Static_assert(sizeof(F32) == 4 && sizeof(F64) == 8, "F32/F64 must be IEEE single/double");

/** Returns v clamped into lo..hi (lo <= hi). */
static inline I16 Clamp16(I32 v, I16 lo, I16 hi)
{
    return v < lo ? lo : (v > hi ? hi : (I16)v);
}

#if __has_include("esp_timer.h")
/** Returns monotonic time since boot in ms, from esp_timer. */
static inline U64 NowMs(void)
{
    return (U64)(esp_timer_get_time() / 1000);
}
#endif
