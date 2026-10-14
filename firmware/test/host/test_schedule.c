/* Host tests for the day/night program schedule and the morning heat boost. */
#include "core.h"
#include "tinytest.h"
#include "schedule.h"
#include "control.h"

#define HM(h, m) ((U16)((h) * 60 + (m)))

static CtrlParams s_p;
static SchedCfg s_c;

/** Default control parameters and default schedule. */
static void Reset(void)
{
    s_p = (CtrlParams)CTRL_PARAMS_DEFAULT();
    s_c = (SchedCfg)SCHED_CFG_DEFAULT();
}

/** Default schedule values, and the default morning boost window is exactly 06:00..07:59. */
static void DefaultsMatchSpec(void)
{
    Reset();
    CHECK_EQ(s_c.dayHeatStart, HM(6, 0));
    CHECK_EQ(s_c.boostMin, 120);
    CHECK_EQ(s_c.boostOffset, 50);
    /* boost window is exactly 06:00..07:59 */
    CHECK(!SchedBoostWindow(&s_c, HM(5, 59)));
    CHECK(SchedBoostWindow(&s_c, HM(6, 0)));
    CHECK(SchedBoostWindow(&s_c, HM(7, 59)));
    CHECK(!SchedBoostWindow(&s_c, HM(8, 0)));
}

/** SchedPeriodAt() for a day period inside one calendar day (06:00..22:00). */
static void PeriodSameDay(void)
{
    /* day 06:00, night 22:00 */
    CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(22, 0), HM(5, 59)), kSchedNight);
    CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(22, 0), HM(6, 0)), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(22, 0), HM(12, 0)), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(22, 0), HM(21, 59)), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(22, 0), HM(22, 0)), kSchedNight);
    CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(22, 0), HM(23, 59)), kSchedNight);
    CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(22, 0), 0), kSchedNight);
}

/**
 * SchedPeriodAt() for periods crossing midnight, equal starts (always day), and exactly two transitions per day.
 */
static void PeriodCrossingMidnight(void)
{
    /* night shift: "day" 20:00 -> 04:00 crosses midnight */
    CHECK_EQ(SchedPeriodAt(HM(20, 0), HM(4, 0), HM(19, 59)), kSchedNight);
    CHECK_EQ(SchedPeriodAt(HM(20, 0), HM(4, 0), HM(20, 0)), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(20, 0), HM(4, 0), HM(23, 59)), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(20, 0), HM(4, 0), 0), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(20, 0), HM(4, 0), HM(3, 59)), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(20, 0), HM(4, 0), HM(4, 0)), kSchedNight);
    /* night starting after midnight (day 07:00, night 01:30): night crosses nothing, day crosses midnight */
    CHECK_EQ(SchedPeriodAt(HM(7, 0), HM(1, 30), HM(0, 30)), kSchedDay);
    CHECK_EQ(SchedPeriodAt(HM(7, 0), HM(1, 30), HM(1, 30)), kSchedNight);
    CHECK_EQ(SchedPeriodAt(HM(7, 0), HM(1, 30), HM(6, 59)), kSchedNight);
    /* equal start times: schedule disabled, always day */
    for (U16 m = 0; m < SCHED_MIN_PER_DAY; m += 7)
        CHECK_EQ(SchedPeriodAt(HM(6, 0), HM(6, 0), m), kSchedDay);
    /* exhaustive: exactly one transition into each period per day */
    U16 starts[][2] = {{HM(6, 0), HM(22, 0)}, {HM(20, 0), HM(4, 0)}, {0, HM(23, 59)}, {HM(23, 59), 0}};
    for (int k = 0; k < 4; k++)
    {
        int flips = 0;
        for (U16 m = 0; m < SCHED_MIN_PER_DAY; m++)
            flips += SchedPeriodAt(starts[k][0], starts[k][1], m) !=
                     SchedPeriodAt(starts[k][0], starts[k][1], (U16)((m + 1) % SCHED_MIN_PER_DAY));
        CHECK_EQ(flips, 2);
    }
}

