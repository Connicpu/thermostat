/* Host tests for the day/night program schedule and the morning heat boost. */
#include "tinytest.h"
#include "schedule.h"
#include "control.h"

#define HM(h, m) ((uint16_t)((h) * 60 + (m)))

static ctrl_params_t P;
static sched_cfg_t C;
static void reset(void) { P = (ctrl_params_t)CTRL_PARAMS_DEFAULT(); C = (sched_cfg_t)SCHED_CFG_DEFAULT(); }

static void defaults_match_spec(void)
{
    reset();
    CHECK_EQ(C.day_heat_start, HM(6, 0));
    CHECK_EQ(C.boost_min, 120);
    CHECK_EQ(C.boost_offset, 50);
    /* boost window is exactly 06:00..07:59 */
    CHECK(!sched_boost_window(&C, HM(5, 59)));
    CHECK(sched_boost_window(&C, HM(6, 0)));
    CHECK(sched_boost_window(&C, HM(7, 59)));
    CHECK(!sched_boost_window(&C, HM(8, 0)));
}

static void period_same_day(void)
{
    /* day 06:00, night 22:00 */
    CHECK_EQ(sched_period(HM(6, 0), HM(22, 0), HM(5, 59)), SCHED_NIGHT);
    CHECK_EQ(sched_period(HM(6, 0), HM(22, 0), HM(6, 0)), SCHED_DAY);
    CHECK_EQ(sched_period(HM(6, 0), HM(22, 0), HM(12, 0)), SCHED_DAY);
    CHECK_EQ(sched_period(HM(6, 0), HM(22, 0), HM(21, 59)), SCHED_DAY);
    CHECK_EQ(sched_period(HM(6, 0), HM(22, 0), HM(22, 0)), SCHED_NIGHT);
    CHECK_EQ(sched_period(HM(6, 0), HM(22, 0), HM(23, 59)), SCHED_NIGHT);
    CHECK_EQ(sched_period(HM(6, 0), HM(22, 0), 0), SCHED_NIGHT);
}

static void period_crossing_midnight(void)
{
    /* night shift: "day" 20:00 -> 04:00 crosses midnight */
    CHECK_EQ(sched_period(HM(20, 0), HM(4, 0), HM(19, 59)), SCHED_NIGHT);
    CHECK_EQ(sched_period(HM(20, 0), HM(4, 0), HM(20, 0)), SCHED_DAY);
    CHECK_EQ(sched_period(HM(20, 0), HM(4, 0), HM(23, 59)), SCHED_DAY);
    CHECK_EQ(sched_period(HM(20, 0), HM(4, 0), 0), SCHED_DAY);
    CHECK_EQ(sched_period(HM(20, 0), HM(4, 0), HM(3, 59)), SCHED_DAY);
    CHECK_EQ(sched_period(HM(20, 0), HM(4, 0), HM(4, 0)), SCHED_NIGHT);
    /* night starting after midnight (day 07:00, night 01:30): night crosses nothing, day crosses midnight */
    CHECK_EQ(sched_period(HM(7, 0), HM(1, 30), HM(0, 30)), SCHED_DAY);
    CHECK_EQ(sched_period(HM(7, 0), HM(1, 30), HM(1, 30)), SCHED_NIGHT);
    CHECK_EQ(sched_period(HM(7, 0), HM(1, 30), HM(6, 59)), SCHED_NIGHT);
    /* equal start times: schedule disabled, always day */
    for (uint16_t m = 0; m < SCHED_MIN_PER_DAY; m += 7) CHECK_EQ(sched_period(HM(6, 0), HM(6, 0), m), SCHED_DAY);
    /* exhaustive: exactly one transition into each period per day */
    uint16_t starts[][2] = {{HM(6, 0), HM(22, 0)}, {HM(20, 0), HM(4, 0)}, {0, HM(23, 59)}, {HM(23, 59), 0}};
    for (int k = 0; k < 4; k++) {
        int flips = 0;
        for (uint16_t m = 0; m < SCHED_MIN_PER_DAY; m++)
            flips += sched_period(starts[k][0], starts[k][1], m) != sched_period(starts[k][0], starts[k][1], (uint16_t)((m + 1) % SCHED_MIN_PER_DAY));
        CHECK_EQ(flips, 2);
    }
}

