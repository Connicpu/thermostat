/* Host unit tests for the control logic and the output interlock. */
#include "core.h"
#include "tinytest.h"
#include "control.h"
#include "hvac_interlock.h"
#include <string.h>

#define S(x) ((U64)(x) * 1000u)
#define MIN(x) S((x) * 60u)

static Ctrl s_c;
static CtrlParams s_p;
static CtrlSettings s_st;
static U64 s_now;
static CtrlOutput s_out;

static void Setup(CtrlMode mode, I16 heat, I16 cool)
{
    s_p = (CtrlParams)CTRL_PARAMS_DEFAULT();
    s_p.hysteresis = 30;                  /* legacy tests were written for 0.3 °C */
    s_now = 0;
    CtrlInit(&s_c, &s_p, s_now);
    s_st = (CtrlSettings){.mode = mode, .fan = kCtrlFanAuto, .heatSp = heat, .coolSp = cool};
}

/* advance `ms` in 1 s steps feeding temperature `t` (or an invalid read) */
static void Run(U64 ms, I16 t, bool valid)
{
    U64 end = s_now + ms;
    while (s_now < end) {
        s_now += 1000;
        CtrlInput in = {.valid = valid, .temp = t};
        CtrlFeedSensor(&s_c, &in, s_now);
        s_out = CtrlStep(&s_c, &s_st, s_now);
        if (s_out.w && s_out.y) { printf("  W and Y together at t=%llu\n", (unsigned long long)s_now); s_ttCurFail = 1; }
    }
}

static void BootFailsafeUntilGoodSamples(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    s_out = CtrlStep(&s_c, &s_st, 0);
    CHECK(s_out.fault != kCtrlFaultNone);
    CHECK(!s_out.w && !s_out.y && !s_out.g);
    Run(S(2), 1500, true);              /* 2 good samples: still failsafe */
    CHECK(s_out.fault != kCtrlFaultNone);
    CHECK(!s_out.w);
    Run(S(1), 1500, true);              /* 3rd -> recovered */
    CHECK_EQ(s_out.fault, kCtrlFaultNone);
}

static void HeatHysteresis(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(2), 1980, true);            /* 19.8: inside band, never called */
    CHECK(!s_out.w);
    Run(S(5), 1970, true);              /* 19.7 = sp - 0.3 -> on */
    CHECK(s_out.w);
    Run(MIN(5), 2020, true);            /* 20.2: still inside band -> stays on */
    CHECK(s_out.w);
    Run(S(1), 2030, true);              /* 20.3 -> off */
    CHECK(!s_out.w);
    CHECK(!s_out.y);
}

static void HeatMinOnTime(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(2), 1800, true);
    CHECK(s_out.w);
    U64 onAt = s_c.wOnAt;
    Run(S(10), 2200, true);             /* satisfied immediately (e.g. sensor bumped) */
    CHECK(s_out.w);                       /* held by min on-time */
    while (s_out.w && s_now < onAt + MIN(5)) Run(S(1), 2200, true);
    CHECK(!s_out.w);
    CHECK(s_now - onAt >= s_p.heatMinOnMs);
    CHECK(s_now - onAt <= s_p.heatMinOnMs + 1000);
}

static void CompressorMinOffAfterBoot(void)
{
    Setup(kCtrlModeCool, 2000, 2400);
    Run(S(1), 2800, true);
    while (!s_out.y && s_now < MIN(10)) {
        CHECK(!s_out.y);
        Run(S(1), 2800, true);
    }
    CHECK(s_out.y);
    CHECK(s_now >= s_p.compMinOffMs);    /* power-up counts as "just turned off" */
    CHECK(s_out.g);                       /* fan runs with the compressor */
}

static void CompressorMinOffBetweenCycles(void)
{
    Setup(kCtrlModeCool, 2000, 2400);
    Run(MIN(6), 2800, true);
    CHECK(s_out.y);
    Run(MIN(4), 2300, true);            /* satisfied, min on (3 min) elapsed */
    CHECK(!s_out.y);
    U64 offAt = s_c.yOffAt;
    Run(S(5), 2900, true);              /* hot again right away */
    CHECK(!s_out.y);
    CHECK(s_out.waiting);
    while (!s_out.y && s_now < offAt + MIN(10)) Run(S(1), 2900, true);
    CHECK(s_out.y);
    CHECK(s_now - offAt >= s_p.compMinOffMs);
}

