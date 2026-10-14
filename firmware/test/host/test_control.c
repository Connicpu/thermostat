/* Host unit tests for the control logic and the output interlock. */
#include "tinytest.h"
#include "control.h"
#include "hvac_interlock.h"
#include <string.h>

#define S(x) ((uint64_t)(x) * 1000u)
#define MIN(x) S((x) * 60u)

static ctrl_t c;
static ctrl_params_t P;
static ctrl_settings_t st;
static uint64_t now;
static ctrl_output_t out;

static void setup(ctrl_mode_t mode, int16_t heat, int16_t cool)
{
    P = (ctrl_params_t)CTRL_PARAMS_DEFAULT();
    P.hysteresis = 30;                  /* legacy tests were written for 0.3 °C */
    now = 0;
    ctrl_init(&c, &P, now);
    st = (ctrl_settings_t){.mode = mode, .fan = CTRL_FAN_AUTO, .heat_sp = heat, .cool_sp = cool};
}

/* advance `ms` in 1 s steps feeding temperature `t` (or an invalid read) */
static void run(uint64_t ms, int16_t t, bool valid)
{
    uint64_t end = now + ms;
    while (now < end) {
        now += 1000;
        ctrl_input_t in = {.valid = valid, .temp = t};
        ctrl_feed_sensor(&c, &in, now);
        out = ctrl_step(&c, &st, now);
        if (out.w && out.y) { printf("  W and Y together at t=%llu\n", (unsigned long long)now); tt_cur_fail = 1; }
    }
}

static void boot_failsafe_until_good_samples(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    out = ctrl_step(&c, &st, 0);
    CHECK(out.fault != CTRL_FAULT_NONE);
    CHECK(!out.w && !out.y && !out.g);
    run(S(2), 1500, true);              /* 2 good samples: still failsafe */
    CHECK(out.fault != CTRL_FAULT_NONE);
    CHECK(!out.w);
    run(S(1), 1500, true);              /* 3rd -> recovered */
    CHECK_EQ(out.fault, CTRL_FAULT_NONE);
}

static void heat_hysteresis(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(2), 1980, true);            /* 19.8: inside band, never called */
    CHECK(!out.w);
    run(S(5), 1970, true);              /* 19.7 = sp - 0.3 -> on */
    CHECK(out.w);
    run(MIN(5), 2020, true);            /* 20.2: still inside band -> stays on */
    CHECK(out.w);
    run(S(1), 2030, true);              /* 20.3 -> off */
    CHECK(!out.w);
    CHECK(!out.y);
}

static void heat_min_on_time(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(2), 1800, true);
    CHECK(out.w);
    uint64_t on_at = c.w_on_at;
    run(S(10), 2200, true);             /* satisfied immediately (e.g. sensor bumped) */
    CHECK(out.w);                       /* held by min on-time */
    while (out.w && now < on_at + MIN(5)) run(S(1), 2200, true);
    CHECK(!out.w);
    CHECK(now - on_at >= P.heat_min_on_ms);
    CHECK(now - on_at <= P.heat_min_on_ms + 1000);
}

static void compressor_min_off_after_boot(void)
{
    setup(CTRL_MODE_COOL, 2000, 2400);
    run(S(1), 2800, true);
    while (!out.y && now < MIN(10)) {
        CHECK(!out.y);
        run(S(1), 2800, true);
    }
    CHECK(out.y);
    CHECK(now >= P.comp_min_off_ms);    /* power-up counts as "just turned off" */
    CHECK(out.g);                       /* fan runs with the compressor */
}

static void compressor_min_off_between_cycles(void)
{
    setup(CTRL_MODE_COOL, 2000, 2400);
    run(MIN(6), 2800, true);
    CHECK(out.y);
    run(MIN(4), 2300, true);            /* satisfied, min on (3 min) elapsed */
    CHECK(!out.y);
    uint64_t off_at = c.y_off_at;
    run(S(5), 2900, true);              /* hot again right away */
    CHECK(!out.y);
    CHECK(out.waiting);
    while (!out.y && now < off_at + MIN(10)) run(S(1), 2900, true);
    CHECK(out.y);
    CHECK(now - off_at >= P.comp_min_off_ms);
}