/** The heating and cooling programs switch periods independently. */
static void HeatAndCoolProgramsIndependent(void)
{
    Reset();
    /* 07:00: heat program is day (06:00..), cool program is still night (day starts 08:00) */
    CHECK_EQ(SchedCurrent(&s_c, kCtrlModeHeat, true, HM(7, 0)), kSchedDay);
    CHECK_EQ(SchedCurrent(&s_c, kCtrlModeCool, true, HM(7, 0)), kSchedNight);
    I16 h, c;
    SchedProgramSp(&s_c, true, HM(7, 0), &h, &c);
    CHECK_EQ(h, s_c.dayHeat);
    CHECK_EQ(c, s_c.nightCool);
    SchedProgramSp(&s_c, true, HM(23, 0), &h, &c);
    CHECK_EQ(h, s_c.nightHeat);
    CHECK_EQ(c, s_c.nightCool);
}

/**
 * Morning boost only in HEAT with a known time; disabled by 0 minutes or 0 offset; window across midnight; clamped to maxSp.
 */
static void BoostWindowAndMode(void)
{
    Reset();
    CtrlSettings s = {.mode = kCtrlModeHeat, .fan = kCtrlFanAuto};
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(6, 30), &s), kSchedStateDayBoost);
    CHECK_EQ(s.heatSp, s_c.dayHeat + 50);
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(8, 0), &s), kSchedStateDay);
    CHECK_EQ(s.heatSp, s_c.dayHeat);
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(5, 0), &s), kSchedStateNight);
    CHECK_EQ(s.heatSp, s_c.nightHeat);
    /* cool mode: never boosted (heat SP untouched, cool SP from the cool program) */
    s.mode = kCtrlModeCool;
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(6, 30), &s), kSchedStateNight);
    CHECK_EQ(s.heatSp, s_c.dayHeat);
    CHECK_EQ(s.coolSp, s_c.nightCool);
    s.mode = kCtrlModeOff;
    SchedApply(&s_p, &s_c, true, HM(6, 30), &s);
    CHECK_EQ(s.heatSp, s_c.dayHeat);
    /* time unknown: day setpoints, no boost */
    s.mode = kCtrlModeHeat;
    CHECK_EQ(SchedApply(&s_p, &s_c, false, HM(6, 30), &s), kSchedStateTimeUnknown);
    CHECK_EQ(s.heatSp, s_c.dayHeat);
    CHECK_EQ(s.coolSp, s_c.dayCool);
    /* disabled by duration 0 or offset 0 */
    s_c.boostMin = 0;
    CHECK(!SchedBoostWindow(&s_c, HM(6, 0)));
    s_c.boostMin = 120;
    s_c.boostOffset = 0;
    CHECK(!SchedBoostWindow(&s_c, HM(6, 0)));
    /* window crossing midnight: day heat start 23:30, 60 min */
    s_c.boostOffset = 50;
    s_c.dayHeatStart = HM(23, 30);
    s_c.boostMin = 60;
    CHECK(SchedBoostWindow(&s_c, HM(23, 45)));
    CHECK(SchedBoostWindow(&s_c, HM(0, 29)));
    CHECK(!SchedBoostWindow(&s_c, HM(0, 30)));
    CHECK(!SchedBoostWindow(&s_c, HM(23, 29)));
    /* boost clamped to maxSp */
    Reset();
    s_c.dayHeat = s_p.maxSp;
    s.mode = kCtrlModeHeat;
    SchedApply(&s_p, &s_c, true, HM(6, 0), &s);
    CHECK_EQ(s.heatSp, s_p.maxSp);
}

/**
 * SchedEditCurrent() changes only the current period of the right program (DAY when the time is unknown), clamps, and edits the base value during a boost.
 */