static void CompressorMinOnTime(void)
{
    Setup(kCtrlModeCool, 2000, 2400);
    Run(MIN(6), 2800, true);
    CHECK(s_out.y);
    U64 onAt = s_c.yOnAt;
    Run(S(30), 2000, true);
    CHECK(s_out.y);
    while (s_out.y && s_now < onAt + MIN(10)) Run(S(1), 2000, true);
    CHECK(s_now - onAt >= s_p.compMinOnMs);
}

static void FailsafeSensorTimeoutOverridesMinOn(void)
{
    Setup(kCtrlModeCool, 2000, 2400);
    Run(MIN(6), 2800, true);
    CHECK(s_out.y);
    Run(S(29), 0, false);               /* 29 s of failed reads: still running */
    CHECK(s_out.y);
    Run(S(2), 0, false);                /* > 30 s -> failsafe, all off */
    CHECK(!s_out.y && !s_out.w && !s_out.g);
    CHECK_EQ(s_out.fault, kCtrlFaultSensorTimeout);
    /* sensor comes back: after recovery the compressor still waits min-off */
    U64 offAt = s_c.yOffAt;
    Run(S(10), 2800, true);
    CHECK_EQ(s_out.fault, kCtrlFaultNone);
    CHECK(!s_out.y);
    while (!s_out.y && s_now < offAt + MIN(10)) Run(S(1), 2800, true);
    CHECK(s_now - offAt >= s_p.compMinOffMs);
}

static void FailsafeOutOfRangeIsImmediate(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(2), 1800, true);
    CHECK(s_out.w);
    Run(S(1), 6000, true);              /* 60 °C: sensor shorted / on fire */
    CHECK(!s_out.w && !s_out.g && !s_out.y);
    CHECK_EQ(s_out.fault, kCtrlFaultTempRange);
    Run(S(1), -1000, true);             /* -10 °C also implausible */
    CHECK_EQ(s_out.fault, kCtrlFaultTempRange);
    CHECK(!s_out.w);
    Run(S(1), 1800, true);              /* one good sample is not enough */
    CHECK(s_out.fault != kCtrlFaultNone);
    Run(S(5), 1800, true);
    CHECK_EQ(s_out.fault, kCtrlFaultNone);
}

static void FailsafeKillsFanOnAndRunon(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    s_st.fan = kCtrlFanOn;
    Run(S(5), 2000, true);
    CHECK(s_out.g);
    Run(S(1), 9000, true);
    CHECK(!s_out.g);
}

static void ModeOffIsImmediate(void)
{
    Setup(kCtrlModeCool, 2000, 2400);
    Run(MIN(6), 2800, true);
    CHECK(s_out.y);
    s_st.mode = kCtrlModeOff;
    Run(S(1), 2800, true);              /* user OFF beats min on-time */
    CHECK(!s_out.y && !s_out.w);
}

static void FanRunonAfterHeat(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(2), 1800, true);
    CHECK(s_out.w);
    CHECK(!s_out.g);                      /* gas furnace: board runs its own blower */
    while (s_out.w && s_now < MIN(10)) Run(S(1), 2100, true);
    CHECK(!s_out.w);
    CHECK(s_out.g);                       /* run-on */
    U64 offAt = s_c.wOffAt;
    while (s_out.g && s_now < offAt + MIN(5)) Run(S(1), 2100, true);
    CHECK(!s_out.g);
    CHECK(s_now - offAt >= s_p.fanRunonHeatMs);
    CHECK(s_now - offAt <= s_p.fanRunonHeatMs + 1000);
}

static void FanOnMode(void)
{
    Setup(kCtrlModeOff, 2000, 2400);
    s_st.fan = kCtrlFanOn;
    Run(S(5), 2100, true);
    CHECK(s_out.g && !s_out.w && !s_out.y);
    s_st.fan = kCtrlFanAuto;
    Run(S(1), 2100, true);
    CHECK(!s_out.g);
}

