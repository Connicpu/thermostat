/* control.c - see control.h. Pure C99, no platform dependencies. */
#include "control.h"
#include <string.h>

static int16_t clamp16(int32_t v, int16_t lo, int16_t hi)
{
    return v < lo ? lo : (v > hi ? hi : (int16_t)v);
}

static bool elapsed(uint64_t now, uint64_t since, uint32_t dur)
{
    return now >= since && (now - since) >= dur;
}

void ctrl_init(ctrl_t *c, const ctrl_params_t *p, uint64_t now_ms)
{
    memset(c, 0, sizeof(*c));
    c->p = *p;
    c->p.hysteresis = ctrl_clamp_swing(p->hysteresis);
    /* Treat boot as "everything just turned off": after a power blip the
     * compressor gets its full minimum off-time before restarting. */
    c->w_off_at = c->y_off_at = now_ms;
    c->last_valid_at = now_ms;
    /* Start in failsafe until the first good samples arrive. */
    c->fault = CTRL_FAULT_SENSOR_TIMEOUT;
}

void ctrl_sanitize_settings(const ctrl_params_t *p, ctrl_settings_t *s, bool keep_heat)
{
    if (!ctrl_mode_valid(s->mode)) s->mode = CTRL_MODE_OFF;
    if (s->fan != CTRL_FAN_ON && s->fan != CTRL_FAN_AUTO) s->fan = CTRL_FAN_AUTO;
    s->heat_sp = clamp16(s->heat_sp, p->min_sp, (int16_t)(p->max_sp - p->deadband));
    s->cool_sp = clamp16(s->cool_sp, (int16_t)(p->min_sp + p->deadband), p->max_sp);
    if (s->cool_sp - s->heat_sp < p->deadband) {
        if (keep_heat) s->cool_sp = (int16_t)(s->heat_sp + p->deadband);
        else           s->heat_sp = (int16_t)(s->cool_sp - p->deadband);
    }
}

void ctrl_feed_sensor(ctrl_t *c, const ctrl_input_t *in, uint64_t now_ms)
{
    if (!in->valid) {
        c->good_streak = 0;
        return; /* timeout logic in ctrl_step handles a dead sensor */
    }
    if (in->temp < c->p.temp_min || in->temp > c->p.temp_max) {
        c->fault = CTRL_FAULT_TEMP_RANGE;
        c->good_streak = 0;
        c->last_temp = in->temp;
        c->have_temp = true;
        return;
    }
    c->last_valid_at = now_ms;
    c->last_temp = in->temp;
    c->have_temp = true;
    if (c->fault != CTRL_FAULT_NONE) {
        if (++c->good_streak >= c->p.recover_samples) {
            c->fault = CTRL_FAULT_NONE;
            c->good_streak = 0;
        }
    }
}

static void set_w(ctrl_t *c, bool on, uint64_t now)
{
    if (on == c->w) return;
    c->w = on;
    if (on) { c->w_on_at = now; c->w_ran = true; }
    else {
        c->w_off_at = now;
        c->fan_runon_until = now + c->p.fan_runon_heat_ms;
    }
}

static void set_y(ctrl_t *c, bool on, uint64_t now)
{
    if (on == c->y) return;
    c->y = on;
    if (on) { c->y_on_at = now; c->y_ran = true; }
    else {
        c->y_off_at = now;
        c->fan_runon_until = now + c->p.fan_runon_cool_ms;
    }
}