static void EditChangesCurrentPeriodOnly(void)
{
    Reset();
    SchedCfg before = s_c;
    /* 12:00 heating: day */
    CHECK(SchedEditCurrent(&s_p, &s_c, true, true, HM(12, 0), 2150));
    CHECK_EQ(s_c.dayHeat, 2150);
    CHECK_EQ(s_c.nightHeat, before.nightHeat);
    /* 23:00 heating: night */
    CHECK(SchedEditCurrent(&s_p, &s_c, true, true, HM(23, 0), 1750));
    CHECK_EQ(s_c.nightHeat, 1750);
    CHECK_EQ(s_c.dayHeat, 2150);
    /* 07:00 cooling: night (cool day starts 08:00) */
    CHECK(SchedEditCurrent(&s_p, &s_c, false, true, HM(7, 0), 2300));
    CHECK_EQ(s_c.nightCool, 2300);
    CHECK_EQ(s_c.dayCool, before.dayCool);
    /* time unknown: day period */
    CHECK(SchedEditCurrent(&s_p, &s_c, false, false, HM(7, 0), 2500));
    CHECK_EQ(s_c.dayCool, 2500);
    CHECK_EQ(s_c.nightCool, 2300);
    /* unchanged value -> false; clamped */
    CHECK(!SchedEditCurrent(&s_p, &s_c, false, false, 0, 2500));
    CHECK(SchedEditCurrent(&s_p, &s_c, true, true, HM(12, 0), 9999));
    CHECK_EQ(s_c.dayHeat, s_p.maxSp);
    /* editing during the morning boost changes the base (day) value, not boost */
    Reset();
    CHECK(SchedEditCurrent(&s_p, &s_c, true, true, HM(6, 30), 2100));
    CHECK_EQ(s_c.dayHeat, 2100);
    CHECK_EQ(s_c.boostOffset, 50);
    CtrlSettings s = {.mode = kCtrlModeHeat};
    SchedApply(&s_p, &s_c, true, HM(6, 30), &s);
    CHECK_EQ(s.heatSp, 2150);
    /* across midnight: day 20:00 -> 04:00, edit at 01:00 lands in day */
    Reset();
    s_c.dayHeatStart = HM(20, 0);
    s_c.nightHeatStart = HM(4, 0);
    CHECK(SchedEditCurrent(&s_p, &s_c, true, true, HM(1, 0), 1900));
    CHECK_EQ(s_c.dayHeat, 1900);
    CHECK_EQ(s_c.nightHeat, 1800);
}

/** The boost window is cut short at nightHeatStart, also across midnight. */
static void BoostClampedToNightStart(void)
{
    Reset();
    /* day heat 06:00, night 07:00, boost 120 min -> boost only 06:00..06:59 */
    s_c.nightHeatStart = HM(7, 0);
    CHECK_EQ(SchedBoostLength(&s_c), 60);
    CHECK(SchedBoostWindow(&s_c, HM(6, 59)));
    CHECK(!SchedBoostWindow(&s_c, HM(7, 0)));
    CHECK(!SchedBoostWindow(&s_c, HM(7, 59)));
    CtrlSettings s = {.mode = kCtrlModeHeat};
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(7, 30), &s), kSchedStateNight);
    CHECK_EQ(s.heatSp, s_c.nightHeat); /* night setpoint, not boosted */
    /* crossing midnight: day 23:00, night 00:30, boost 120 -> 23:00..00:29 */
    s_c.dayHeatStart = HM(23, 0);
    s_c.nightHeatStart = HM(0, 30);
    CHECK_EQ(SchedBoostLength(&s_c), 90);
    CHECK(SchedBoostWindow(&s_c, HM(0, 29)));
    CHECK(!SchedBoostWindow(&s_c, HM(0, 30)));
    /* long day: full duration */
    Reset();
    CHECK_EQ(SchedBoostLength(&s_c), 120);
}

/** With equal day/night heat starts (program off) there is no morning boost. */
static void BoostOffWhenStartsEqual(void)
{
    Reset();
    s_c.nightHeatStart = s_c.dayHeatStart;
    CHECK_EQ(SchedBoostLength(&s_c), 0);
    for (U16 m = 0; m < SCHED_MIN_PER_DAY; m++)
        CHECK(!SchedBoostWindow(&s_c, m));
    CtrlSettings s = {.mode = kCtrlModeHeat};
    CHECK_EQ(SchedApply(&s_p, &s_c, true, s_c.dayHeatStart, &s), kSchedStateDay);
    CHECK_EQ(s.heatSp, s_c.dayHeat);
}

