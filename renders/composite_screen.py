"""Swap the e-paper image on an existing Blender render without re-rendering.

Usage: python3 renders/composite_screen.py renders/v3-assembled.png firmware/docs/display_preview.png

render.py maps firmware/docs/display_preview.png onto the screen as an emission texture. When only the
screen content changes, this script redraws the active area in place: it warps the 2x preview onto the
screen quad (found by correlating the old preview with the render), keeps the render's own paper
shading (max filter removes the old strokes) and darkens ink to the render's measured ink/paper ratio.
Needs Pillow only. Re-render with render.py instead when the case or camera changes.
"""
import sys
from PIL import Image, ImageDraw, ImageFilter

# Screen active-area corners in the 1600x1100 'assembled' view (TL, TR, BR, BL of the preview image).
QUAD = [(515, 333), (1075, 534), (899, 687), (322, 454)]
INK_RATIO = 0.525  # rendered ink luminance / paper luminance, measured on the original render


def Coeffs(dst, src):
    """Perspective coefficients mapping output pixels in quad `dst` to source pixels `src`."""
    a, b = [], []
    for (x, y), (u, v) in zip(dst, src):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        b.append(u)
        a.append([0, 0, 0, x, y, 1, -v * x, -v * y])
        b.append(v)
    m = [a[i] + [b[i]] for i in range(8)]
    for c in range(8):
        p = max(range(c, 8), key=lambda r: abs(m[r][c]))
        m[c], m[p] = m[p], m[c]
        d = m[c][c]
        m[c] = [v / d for v in m[c]]
        for r in range(8):
            if r != c:
                f = m[r][c]
                m[r] = [x - f * y for x, y in zip(m[r], m[c])]
    return [m[i][8] for i in range(8)]


def main(renderPath, screenPath):
    rgb = Image.open(renderPath).convert('RGB')
    scr = Image.open(screenPath).convert('L')
    w, h = rgb.size
    sw, sh = scr.size
    warped = scr.transform((w, h), Image.PERSPECTIVE, Coeffs(QUAD, [(0, 0), (sw, 0), (sw, sh), (0, sh)]), Image.BICUBIC)
    mask = Image.new('L', (w, h), 0)
    ImageDraw.Draw(mask).polygon(QUAD, fill=255)
    mask = mask.filter(ImageFilter.MinFilter(5))  # leave the bezel edge shading untouched
    paper = rgb.filter(ImageFilter.MaxFilter(15)).filter(ImageFilter.GaussianBlur(6))
    out, pp, wp, mp = rgb.copy(), paper.load(), warped.load(), mask.load()
    op = out.load()
    for y in range(h):
        for x in range(w):
            if mp[x, y]:
                f = INK_RATIO + (1 - INK_RATIO) * wp[x, y] / 255.0
                op[x, y] = tuple(int(pp[x, y][c] * f) for c in range(3))
    out.save(renderPath, optimize=True)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
