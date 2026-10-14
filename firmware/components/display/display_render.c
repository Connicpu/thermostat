#include "display_model.h"
#include <stdio.h>
#include <string.h>

static int32_t round_div(int32_t v, int32_t d) { return v >= 0 ? (v + d / 2) / d : -((-v + d / 2) / d); }

int temp_to_display(int16_t c100, bool f, int *tenths)
{
    int32_t v = f ? (int32_t)c100 * 9 / 5 + 3200 : c100;  /* hundredths of the display unit */
    int32_t t = round_div(v, 10);
    if (tenths) *tenths = (int)((t < 0 ? -t : t) % 10);
    return (int)(t / 10);
}

void format_temp(char *out, int n, int16_t c100, bool f, bool with_tenths)
{
    int32_t v = f ? (int32_t)c100 * 9 / 5 + 3200 : c100;
    if (with_tenths) {
        int32_t t = round_div(v, 10);
        int32_t a = t < 0 ? -t : t;
        snprintf(out, n, "%s%ld.%ld", t < 0 ? "-" : "", (long)(a / 10), (long)(a % 10));
    } else {
        snprintf(out, n, "%ld", (long)round_div(v, 100));
    }
}

static const char *mode_str(ctrl_mode_t m)
{
    switch (m) {
    case CTRL_MODE_HEAT: return "HEAT";
    case CTRL_MODE_COOL: return "COOL";
    default:             return "OFF";
    }
}

static const char *link_str(link_state_t l)
{
    switch (l) {
    case LINK_JOINED:  return "Zigbee: connected";
    case LINK_PAIRING: return "Zigbee: pairing...";
    case LINK_LOST:    return "Zigbee: lost parent";
    default:           return "Zigbee: not paired";
    }
}

/* ---------------- menu views ---------------- */

#define TITLE_H   22
#define ROW_Y0    24
#define ROW_H     20
#define ROWS_VIS  4
#define HINT_Y    108

static void title_bar(gfx_fb_t *fb, const char *t, const char *right)
{
    gfx_fill_rect(fb, 0, 0, GFX_W, TITLE_H, true);
    gfx_text(fb, &font_small, 6, 2, t, false);
    if (right) gfx_text(fb, &font_small, GFX_W - 6 - gfx_text_width(&font_small, right), 2, right, false);
}

static void hint_line(gfx_fb_t *fb, const char *h)
{
    gfx_hline(fb, 0, HINT_Y - 4, GFX_W);
    gfx_text(fb, &font_small, 6, HINT_Y, h, true);
}

static void render_rows(gfx_fb_t *fb, const menu_view_t *v, bool select)
{
    int first = 0;
    if (select && v->sel >= ROWS_VIS) first = v->sel - ROWS_VIS + 1;
    for (int i = first; i < v->n_rows && i < first + ROWS_VIS; i++) {
        int y = ROW_Y0 + (i - first) * ROW_H;
        bool inv = select && i == v->sel;
        if (inv) gfx_fill_rect(fb, 0, y - 1, GFX_W, ROW_H, true);
        gfx_text(fb, &font_small, 10, y, v->label[i], !inv);
        if (v->value[i][0])
            gfx_text(fb, &font_small, GFX_W - 10 - gfx_text_width(&font_small, v->value[i]), y, v->value[i], !inv);
    }
    /* scroll marks on the right edge when rows are hidden */
    if (select && first > 0) gfx_fill_rect(fb, GFX_W - 4, ROW_Y0, 3, 6, true);
    if (select && first + ROWS_VIS < v->n_rows) gfx_fill_rect(fb, GFX_W - 4, ROW_Y0 + ROWS_VIS * ROW_H - 8, 3, 6, true);
}

static void render_view(gfx_fb_t *fb, const menu_view_t *v)
{
    char pos[8];
    gfx_clear(fb);
    switch (v->kind) {
    case VIEW_LIST:
        snprintf(pos, sizeof pos, "%u/%u", (unsigned)(v->sel + 1), (unsigned)v->n_rows);
        title_bar(fb, v->title, pos);
        render_rows(fb, v, true);
        break;
    case VIEW_TEXT:
        title_bar(fb, v->title, NULL);
        render_rows(fb, v, false);
        break;
    case VIEW_VALUE: {
        title_bar(fb, v->title, NULL);
        const font_t *f = v->big_is_text ? &font_med : &font_big;
        int w = gfx_text_width(f, v->big), uw = v->big_unit[0] ? gfx_text_width(&font_med, v->big_unit) + 4 : 0;
        int x = (GFX_W - w - uw) / 2, y = v->big_is_text ? 48 : 34;
        gfx_text(fb, f, x, y, v->big, true);
        if (uw) gfx_text(fb, &font_med, x + w + 4, y + 6, v->big_unit, true);
        /* up/down arrows either side */
        for (int i = 0; i < 7; i++) {
            gfx_hline(fb, 20 - i, 50 + i, 1 + 2 * i);
            gfx_hline(fb, GFX_W - 20 - i, 70 - i, 1 + 2 * i);
        }
        break;
    }
    case VIEW_WARNING: {
        gfx_fill_rect(fb, 0, 0, GFX_W, GFX_H, true);
        int w = gfx_text_width(&font_med, v->title);
        gfx_text(fb, &font_med, (GFX_W - w) / 2, 6, v->title, false);
        for (int i = 0; i < v->n_rows; i++) {
            w = gfx_text_width(&font_small, v->label[i]);
            gfx_text(fb, &font_small, (GFX_W - w) / 2, 46 + i * 24, v->label[i], false);
        }
        return;
    }
    default: break;
    }
    if (v->hint[0]) hint_line(fb, v->hint);
}

