#include "gfx.h"
#include <string.h>

/* Landscape (x right, y down) -> native portrait RAM.
 * EPD_ROTATION 1: FPC on the right edge seen from the front (panel rotated
 * 90 deg clockwise from its native portrait orientation). Flip with
 * -DEPD_ROTATION=3 if the image comes out upside down. */
#ifndef EPD_ROTATION
#define EPD_ROTATION 1
#endif

static inline bool map(int x, int y, int *nx, int *ny)
{
    if (x < 0 || y < 0 || x >= GFX_W || y >= GFX_H) return false;
#if EPD_ROTATION == 1
    *nx = EPD_NATIVE_W - 1 - y;
    *ny = x;
#else
    *nx = y;
    *ny = EPD_NATIVE_H - 1 - x;
#endif
    return true;
}

void gfx_clear(gfx_fb_t *fb) { memset(fb->buf, 0xFF, sizeof fb->buf); }

void gfx_pixel(gfx_fb_t *fb, int x, int y, bool black)
{
    int nx, ny;
    if (!map(x, y, &nx, &ny)) return;
    uint8_t *b = &fb->buf[ny * (EPD_NATIVE_W / 8) + nx / 8];
    uint8_t m = 0x80 >> (nx & 7);
    if (black) *b &= (uint8_t)~m; else *b |= m;
}

bool gfx_get(const gfx_fb_t *fb, int x, int y)
{
    int nx, ny;
    if (!map(x, y, &nx, &ny)) return false;
    return !(fb->buf[ny * (EPD_NATIVE_W / 8) + nx / 8] & (0x80 >> (nx & 7)));
}

void gfx_fill_rect(gfx_fb_t *fb, int x, int y, int w, int h, bool black)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) gfx_pixel(fb, i, j, black);
}

void gfx_hline(gfx_fb_t *fb, int x, int y, int w) { gfx_fill_rect(fb, x, y, w, 1, true); }
void gfx_vline(gfx_fb_t *fb, int x, int y, int h) { gfx_fill_rect(fb, x, y, 1, h, true); }

static const font_glyph_t *glyph(const font_t *f, char c)
{
    unsigned u = (unsigned char)c;
    if (u < 32 || u > 127) u = '?';
    const font_glyph_t *g = &f->glyphs[u - 32];
    if (!g->w) g = &f->glyphs[0]; /* space */
    return g;
}

int gfx_text_width(const font_t *f, const char *s)
{
    int w = 0;
    for (; *s; s++) w += glyph(f, *s)->w;
    return w;
}

int gfx_text(gfx_fb_t *fb, const font_t *f, int x, int y, const char *s, bool black)
{
    for (; *s; s++) {
        const font_glyph_t *g = glyph(f, *s);
        int bpr = (g->w + 7) / 8;
        const uint8_t *p = f->bits + g->off;
        if (g == &f->glyphs[0]) { x += g->w; continue; }
        for (int r = 0; r < f->height; r++)
            for (int c = 0; c < g->w; c++)
                if (p[r * bpr + c / 8] & (0x80 >> (c & 7))) gfx_pixel(fb, x + c, y + r, black);
        x += g->w;
    }
    return x;
}