static void heat_and_cool_programs_independent(void)
{
    reset();
    /* 07:00: heat program is day (06:00..), cool program is still night (day starts 08:00) */
    CHECK_EQ(sched_current(&C, CTRL_MODE_HEAT, true, HM(7, 0)), SCHED_DAY);
    CHECK_EQ(sched_current(&C, CTRL_MODE_COOL, true, HM(7, 0)), SCHED_NIGHT);
    int16_t h, c;
    sched_program_sp(&C, true, HM(7, 0), &h, &c);
    CHECK_EQ(h, C.day_heat);
    CHECK_EQ(c, C.night_cool);
    sched_program_sp(&C, true, HM(23, 0), &h, &c);
    CHECK_EQ(h, C.night_heat);
    CHECK_EQ(c, C.night_cool);
}

static void boost_window_and_mode(void)
{
    reset();
    ctrl_settings_t s = {.mode = CTRL_MODE_HEAT, .fan = CTRL_FAN_AUTO};
    CHECK_EQ(sched_apply(&P, &C, true, HM(6, 30), &s), SCHED_STATE_DAY_BOOST);
    CHECK_EQ(s.heat_sp, C.day_heat + 50);
    CHECK_EQ(sched_apply(&P, &C, true, HM(8, 0), &s), SCHED_STATE_DAY);
    CHECK_EQ(s.heat_sp, C.day_heat);
    CHECK_EQ(sched_apply(&P, &C, true, HM(5, 0), &s), SCHED_STATE_NIGHT);
    CHECK_EQ(s.heat_sp, C.night_heat);
    /* cool mode: never boosted (heat SP untouched, cool SP from the cool program) */
    s.mode = CTRL_MODE_COOL;
    CHECK_EQ(sched_apply(&P, &C, true, HM(6, 30), &s), SCHED_STATE_NIGHT);
    CHECK_EQ(s.heat_sp, C.day_heat);
    CHECK_EQ(s.cool_sp, C.night_cool);
    s.mode = CTRL_MODE_OFF;
    sched_apply(&P, &C, true, HM(6, 30), &s);
    CHECK_EQ(s.heat_sp, C.day_heat);
    /* time unknown: day setpoints, no boost */
    s.mode = CTRL_MODE_HEAT;
    CHECK_EQ(sched_apply(&P, &C, false, HM(6, 30), &s), SCHED_STATE_TIME_UNKNOWN);
    CHECK_EQ(s.heat_sp, C.day_heat);
    CHECK_EQ(s.cool_sp, C.day_cool);
    /* disabled by duration 0 or offset 0 */
    C.boost_min = 0;
    CHECK(!sched_boost_window(&C, HM(6, 0)));
    C.boost_min = 120; C.boost_offset = 0;
    CHECK(!sched_boost_window(&C, HM(6, 0)));
    /* window crossing midnight: day heat start 23:30, 60 min */
    C.boost_offset = 50; C.day_heat_start = HM(23, 30); C.boost_min = 60;
    CHECK(sched_boost_window(&C, HM(23, 45)));
    CHECK(sched_boost_window(&C, HM(0, 29)));
    CHECK(!sched_boost_window(&C, HM(0, 30)));
    CHECK(!sched_boost_window(&C, HM(23, 29)));
    /* boost clamped to max_sp */
    reset();
    C.day_heat = P.max_sp;
    s.mode = CTRL_MODE_HEAT;
    sched_apply(&P, &C, true, HM(6, 0), &s);
    CHECK_EQ(s.heat_sp, P.max_sp);
}