static void HeatToCoolChangeoverDelay(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(2), 1800, true);
    CHECK(s_out.w);
    Run(MIN(3), 2600, true);            /* house overshoots */
    CHECK(!s_out.w);
    U64 wOff = s_c.wOffAt;
    s_st.mode = kCtrlModeCool;           /* user switches to cooling right away */
    CHECK(!s_out.y);
    while (!s_out.y && s_now < wOff + MIN(10)) { CHECK(!s_out.w); Run(S(1), 2600, true); }
    CHECK(s_out.y);
    CHECK(s_now - wOff >= s_p.changeoverMs);
}

static void HeatToCoolModeSwitchNeverOverlaps(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(10), 1800, true);
    CHECK(s_out.w);
    s_st.mode = kCtrlModeCool;
    s_st.heatSp = 1500;
    s_st.coolSp = 1700;                  /* cool demand is immediate */
    Run(S(1), 1800, true);
    CHECK(!s_out.w);
    CHECK(!s_out.y);                      /* changeover delay */
    Run(MIN(6), 1800, true);
    CHECK(s_out.y);
}

static void SanitizeSettings(void)
{
    s_p = (CtrlParams)CTRL_PARAMS_DEFAULT();
    CtrlSettings s = {.mode = 7, .fan = 9, .heatSp = 2300, .coolSp = 2350};
    CtrlSanitizeSettings(&s_p, &s, true);
    CHECK_EQ(s.mode, kCtrlModeOff);
    CHECK_EQ(s.fan, kCtrlFanAuto);
    CHECK_EQ(s.heatSp, 2300);          /* no deadband any more: setpoints are independent */
    CHECK_EQ(s.coolSp, 2350);
    s = (CtrlSettings){.mode = 0x01 /* old AUTO */, .fan = kCtrlFanOn, .heatSp = 2400, .coolSp = 2200};
    CtrlSanitizeSettings(&s_p, &s, false);
    CHECK_EQ(s.mode, kCtrlModeOff);
    CHECK_EQ(s.heatSp, 2400);
    CHECK_EQ(s.coolSp, 2200);
    s = (CtrlSettings){.mode = kCtrlModeHeat, .fan = kCtrlFanAuto, .heatSp = -500, .coolSp = 9000};
    CtrlSanitizeSettings(&s_p, &s, true);
    CHECK_EQ(s.heatSp, s_p.minSp);
    CHECK_EQ(s.coolSp, s_p.maxSp);
}

static void ModeCycle(void)
{
    CHECK_EQ(CtrlNextMode(kCtrlModeOff), kCtrlModeHeat);
    CHECK_EQ(CtrlNextMode(kCtrlModeHeat), kCtrlModeCool);
    CHECK_EQ(CtrlNextMode(kCtrlModeCool), kCtrlModeOff);
    CHECK(!CtrlModeValid(0x01));      /* AUTO was removed */
    CHECK(!CtrlModeValid(0x02));
    CHECK(CtrlModeValid(kCtrlModeOff) && CtrlModeValid(kCtrlModeHeat) && CtrlModeValid(kCtrlModeCool));
}

static void InterlockRule(void)
{
    bool f;
    HvacState r = HvacInterlock((HvacState){.w = true, .y = true, .g = true}, &f);
    CHECK(f);
    CHECK(!r.w && !r.y && r.g);
    for (int i = 0; i < 8; i++) {
        HvacState q = {.w = i & 1, .y = (i >> 1) & 1, .g = (i >> 2) & 1};
        r = HvacInterlock(q, &f);
        CHECK(!(r.w && r.y));
        if (!(q.w && q.y)) { CHECK(!f); CHECK(r.w == q.w && r.y == q.y && r.g == q.g); }
    }
}

/* Random long-running soak: random temperatures, setpoints, mode changes and
 * sensor dropouts. Checks the safety invariants on every step. */
