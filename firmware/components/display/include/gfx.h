/* gfx.h - tiny 1-bpp drawing on a landscape 296x128 canvas that maps onto
 * the SSD1680's native 128x296 (portrait) RAM layout. Pure C (host-testable). */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
#include "font.h"

#define GFX_W 296
#define GFX_H 128
#define EPD_NATIVE_W 128
#define EPD_NATIVE_H 296
#define EPD_FB_SIZE (EPD_NATIVE_W / 8 * EPD_NATIVE_H)   /* 4736 bytes */

/* Native RAM: 1 = white, 0 = black, row-major, 16 bytes per gate line. */
typedef struct { U8 buf[EPD_FB_SIZE]; } GfxFb;

void GfxClear(GfxFb *pFb);                          /* all white */
void GfxPixel(GfxFb *pFb, int x, int y, bool black);
bool GfxGet(const GfxFb *pFb, int x, int y);
void GfxFillRect(GfxFb *pFb, int x, int y, int w, int h, bool black);
void GfxHline(GfxFb *pFb, int x, int y, int w);
void GfxVline(GfxFb *pFb, int x, int y, int h);
int  GfxTextWidth(const Font *pF, const char *pS);
/* Draws text with its top-left at (x,y); returns end x. */
int  GfxText(GfxFb *pFb, const Font *pF, int x, int y, const char *pS, bool black);
