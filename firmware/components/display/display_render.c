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
    case CTRL_MODE_AUTO: return "AUTO";
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

void display_render(gfx_fb_t *fb, const display_model_t *m)
{
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

    /* ---- right column: mode, setpoint(s), running state ---- */
    const int rx = 176;
    gfx_vline(fb, rx - 8, 4, 92);
    gfx_text(fb, &font_med, rx, 2, mode_str(m->s.mode), true);
    if (m->s.fan == CTRL_FAN_ON) gfx_text(fb, &font_small, rx, 84, "Fan: on", true);

    char h[8], c[8];
    format_temp(h, sizeof h, m->s.heat_sp, F, !F);
    format_temp(c, sizeof c, m->s.cool_sp, F, !F);
    switch (m->s.mode) {
    case CTRL_MODE_HEAT: snprintf(b, sizeof b, "Set %s" FONT_DEGREE, h); break;
    case CTRL_MODE_COOL: snprintf(b, sizeof b, "Set %s" FONT_DEGREE, c); break;
    case CTRL_MODE_AUTO: snprintf(b, sizeof b, "%s - %s" FONT_DEGREE, h, c); break;
    default:             b[0] = 0; break;
    }
    gfx_text(fb, m->s.mode == CTRL_MODE_AUTO && !F ? &font_small : &font_med, rx, 32, b, true);

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
    if (m->banner) {
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
