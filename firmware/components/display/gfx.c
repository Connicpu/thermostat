#include "core.h"
#include "gfx.h"
#include <string.h>

/* Landscape (x right, y down) -> native portrait RAM.
 * EPD_ROTATION 1: FPC on the right edge seen from the front (panel rotated
 * 90 deg clockwise from its native portrait orientation). Flip with
 * -DEPD_ROTATION=3 if the image comes out upside down. */
#ifndef EPD_ROTATION
#define EPD_ROTATION 1
#endif

static inline bool Map(int x, int y, int *pNx, int *pNy)
{
    if (x < 0 || y < 0 || x >= GFX_W || y >= GFX_H) return false;
#if EPD_ROTATION == 1
    *pNx = EPD_NATIVE_W - 1 - y;
    *pNy = x;
#else
    *pNx = y;
    *pNy = EPD_NATIVE_H - 1 - x;
#endif
    return true;
}

void GfxClear(GfxFb *pFb) { memset(pFb->buf, 0xFF, sizeof pFb->buf); }

void GfxPixel(GfxFb *pFb, int x, int y, bool black)
{
    int nx, ny;
    if (!Map(x, y, &nx, &ny)) return;
    U8 *pB = &pFb->buf[ny * (EPD_NATIVE_W / 8) + nx / 8];
    U8 m = 0x80 >> (nx & 7);
    if (black) *pB &= (U8)~m; else *pB |= m;
}

bool GfxGet(const GfxFb *pFb, int x, int y)
{
    int nx, ny;
    if (!Map(x, y, &nx, &ny)) return false;
    return !(pFb->buf[ny * (EPD_NATIVE_W / 8) + nx / 8] & (0x80 >> (nx & 7)));
}

void GfxFillRect(GfxFb *pFb, int x, int y, int w, int h, bool black)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) GfxPixel(pFb, i, j, black);
}

void GfxHline(GfxFb *pFb, int x, int y, int w) { GfxFillRect(pFb, x, y, w, 1, true); }
void GfxVline(GfxFb *pFb, int x, int y, int h) { GfxFillRect(pFb, x, y, 1, h, true); }

static const FontGlyph *Glyph(const Font *pF, char c)
{
    unsigned u = (unsigned char)c;
    if (u < 32 || u > 127) u = '?';
    const FontGlyph *pG = &pF->pGlyphs[u - 32];
    if (!pG->w) pG = &pF->pGlyphs[0]; /* space */
    return pG;
}

int GfxTextWidth(const Font *pF, const char *pS)
{
    int w = 0;
    for (; *pS; pS++) w += Glyph(pF, *pS)->w;
    return w;
}

int GfxText(GfxFb *pFb, const Font *pF, int x, int y, const char *pS, bool black)
{
    for (; *pS; pS++) {
        const FontGlyph *pG = Glyph(pF, *pS);
        int bpr = (pG->w + 7) / 8;
        const U8 *pP = pF->pBits + pG->off;
        if (pG == &pF->pGlyphs[0]) { x += pG->w; continue; }
        for (int r = 0; r < pF->height; r++)
            for (int c = 0; c < pG->w; c++)
                if (pP[r * bpr + c / 8] & (0x80 >> (c & 7))) GfxPixel(pFb, x + c, y + r, black);
        x += pG->w;
    }
    return x;
}