static void compressor_min_on_time(void)
{
    setup(CTRL_MODE_COOL, 2000, 2400);
    run(MIN(6), 2800, true);
    CHECK(out.y);
    uint64_t on_at = c.y_on_at;
    run(S(30), 2000, true);
    CHECK(out.y);
    while (out.y && now < on_at + MIN(10)) run(S(1), 2000, true);
    CHECK(now - on_at >= P.comp_min_on_ms);
}

static void failsafe_sensor_timeout_overrides_min_on(void)
{
    setup(CTRL_MODE_COOL, 2000, 2400);
    run(MIN(6), 2800, true);
    CHECK(out.y);
    run(S(29), 0, false);               /* 29 s of failed reads: still running */
    CHECK(out.y);
    run(S(2), 0, false);                /* > 30 s -> failsafe, all off */
    CHECK(!out.y && !out.w && !out.g);
    CHECK_EQ(out.fault, CTRL_FAULT_SENSOR_TIMEOUT);
    /* sensor comes back: after recovery the compressor still waits min-off */
    uint64_t off_at = c.y_off_at;
    run(S(10), 2800, true);
    CHECK_EQ(out.fault, CTRL_FAULT_NONE);
    CHECK(!out.y);
    while (!out.y && now < off_at + MIN(10)) run(S(1), 2800, true);
    CHECK(now - off_at >= P.comp_min_off_ms);
}

static void failsafe_out_of_range_is_immediate(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(2), 1800, true);
    CHECK(out.w);
    run(S(1), 6000, true);              /* 60 °C: sensor shorted / on fire */
    CHECK(!out.w && !out.g && !out.y);
    CHECK_EQ(out.fault, CTRL_FAULT_TEMP_RANGE);
    run(S(1), -1000, true);             /* -10 °C also implausible */
    CHECK_EQ(out.fault, CTRL_FAULT_TEMP_RANGE);
    CHECK(!out.w);
    run(S(1), 1800, true);              /* one good sample is not enough */
    CHECK(out.fault != CTRL_FAULT_NONE);
    run(S(5), 1800, true);
    CHECK_EQ(out.fault, CTRL_FAULT_NONE);
}

static void failsafe_kills_fan_on_and_runon(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    st.fan = CTRL_FAN_ON;
    run(S(5), 2000, true);
    CHECK(out.g);
    run(S(1), 9000, true);
    CHECK(!out.g);
}

static void mode_off_is_immediate(void)
{
    setup(CTRL_MODE_COOL, 2000, 2400);
    run(MIN(6), 2800, true);
    CHECK(out.y);
    st.mode = CTRL_MODE_OFF;
    run(S(1), 2800, true);              /* user OFF beats min on-time */
    CHECK(!out.y && !out.w);
}

static void fan_runon_after_heat(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(2), 1800, true);
    CHECK(out.w);
    CHECK(!out.g);                      /* gas furnace: board runs its own blower */
    while (out.w && now < MIN(10)) run(S(1), 2100, true);
    CHECK(!out.w);
    CHECK(out.g);                       /* run-on */
    uint64_t off_at = c.w_off_at;
    while (out.g && now < off_at + MIN(5)) run(S(1), 2100, true);
    CHECK(!out.g);
    CHECK(now - off_at >= P.fan_runon_heat_ms);
    CHECK(now - off_at <= P.fan_runon_heat_ms + 1000);
}

static void fan_on_mode(void)
{
    setup(CTRL_MODE_OFF, 2000, 2400);
    st.fan = CTRL_FAN_ON;
    run(S(5), 2100, true);
    CHECK(out.g && !out.w && !out.y);
    st.fan = CTRL_FAN_AUTO;
    run(S(1), 2100, true);
    CHECK(!out.g);
}

