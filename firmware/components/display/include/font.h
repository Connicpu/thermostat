#pragma once
#include <stdint.h>
typedef struct { uint8_t w; uint16_t off; } font_glyph_t;
typedef struct { uint8_t height; const font_glyph_t *glyphs; const uint8_t *bits; } font_t;
extern const font_t font_small, font_med, font_big;
#define FONT_DEGREE "\x7f"
