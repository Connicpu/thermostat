/* Minimal test harness (no dependencies). */
#pragma once
#include <stdio.h>
#include <stdlib.h>
static int tt_fail, tt_run, tt_cur_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); tt_cur_fail = 1; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("  FAIL %s:%d: %s == %ld, expected %s == %ld\n", __FILE__, __LINE__, #a, _a, #b, _b); tt_cur_fail = 1; } } while (0)
#define RUN(t) do { tt_cur_fail = 0; tt_run++; t(); printf("%s %s\n", tt_cur_fail ? "[FAIL]" : "[ ok ]", #t); tt_fail += tt_cur_fail; } while (0)
#define DONE() do { printf("\n%d tests, %d failed\n", tt_run, tt_fail); return tt_fail ? 1 : 0; } while (0)