static void edit_changes_current_period_only(void)
{
    reset();
    sched_cfg_t before = C;
    /* 12:00 heating: day */
    CHECK(sched_edit_current(&P, &C, true, true, HM(12, 0), 2150));
    CHECK_EQ(C.day_heat, 2150);
    CHECK_EQ(C.night_heat, before.night_heat);
    /* 23:00 heating: night */
    CHECK(sched_edit_current(&P, &C, true, true, HM(23, 0), 1750));
    CHECK_EQ(C.night_heat, 1750);
    CHECK_EQ(C.day_heat, 2150);
    /* 07:00 cooling: night (cool day starts 08:00) */
    CHECK(sched_edit_current(&P, &C, false, true, HM(7, 0), 2300));
    CHECK_EQ(C.night_cool, 2300);
    CHECK_EQ(C.day_cool, before.day_cool);
    /* time unknown: day period */
    CHECK(sched_edit_current(&P, &C, false, false, HM(7, 0), 2500));
    CHECK_EQ(C.day_cool, 2500);
    CHECK_EQ(C.night_cool, 2300);
    /* unchanged value -> false; clamped */
    CHECK(!sched_edit_current(&P, &C, false, false, 0, 2500));
    CHECK(sched_edit_current(&P, &C, true, true, HM(12, 0), 9999));
    CHECK_EQ(C.day_heat, P.max_sp);
    /* editing during the morning boost changes the base (day) value, not boost */
    reset();
    CHECK(sched_edit_current(&P, &C, true, true, HM(6, 30), 2100));
    CHECK_EQ(C.day_heat, 2100);
    CHECK_EQ(C.boost_offset, 50);
    ctrl_settings_t s = {.mode = CTRL_MODE_HEAT};
    sched_apply(&P, &C, true, HM(6, 30), &s);
    CHECK_EQ(s.heat_sp, 2150);
    /* across midnight: day 20:00 -> 04:00, edit at 01:00 lands in day */
    reset();
    C.day_heat_start = HM(20, 0); C.night_heat_start = HM(4, 0);
    CHECK(sched_edit_current(&P, &C, true, true, HM(1, 0), 1900));
    CHECK_EQ(C.day_heat, 1900);
    CHECK_EQ(C.night_heat, 1800);
}

static void sanitize_ranges(void)
{
    reset();
    C.day_cool_start = 5000; C.night_heat_start = 1440;
    C.day_heat = -100; C.night_cool = 9000;
    C.boost_min = 999; C.boost_offset = -5;
    sched_sanitize(&P, &C);
    CHECK_EQ(C.day_cool_start, 1439);
    CHECK_EQ(C.night_heat_start, 1439);
    CHECK_EQ(C.day_heat, P.min_sp);
    CHECK_EQ(C.night_cool, P.max_sp);
    CHECK_EQ(C.boost_min, SCHED_BOOST_MAX_MIN);
    CHECK_EQ(C.boost_offset, 0);
    C.boost_offset = 1000;
    sched_sanitize(&P, &C);
    CHECK_EQ(C.boost_offset, SCHED_BOOST_OFS_MAX);
}

/* End to end with the controller: heat mode at 05:59 holds night SP, the 06:00
 * boost raises it by 0.5 °C and the furnace starts, 08:00 drops back. */
static void controller_follows_schedule(void)
{
    reset();
    P.hysteresis = 30;
    ctrl_t c;
    ctrl_init(&c, &P, 0);
    uint64_t now = 0;
    ctrl_output_t o = {0};
    ctrl_settings_t base = {.mode = CTRL_MODE_HEAT, .fan = CTRL_FAN_AUTO};
    for (uint16_t minute = HM(5, 50); minute < HM(8, 10); minute++) {
        for (int k = 0; k < 60; k++) {
            now += 1000;
            /* 20.1: no call at night (18.0); calls under the boost (20.5-0.3); after 08:00
             * 20.35 is above day 20.0+0.3 (off) but would still be heating under the boost */
            int16_t room = minute >= HM(8, 0) ? 2035 : 2010;
            ctrl_input_t in = {.valid = true, .temp = room};
            ctrl_feed_sensor(&c, &in, now);
            ctrl_settings_t eff = base;
            sched_apply(&P, &C, true, minute, &eff);
            o = ctrl_step(&c, &eff, now);
        }
        if (minute == HM(5, 59)) CHECK(!o.w);            /* night 18.0 */
        if (minute == HM(6, 5))  CHECK(o.w);             /* boosted 20.5 */
        if (minute == HM(7, 59)) CHECK(o.w);
        if (minute == HM(8, 5))  CHECK(!o.w);            /* back to day 20.0: satisfied */
    }
}

int main(void)
{
    RUN(defaults_match_spec);
    RUN(period_same_day);
    RUN(period_crossing_midnight);
    RUN(heat_and_cool_programs_independent);
    RUN(boost_window_and_mode);
    RUN(edit_changes_current_period_only);
    RUN(sanitize_ranges);
    RUN(controller_follows_schedule);
    DONE();
}