static void heat_to_cool_changeover_delay(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(2), 1800, true);
    CHECK(out.w);
    run(MIN(3), 2600, true);            /* house overshoots */
    CHECK(!out.w);
    uint64_t w_off = c.w_off_at;
    st.mode = CTRL_MODE_COOL;           /* user switches to cooling right away */
    CHECK(!out.y);
    while (!out.y && now < w_off + MIN(10)) { CHECK(!out.w); run(S(1), 2600, true); }
    CHECK(out.y);
    CHECK(now - w_off >= P.changeover_ms);
}

static void heat_to_cool_mode_switch_never_overlaps(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(10), 1800, true);
    CHECK(out.w);
    st.mode = CTRL_MODE_COOL;
    st.heat_sp = 1500;
    st.cool_sp = 1700;                  /* cool demand is immediate */
    run(S(1), 1800, true);
    CHECK(!out.w);
    CHECK(!out.y);                      /* changeover delay */
    run(MIN(6), 1800, true);
    CHECK(out.y);
}

static void sanitize_settings(void)
{
    P = (ctrl_params_t)CTRL_PARAMS_DEFAULT();
    ctrl_settings_t s = {.mode = 7, .fan = 9, .heat_sp = 2300, .cool_sp = 2350};
    ctrl_sanitize_settings(&P, &s, true);
    CHECK_EQ(s.mode, CTRL_MODE_OFF);
    CHECK_EQ(s.fan, CTRL_FAN_AUTO);
    CHECK_EQ(s.heat_sp, 2300);          /* no deadband any more: setpoints are independent */
    CHECK_EQ(s.cool_sp, 2350);
    s = (ctrl_settings_t){.mode = 0x01 /* old AUTO */, .fan = CTRL_FAN_ON, .heat_sp = 2400, .cool_sp = 2200};
    ctrl_sanitize_settings(&P, &s, false);
    CHECK_EQ(s.mode, CTRL_MODE_OFF);
    CHECK_EQ(s.heat_sp, 2400);
    CHECK_EQ(s.cool_sp, 2200);
    s = (ctrl_settings_t){.mode = CTRL_MODE_HEAT, .fan = CTRL_FAN_AUTO, .heat_sp = -500, .cool_sp = 9000};
    ctrl_sanitize_settings(&P, &s, true);
    CHECK_EQ(s.heat_sp, P.min_sp);
    CHECK_EQ(s.cool_sp, P.max_sp);
}

static void mode_cycle(void)
{
    CHECK_EQ(ctrl_next_mode(CTRL_MODE_OFF), CTRL_MODE_HEAT);
    CHECK_EQ(ctrl_next_mode(CTRL_MODE_HEAT), CTRL_MODE_COOL);
    CHECK_EQ(ctrl_next_mode(CTRL_MODE_COOL), CTRL_MODE_OFF);
    CHECK(!ctrl_mode_valid(0x01));      /* AUTO was removed */
    CHECK(!ctrl_mode_valid(0x02));
    CHECK(ctrl_mode_valid(CTRL_MODE_OFF) && ctrl_mode_valid(CTRL_MODE_HEAT) && ctrl_mode_valid(CTRL_MODE_COOL));
}

static void interlock_rule(void)
{
    bool f;
    hvac_state_t r = hvac_interlock((hvac_state_t){.w = true, .y = true, .g = true}, &f);
    CHECK(f);
    CHECK(!r.w && !r.y && r.g);
    for (int i = 0; i < 8; i++) {
        hvac_state_t q = {.w = i & 1, .y = (i >> 1) & 1, .g = (i >> 2) & 1};
        r = hvac_interlock(q, &f);
        CHECK(!(r.w && r.y));
        if (!(q.w && q.y)) { CHECK(!f); CHECK(r.w == q.w && r.y == q.y && r.g == q.g); }
    }
}

/* Random long-running soak: random temperatures, setpoints, mode changes and
 * sensor dropouts. Checks the safety invariants on every step. */
static uint32_t rng = 12345;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 8; }