static U32 s_rng = 12345;
static U32 Rnd(void) { s_rng = s_rng * 1103515245u + 12345u; return s_rng >> 8; }

static void FuzzSafetyInvariants(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    I16 t = 2200;
    bool prevY = false, prevW = false;
    U64 yOff = 0, yOn = 0, wOn = 0;
    int yStarts = 0, wStarts = 0;
    bool yOffByForce = false;
    for (int i = 0; i < 7 * 24 * 3600; i++) {          /* one simulated week, 1 s steps */
        s_now += 1000;
        if (Rnd() % 600 == 0) s_st.mode = (CtrlMode[]){kCtrlModeOff, kCtrlModeHeat, kCtrlModeCool, kCtrlModeHeat}[Rnd() % 4];
        if (Rnd() % 900 == 0) { s_st.heatSp = 1500 + Rnd() % 1000; s_st.coolSp = 1800 + Rnd() % 1200; }
        if (Rnd() % 1800 == 0) s_st.fan = (Rnd() & 1) ? kCtrlFanOn : kCtrlFanAuto;
        t += (I16)((int)(Rnd() % 21) - 10);
        if (t < 1200) t = 1200;
        if (t > 3200) t = 3200;
        bool valid = Rnd() % 50 != 0;
        I16 meas = t;
        if (Rnd() % 20000 == 0) meas = 8000;              /* rare garbage reading */
        CtrlInput in = {.valid = valid, .temp = meas};
        CtrlFeedSensor(&s_c, &in, s_now);
        s_out = CtrlStep(&s_c, &s_st, s_now);

        CHECK(!(s_out.w && s_out.y));
        if (s_out.fault != kCtrlFaultNone) CHECK(!s_out.w && !s_out.y && !s_out.g);
        if (s_st.mode == kCtrlModeOff) CHECK(!s_out.w && !s_out.y);
        if (s_st.mode == kCtrlModeHeat) CHECK(!s_out.y);
        if (s_st.mode == kCtrlModeCool) CHECK(!s_out.w);
        if (s_out.y && !prevY) {                          /* compressor start */
            CHECK(s_now - yOff >= s_p.compMinOffMs);
            yOn = s_now; yStarts++;
        }
        if (!s_out.y && prevY) {                          /* compressor stop */
            bool forced = s_out.fault != kCtrlFaultNone || s_st.mode == kCtrlModeOff || s_st.mode == kCtrlModeHeat;
            if (!forced) CHECK(s_now - yOn >= s_p.compMinOnMs);
            yOff = s_now; yOffByForce = forced;
        }
        if (s_out.w && !prevW) { wOn = s_now; wStarts++; }
        (void)wOn; (void)yOffByForce;
        prevY = s_out.y; prevW = s_out.w;
        if (s_ttCurFail) { printf("  at step %d\n", i); return; }
    }
    printf("  (week soak: %d compressor starts, %d heat starts)\n", yStarts, wStarts);
    CHECK(yStarts > 5 && wStarts > 5);
}


/* ---------------------------- boost ---------------------------- */

static void BoostTargetHeatReachesAndReturns(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(2), 2000, true);                 /* satisfied at normal SP */
    CHECK(!s_out.w);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirAuto, 2250, 0, s_now), kBoostOk);
    CtrlBoostStatus b = CtrlGetBoostStatus(&s_c, s_now);
    CHECK_EQ(b.kind, kBoostToTarget);
    CHECK_EQ(b.dir, kCtrlModeHeat);
    Run(S(2), 2000, true);
    CHECK(s_out.w);
    Run(MIN(3), 2240, true);                 /* still below target */
    CHECK(s_out.w);
    Run(S(1), 2250, true);                   /* reached */
    b = CtrlGetBoostStatus(&s_c, s_now);
    CHECK_EQ(b.kind, kBoostNone);
    CHECK_EQ(b.lastEnd, kBoostEndReached);
    CHECK(!s_out.w);                           /* back to normal: 22.5 >> 20.0 */
}

