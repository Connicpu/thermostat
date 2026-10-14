/* schedule.c - see schedule.h. Pure C99. */
#include "schedule.h"
#include <stddef.h>

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

uint16_t sched_boost_length(const sched_cfg_t *c)
{
    if (!c->boost_min || !c->boost_offset) return 0;
    uint16_t day = c->day_heat_start % SCHED_MIN_PER_DAY, night = c->night_heat_start % SCHED_MIN_PER_DAY;
    if (day == night) return 0;           /* program disabled: no "morning" to boost */
    uint16_t day_len = (uint16_t)((night + SCHED_MIN_PER_DAY - day) % SCHED_MIN_PER_DAY);
    return c->boost_min < day_len ? c->boost_min : day_len;   /* never runs into the night period */
}

bool sched_boost_window(const sched_cfg_t *c, uint16_t minute)
{
    uint16_t len = sched_boost_length(c);
    if (!len) return false;
    minute %= SCHED_MIN_PER_DAY;
    uint16_t since = (uint16_t)((minute + SCHED_MIN_PER_DAY - c->day_heat_start % SCHED_MIN_PER_DAY) % SCHED_MIN_PER_DAY);
    return since < len;
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
    /* the period shown/reported follows the active program; OFF shows the heating program */
    sched_period_t per = sched_current(c, s->mode == CTRL_MODE_COOL ? CTRL_MODE_COOL : CTRL_MODE_HEAT, true, minute);
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

sched_kind_t sched_item_kind(sched_item_t it)
{
    switch (it) {
    case SCHED_IT_DAY_HEAT_START: case SCHED_IT_NIGHT_HEAT_START:
    case SCHED_IT_DAY_COOL_START: case SCHED_IT_NIGHT_COOL_START: return SCHED_KIND_TIME;
    case SCHED_IT_BOOST_MIN: return SCHED_KIND_MINUTES;
    case SCHED_IT_BOOST_OFS: return SCHED_KIND_DELTA;
    default: return SCHED_KIND_TEMP;
    }
}

static void *item_ptr(const sched_cfg_t *c, sched_item_t it)
{
    sched_cfg_t *m = (sched_cfg_t *)c;
    switch (it) {
    case SCHED_IT_DAY_HEAT_START:   return &m->day_heat_start;
    case SCHED_IT_DAY_HEAT:         return &m->day_heat;
    case SCHED_IT_NIGHT_HEAT_START: return &m->night_heat_start;
    case SCHED_IT_NIGHT_HEAT:       return &m->night_heat;
    case SCHED_IT_BOOST_MIN:        return &m->boost_min;
    case SCHED_IT_BOOST_OFS:        return &m->boost_offset;
    case SCHED_IT_DAY_COOL_START:   return &m->day_cool_start;
    case SCHED_IT_DAY_COOL:         return &m->day_cool;
    case SCHED_IT_NIGHT_COOL_START: return &m->night_cool_start;
    case SCHED_IT_NIGHT_COOL:       return &m->night_cool;
    default:                        return NULL;
    }
}

static bool item_unsigned(sched_item_t it)
{
    sched_kind_t k = sched_item_kind(it);
    return k == SCHED_KIND_TIME || k == SCHED_KIND_MINUTES;
}

int32_t sched_get(const sched_cfg_t *c, sched_item_t it)
{
    void *p = item_ptr(c, it);
    if (!p) return 0;
    return item_unsigned(it) ? *(uint16_t *)p : *(int16_t *)p;
}

void sched_set(sched_cfg_t *c, sched_item_t it, int32_t v)
{
    void *p = item_ptr(c, it);
    if (!p) return;
    if (item_unsigned(it)) *(uint16_t *)p = (uint16_t)(v < 0 ? 0 : v > 0xFFFF ? 0xFFFF : v);
    else                   *(int16_t *)p = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
}