static void fuzz_safety_invariants(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    int16_t t = 2200;
    bool prev_y = false, prev_w = false;
    uint64_t y_off = 0, y_on = 0, w_on = 0;
    int y_starts = 0, w_starts = 0;
    bool y_off_by_force = false;
    for (int i = 0; i < 7 * 24 * 3600; i++) {          /* one simulated week, 1 s steps */
        now += 1000;
        if (rnd() % 600 == 0) st.mode = (ctrl_mode_t[]){CTRL_MODE_OFF, CTRL_MODE_HEAT, CTRL_MODE_COOL, CTRL_MODE_HEAT}[rnd() % 4];
        if (rnd() % 900 == 0) { st.heat_sp = 1500 + rnd() % 1000; st.cool_sp = 1800 + rnd() % 1200; }
        if (rnd() % 1800 == 0) st.fan = (rnd() & 1) ? CTRL_FAN_ON : CTRL_FAN_AUTO;
        t += (int16_t)((int)(rnd() % 21) - 10);
        if (t < 1200) t = 1200;
        if (t > 3200) t = 3200;
        bool valid = rnd() % 50 != 0;
        int16_t meas = t;
        if (rnd() % 20000 == 0) meas = 8000;              /* rare garbage reading */
        ctrl_input_t in = {.valid = valid, .temp = meas};
        ctrl_feed_sensor(&c, &in, now);
        out = ctrl_step(&c, &st, now);

        CHECK(!(out.w && out.y));
        if (out.fault != CTRL_FAULT_NONE) CHECK(!out.w && !out.y && !out.g);
        if (st.mode == CTRL_MODE_OFF) CHECK(!out.w && !out.y);
        if (st.mode == CTRL_MODE_HEAT) CHECK(!out.y);
        if (st.mode == CTRL_MODE_COOL) CHECK(!out.w);
        if (out.y && !prev_y) {                          /* compressor start */
            CHECK(now - y_off >= P.comp_min_off_ms);
            y_on = now; y_starts++;
        }
        if (!out.y && prev_y) {                          /* compressor stop */
            bool forced = out.fault != CTRL_FAULT_NONE || st.mode == CTRL_MODE_OFF || st.mode == CTRL_MODE_HEAT;
            if (!forced) CHECK(now - y_on >= P.comp_min_on_ms);
            y_off = now; y_off_by_force = forced;
        }
        if (out.w && !prev_w) { w_on = now; w_starts++; }
        (void)w_on; (void)y_off_by_force;
        prev_y = out.y; prev_w = out.w;
        if (tt_cur_fail) { printf("  at step %d\n", i); return; }
    }
    printf("  (week soak: %d compressor starts, %d heat starts)\n", y_starts, w_starts);
    CHECK(y_starts > 5 && w_starts > 5);
}


/* ---------------------------- boost ---------------------------- */

static void boost_target_heat_reaches_and_returns(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(2), 2000, true);                 /* satisfied at normal SP */
    CHECK(!out.w);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_AUTO, 2250, 0, now), BOOST_OK);
    ctrl_boost_status_t b = ctrl_boost_status(&c, now);
    CHECK_EQ(b.kind, BOOST_TO_TARGET);
    CHECK_EQ(b.dir, CTRL_MODE_HEAT);
    run(S(2), 2000, true);
    CHECK(out.w);
    run(MIN(3), 2240, true);                 /* still below target */
    CHECK(out.w);
    run(S(1), 2250, true);                   /* reached */
    b = ctrl_boost_status(&c, now);
    CHECK_EQ(b.kind, BOOST_NONE);
    CHECK_EQ(b.last_end, BOOST_END_REACHED);
    CHECK(!out.w);                           /* back to normal: 22.5 >> 20.0 */
}

static void boost_target_cool_auto_direction(void)
{
    setup(CTRL_MODE_OFF, 2000, 2400);
    run(MIN(6), 2600, true);
    CHECK(!out.y);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_AUTO, 2300, 0, now), BOOST_OK);
    CHECK_EQ(ctrl_boost_status(&c, now).dir, CTRL_MODE_COOL);
    run(S(1), 2600, true);
    CHECK(out.y && out.g && !out.w);
    run(MIN(1), 2290, true);                 /* reached after 1 min: min on-time still holds Y */
    CHECK_EQ(ctrl_boost_status(&c, now).kind, BOOST_NONE);
    CHECK(out.y);
    uint64_t on_at = c.y_on_at;
    while (out.y && now < on_at + MIN(10)) run(S(1), 2290, true);
    CHECK(now - on_at >= P.comp_min_on_ms);
    CHECK(now - on_at <= P.comp_min_on_ms + 1000);
}