/**
 * In OFF the reported period follows the heating program, with no morning boost; COOL follows the cooling program.
 */
static void OffModeReportsHeatingProgram(void)
{
    Reset();
    CtrlSettings s = {.mode = kCtrlModeOff};
    /* 07:00: heating program = day, cooling program = night (starts 08:00) */
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(7, 0), &s), kSchedStateDay);
    CHECK_EQ(s.heatSp, s_c.dayHeat); /* and no morning boost in OFF */
    s.mode = kCtrlModeCool;
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(7, 0), &s), kSchedStateNight);
    s.mode = kCtrlModeOff;
    CHECK_EQ(SchedApply(&s_p, &s_c, true, HM(23, 0), &s), kSchedStateNight);
}

/** SchedSanitize() clamps start times, temperatures, boost minutes and offset. */
static void SanitizeRanges(void)
{
    Reset();
    s_c.dayCoolStart = 5000;
    s_c.nightHeatStart = 1440;
    s_c.dayHeat = -100;
    s_c.nightCool = 9000;
    s_c.boostMin = 999;
    s_c.boostOffset = -5;
    SchedSanitize(&s_p, &s_c);
    CHECK_EQ(s_c.dayCoolStart, 1439);
    CHECK_EQ(s_c.nightHeatStart, 1439);
    CHECK_EQ(s_c.dayHeat, s_p.minSp);
    CHECK_EQ(s_c.nightCool, s_p.maxSp);
    CHECK_EQ(s_c.boostMin, SCHED_BOOST_MAX_MIN);
    CHECK_EQ(s_c.boostOffset, 0);
    s_c.boostOffset = 1000;
    SchedSanitize(&s_p, &s_c);
    CHECK_EQ(s_c.boostOffset, SCHED_BOOST_OFS_MAX);
}

/**
 * End to end with the controller: night setpoint at 05:59, the 06:00 boost starts the furnace, and 08:00 drops back to the day setpoint.
 */
static void ControllerFollowsSchedule(void)
{
    Reset();
    s_p.hysteresis = 30;
    Ctrl c;
    CtrlInit(&c, &s_p, 0);
    U64 now = 0;
    CtrlOutput o = {0};
    CtrlSettings base = {.mode = kCtrlModeHeat, .fan = kCtrlFanAuto};
    for (U16 minute = HM(5, 50); minute < HM(8, 10); minute++)
    {
        for (int k = 0; k < 60; k++)
        {
            now += 1000;
            /* 20.1: no call at night (18.0); calls under the boost (20.5-0.3); after 08:00
             * 20.35 is above day 20.0+0.3 (off) but would still be heating under the boost */
            I16 room = minute >= HM(8, 0) ? 2035 : 2010;
            CtrlInput in = {.valid = true, .temp = room};
            CtrlFeedSensor(&c, &in, now);
            CtrlSettings eff = base;
            SchedApply(&s_p, &s_c, true, minute, &eff);
            o = CtrlStep(&c, &eff, now);
        }
        if (minute == HM(5, 59))
            CHECK(!o.w); /* night 18.0 */
        if (minute == HM(6, 5))
            CHECK(o.w); /* boosted 20.5 */
        if (minute == HM(7, 59))
            CHECK(o.w);
        if (minute == HM(8, 5))
            CHECK(!o.w); /* back to day 20.0: satisfied */
    }
}

/** Run every schedule test; exit status 1 if any failed (tinytest DONE()). */
int main(void)
{
    RUN(DefaultsMatchSpec);
    RUN(PeriodSameDay);
    RUN(PeriodCrossingMidnight);
    RUN(HeatAndCoolProgramsIndependent);
    RUN(BoostWindowAndMode);
    RUN(EditChangesCurrentPeriodOnly);
    RUN(BoostClampedToNightStart);
    RUN(BoostOffWhenStartsEqual);
    RUN(OffModeReportsHeatingProgram);
    RUN(SanitizeRanges);
    RUN(ControllerFollowsSchedule);
    DONE();
}
