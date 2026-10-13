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

static void auto_changeover_delay(void)
{
    setup(CTRL_MODE_AUTO, 2000, 2400);
    run(MIN(2), 1800, true);
    CHECK(out.w);
    run(MIN(3), 2600, true);            /* house overshoots into cooling range */
    CHECK(!out.w);
    uint64_t w_off = c.w_off_at;
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
    CHECK_EQ(s.heat_sp, 2300);
    CHECK_EQ(s.cool_sp, 2450);
    s = (ctrl_settings_t){.mode = CTRL_MODE_AUTO, .fan = CTRL_FAN_ON, .heat_sp = 2300, .cool_sp = 2350};
    ctrl_sanitize_settings(&P, &s, false);
    CHECK_EQ(s.heat_sp, 2200);
    CHECK_EQ(s.cool_sp, 2350);
    s = (ctrl_settings_t){.mode = CTRL_MODE_HEAT, .fan = CTRL_FAN_AUTO, .heat_sp = -500, .cool_sp = 9000};
    ctrl_sanitize_settings(&P, &s, true);
    CHECK_EQ(s.heat_sp, P.min_sp);
    CHECK_EQ(s.cool_sp, P.max_sp);
}

static void mode_cycle(void)
{
    CHECK_EQ(ctrl_next_mode(CTRL_MODE_OFF), CTRL_MODE_HEAT);
    CHECK_EQ(ctrl_next_mode(CTRL_MODE_HEAT), CTRL_MODE_COOL);
    CHECK_EQ(ctrl_next_mode(CTRL_MODE_COOL), CTRL_MODE_AUTO);
    CHECK_EQ(ctrl_next_mode(CTRL_MODE_AUTO), CTRL_MODE_OFF);
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
    setup(CTRL_MODE_AUTO, 2000, 2400);
    int16_t t = 2200;
    bool prev_y = false, prev_w = false;
    uint64_t y_off = 0, y_on = 0, w_on = 0;
    int y_starts = 0, w_starts = 0;
    bool y_off_by_force = false;
    for (int i = 0; i < 7 * 24 * 3600; i++) {          /* one simulated week, 1 s steps */
        now += 1000;
        if (rnd() % 600 == 0) st.mode = (ctrl_mode_t[]){CTRL_MODE_OFF, CTRL_MODE_AUTO, CTRL_MODE_COOL, CTRL_MODE_HEAT}[rnd() % 4];
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

int main(void)
{
    RUN(boot_failsafe_until_good_samples);
    RUN(heat_hysteresis);
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
    RUN(auto_changeover_delay);
    RUN(heat_to_cool_mode_switch_never_overlaps);
    RUN(sanitize_settings);
    RUN(mode_cycle);
    RUN(interlock_rule);
    RUN(fuzz_safety_invariants);
    DONE();
}
