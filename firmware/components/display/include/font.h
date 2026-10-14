#pragma once
#include "core.h"
#include <stdint.h>
typedef struct { U8 w; U16 off; } FontGlyph;
typedef struct { U8 height; const FontGlyph *pGlyphs; const U8 *pBits; } Font;
extern const Font kFontSmall, kFontMed, kFontBig;
#define FONT_DEGREE "\x7f"
