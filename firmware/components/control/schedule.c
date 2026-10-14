/* schedule.c - see schedule.h. Pure C99. */
#include "schedule.h"

static int16_t clamp16(int32_t v, int16_t lo, int16_t hi) { return v < lo ? lo : (v > hi ? hi : (int16_t)v); }
static uint16_t clamp_min(uint16_t m) { return m >= SCHED_MIN_PER_DAY ? SCHED_MIN_PER_DAY - 1 : m; }

void sched_sanitize(const ctrl_params_t *p, sched_cfg_t *c)
{
    c->day_cool_start = clamp_min(c->day_cool_start);
    c->night_cool_start = clamp_min(c->night_cool_start);
    c->day_heat_start = clamp_min(c->day_heat_start);
    c->night_heat_start = clamp_min(c->night_heat_start);
    c->day_cool = clamp16(c->day_cool, p->min_sp, p->max_sp);
    c->night_cool = clamp16(c->night_cool, p->min_sp, p->max_sp);
    c->day_heat = clamp16(c->day_heat, p->min_sp, p->max_sp);
    c->night_heat = clamp16(c->night_heat, p->min_sp, p->max_sp);
    if (c->boost_min > SCHED_BOOST_MAX_MIN) c->boost_min = SCHED_BOOST_MAX_MIN;
    c->boost_offset = clamp16(c->boost_offset, 0, SCHED_BOOST_OFS_MAX);
}

sched_period_t sched_period(uint16_t day, uint16_t night, uint16_t minute)
{
    minute %= SCHED_MIN_PER_DAY;
    if (day == night) return SCHED_DAY;
    if (day < night) return (minute >= day && minute < night) ? SCHED_DAY : SCHED_NIGHT;
    /* day period crosses midnight (e.g. day 20:00, night 04:00) */
    return (minute >= day || minute < night) ? SCHED_DAY : SCHED_NIGHT;
}

bool sched_boost_window(const sched_cfg_t *c, uint16_t minute)
{
    if (!c->boost_min || !c->boost_offset) return false;
    minute %= SCHED_MIN_PER_DAY;
    uint16_t since = (uint16_t)((minute + SCHED_MIN_PER_DAY - c->day_heat_start % SCHED_MIN_PER_DAY) % SCHED_MIN_PER_DAY);
    return since < c->boost_min;
}

sched_period_t sched_current(const sched_cfg_t *c, ctrl_mode_t mode, bool time_known, uint16_t minute)
{
    if (!time_known) return SCHED_DAY;
    return mode == CTRL_MODE_HEAT ? sched_period(c->day_heat_start, c->night_heat_start, minute)
                                  : sched_period(c->day_cool_start, c->night_cool_start, minute);
}

void sched_program_sp(const sched_cfg_t *c, bool known, uint16_t minute, int16_t *heat, int16_t *cool)
{
    *heat = sched_current(c, CTRL_MODE_HEAT, known, minute) == SCHED_DAY ? c->day_heat : c->night_heat;
    *cool = sched_current(c, CTRL_MODE_COOL, known, minute) == SCHED_DAY ? c->day_cool : c->night_cool;
}

sched_state_t sched_apply(const ctrl_params_t *p, const sched_cfg_t *c, bool known, uint16_t minute,
                          ctrl_settings_t *s)
{
    sched_program_sp(c, known, minute, &s->heat_sp, &s->cool_sp);
    if (!known) return SCHED_STATE_TIME_UNKNOWN;
    sched_period_t per = sched_current(c, s->mode, true, minute);
    if (s->mode == CTRL_MODE_HEAT && sched_boost_window(c, minute)) {
        s->heat_sp = clamp16((int32_t)s->heat_sp + c->boost_offset, p->min_sp, p->max_sp);
        return SCHED_STATE_DAY_BOOST;
    }
    return per == SCHED_DAY ? SCHED_STATE_DAY : SCHED_STATE_NIGHT;
}

bool sched_edit_current(const ctrl_params_t *p, sched_cfg_t *c, bool heat, bool known, uint16_t minute, int16_t sp)
{
    sp = clamp16(sp, p->min_sp, p->max_sp);
    sched_period_t per = sched_current(c, heat ? CTRL_MODE_HEAT : CTRL_MODE_COOL, known, minute);
    int16_t *dst = heat ? (per == SCHED_DAY ? &c->day_heat : &c->night_heat)
                        : (per == SCHED_DAY ? &c->day_cool : &c->night_cool);
    if (*dst == sp) return false;
    *dst = sp;
    return true;
}
