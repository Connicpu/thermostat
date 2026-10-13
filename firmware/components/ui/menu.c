/* menu.c - see menu.h. Pure C, no platform dependencies. */
#include "menu.h"
#include <stdio.h>
#include <string.h>

#define SP_MIN 1000
#define SP_MAX 3200

static const ctrl_mode_t MODES[] = {CTRL_MODE_OFF, CTRL_MODE_HEAT, CTRL_MODE_COOL, CTRL_MODE_AUTO};

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

int16_t menu_step_temp(int16_t sp, int dir, bool fahrenheit)
{
    if (fahrenheit) {
        int32_t f10 = (int32_t)sp * 9 / 5 + 3200;                 /* c°F */
        int32_t f = (f10 >= 0 ? f10 + 50 : f10 - 50) / 100 + dir; /* whole °F */
        return (int16_t)(((f * 100 - 3200) * 5 + (f >= 32 ? 4 : -4)) / 9);
    }
    int32_t half = (sp + (sp >= 0 ? 25 : -25)) / 50 + dir;
    return (int16_t)(half * 50);
}

void menu_init(menu_t *m) { memset(m, 0, sizeof *m); }

bool menu_is_home(const menu_t *m) { return m->st <= MS_HOLD_RESET; }

static uint8_t boost_count(const menu_ctx_t *c) { return c->boost_active ? 4 : 3; }
static menu_boost_item_t boost_item(const menu_ctx_t *c, uint8_t i)
{
    if (!c->boost_active && i >= MB_CANCEL) return MB_BACK;
    return (menu_boost_item_t)i;
}

static menu_action_t act(menu_action_kind_t k, int32_t v) { return (menu_action_t){.kind = k, .value = v, .redraw = true}; }
static menu_action_t redraw(void) { return act(MA_NONE, 0); }
static menu_action_t go_home(menu_t *m, menu_action_kind_t k, int32_t v)
{
    m->st = MS_HOME;
    menu_action_t a = act(k, v);
    a.full_refresh = true;
    return a;
}

static int mode_index(ctrl_mode_t md)
{
    for (int i = 0; i < 4; i++) if (MODES[i] == md) return i;
    return 0;
}

static void open_item(menu_t *m, const menu_ctx_t *c)
{
    switch ((menu_item_t)m->cursor) {
    case MI_MODE:   m->st = MS_EDIT_MODE;  m->edit = mode_index(c->mode); break;
    case MI_BOOST:  m->st = MS_BOOST;      m->bcursor = 0; break;
    case MI_SWING:  m->st = MS_EDIT_SWING; m->edit = clamp32(c->swing, CTRL_SWING_MIN, CTRL_SWING_MAX); break;
    case MI_UNITS:  m->st = MS_EDIT_UNITS; m->edit = c->fahrenheit; break;
    case MI_ZIGBEE: m->st = MS_ZIGBEE; break;
    case MI_INFO:   m->st = MS_INFO; break;
    default: break;
    }
}

static menu_action_t key_home(menu_t *m, menu_key_t k, btn_event_t ev)
{
    if (k != MENU_KEY_M) return (menu_action_t){.kind = MA_HOME_KEY};
    switch (m->st) {
    case MS_HOME:
        if (ev == BTN_EV_PRESS) { m->st = MS_LIST; m->cursor = 0; return redraw(); }
        if (ev == BTN_EV_LONG)  { m->st = MS_HOLD_PAIR; return redraw(); }
        break;
    case MS_HOLD_PAIR:
        if (ev == BTN_EV_RELEASE) { m->st = MS_HOME; return act(MA_PAIR, 0); }
        if (ev == BTN_EV_WARN)    { m->st = MS_HOLD_RESET; menu_action_t a = redraw(); a.full_refresh = true; return a; }
        break;
    case MS_HOLD_RESET:
        if (ev == BTN_EV_RELEASE) return go_home(m, MA_NONE, 0);       /* cancelled */
        if (ev == BTN_EV_VLONG)   { m->swallow_m = true; m->st = MS_HOME; return act(MA_FACTORY_RESET, 0); }
        break;
    default: break;
    }
    return (menu_action_t){0};
}