static void boost_target_rejects_bad_requests(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_AUTO, 2200, 0, now), BOOST_ERR_FAULT); /* no reading yet */
    run(S(5), 2200, true);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_AUTO, 2210, 0, now), BOOST_ERR_AT_TARGET);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_HEAT, 2100, 0, now), BOOST_ERR_AT_TARGET);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_COOL, 2300, 0, now), BOOST_ERR_AT_TARGET);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 0, now), BOOST_ERR_ARG);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 121, now), BOOST_ERR_ARG);
    CHECK_EQ(ctrl_boost_start(&c, &st, (ctrl_boost_kind_t)9, BOOST_DIR_HEAT, 0, 5, now), BOOST_ERR_ARG);
    /* target is clamped to the setpoint range */
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_HEAT, 9000, 0, now), BOOST_OK);
    CHECK_EQ(ctrl_boost_status(&c, now).sp, P.max_sp);
    /* timed AUTO with nothing to do (in deadband, mode OFF) */
    st.mode = CTRL_MODE_OFF;
    ctrl_boost_cancel(&c);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_AUTO, 0, 10, now), BOOST_ERR_NO_DIRECTION);
}

static void boost_target_gives_up_after_max(void)
{
    setup(CTRL_MODE_OFF, 2000, 2400);
    run(S(5), 1800, true);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_HEAT, 2500, 0, now), BOOST_OK);
    run(MIN(BOOST_TARGET_MAX_MIN) - S(5), 1800, true);
    CHECK(out.w);
    run(S(10), 1800, true);
    CHECK_EQ(ctrl_boost_status(&c, now).last_end, BOOST_END_EXPIRED);
    CHECK(!out.w);                           /* mode OFF again */
}

static void boost_timed_runs_n_minutes_and_ends(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(2), 2100, true);                 /* above SP: normal would not heat */
    CHECK(!out.w);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_AUTO, 0, 15, now), BOOST_OK);
    CHECK_EQ(ctrl_boost_status(&c, now).dir, CTRL_MODE_HEAT);
    CHECK_EQ(ctrl_boost_status(&c, now).remaining_s, 15 * 60);
    run(S(1), 2300, true);
    CHECK(out.w);                            /* forced call despite 23 > 20 */
    run(MIN(10), 2300, true);
    CHECK(out.w);
    CHECK_EQ(ctrl_boost_status(&c, now).remaining_s, 15 * 60 - 601);
    run(MIN(5), 2300, true);
    CHECK_EQ(ctrl_boost_status(&c, now).kind, BOOST_NONE);
    CHECK_EQ(ctrl_boost_status(&c, now).last_end, BOOST_END_EXPIRED);
    CHECK(!out.w);
}

static void boost_timed_cool_respects_compressor_min_off(void)
{
    setup(CTRL_MODE_COOL, 2000, 2400);
    run(MIN(6), 2800, true);
    CHECK(out.y);
    run(MIN(4), 2300, true);
    CHECK(!out.y);
    uint64_t off_at = c.y_off_at;
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_COOL, 0, 30, now), BOOST_OK);
    run(S(1), 2300, true);
    CHECK(!out.y);                           /* boost cannot bypass the 5 min off-time */
    CHECK(out.waiting);
    while (!out.y && now < off_at + MIN(10)) run(S(1), 2300, true);
    CHECK(now - off_at >= P.comp_min_off_ms);
}

static void boost_cancel_returns_to_normal(void)
{
    setup(CTRL_MODE_OFF, 2000, 2400);
    run(S(5), 1900, true);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 60, now), BOOST_OK);
    run(MIN(5), 1900, true);
    CHECK(out.w);
    ctrl_boost_cancel(&c);
    run(S(1), 1900, true);
    CHECK(!out.w);                           /* min on (2 min) already satisfied */
    CHECK_EQ(ctrl_boost_status(&c, now).last_end, BOOST_END_CANCELLED);
    ctrl_boost_cancel(&c);                   /* idempotent */
    CHECK_EQ(ctrl_boost_status(&c, now).kind, BOOST_NONE);
}

