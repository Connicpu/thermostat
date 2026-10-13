/* gfx.h - tiny 1-bpp drawing on a landscape 296x128 canvas that maps onto
 * the SSD1680's native 128x296 (portrait) RAM layout. Pure C (host-testable). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "font.h"

#define GFX_W 296
#define GFX_H 128
#define EPD_NATIVE_W 128
#define EPD_NATIVE_H 296
#define EPD_FB_SIZE (EPD_NATIVE_W / 8 * EPD_NATIVE_H)   /* 4736 bytes */

/* Native RAM: 1 = white, 0 = black, row-major, 16 bytes per gate line. */
typedef struct { uint8_t buf[EPD_FB_SIZE]; } gfx_fb_t;

void gfx_clear(gfx_fb_t *fb);                          /* all white */
void gfx_pixel(gfx_fb_t *fb, int x, int y, bool black);
bool gfx_get(const gfx_fb_t *fb, int x, int y);
void gfx_fill_rect(gfx_fb_t *fb, int x, int y, int w, int h, bool black);
void gfx_hline(gfx_fb_t *fb, int x, int y, int w);
void gfx_vline(gfx_fb_t *fb, int x, int y, int h);
int  gfx_text_width(const font_t *f, const char *s);
/* Draws text with its top-left at (x,y); returns end x. */
int  gfx_text(gfx_fb_t *fb, const font_t *f, int x, int y, const char *s, bool black);