menu_action_t menu_key(menu_t *m, const menu_ctx_t *c, menu_key_t k, btn_event_t ev, uint64_t now)
{
    if (k == MENU_KEY_M && m->swallow_m) {
        if (ev == BTN_EV_RELEASE || ev == BTN_EV_PRESS) m->swallow_m = false;
        return (menu_action_t){0};
    }
    if (menu_is_home(m)) return key_home(m, k, ev);

    m->last_key_ms = now;
    if (k == MENU_KEY_COMBO) return (menu_action_t){0};
    const bool updown = (k == MENU_KEY_UP || k == MENU_KEY_DOWN) && (ev == BTN_EV_PRESS || ev == BTN_EV_REPEAT);
    const int dir = k == MENU_KEY_UP ? 1 : -1;
    const bool sel = k == MENU_KEY_M && ev == BTN_EV_PRESS;
    if (!updown && !sel && !(k == MENU_KEY_M && ev == BTN_EV_HOLD)) return (menu_action_t){0};

    switch (m->st) {
    case MS_LIST:
        if (updown) { m->cursor = (uint8_t)((m->cursor + MI_COUNT - dir) % MI_COUNT); return redraw(); }  /* UP = previous row */
        if (sel) {
            if (m->cursor == MI_BACK) return go_home(m, MA_NONE, 0);
            open_item(m, c);
            return redraw();
        }
        break;
    case MS_EDIT_MODE:
        if (updown) { m->edit = (m->edit + 4 + dir) % 4; return redraw(); }
        if (sel) { m->st = MS_LIST; return act(MA_SET_MODE, MODES[m->edit]); }
        break;
    case MS_EDIT_SWING:
        if (updown) { m->edit = clamp32(m->edit + dir * 10, CTRL_SWING_MIN, CTRL_SWING_MAX); return redraw(); }
        if (sel) { m->st = MS_LIST; return act(MA_SET_SWING, m->edit); }
        break;
    case MS_EDIT_UNITS:
        if (updown) { m->edit = !m->edit; return redraw(); }
        if (sel) { m->st = MS_LIST; return act(MA_SET_UNITS, m->edit); }
        break;
    case MS_BOOST: {
        uint8_t n = boost_count(c);
        if (m->bcursor >= n) m->bcursor = (uint8_t)(n - 1);
        if (updown) { m->bcursor = (uint8_t)((m->bcursor + n - dir) % n); return redraw(); }
        if (sel) {
            switch (boost_item(c, m->bcursor)) {
            case MB_TARGET:
                m->st = MS_EDIT_BOOST_SP;
                m->edit = clamp32(menu_step_temp(c->boost_sp, 0, c->fahrenheit), SP_MIN, SP_MAX);
                return redraw();
            case MB_TIMED: {
                int32_t v = (c->boost_minutes + MENU_BOOST_MIN_STEP / 2) / MENU_BOOST_MIN_STEP * MENU_BOOST_MIN_STEP;
                m->edit = clamp32(v, MENU_BOOST_MIN_MIN, MENU_BOOST_MIN_MAX);
                m->st = MS_EDIT_BOOST_MIN;
                return redraw();
            }
            case MB_CANCEL: return go_home(m, MA_BOOST_CANCEL, 0);
            default: m->st = MS_LIST; return redraw();
            }
        }
        break;
    }
    case MS_EDIT_BOOST_SP:
        if (updown) {
            int32_t v = menu_step_temp((int16_t)m->edit, dir, c->fahrenheit);
            if (v >= SP_MIN && v <= SP_MAX) m->edit = v;
            return redraw();
        }
        if (sel) return go_home(m, MA_BOOST_TARGET, m->edit);
        break;
    case MS_EDIT_BOOST_MIN:
        if (updown) { m->edit = clamp32(m->edit + dir * MENU_BOOST_MIN_STEP, MENU_BOOST_MIN_MIN, MENU_BOOST_MIN_MAX); return redraw(); }
        if (sel) return go_home(m, MA_BOOST_TIMED, m->edit);
        break;
    case MS_ZIGBEE:
        if (k == MENU_KEY_M && ev == BTN_EV_HOLD) { m->swallow_m = true; return go_home(m, MA_PAIR, 0); }
        if (sel) { m->st = MS_LIST; return redraw(); }
        break;
    case MS_INFO:
        if (sel) { m->st = MS_LIST; return redraw(); }
        break;
    default: break;
    }
    return (menu_action_t){0};
}

