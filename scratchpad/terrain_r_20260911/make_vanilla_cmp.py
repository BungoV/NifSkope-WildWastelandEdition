"""Our pyramid's level-0 colour beside VANILLA's own shipped far sheet, for the
same ground, at the same density, in one picture (`ww-vanilla-compare`).

The two are directly comparable and that is not an assumption: vanilla's dim-4
chunk sheet is 512 texels over 4 cells = 32 world units a texel, and the
pyramid's default finest level is dim 2 at 256 content texels = 32 world units a
texel. So one pyramid tile is EXACTLY one quadrant of one vanilla chunk sheet,
texel for texel, with no resampling on either side -- which is what makes a
side-by-side honest rather than decorative.
"""

import os
import struct
import sys

sys.path.insert(0, 'tests/spells')
from PIL import Image, ImageDraw, ImageFont      # noqa: E402
from lodgen_terrain_model import Lodt, Dds       # noqa: E402

OUT = 'scratchpad/terrain_r_20260911/images'
VAN = (r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/'
       'Commonwealth.4.-20.24.DDS')
OURS = 'scratchpad/terrain_r_20260911/out/vt/Terrain/Commonwealth.VT.2.lodt'
CX0, CY0 = -20, 24          # the tile, by its south-west cell


def font(sz):
    for p in (r'C:\Windows\Fonts\consola.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


F, FS, FT = font(15), font(13), font(20)


def main():
    v = Lodt(OURS)
    tx = (CX0 - v.west) // v.levelDim
    ty = (v.north - (CY0 + v.levelDim - 1)) // v.levelDim
    idx = ty * v.tilesX + tx
    px, side = v.sheet(idx, 1, 0)
    C, B = v.content, v.border

    ours = Image.new('RGB', (C, C))
    d = ours.load()
    for j in range(C):
        for i in range(C):
            p = px[(B + j) * side + (B + i)]
            d[i, j] = (p[0], p[1], p[2])

    # the vanilla chunk: 512 texels over cells -20..-17 x 24..27, row 0 NORTH.
    # Our tile is cells -20..-19 x 24..25 -> the chunk's SOUTH-WEST quadrant,
    # columns 0..255 and rows 256..511. Derived, not eyeballed.
    t = Dds(VAN)
    lvl, w, h = t._level(0)
    vx0 = ((CX0 - (-20)) // 2) * 256
    vy0 = 512 - (((CY0 - 24) // 2) + 1) * 256
    van = Image.new('RGB', (256, 256))
    dv = van.load()
    for j in range(256):
        for i in range(256):
            c = lvl[(vy0 + j) * w + (vx0 + i)]
            dv[i, j] = tuple(min(255, max(0, int(round(c[k] * 255)))) for k in range(3))

    sc = 2
    ours = ours.resize((C * sc, C * sc), Image.NEAREST)
    van = van.resize((256 * sc, 256 * sc), Image.NEAREST)

    # the difference, x4, so a reader can see WHERE they differ rather than only
    # that they do
    diff = Image.new('RGB', (256 * sc, 256 * sc))
    dd = diff.load()
    do, dn = ours.load(), van.load()
    tot = 0
    for j in range(256 * sc):
        for i in range(256 * sc):
            a, b = do[i, j], dn[i, j]
            e = tuple(min(255, abs(a[k] - b[k]) * 4) for k in range(3))
            dd[i, j] = e
            if (i % sc) == 0 and (j % sc) == 0:
                tot += max(abs(a[k] - b[k]) for k in range(3))
    mean = tot / (256.0 * 256.0)

    pad, topH, capH = 16, 78, 60
    cellW = C * sc
    W = pad + 3 * (cellW + pad)
    H = topH + cellW + capH + pad
    im = Image.new('RGB', (W, H), (22, 22, 24))
    dr = ImageDraw.Draw(im)
    dr.text((pad, 12), 'Our far terrain beside Bethesda\'s, same ground, same density',
            font=FT, fill=(235, 235, 238))
    dr.text((pad, 36),
            'Left: the .lodt v2 pyramid, level 0, tile (%d,%d) -- cells %d..%d x %d..%d, '
            '256 content texels at 32 world units each.'
            % (tx, ty, CX0, CX0 + 1, CY0, CY0 + 1), font=FS, fill=(150, 152, 158))
    dr.text((pad, 51),
            'Middle: vanilla Commonwealth.4.-20.24.DDS, its south-west quadrant -- the '
            'SAME 256 x 256 texels at the same 32 units. Both magnified %dx.' % sc,
            font=FS, fill=(150, 152, 158))
    caps = [
        ('OURS: the pyramid colour sheet',
         'the grass tint folded in at 0.35', 'ground cover from the LTEX GNAM chain'),
        ('VANILLA: the shipped chunk sheet',
         'Bethesda\'s own graded bake', 'DXT5, as all 2,001 of them are'),
        ('|difference| x 4',
         'mean %.1f of 255 over the 65,536 texels' % mean,
         'this is a LIKENESS, not an identity gate'),
    ]
    for k, img in enumerate((ours, van, diff)):
        cx = pad + k * (cellW + pad)
        im.paste(img, (cx, topH))
        dr.rectangle([cx, topH, cx + cellW - 1, topH + cellW - 1], outline=(70, 70, 76))
        ty2 = topH + cellW + 5
        for n, line in enumerate(caps[k]):
            dr.text((cx + 2, ty2), line, font=F if n == 0 else FS,
                    fill=(120, 190, 255) if n == 0 else (150, 152, 158))
            ty2 += 17 if n == 0 else 15
    p = os.path.join(OUT, 'ours_vs_vanilla_tile.png')
    im.save(p)
    print('wrote %s  %dx%d  meanDiff %.2f' % (p, W, H, mean))


if __name__ == '__main__':
    main()