static void BoostTargetCoolAutoDirection(void)
{
    Setup(kCtrlModeOff, 2000, 2400);
    Run(MIN(6), 2600, true);
    CHECK(!s_out.y);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirAuto, 2300, 0, s_now), kBoostOk);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).dir, kCtrlModeCool);
    Run(S(1), 2600, true);
    CHECK(s_out.y && s_out.g && !s_out.w);
    Run(MIN(1), 2290, true);                 /* reached after 1 min: min on-time still holds Y */
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).kind, kBoostNone);
    CHECK(s_out.y);
    U64 onAt = s_c.yOnAt;
    while (s_out.y && s_now < onAt + MIN(10)) Run(S(1), 2290, true);
    CHECK(s_now - onAt >= s_p.compMinOnMs);
    CHECK(s_now - onAt <= s_p.compMinOnMs + 1000);
}

static void BoostTargetRejectsBadRequests(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirAuto, 2200, 0, s_now), kBoostErrFault); /* no reading yet */
    Run(S(5), 2200, true);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirAuto, 2210, 0, s_now), kBoostErrAtTarget);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirHeat, 2100, 0, s_now), kBoostErrAtTarget);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirCool, 2300, 0, s_now), kBoostErrAtTarget);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 0, s_now), kBoostErrArg);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 121, s_now), kBoostErrArg);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, (CtrlBoostKind)9, kBoostDirHeat, 0, 5, s_now), kBoostErrArg);
    /* target is clamped to the setpoint range */
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirHeat, 9000, 0, s_now), kBoostOk);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).sp, s_p.maxSp);
    /* timed AUTO with nothing to do (in deadband, mode OFF) */
    s_st.mode = kCtrlModeOff;
    CtrlBoostCancel(&s_c);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirAuto, 0, 10, s_now), kBoostErrNoDirection);
}

static void BoostTargetGivesUpAfterMax(void)
{
    Setup(kCtrlModeOff, 2000, 2400);
    Run(S(5), 1800, true);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirHeat, 2500, 0, s_now), kBoostOk);
    Run(MIN(BOOST_TARGET_MAX_MIN) - S(5), 1800, true);
    CHECK(s_out.w);
    Run(S(10), 1800, true);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).lastEnd, kBoostEndExpired);
    CHECK(!s_out.w);                           /* mode OFF again */
}

static void BoostTimedRunsNMinutesAndEnds(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(2), 2100, true);                 /* above SP: normal would not heat */
    CHECK(!s_out.w);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirAuto, 0, 15, s_now), kBoostOk);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).dir, kCtrlModeHeat);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).remainingS, 15 * 60);
    Run(S(1), 2300, true);
    CHECK(s_out.w);                            /* forced call despite 23 > 20 */
    Run(MIN(10), 2300, true);
    CHECK(s_out.w);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).remainingS, 15 * 60 - 601);
    Run(MIN(5), 2300, true);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).kind, kBoostNone);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).lastEnd, kBoostEndExpired);
    CHECK(!s_out.w);
}

static void BoostTimedCoolRespectsCompressorMinOff(void)
{
    Setup(kCtrlModeCool, 2000, 2400);
    Run(MIN(6), 2800, true);
    CHECK(s_out.y);
    Run(MIN(4), 2300, true);
    CHECK(!s_out.y);
    U64 offAt = s_c.yOffAt;
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirCool, 0, 30, s_now), kBoostOk);
    Run(S(1), 2300, true);
    CHECK(!s_out.y);                           /* boost cannot bypass the 5 min off-time */
    CHECK(s_out.waiting);
    while (!s_out.y && s_now < offAt + MIN(10)) Run(S(1), 2300, true);
    CHECK(s_now - offAt >= s_p.compMinOffMs);
}

static void BoostCancelReturnsToNormal(void)
{
    Setup(kCtrlModeOff, 2000, 2400);
    Run(S(5), 1900, true);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 60, s_now), kBoostOk);
    Run(MIN(5), 1900, true);
    CHECK(s_out.w);
    CtrlBoostCancel(&s_c);
    Run(S(1), 1900, true);
    CHECK(!s_out.w);                           /* min on (2 min) already satisfied */
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).lastEnd, kBoostEndCancelled);
    CtrlBoostCancel(&s_c);                   /* idempotent */
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).kind, kBoostNone);
}