ctrl_output_t ctrl_step(ctrl_t *c, const ctrl_settings_t *s_in, uint64_t now)
{
    ctrl_output_t o = {0};
    ctrl_settings_t s = *s_in;
    ctrl_sanitize_settings(&c->p, &s, true);

    /* ---- failsafe: stale or implausible sensor => everything off, now. ---- */
    if (!elapsed(now, c->last_valid_at, 0) || (now - c->last_valid_at) > c->p.sensor_timeout_ms) {
        if (c->fault == CTRL_FAULT_NONE) c->good_streak = 0;
        if (c->fault != CTRL_FAULT_TEMP_RANGE) c->fault = CTRL_FAULT_SENSOR_TIMEOUT;
    }
    if (c->fault != CTRL_FAULT_NONE || !c->have_temp) {
        set_w(c, false, now);
        set_y(c, false, now);
        c->g = false;
        c->fan_runon_until = 0;      /* no run-on in failsafe */
        c->heat_call = c->cool_call = false;
        if (c->boost.kind) { c->boost.kind = BOOST_NONE; c->boost.last_end = BOOST_END_FAULT; }
        o.fault = c->fault != CTRL_FAULT_NONE ? c->fault : CTRL_FAULT_SENSOR_TIMEOUT;
        return o;
    }

    const int16_t t = c->last_temp, h = c->p.hysteresis;

    /* ---- boost: end conditions, then override what we ask for ---- */
    bool force_heat = false, force_cool = false;
    if (c->boost.kind != BOOST_NONE) {
        ctrl_boost_t *b = &c->boost;
        ctrl_boost_end_t end = BOOST_END_NONE;
        if (s.mode != b->base_mode) end = BOOST_END_MODE_CHANGED;
        else if (now >= b->end_at) end = BOOST_END_EXPIRED;
        else if (b->kind == BOOST_TO_TARGET &&
                 ((b->dir == CTRL_MODE_HEAT && t >= b->sp) || (b->dir == CTRL_MODE_COOL && t <= b->sp)))
            end = BOOST_END_REACHED;
        else if (b->kind == BOOST_TIMED &&
                 ((b->dir == CTRL_MODE_HEAT && t >= c->p.max_sp) || (b->dir == CTRL_MODE_COOL && t <= c->p.min_sp)))
            end = BOOST_END_LIMIT;            /* comfort cap on timed boosts */
        if (end != BOOST_END_NONE) {
            b->kind = BOOST_NONE;
            b->last_end = end;
            c->soft_stop = true;
            /* drop the boost's latched call; normal hysteresis re-evaluates below */
            if (b->dir == CTRL_MODE_HEAT) c->heat_call = false; else c->cool_call = false;
        } else {
            s.mode = b->dir;
            if (b->dir == CTRL_MODE_HEAT) { force_heat = true; }
            else                          { force_cool = true; }
        }
    }
    const bool heat_allowed = (s.mode == CTRL_MODE_HEAT || s.mode == CTRL_MODE_AUTO);
    const bool cool_allowed = (s.mode == CTRL_MODE_COOL || s.mode == CTRL_MODE_AUTO);

    /* ---- hysteresis (latched calls) ---- */
    if (!heat_allowed)               c->heat_call = false;
    else if (t <= s.heat_sp - h)     c->heat_call = true;
    else if (t >= s.heat_sp + h)     c->heat_call = false;

    if (!cool_allowed)               c->cool_call = false;
    else if (t >= s.cool_sp + h)     c->cool_call = true;
    else if (t <= s.cool_sp - h)     c->cool_call = false;

    if (force_heat) { c->heat_call = true; c->cool_call = false; }
    if (force_cool) { c->cool_call = true; c->heat_call = false; }

    /* Deadband makes both calls impossible, but be explicit: an active stage wins. */
    if (c->heat_call && c->cool_call) {
        if (c->y) c->heat_call = false; else c->cool_call = false;
    }
    o.heat_demand = c->heat_call;
    o.cool_demand = c->cool_call;

    /* ---- heat (W) ---- */
    if (c->w) {
        bool stop = (!heat_allowed && (!c->soft_stop || elapsed(now, c->w_on_at, c->p.heat_min_on_ms))) ||
                    (!c->heat_call && elapsed(now, c->w_on_at, c->p.heat_min_on_ms));
        if (stop) set_w(c, false, now);
    } else if (c->heat_call && !c->y) {
        if (elapsed(now, c->w_off_at, c->p.heat_min_off_ms) &&
            (!c->y_ran || elapsed(now, c->y_off_at, c->p.changeover_ms)))
            set_w(c, true, now);
        else
            o.waiting = true;
    }

    /* ---- cool (Y, compressor) ---- */
    if (c->y) {
        bool stop = (!cool_allowed && (!c->soft_stop || elapsed(now, c->y_on_at, c->p.comp_min_on_ms))) ||
                    (!c->cool_call && elapsed(now, c->y_on_at, c->p.comp_min_on_ms));
        if (stop) set_y(c, false, now);
    } else if (c->cool_call && !c->w) {
        if (elapsed(now, c->y_off_at, c->p.comp_min_off_ms) &&
            (!c->w_ran || elapsed(now, c->w_off_at, c->p.changeover_ms)))
            set_y(c, true, now);
        else
            o.waiting = true;
    }

    if (!c->w && !c->y) c->soft_stop = false;

    /* ---- fan (G) ---- */
    bool g = (s.fan == CTRL_FAN_ON) || c->y || (c->w && c->p.heat_drives_fan) ||
             (now < c->fan_runon_until);
    c->g = g;

    o.w = c->w;
    o.y = c->y;
    o.g = c->g;
    o.fault = CTRL_FAULT_NONE;
    return o;
}

ctrl_mode_t ctrl_next_mode(ctrl_mode_t m)
{
    switch (m) {
    case CTRL_MODE_OFF:  return CTRL_MODE_HEAT;
    case CTRL_MODE_HEAT: return CTRL_MODE_COOL;
    case CTRL_MODE_COOL: return CTRL_MODE_AUTO;
    default:             return CTRL_MODE_OFF;
    }
}

