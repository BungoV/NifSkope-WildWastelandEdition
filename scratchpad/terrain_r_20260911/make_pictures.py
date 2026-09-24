"""The three texel pictures for lane TERRAIN-R (`ww-texel-picture`).

Each picture is built on a FIXED CELL GRID -- one cell per panel, captions in a
band of their own under each cell -- so a caption can never clip or overlap the
texels it describes, and the number a caption quotes is computed from the SAME
array the panel is drawn from rather than re-derived.
"""

import os
import sys

sys.path.insert(0, 'tests/spells')
from PIL import Image, ImageDraw, ImageFont       # noqa: E402
from lodgen_terrain_model import Lodt             # noqa: E402

OUT = 'scratchpad/terrain_r_20260911/images'
os.makedirs(OUT, exist_ok=True)

BG = (22, 22, 24)
FG = (235, 235, 238)
DIM = (150, 152, 158)
ACC = (120, 190, 255)


def font(sz):
    for p in (r'C:\Windows\Fonts\consola.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


F = font(15)
FS = font(13)
FT = font(20)


def panel_from(px, side, x0, y0, w, h, chan, scale):
    """One channel of a decoded sheet as a nearest-neighbour magnified image."""
    im = Image.new('RGB', (w * scale, h * scale))
    d = im.load()
    for j in range(h):
        for i in range(w):
            p = px[(y0 + j) * side + (x0 + i)]
            v = p[chan] if chan >= 0 else 0
            c = (v, v, v)
            for sy in range(scale):
                for sx in range(scale):
                    d[i * scale + sx, j * scale + sy] = c
    return im


def rgb_panel(px, side, x0, y0, w, h, scale):
    im = Image.new('RGB', (w * scale, h * scale))
    d = im.load()
    for j in range(h):
        for i in range(w):
            p = px[(y0 + j) * side + (x0 + i)]
            c = (p[0], p[1], p[2])
            for sy in range(scale):
                for sx in range(scale):
                    d[i * scale + sx, j * scale + sy] = c
    return im


def compose(cells, cols, cellW, imgH, capH, title, sub, path):
    """A fixed grid: every cell the same width, captions in their own band."""
    rows = (len(cells) + cols - 1) // cols
    pad = 16
    topH = 58 + 15 * max(0, (len(sub) // 110))
    W = pad + cols * (cellW + pad)
    H = topH + rows * (imgH + capH + pad) + pad
    im = Image.new('RGB', (W, H), BG)
    dr = ImageDraw.Draw(im)
    dr.text((pad, 12), title, font=FT, fill=FG)
    # the subtitle WRAPS to the picture's own width; a subtitle that ran off the
    # right edge is the clip this layout exists to prevent
    words = sub.split()
    line, ly = '', 36
    for w in words:
        t = (line + ' ' + w).strip()
        if dr.textlength(t, font=FS) > W - 2 * pad:
            dr.text((pad, ly), line, font=FS, fill=DIM)
            ly += 15
            line = w
        else:
            line = t
    if line:
        dr.text((pad, ly), line, font=FS, fill=DIM)
    for k, (img, caps) in enumerate(cells):
        cx = pad + (k % cols) * (cellW + pad)
        cy = topH + (k // cols) * (imgH + capH + pad)
        im.paste(img, (cx + (cellW - img.width) // 2, cy))
        dr.rectangle([cx, cy, cx + cellW - 1, cy + img.height - 1], outline=(70, 70, 76))
        ty = cy + img.height + 5
        for n, line in enumerate(caps):
            dr.text((cx + 2, ty), line, font=F if n == 0 else FS,
                    fill=ACC if n == 0 else DIM)
            ty += 17 if n == 0 else 15
    im.save(path)
    print('wrote %s  %dx%d' % (path, W, H))


def stats(px, side, x0, y0, w, h, chan):
    vs = [px[(y0 + j) * side + (x0 + i)][chan] for j in range(h) for i in range(w)]
    return min(vs), max(vs), sum(vs) / len(vs), len(set(vs))


# ---------------------------------------------------------------- 1. the mask
def pic_mask():
    v = Lodt('scratchpad/terrain_r_20260911/out/vt/Terrain/Commonwealth.VT.2.lodt')
    idx = next(i for i, e in enumerate(v.table) if e['flags'] & 1)
    px, side = v.sheet(idx, 5, 0)
    # a 96-texel window inside the CONTENT region, away from the border, where
    # the paint actually changes -- a flat window would prove nothing
    best, bx, by = -1, v.border, v.border
    for y in range(v.border, side - v.border - 96, 32):
        for x in range(v.border, side - v.border - 96, 32):
            s = stats(px, side, x, y, 96, 96, 0)[3] + stats(px, side, x, y, 96, 96, 3)[3]
            if s > best:
                best, bx, by = s, x, y
    W = 96
    sc = 3
    names = [('R roughness', 0, '1 - smoothness x the _s GREEN channel'),
             ('G metallic', 1, 'PBRM only; legacy contributes 0'),
             ('B AO', 2, '8-direction 2,048-unit sky horizon march'),
             ('A ground cover', 3, 'cover / COVER_FULL, which is 96')]
    cells = []
    for nm, ch, law in names:
        mn, mx, av, dis = stats(px, side, bx, by, W, W, ch)
        cells.append((panel_from(px, side, bx, by, W, W, ch, sc),
                      ['%s' % nm, law,
                       'min %d  max %d  mean %.1f  distinct %d' % (mn, mx, av, dis)]))
    compose(cells, 4, W * sc, W * sc, 56,
            'The .lodt v2 MASK sheet, one tile, %d x %d texels at 32 units each'
            % (W, W),
            'Commonwealth.VT.2.lodt tile %d, mip 0, content texels (%d,%d)..(%d,%d). '
            'Greyscale = the raw channel byte, magnified %dx, nearest neighbour.'
            % (idx, bx, by, bx + W - 1, by + W - 1, sc),
            os.path.join(OUT, 'mask_sheet_tile.png'))


# ------------------------------------------------------------ 2. the emissive
def pic_emissive():
    a = Lodt('scratchpad/terrain_r_20260911/out/pbr/Terrain/Commonwealth.VT.2.lodt')
    b = Lodt('scratchpad/terrain_r_20260911/out/vt/Terrain/Commonwealth.VT.2.lodt')
    ia = next(i for i, e in enumerate(a.table) if e['flags'] & 1)
    ib = next(i for i, e in enumerate(b.table) if e['flags'] & 1)
    ra = [s['role'] for s in a.sheets[:a.sheetCount]]
    rb = [s['role'] for s in b.sheets[:b.sheetCount]]
    em, side = a.sheet(ia, 6, 0)
    W, sc = 96, 3
    x0 = y0 = a.border + 40
    cells = []
    cells.append((rgb_panel(em, side, x0, y0, W, W, sc),
                  ['EMISSIVE sheet PRESENT',
                   'roles %s  sheetCount %d' % (ra, a.sheetCount),
                   'five layers carry a PBRM emissive map']))
    blank = Image.new('RGB', (W * sc, W * sc), (14, 14, 16))
    d = ImageDraw.Draw(blank)
    d.text((14, W * sc // 2 - 26), 'no role 6 in this container', font=F, fill=(200, 90, 90))
    d.text((14, W * sc // 2 - 6), 'terrain.emissive = "none"', font=FS, fill=DIM)
    d.text((14, W * sc // 2 + 12), 'nothing is written, not a black sheet', font=FS, fill=DIM)
    cells.append((blank,
                  ['EMISSIVE sheet ABSENT',
                   'roles %s  sheetCount %d' % (rb, b.sheetCount),
                   'the same region, no emissive map anywhere']))
    # the colour sheet of the same tile, so the reader sees what ground it is
    col, cside = b.sheet(ib, 1, 0)
    cells.append((rgb_panel(col, cside, x0, y0, W, W, sc),
                  ['the COLOUR sheet of the same texels',
                   'for orientation only',
                   'Sanctuary, cells -20..-19 x 24..25']))
    compose(cells, 3, W * sc, W * sc, 56,
            'The emissive sheet is written only when a layer supplies one',
            'Left: the fixture bake, five landscape textures given a PBRM with an emissive map. Middle: the same region with none. Both %d x %d texels, magnified %dx.' % (W, W, sc),
            os.path.join(OUT, 'emissive_presence.png'))


# --------------------------------------------------------------- 3. the ring 0
def pic_ring0():
    import math
    from lodgen_cover_model import Esm, bilinear, dominant_base   # noqa: E402
    import lodgen_terrain_model as M                              # noqa: E402
    v = Lodt('scratchpad/terrain_r_20260911/out/vt0/Terrain/Commonwealth.VT.2.lodt')
    e = Esm(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm')
    e.walk(0x3C)
    data = r'E:/Tools/Fallout 4/DataUnpacked/Data'
    cx0, cy0 = -20, 24
    idx, tx, ty = M.tile_index(v, cx0, cy0)
    px, side = v.sheet(idx, 1, 0)
    upt = v.levelDim * 4096.0 / v.content
    domBase = dominant_base(e, (cx0 // 4) * 4, (cy0 // 4) * 4, 4)
    cache = {}
    W, sc = 64, 5
    x0 = y0 = v.border + 64
    tileN = (cy0 + v.levelDim) * 4096.0
    tileW = cx0 * 4096.0
    blend = Image.new('RGB', (W * sc, W * sc))
    baked = Image.new('RGB', (W * sc, W * sc))
    diff = Image.new('RGB', (W * sc, W * sc))
    db, dk, dd = blend.load(), baked.load(), diff.load()
    errs = []
    for j in range(W):
        for i in range(W):
            ii, jj = x0 + i, y0 + j
            wx = tileW + (ii - v.border + 0.5) * upt
            wy = tileN - (jj - v.border + 0.5) * upt
            c = M.composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt)
            p = px[jj * side + ii]
            mc = (0, 0, 0) if c is None else tuple(
                min(255, max(0, int(round(c['colour'][k] * 255)))) for k in range(3))
            d8 = tuple(min(255, abs(mc[k] - p[k]) * 8) for k in range(3))
            errs.append(max(abs(mc[k] - p[k]) for k in range(3)))
            for sy in range(sc):
                for sx in range(sc):
                    db[i * sc + sx, j * sc + sy] = mc
                    dk[i * sc + sx, j * sc + sy] = (p[0], p[1], p[2])
                    dd[i * sc + sx, j * sc + sy] = d8
    errs.sort()
    mean = sum(errs) / len(errs)
    cells = [
        (blend, ['the RUNTIME formula, blended here',
                 'contract 2.5, implemented independently',
                 'own ESM walk, own BC decode, own mip choice']),
        (baked, ['the PYRAMID, level 0, as baked',
                 'Commonwealth.VT.2.lodt colour sheet',
                 '--grass-tint 0, so the one term the model']),
        (diff, ['|difference| x 8',
                'mean %.2f  p95 %d  max %d of 255' % (mean, errs[int(len(errs) * .95)], errs[-1]),
                'does not carry is out of the picture']),
    ]
    compose(cells, 3, W * sc, W * sc, 56,
            'Ring 0: the runtime blend and the pyramid, on the same %d x %d texels' % (W, W),
            'Tile (%d,%d) of Commonwealth.VT.2.lodt, content texels (%d,%d)..(%d,%d), '
            'magnified %dx. The right panel is the absolute difference multiplied by '
            'EIGHT -- at 1x it is black.'
            % (tx, ty, x0, y0, x0 + W - 1, y0 + W - 1, sc),
            os.path.join(OUT, 'ring0_blend_vs_bake.png'))


if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if which in ('all', 'mask'):
        pic_mask()
    if which in ('all', 'emissive'):
        pic_emissive()
    if which in ('all', 'ring0'):
        pic_ring0()