static void BoostModeChangeCancels(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(S(5), 1900, true);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostToTarget, kBoostDirHeat, 2300, 0, s_now), kBoostOk);
    Run(MIN(3), 1900, true);
    CHECK(s_out.w);
    s_st.mode = kCtrlModeCool;
    Run(S(1), 1900, true);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).lastEnd, kBoostEndModeChanged);
    CHECK(!s_out.w && !s_out.y);
}

static void BoostOppositeDirectionNeverOverlaps(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    Run(MIN(10), 1800, true);
    CHECK(s_out.w);                            /* normal heat running */
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirCool, 0, 30, s_now), kBoostOk);
    for (int i = 0; i < 600; i++) { Run(S(1), 1800, true); CHECK(!(s_out.w && s_out.y)); }
    CHECK(!s_out.w);
    CHECK(s_out.y);                            /* after min-on + changeover */
    CHECK(s_c.yOnAt - s_c.wOffAt >= s_p.changeoverMs);
}

static void BoostFailsafeKillsAndCancels(void)
{
    Setup(kCtrlModeOff, 2000, 2400);
    Run(S(5), 1900, true);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 60, s_now), kBoostOk);
    Run(S(65), 1900, true);                  /* furnace min-off from boot is 60 s */
    CHECK(s_out.w);
    Run(S(1), 7000, true);                   /* implausible reading */
    CHECK(!s_out.w && !s_out.y && !s_out.g);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).kind, kBoostNone);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).lastEnd, kBoostEndFault);
    Run(MIN(5), 1900, true);                 /* sensor recovers: boost does NOT resume */
    CHECK(!s_out.w);
    /* sensor timeout during a boost behaves the same */
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 60, s_now), kBoostOk);
    Run(MIN(2), 1900, true);
    CHECK(s_out.w);
    Run(S(31), 0, false);
    CHECK(!s_out.w);
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).lastEnd, kBoostEndFault);
}

static void BoostTimedComfortLimit(void)
{
    Setup(kCtrlModeOff, 2000, 2400);
    Run(S(5), 3100, true);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 60, s_now), kBoostOk);
    Run(MIN(3), 3100, true);
    CHECK(s_out.w);
    Run(S(1), 3200, true);                   /* hit maxSp: boost stops */
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).lastEnd, kBoostEndLimit);
    CHECK(!s_out.w);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 60, s_now), kBoostErrAtTarget);
}

static void BoostNotPersistedAcrossReboot(void)
{
    Setup(kCtrlModeOff, 2000, 2400);
    Run(S(5), 1900, true);
    CHECK_EQ(CtrlBoostStart(&s_c, &s_st, kBoostTimed, kBoostDirHeat, 0, 60, s_now), kBoostOk);
    Run(MIN(2), 1900, true);
    CHECK(s_out.w);
    /* "reboot": the settings survive (NVS), the controller is re-initialised */
    CtrlInit(&s_c, &s_p, 0);
    s_now = 0;
    CHECK_EQ(CtrlGetBoostStatus(&s_c, s_now).kind, kBoostNone);
    Run(MIN(10), 1900, true);
    CHECK(!s_out.w);                           /* mode OFF: nothing runs */
}

