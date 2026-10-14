/* gfx.h - tiny 1-bpp drawing on a landscape 296x128 canvas that maps onto
 * the SSD1680's native 128x296 (portrait) RAM layout. Pure C (host-testable). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "font.h"

#define GFX_W        296
#define GFX_H        128
#define EPD_NATIVE_W 128
#define EPD_NATIVE_H 296
#define EPD_FB_SIZE  (EPD_NATIVE_W / 8 * EPD_NATIVE_H) /* 4736 bytes */

/**
 * Frame buffer in the SSD1680's native RAM layout: 1 = white, 0 = black, row-major, 16 bytes
 * (128 px) per gate line, 296 lines. Drawing uses landscape coordinates (GFX_W x GFX_H).
 */
typedef struct
{
    U8 buf[EPD_FB_SIZE];
} GfxFb;

/** Fill the frame with white. */
void GfxClear(GfxFb *pFb);

/** Set one landscape pixel (x right, y down) to black or white. Out-of-range pixels are ignored. */
void GfxPixel(GfxFb *pFb, int x, int y, bool black);

/** Returns true if the landscape pixel is black (false outside the canvas). */
bool GfxGet(const GfxFb *pFb, int x, int y);

/** Fill a w x h rectangle with its top-left at (x,y), clipped to the canvas. */
void GfxFillRect(GfxFb *pFb, int x, int y, int w, int h, bool black);

/** Draw a black horizontal line of w pixels starting at (x,y). */
void GfxHline(GfxFb *pFb, int x, int y, int w);

/** Draw a black vertical line of h pixels starting at (x,y). */
void GfxVline(GfxFb *pFb, int x, int y, int h);

/** Returns the width in pixels of the string in font pF (unknown chars count as '?' or a space). */
int GfxTextWidth(const Font *pF, const char *pS);

/**
 * Draw text with its top-left at (x,y), clipped to the canvas. Characters outside 32..127 are
 * drawn as '?', and glyphs missing from the font as a space. Only set pixels are drawn
 * (transparent background).
 *
 * `black` is the ink colour.
 *
 * Returns the x just past the last glyph.
 */
int GfxText(GfxFb *pFb, const Font *pF, int x, int y, const char *pS, bool black);