void display_render(gfx_fb_t *fb, const display_model_t *m)
{
    if (m->view.kind != VIEW_HOME && m->view.kind != VIEW_HOME_BANNER) { render_view(fb, &m->view); return; }
    char b[32];
    gfx_clear(fb);
    const bool F = m->fahrenheit;

    /* ---- left: big room temperature ---- */
    if (m->temp_valid) {
        format_temp(b, sizeof b, m->temp, F, true);
        int x = gfx_text(fb, &font_big, 4, 2, b, true);
        gfx_text(fb, &font_med, x + 2, 8, F ? FONT_DEGREE "F" : FONT_DEGREE "C", true);
    } else {
        gfx_text(fb, &font_big, 4, 2, "--.-", true);
    }
    if (m->rh_valid) {
        snprintf(b, sizeof b, "%u%% RH", (unsigned)((m->rh + 50) / 100));
        gfx_text(fb, &font_small, 8, 66, b, true);
    }
    if (m->swing > 0 && m->s.mode != CTRL_MODE_OFF) {
        /* swing is a temperature *difference*: °F = °C * 1.8, no offset */
        int d = F ? (m->swing * 18 + 50) / 100 : (m->swing + 5) / 10;   /* tenths */
        snprintf(b, sizeof b, "%s%d.%d" FONT_DEGREE, "+/-", d / 10, d % 10);
        gfx_text(fb, &font_small, 96, 66, b, true);
    }

    /* program period (left column, under RH) */
    if (m->s.mode != CTRL_MODE_OFF) {
        const char *ps = NULL;
        switch (m->sched_state) {
        case 0:  ps = "Day (no clock)"; break;
        case 1:  ps = "Day"; break;
        case 2:  ps = "Night"; break;
        case 3:  ps = "Day, morning boost"; break;
        default: break;
        }
        if (ps) gfx_text(fb, &font_small, 8, 84, ps, true);
    }

    /* ---- right column: mode, setpoint(s), running state ---- */
    const int rx = 176;
    gfx_vline(fb, rx - 8, 4, 92);
    gfx_text(fb, &font_med, rx, 2, mode_str(m->s.mode), true);
    if (m->s.fan == CTRL_FAN_ON) gfx_text(fb, &font_small, rx, 84, "Fan: on", true);

    char h[8], c[8];
    format_temp(h, sizeof h, m->s.heat_sp, F, !F);
    format_temp(c, sizeof c, m->s.cool_sp, F, !F);
    switch (m->s.mode) {
    /* "Set 21.5°" does not fit the column in medium font: drop "Set" in °C */
    case CTRL_MODE_HEAT: snprintf(b, sizeof b, "%s%s" FONT_DEGREE, F ? "Set " : "", h); break;
    case CTRL_MODE_COOL: snprintf(b, sizeof b, "%s%s" FONT_DEGREE, F ? "Set " : "", c); break;
    default:             b[0] = 0; break;
    }
    gfx_text(fb, &font_med, rx, 32, b, true);

    const char *st = "Idle";
    if (m->fault != CTRL_FAULT_NONE) st = ctrl_fault_str(m->fault);
    else if (m->w) st = "Heating";
    else if (m->y) st = "Cooling";
    else if (m->waiting) st = "Waiting";
    else if (m->g) st = "Fan";
    /* running state shown inverted when something is on */
    bool active = m->w || m->y || m->g || m->fault != CTRL_FAULT_NONE;
    int tw = gfx_text_width(&font_small, st);
    if (active) gfx_fill_rect(fb, rx - 2, 62, tw + 6, font_small.height + 2, true);
    gfx_text(fb, &font_small, rx + 1, 63, st, !active);

    /* ---- bottom status bar ---- */
    gfx_hline(fb, 0, 104, GFX_W);
    if (m->view.kind == VIEW_HOME_BANNER) {
        gfx_fill_rect(fb, 0, 105, GFX_W, GFX_H - 105, true);
        gfx_text(fb, &font_small, 4, 108, m->view.hint, false);
    } else if (m->banner) {
        gfx_text(fb, &font_small, 4, 108, m->banner, true);
    } else if (m->boost != BOOST_NONE) {
        const char *d = m->boost_dir == CTRL_MODE_COOL ? "cool" : "heat";
        if (m->boost == BOOST_TO_TARGET) {
            format_temp(h, sizeof h, m->boost_sp, F, !F);
            snprintf(b, sizeof b, "BOOST %s to %s" FONT_DEGREE "%s", d, h, F ? "F" : "C");
        } else {
            snprintf(b, sizeof b, "BOOST %s %u min left", d, (unsigned)m->boost_min_left);
        }
        /* inverted bar so a running boost is obvious from across the room */
        gfx_fill_rect(fb, 0, 105, GFX_W, GFX_H - 105, true);
        gfx_text(fb, &font_small, 4, 108, b, false);
    } else {
        gfx_text(fb, &font_small, 4, 108, link_str(m->link), true);
    }
}