menu_action_t menu_tick(menu_t *m, uint64_t now)
{
    if (!menu_is_home(m) && now - m->last_key_ms >= MENU_TIMEOUT_MS) return go_home(m, MA_NONE, 0);
    return (menu_action_t){0};
}

/* ---------------- view ---------------- */

static void fmt_swing(char *o, int n, int32_t sw, bool f)
{
    int d = f ? (int)((sw * 18 + 50) / 100) : (int)((sw + 5) / 10);   /* tenths of the display unit */
    snprintf(o, n, "%d.%d", d / 10, d % 10);
}

static const char *mode_name(ctrl_mode_t md)
{
    switch (md) {
    case CTRL_MODE_HEAT: return "Heat";
    case CTRL_MODE_COOL: return "Cool";
    case CTRL_MODE_AUTO: return "Auto";
    default:             return "Off";
    }
}

static const char *link_name(link_state_t l)
{
    switch (l) {
    case LINK_JOINED:  return "connected";
    case LINK_PAIRING: return "pairing";
    case LINK_LOST:    return "lost parent";
    default:           return "not paired";
    }
}

static void row(menu_view_t *v, const char *l, const char *val)
{
    if (v->n_rows >= VIEW_MAX_ROWS) return;
    snprintf(v->label[v->n_rows], sizeof v->label[0], "%s", l);
    snprintf(v->value[v->n_rows], sizeof v->value[0], "%s", val ? val : "");
    v->n_rows++;
}