static void boost_mode_change_cancels(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(S(5), 1900, true);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TO_TARGET, BOOST_DIR_HEAT, 2300, 0, now), BOOST_OK);
    run(MIN(3), 1900, true);
    CHECK(out.w);
    st.mode = CTRL_MODE_COOL;
    run(S(1), 1900, true);
    CHECK_EQ(ctrl_boost_status(&c, now).last_end, BOOST_END_MODE_CHANGED);
    CHECK(!out.w && !out.y);
}

static void boost_opposite_direction_never_overlaps(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    run(MIN(10), 1800, true);
    CHECK(out.w);                            /* normal heat running */
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_COOL, 0, 30, now), BOOST_OK);
    for (int i = 0; i < 600; i++) { run(S(1), 1800, true); CHECK(!(out.w && out.y)); }
    CHECK(!out.w);
    CHECK(out.y);                            /* after min-on + changeover */
    CHECK(c.y_on_at - c.w_off_at >= P.changeover_ms);
}

static void boost_failsafe_kills_and_cancels(void)
{
    setup(CTRL_MODE_OFF, 2000, 2400);
    run(S(5), 1900, true);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 60, now), BOOST_OK);
    run(S(65), 1900, true);                  /* furnace min-off from boot is 60 s */
    CHECK(out.w);
    run(S(1), 7000, true);                   /* implausible reading */
    CHECK(!out.w && !out.y && !out.g);
    CHECK_EQ(ctrl_boost_status(&c, now).kind, BOOST_NONE);
    CHECK_EQ(ctrl_boost_status(&c, now).last_end, BOOST_END_FAULT);
    run(MIN(5), 1900, true);                 /* sensor recovers: boost does NOT resume */
    CHECK(!out.w);
    /* sensor timeout during a boost behaves the same */
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 60, now), BOOST_OK);
    run(MIN(2), 1900, true);
    CHECK(out.w);
    run(S(31), 0, false);
    CHECK(!out.w);
    CHECK_EQ(ctrl_boost_status(&c, now).last_end, BOOST_END_FAULT);
}

static void boost_timed_comfort_limit(void)
{
    setup(CTRL_MODE_OFF, 2000, 2400);
    run(S(5), 3100, true);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 60, now), BOOST_OK);
    run(MIN(3), 3100, true);
    CHECK(out.w);
    run(S(1), 3200, true);                   /* hit max_sp: boost stops */
    CHECK_EQ(ctrl_boost_status(&c, now).last_end, BOOST_END_LIMIT);
    CHECK(!out.w);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 60, now), BOOST_ERR_AT_TARGET);
}

static void boost_not_persisted_across_reboot(void)
{
    setup(CTRL_MODE_OFF, 2000, 2400);
    run(S(5), 1900, true);
    CHECK_EQ(ctrl_boost_start(&c, &st, BOOST_TIMED, BOOST_DIR_HEAT, 0, 60, now), BOOST_OK);
    run(MIN(2), 1900, true);
    CHECK(out.w);
    /* "reboot": the settings survive (NVS), the controller is re-initialised */
    ctrl_init(&c, &P, 0);
    now = 0;
    CHECK_EQ(ctrl_boost_status(&c, now).kind, BOOST_NONE);
    run(MIN(10), 1900, true);
    CHECK(!out.w);                           /* mode OFF: nothing runs */
}