static void FuzzWithBoosts(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    I16 t = 2200;
    bool prevY = false;
    U64 yOff = 0, yOn = 0;
    int boosts = 0;
    for (int i = 0; i < 3 * 24 * 3600; i++) {
        s_now += 1000;
        U32 r = Rnd();
        if (r % 400 == 0) {
            CtrlBoostKind k = (Rnd() & 1) ? kBoostToTarget : kBoostTimed;
            if (CtrlBoostStart(&s_c, &s_st, k, (CtrlBoostDir)(Rnd() % 3), 1500 + Rnd() % 1500, 1 + Rnd() % 120, s_now) == kBoostOk) boosts++;
        }
        if (r % 1500 == 1) CtrlBoostCancel(&s_c);
        if (r % 3000 == 2) s_st.mode = (CtrlMode[]){kCtrlModeOff, kCtrlModeHeat, kCtrlModeCool, kCtrlModeHeat}[Rnd() % 4];
        t += (I16)((int)(Rnd() % 21) - 10);
        if (t < 1200) t = 1200;
        if (t > 3300) t = 3300;
        CtrlInput in = {.valid = Rnd() % 60 != 0, .temp = (Rnd() % 30000 == 0) ? 9000 : t};
        CtrlFeedSensor(&s_c, &in, s_now);
        s_out = CtrlStep(&s_c, &s_st, s_now);
        CHECK(!(s_out.w && s_out.y));
        if (s_out.fault != kCtrlFaultNone) CHECK(!s_out.w && !s_out.y && !s_out.g && CtrlGetBoostStatus(&s_c, s_now).kind == kBoostNone);
        if (s_out.y && !prevY) { CHECK(s_now - yOff >= s_p.compMinOffMs); yOn = s_now; }
        if (!s_out.y && prevY) {
            if (s_out.fault == kCtrlFaultNone && s_st.mode != kCtrlModeOff && s_st.mode != kCtrlModeHeat)
                CHECK(s_now - yOn >= s_p.compMinOnMs);
            yOff = s_now;
        }
        CtrlBoostStatus b = CtrlGetBoostStatus(&s_c, s_now);
        if (b.kind) CHECK(b.remainingS <= BOOST_TARGET_MAX_MIN * 60u);
        prevY = s_out.y;
        if (s_ttCurFail) { printf("  at step %d\n", i); return; }
    }
    printf("  (3-day soak with %d boosts)\n", boosts);
    CHECK(boosts > 20);
}

static void SwingDefaultAndClamp(void)
{
    CtrlParams d = CTRL_PARAMS_DEFAULT();
    CHECK_EQ(d.hysteresis, 50);
    CHECK_EQ(CtrlClampSwing(0), CTRL_SWING_MIN);
    CHECK_EQ(CtrlClampSwing(-50), CTRL_SWING_MIN);
    CHECK_EQ(CtrlClampSwing(500), CTRL_SWING_MAX);
    CHECK_EQ(CtrlClampSwing(70), 70);
    Setup(kCtrlModeHeat, 2000, 2400);
    CtrlSetSwing(&s_c, 1000);
    CHECK_EQ(s_c.p.hysteresis, CTRL_SWING_MAX);
}

static void HeatSwing1c(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    CtrlSetSwing(&s_c, 100);
    Run(MIN(2), 1910, true);            /* 19.1: inside ±1.0 band */
    CHECK(!s_out.w);
    Run(S(5), 1900, true);              /* 19.0 = sp - 1.0 -> on */
    CHECK(s_out.w);
    Run(MIN(5), 2090, true);            /* 20.9: still on */
    CHECK(s_out.w);
    Run(S(1), 2100, true);              /* 21.0 -> off */
    CHECK(!s_out.w);
}

static void CoolSwingMirror(void)
{
    Setup(kCtrlModeCool, 2000, 2400);
    CtrlSetSwing(&s_c, 70);
    Run(MIN(6), 2460, true);            /* 24.6 < 24.7: no call */
    CHECK(!s_out.y);
    Run(S(2), 2470, true);              /* 24.7 = sp + 0.7 -> on (min off long elapsed) */
    CHECK(s_out.y);
    Run(MIN(4), 2340, true);            /* 23.4 > 23.3: stays on */
    CHECK(s_out.y);
    Run(S(1), 2330, true);              /* 23.3 -> off (min on elapsed) */
    CHECK(!s_out.y);
}

static void SwingChangeWhileRunning(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    CtrlSetSwing(&s_c, 100);
    Run(MIN(2), 1890, true);
    CHECK(s_out.w);
    Run(MIN(3), 2040, true);            /* inside wide band */
    CHECK(s_out.w);
    CtrlSetSwing(&s_c, 30);             /* narrow it: 20.4 >= 20.3 -> off */
    Run(S(1), 2040, true);
    CHECK(!s_out.w);
}

