#pragma once
#include "core.h"
#include <stdint.h>

/**
 * One glyph: w = advance width in pixels (0 = not in the font, drawn as a space),
 * off = byte offset of its bitmap in Font.pBits. Bitmaps are row-major, MSB first, (w+7)/8 bytes
 * per row, Font.height rows.
 */
typedef struct
{
    U8 w;
    U16 off;
} FontGlyph;

/**
 * A generated bitmap font (tools/gen_font.py): 96 glyphs for chars 32..127 (127 = degree sign),
 * all of the same height in pixels.
 */
typedef struct
{
    U8 height;
    const FontGlyph *pGlyphs;
    const U8 *pBits;
} Font;
extern const Font kFontSmall, kFontMed, kFontBig;
#define FONT_DEGREE "\x7f"