static void fuzz_with_boosts(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    int16_t t = 2200;
    bool prev_y = false;
    uint64_t y_off = 0, y_on = 0;
    int boosts = 0;
    for (int i = 0; i < 3 * 24 * 3600; i++) {
        now += 1000;
        uint32_t r = rnd();
        if (r % 400 == 0) {
            ctrl_boost_kind_t k = (rnd() & 1) ? BOOST_TO_TARGET : BOOST_TIMED;
            if (ctrl_boost_start(&c, &st, k, (ctrl_boost_dir_t)(rnd() % 3), 1500 + rnd() % 1500, 1 + rnd() % 120, now) == BOOST_OK) boosts++;
        }
        if (r % 1500 == 1) ctrl_boost_cancel(&c);
        if (r % 3000 == 2) st.mode = (ctrl_mode_t[]){CTRL_MODE_OFF, CTRL_MODE_HEAT, CTRL_MODE_COOL, CTRL_MODE_HEAT}[rnd() % 4];
        t += (int16_t)((int)(rnd() % 21) - 10);
        if (t < 1200) t = 1200;
        if (t > 3300) t = 3300;
        ctrl_input_t in = {.valid = rnd() % 60 != 0, .temp = (rnd() % 30000 == 0) ? 9000 : t};
        ctrl_feed_sensor(&c, &in, now);
        out = ctrl_step(&c, &st, now);
        CHECK(!(out.w && out.y));
        if (out.fault != CTRL_FAULT_NONE) CHECK(!out.w && !out.y && !out.g && ctrl_boost_status(&c, now).kind == BOOST_NONE);
        if (out.y && !prev_y) { CHECK(now - y_off >= P.comp_min_off_ms); y_on = now; }
        if (!out.y && prev_y) {
            if (out.fault == CTRL_FAULT_NONE && st.mode != CTRL_MODE_OFF && st.mode != CTRL_MODE_HEAT)
                CHECK(now - y_on >= P.comp_min_on_ms);
            y_off = now;
        }
        ctrl_boost_status_t b = ctrl_boost_status(&c, now);
        if (b.kind) CHECK(b.remaining_s <= BOOST_TARGET_MAX_MIN * 60u);
        prev_y = out.y;
        if (tt_cur_fail) { printf("  at step %d\n", i); return; }
    }
    printf("  (3-day soak with %d boosts)\n", boosts);
    CHECK(boosts > 20);
}

static void swing_default_and_clamp(void)
{
    ctrl_params_t d = CTRL_PARAMS_DEFAULT();
    CHECK_EQ(d.hysteresis, 50);
    CHECK_EQ(ctrl_clamp_swing(0), CTRL_SWING_MIN);
    CHECK_EQ(ctrl_clamp_swing(-50), CTRL_SWING_MIN);
    CHECK_EQ(ctrl_clamp_swing(500), CTRL_SWING_MAX);
    CHECK_EQ(ctrl_clamp_swing(70), 70);
    setup(CTRL_MODE_HEAT, 2000, 2400);
    ctrl_set_swing(&c, 1000);
    CHECK_EQ(c.p.hysteresis, CTRL_SWING_MAX);
}

static void heat_swing_1c(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    ctrl_set_swing(&c, 100);
    run(MIN(2), 1910, true);            /* 19.1: inside ±1.0 band */
    CHECK(!out.w);
    run(S(5), 1900, true);              /* 19.0 = sp - 1.0 -> on */
    CHECK(out.w);
    run(MIN(5), 2090, true);            /* 20.9: still on */
    CHECK(out.w);
    run(S(1), 2100, true);              /* 21.0 -> off */
    CHECK(!out.w);
}

static void cool_swing_mirror(void)
{
    setup(CTRL_MODE_COOL, 2000, 2400);
    ctrl_set_swing(&c, 70);
    run(MIN(6), 2460, true);            /* 24.6 < 24.7: no call */
    CHECK(!out.y);
    run(S(2), 2470, true);              /* 24.7 = sp + 0.7 -> on (min off long elapsed) */
    CHECK(out.y);
    run(MIN(4), 2340, true);            /* 23.4 > 23.3: stays on */
    CHECK(out.y);
    run(S(1), 2330, true);              /* 23.3 -> off (min on elapsed) */
    CHECK(!out.y);
}

static void swing_change_while_running(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    ctrl_set_swing(&c, 100);
    run(MIN(2), 1890, true);
    CHECK(out.w);
    run(MIN(3), 2040, true);            /* inside wide band */
    CHECK(out.w);
    ctrl_set_swing(&c, 30);             /* narrow it: 20.4 >= 20.3 -> off */
    run(S(1), 2040, true);
    CHECK(!out.w);
}