static void SwingStillHonoursMinOn(void)
{
    Setup(kCtrlModeHeat, 2000, 2400);
    CtrlSetSwing(&s_c, 10);
    Run(MIN(2), 1990, true);            /* 19.9 = sp - 0.1 -> on */
    CHECK(s_out.w);
    U64 onAt = s_c.wOnAt;
    Run(S(10), 2010, true);             /* satisfied, but min on not elapsed */
    CHECK(s_out.w);
    while (s_out.w && s_now < onAt + MIN(5)) Run(S(1), 2010, true);
    CHECK(s_now - onAt >= s_p.heatMinOnMs);
}

static void WideSwingOverlappingSetpointsNeverOverlap(void)
{
    /* No deadband any more: cool SP below heat SP, 1.0 °C swing, alternating modes */
    Setup(kCtrlModeHeat, 2200, 2000);
    CtrlSetSwing(&s_c, 100);
    for (int k = 0; k < 6; k++) {
        s_st.mode = (k & 1) ? kCtrlModeCool : kCtrlModeHeat;
        for (I16 t = 1800; t <= 2350; t += 5) Run(S(20), t, true);
        for (I16 t = 2350; t >= 1800; t -= 5) Run(S(20), t, true);
    }
    CHECK(!(s_out.w && s_out.y));
}

static void FanOnWithoutDemand(void)
{
    /* Fan ON runs G with no heat/cool demand in every mode, and never drives W/Y */
    CtrlMode modes[] = {kCtrlModeOff, kCtrlModeHeat, kCtrlModeCool};
    for (int i = 0; i < 3; i++) {
        Setup(modes[i], 2000, 2400);
        s_st.fan = kCtrlFanOn;
        Run(MIN(10), 2200, true);      /* between the setpoints: no demand */
        CHECK(s_out.g && !s_out.w && !s_out.y);
        CHECK(!s_out.heatDemand && !s_out.coolDemand);
        s_st.fan = kCtrlFanAuto;
        Run(S(1), 2200, true);
        CHECK(!s_out.g);
    }
}

int main(void)
{
    RUN(BootFailsafeUntilGoodSamples);
    RUN(HeatHysteresis);
    RUN(SwingDefaultAndClamp);
    RUN(HeatSwing1c);
    RUN(CoolSwingMirror);
    RUN(SwingChangeWhileRunning);
    RUN(SwingStillHonoursMinOn);
    RUN(WideSwingOverlappingSetpointsNeverOverlap);
    RUN(FanOnWithoutDemand);
    RUN(HeatMinOnTime);
    RUN(CompressorMinOffAfterBoot);
    RUN(CompressorMinOffBetweenCycles);
    RUN(CompressorMinOnTime);
    RUN(FailsafeSensorTimeoutOverridesMinOn);
    RUN(FailsafeOutOfRangeIsImmediate);
    RUN(FailsafeKillsFanOnAndRunon);
    RUN(ModeOffIsImmediate);
    RUN(FanRunonAfterHeat);
    RUN(FanOnMode);
    RUN(HeatToCoolChangeoverDelay);
    RUN(HeatToCoolModeSwitchNeverOverlaps);
    RUN(SanitizeSettings);
    RUN(ModeCycle);
    RUN(InterlockRule);
    RUN(FuzzSafetyInvariants);
    RUN(BoostTargetHeatReachesAndReturns);
    RUN(BoostTargetCoolAutoDirection);
    RUN(BoostTargetRejectsBadRequests);
    RUN(BoostTargetGivesUpAfterMax);
    RUN(BoostTimedRunsNMinutesAndEnds);
    RUN(BoostTimedCoolRespectsCompressorMinOff);
    RUN(BoostCancelReturnsToNormal);
    RUN(BoostModeChangeCancels);
    RUN(BoostOppositeDirectionNeverOverlaps);
    RUN(BoostFailsafeKillsAndCancels);
    RUN(BoostTimedComfortLimit);
    RUN(BoostNotPersistedAcrossReboot);
    RUN(FuzzWithBoosts);
    DONE();
}