bool ctrl_mode_valid(int v)
{
    return v == CTRL_MODE_OFF || v == CTRL_MODE_AUTO || v == CTRL_MODE_COOL || v == CTRL_MODE_HEAT;
}

const char *ctrl_fault_str(ctrl_fault_t f)
{
    switch (f) {
    case CTRL_FAULT_NONE:           return "OK";
    case CTRL_FAULT_SENSOR_TIMEOUT: return "SENSOR FAIL";
    case CTRL_FAULT_TEMP_RANGE:     return "TEMP RANGE";
    default:                        return "?";
    }
}

ctrl_boost_result_t ctrl_boost_start(ctrl_t *c, const ctrl_settings_t *s, ctrl_boost_kind_t kind,
                                     ctrl_boost_dir_t dir, int16_t sp, uint16_t minutes, uint64_t now)
{
    if (c->fault != CTRL_FAULT_NONE || !c->have_temp) return BOOST_ERR_FAULT;
    if (dir != BOOST_DIR_AUTO && dir != BOOST_DIR_HEAT && dir != BOOST_DIR_COOL) return BOOST_ERR_ARG;
    const int16_t t = c->last_temp, h = c->p.hysteresis;
    ctrl_boost_t b = {.kind = kind, .base_mode = s->mode, .last_end = BOOST_END_NONE};

    if (kind == BOOST_TO_TARGET) {
        sp = clamp16(sp, c->p.min_sp, c->p.max_sp);
        if (dir == BOOST_DIR_AUTO) {
            if (t <= sp - h)      b.dir = CTRL_MODE_HEAT;
            else if (t >= sp + h) b.dir = CTRL_MODE_COOL;
            else return BOOST_ERR_AT_TARGET;
        } else {
            b.dir = dir == BOOST_DIR_HEAT ? CTRL_MODE_HEAT : CTRL_MODE_COOL;
            if ((b.dir == CTRL_MODE_HEAT && t >= sp) || (b.dir == CTRL_MODE_COOL && t <= sp)) return BOOST_ERR_AT_TARGET;
        }
        b.sp = sp;
        b.end_at = now + (uint64_t)BOOST_TARGET_MAX_MIN * 60000u;
    } else if (kind == BOOST_TIMED) {
        if (minutes < BOOST_TIMED_MIN_MIN || minutes > BOOST_TIMED_MAX_MIN) return BOOST_ERR_ARG;
        if (dir == BOOST_DIR_AUTO) {
            if (s->mode == CTRL_MODE_HEAT)      b.dir = CTRL_MODE_HEAT;
            else if (s->mode == CTRL_MODE_COOL) b.dir = CTRL_MODE_COOL;
            else if (t < s->heat_sp)            b.dir = CTRL_MODE_HEAT;
            else if (t > s->cool_sp)            b.dir = CTRL_MODE_COOL;
            else return BOOST_ERR_NO_DIRECTION;
        } else {
            b.dir = dir == BOOST_DIR_HEAT ? CTRL_MODE_HEAT : CTRL_MODE_COOL;
        }
        if ((b.dir == CTRL_MODE_HEAT && t >= c->p.max_sp) || (b.dir == CTRL_MODE_COOL && t <= c->p.min_sp))
            return BOOST_ERR_AT_TARGET;
        b.end_at = now + (uint64_t)minutes * 60000u;
    } else {
        return BOOST_ERR_ARG;
    }
    /* switching direction mid-boost: drop the old latched call */
    if (c->boost.kind && c->boost.dir != b.dir) c->heat_call = c->cool_call = false;
    c->boost = b;
    c->soft_stop = true;   /* a stage the boost displaces still gets its min on-time */
    return BOOST_OK;
}

void ctrl_boost_cancel(ctrl_t *c)
{
    if (!c->boost.kind) return;
    if (c->boost.dir == CTRL_MODE_HEAT) c->heat_call = false; else c->cool_call = false;
    c->boost.kind = BOOST_NONE;
    c->boost.last_end = BOOST_END_CANCELLED;
    c->soft_stop = true;
}

ctrl_boost_status_t ctrl_boost_status(const ctrl_t *c, uint64_t now)
{
    ctrl_boost_status_t st = {.kind = c->boost.kind, .dir = c->boost.dir, .sp = c->boost.sp,
                              .last_end = c->boost.last_end};
    if (st.kind) st.remaining_s = now >= c->boost.end_at ? 0 : (uint32_t)((c->boost.end_at - now + 999) / 1000);
    return st;
}

int16_t ctrl_clamp_swing(int32_t v)
{
    return clamp16(v, CTRL_SWING_MIN, CTRL_SWING_MAX);
}

void ctrl_set_swing(ctrl_t *c, int16_t swing)
{
    c->p.hysteresis = ctrl_clamp_swing(swing);
}
