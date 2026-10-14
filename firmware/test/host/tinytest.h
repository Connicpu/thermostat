/* Minimal test harness (no dependencies). */
#pragma once
#include <stdio.h>
#include <stdlib.h>
static int s_ttFail, s_ttRun, s_ttCurFail;
#define CHECK(c)                                                  \
    do                                                            \
    {                                                             \
        if (!(c))                                                 \
        {                                                         \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
            s_ttCurFail = 1;                                      \
        }                                                         \
    } while (0)
#define CHECK_EQ(a, b)                                                                                   \
    do                                                                                                   \
    {                                                                                                    \
        long _a = (long)(a), _b = (long)(b);                                                             \
        if (_a != _b)                                                                                    \
        {                                                                                                \
            printf("  FAIL %s:%d: %s == %ld, expected %s == %ld\n", __FILE__, __LINE__, #a, _a, #b, _b); \
            s_ttCurFail = 1;                                                                             \
        }                                                                                                \
    } while (0)
#define RUN(t)                                                    \
    do                                                            \
    {                                                             \
        s_ttCurFail = 0;                                          \
        s_ttRun++;                                                \
        t();                                                      \
        printf("%s %s\n", s_ttCurFail ? "[FAIL]" : "[ ok ]", #t); \
        s_ttFail += s_ttCurFail;                                  \
    } while (0)
#define DONE()                                                \
    do                                                        \
    {                                                         \
        printf("\n%d tests, %d failed\n", s_ttRun, s_ttFail); \
        return s_ttFail ? 1 : 0;                              \
    } while (0)