static void swing_still_honours_min_on(void)
{
    setup(CTRL_MODE_HEAT, 2000, 2400);
    ctrl_set_swing(&c, 10);
    run(MIN(2), 1990, true);            /* 19.9 = sp - 0.1 -> on */
    CHECK(out.w);
    uint64_t on_at = c.w_on_at;
    run(S(10), 2010, true);             /* satisfied, but min on not elapsed */
    CHECK(out.w);
    while (out.w && now < on_at + MIN(5)) run(S(1), 2010, true);
    CHECK(now - on_at >= P.heat_min_on_ms);
}

static void wide_swing_overlapping_setpoints_never_overlap(void)
{
    /* No deadband any more: cool SP below heat SP, 1.0 °C swing, alternating modes */
    setup(CTRL_MODE_HEAT, 2200, 2000);
    ctrl_set_swing(&c, 100);
    for (int k = 0; k < 6; k++) {
        st.mode = (k & 1) ? CTRL_MODE_COOL : CTRL_MODE_HEAT;
        for (int16_t t = 1800; t <= 2350; t += 5) run(S(20), t, true);
        for (int16_t t = 2350; t >= 1800; t -= 5) run(S(20), t, true);
    }
    CHECK(!(out.w && out.y));
}

static void fan_on_without_demand(void)
{
    /* Fan ON runs G with no heat/cool demand in every mode, and never drives W/Y */
    ctrl_mode_t modes[] = {CTRL_MODE_OFF, CTRL_MODE_HEAT, CTRL_MODE_COOL};
    for (int i = 0; i < 3; i++) {
        setup(modes[i], 2000, 2400);
        st.fan = CTRL_FAN_ON;
        run(MIN(10), 2200, true);      /* between the setpoints: no demand */
        CHECK(out.g && !out.w && !out.y);
        CHECK(!out.heat_demand && !out.cool_demand);
        st.fan = CTRL_FAN_AUTO;
        run(S(1), 2200, true);
        CHECK(!out.g);
    }
}

int main(void)
{
    RUN(boot_failsafe_until_good_samples);
    RUN(heat_hysteresis);
    RUN(swing_default_and_clamp);
    RUN(heat_swing_1c);
    RUN(cool_swing_mirror);
    RUN(swing_change_while_running);
    RUN(swing_still_honours_min_on);
    RUN(wide_swing_overlapping_setpoints_never_overlap);
    RUN(fan_on_without_demand);
    RUN(heat_min_on_time);
    RUN(compressor_min_off_after_boot);
    RUN(compressor_min_off_between_cycles);
    RUN(compressor_min_on_time);
    RUN(failsafe_sensor_timeout_overrides_min_on);
    RUN(failsafe_out_of_range_is_immediate);
    RUN(failsafe_kills_fan_on_and_runon);
    RUN(mode_off_is_immediate);
    RUN(fan_runon_after_heat);
    RUN(fan_on_mode);
    RUN(heat_to_cool_changeover_delay);
    RUN(heat_to_cool_mode_switch_never_overlaps);
    RUN(sanitize_settings);
    RUN(mode_cycle);
    RUN(interlock_rule);
    RUN(fuzz_safety_invariants);
    RUN(boost_target_heat_reaches_and_returns);
    RUN(boost_target_cool_auto_direction);
    RUN(boost_target_rejects_bad_requests);
    RUN(boost_target_gives_up_after_max);
    RUN(boost_timed_runs_n_minutes_and_ends);
    RUN(boost_timed_cool_respects_compressor_min_off);
    RUN(boost_cancel_returns_to_normal);
    RUN(boost_mode_change_cancels);
    RUN(boost_opposite_direction_never_overlaps);
    RUN(boost_failsafe_kills_and_cancels);
    RUN(boost_timed_comfort_limit);
    RUN(boost_not_persisted_across_reboot);
    RUN(fuzz_with_boosts);
    DONE();
}