void menu_view(const menu_t *m, const menu_ctx_t *c, menu_view_t *v)
{
    memset(v, 0, sizeof *v);
    const bool F = c->fahrenheit;
    const char *U = F ? "\x7f" "F" : "\x7f" "C";
    char b[16];
    switch (m->st) {
    case MS_HOME: v->kind = VIEW_HOME; return;
    case MS_HOLD_PAIR:
        v->kind = VIEW_HOME_BANNER;
        snprintf(v->hint, sizeof v->hint, "Let go: pair  |  hold on: reset");
        return;
    case MS_HOLD_RESET:
        v->kind = VIEW_WARNING;
        snprintf(v->title, sizeof v->title, "FACTORY RESET");
        row(v, "Keep holding M to reset", NULL);
        row(v, "Release to cancel", NULL);
        row(v, "Erases settings + Zigbee", NULL);
        return;
    case MS_LIST:
        v->kind = VIEW_LIST;
        snprintf(v->title, sizeof v->title, "Menu");
        row(v, "Mode", mode_name(c->mode));
        row(v, "Boost", c->boost_active ? "running" : "");
        fmt_swing(b, sizeof b, c->swing, F);
        { char s2[32]; snprintf(s2, sizeof s2, "+/-%s%s", b, U); row(v, "Swing", s2); }
        row(v, "Units", U);
        row(v, "Zigbee", link_name(c->link));
        row(v, "Device info", "");
        row(v, "Back", "");
        v->sel = m->cursor;
        break;
    case MS_BOOST:
        v->kind = VIEW_LIST;
        snprintf(v->title, sizeof v->title, "Boost");
        for (uint8_t i = 0; i < boost_count(c); i++) {
            switch (boost_item(c, i)) {
            case MB_TARGET: format_temp(b, sizeof b, c->boost_sp, F, !F); strcat(b, U); row(v, "To target", b); break;
            case MB_TIMED:  row(v, "For N minutes", ""); break;
            case MB_CANCEL: row(v, "Cancel boost", ""); break;
            default:        row(v, "Back", ""); break;
            }
        }
        v->sel = m->bcursor < v->n_rows ? m->bcursor : (uint8_t)(v->n_rows - 1);
        break;
    case MS_EDIT_MODE:
        v->kind = VIEW_VALUE; v->big_is_text = true;
        snprintf(v->title, sizeof v->title, "Mode");
        snprintf(v->big, sizeof v->big, "%s", mode_name(MODES[m->edit]));
        break;
    case MS_EDIT_SWING:
        v->kind = VIEW_VALUE;
        snprintf(v->title, sizeof v->title, "Swing (+/-)");
        fmt_swing(v->big, sizeof v->big, m->edit, F);
        snprintf(v->big_unit, sizeof v->big_unit, "%s", U);
        break;
    case MS_EDIT_UNITS:
        v->kind = VIEW_VALUE;
        snprintf(v->title, sizeof v->title, "Units");
        snprintf(v->big, sizeof v->big, "%s", m->edit ? "\x7f" "F" : "\x7f" "C");
        break;
    case MS_EDIT_BOOST_SP:
        v->kind = VIEW_VALUE;
        snprintf(v->title, sizeof v->title, "Boost to");
        format_temp(v->big, sizeof v->big, (int16_t)m->edit, F, !F);
        snprintf(v->big_unit, sizeof v->big_unit, "%s", U);
        snprintf(v->hint, sizeof v->hint, "Up/Down: change   M: start");
        return;
    case MS_EDIT_BOOST_MIN:
        v->kind = VIEW_VALUE;
        snprintf(v->title, sizeof v->title, "Boost for");
        snprintf(v->big, sizeof v->big, "%ld", (long)m->edit);
        snprintf(v->big_unit, sizeof v->big_unit, "min");
        snprintf(v->hint, sizeof v->hint, "Up/Down: change   M: start");
        return;
    case MS_ZIGBEE:
        v->kind = VIEW_TEXT;
        snprintf(v->title, sizeof v->title, "Zigbee");
        row(v, "Status", link_name(c->link));
        row(v, "Hold M: pair", "");
        snprintf(v->hint, sizeof v->hint, "M: back");
        return;
    case MS_INFO: {
        v->kind = VIEW_TEXT;
        snprintf(v->title, sizeof v->title, "Device info");
        row(v, "Firmware", c->fw_version);
        if (c->raw_valid) {
            int t = c->raw_temp, a = t < 0 ? -t : t;
            snprintf(b, sizeof b, "%s%d.%02d" "\x7f" "C", t < 0 ? "-" : "", a / 100, a % 100);
            row(v, "Raw temp", b);
            snprintf(b, sizeof b, "%u.%02u%%", (unsigned)(c->raw_rh / 100), (unsigned)(c->raw_rh % 100));
            row(v, "Raw RH", b);
        } else {
            row(v, "Raw temp", "--");
            row(v, "Raw RH", "--");
        }
        uint32_t s = c->uptime_s;
        snprintf(b, sizeof b, "%lud %02lu:%02lu", (unsigned long)(s / 86400), (unsigned long)(s / 3600 % 24),
                 (unsigned long)(s / 60 % 60));
        row(v, "Uptime", b);
        snprintf(v->hint, sizeof v->hint, "M: back");
        return;
    }
    }
    if (!v->hint[0])
        snprintf(v->hint, sizeof v->hint, v->kind == VIEW_LIST ? "Up/Down: move   M: select" : "Up/Down: change   M: save");
}
