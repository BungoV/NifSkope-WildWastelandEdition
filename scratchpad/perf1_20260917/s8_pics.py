#!/usr/bin/env python3
"""PERF1 step 8 -- the lane's pictures.

    python s8_pics.py bars                 the two stage-time charts
    python s8_pics.py cells <dirA> <dirB>  the library cells, 1 thread vs 8

Everything drawn here is read off disk by this lane. The bar data is typed from
the measured tables in the lane report (each cell is a `stage times:` reading
from a named log) and printed on the bars, so the picture cannot say anything
the report does not.

The cell picture is NOT a render of the game: it is the `.lodi` instance table
drawn as it is stored -- one tile a cell, each instance a disc at its own
quantised (px, py) with radius from `boundRadius * scale`, coloured by
`drawKey`, which is the thing the two fan-outs could most plausibly have
reordered. Two bakes, one picture each, and the third panel is their pixel
difference.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
OUT = os.path.join(HERE, 'images')
sys.path.insert(0, os.path.join(ROOT, 'tests', 'spells'))

BG = (24, 26, 30)
FG = (226, 228, 232)
DIM = (140, 146, 156)
BEFORE = (196, 122, 96)
AFTER = (104, 170, 214)


def font(sz):
    for p in (r'C:\Windows\Fonts\segoeui.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


# --------------------------------------------------------------------- bars
def bars(title, sub, stages, path):
    """stages: [(name, before_s, after_s), ...]"""
    f14, f12, f18 = font(14), font(12), font(18)
    W, H = 1000, 520
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((28, 22), title, font=f18, fill=FG)
    d.text((28, 50), sub, font=f12, fill=DIM)
    x0, y0, y1 = 150, 100, 430
    top = max(max(b, a) for _, b, a in stages) * 1.18 or 1.0
    span = (W - x0 - 60) / float(len(stages))
    bw = min(46, span / 2.6)
    for i, (name, b, a) in enumerate(stages):
        cx = x0 + span * i + span / 2
        for k, (v, col) in enumerate(((b, BEFORE), (a, AFTER))):
            h = (y1 - y0) * (v / top)
            x = cx - bw * 1.05 + k * bw * 1.1
            d.rectangle([x, y1 - h, x + bw, y1], fill=col)
            d.text((x + bw / 2, y1 - h - 16), '%.1f' % v, font=f12, fill=FG, anchor='ma')
        d.text((cx, y1 + 10), name, font=f14, fill=FG, anchor='ma')
    d.line([x0 - 90, y1, W - 40, y1], fill=DIM)
    for k, (lab, col) in enumerate((('before -- the exe at launch', BEFORE),
                                    ('after -- this lane', AFTER))):
        d.rectangle([40, 462 + k * 24, 62, 478 + k * 24], fill=col)
        d.text((72, 462 + k * 24), lab, font=f14, fill=FG)
    d.text((28, y0 - 22), 'seconds', font=f12, fill=DIM)
    im.save(path)
    print('wrote', path)


# -------------------------------------------------------------------- cells
def cell_image(nat_dir, tiles=4, tile=200):
    """One tile a CELL, each instance a disc.

    The disc's place is the instance's own world x,y, normalised inside the
    tile by that cell's own measured extent -- NOT by the stored px,py, which
    are quantised over the CHUNK (a cell fills a quarter of that range and the
    discs huddle in a corner). Its radius is the base's boundRadius times the
    instance's scale, and its colour is drawKey, which is the field the two
    fan-outs could most plausibly have reordered.
    """
    import lodgen_native_decode as nd
    lodi = lodo = None
    for r, _, fs in os.walk(nat_dir):
        for f in fs:
            if f.endswith('.lodi'):
                lodi = os.path.join(r, f)
            elif f.endswith('.lodo'):
                lodo = os.path.join(r, f)
    if not lodi or not lodo:
        raise SystemExit('no .lodo/.lodi pair under %s' % nat_dir)
    L = nd.read_lodo(lodo)
    T = nd.read_lodi(lodi)
    by = {}
    for r in T['instances']:
        by.setdefault(r['cell'], []).append(r)
    cells = sorted(by, key=lambda c: -len(by[c]))[:tiles * tiles]
    cells.sort()
    pad = 8
    W = H = pad + tiles * (tile + pad)
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    f11 = font(11)
    for n, c in enumerate(cells):
        ox = pad + (n % tiles) * (tile + pad)
        oy = pad + (n // tiles) * (tile + pad)
        d.rectangle([ox, oy, ox + tile, oy + tile], outline=(58, 62, 70))
        rows = by[c]
        xs = [r['x'] for r in rows]
        ys = [r['y'] for r in rows]
        x0, x1 = min(xs), max(xs)
        y0, y1 = min(ys), max(ys)
        sx = (tile - 24) / (x1 - x0) if x1 > x0 else 0.0
        sy = (tile - 24) / (y1 - y0) if y1 > y0 else 0.0
        for r in rows:
            x = ox + 12 + (r['x'] - x0) * sx
            y = oy + 12 + (y1 - r['y']) * sy
            b = L['bases'][r['baseId']] if r['baseId'] < len(L['bases']) else None
            rad = 1.6 + (b['boundRadius'] / 3000.0 if b else 0.0) * r['scaleF'] * 9.0
            rad = max(1.2, min(rad, tile / 6.0))
            k = r['drawKey']
            col = (70 + (k * 53) % 180, 80 + (k * 97) % 170, 90 + (k * 151) % 160)
            d.ellipse([x - rad, y - rad, x + rad, y + rad], outline=col)
        d.text((ox + 5, oy + 3), 'cell %d  %d' % (c, len(rows)), font=f11, fill=DIM)
    return im, len(T['instances']), len(cells)


def cells(a_dir, b_dir):
    ia, na, ca = cell_image(a_dir)
    ib, nb, cb = cell_image(b_dir)
    import numpy as np
    A = np.asarray(ia).astype(np.int16)
    B = np.asarray(ib).astype(np.int16)
    diff = np.abs(A - B).sum(axis=2)
    pct = 100.0 * float((diff > 0).sum()) / float(diff.shape[0] * diff.shape[1])
    dv = np.zeros_like(A, dtype=np.uint8)
    grid = (A.sum(axis=2) > 0) & (B.sum(axis=2) > 0) & (np.abs(A - B).sum(axis=2) == 0)
    dv[..., 0] = np.clip(diff, 0, 255)
    dv[..., 1] = np.where(diff > 0, 0, 22)
    dv[..., 2] = np.where(diff > 0, 0, 26)
    del grid
    dimg = Image.fromarray(dv)

    f12, f16 = font(12), font(16)
    w, h = ia.size
    pad, head = 14, 66
    out = Image.new('RGB', (pad + 3 * (w + pad), head + h + 44), BG)
    d = ImageDraw.Draw(out)
    d.text((pad, 14), 'The same library cells, 1 thread and 8, and their pixel difference',
           font=f16, fill=FG)
    d.text((pad, 38), '%d cells, %d instances, drawn from the .lodi instance table; '
                      'each disc one instance, coloured by drawKey' % (ca, na), font=f12, fill=DIM)
    for i, (img, lab) in enumerate(((ia, '--threads 1 --chunk-threads 1'),
                                    (ib, '--threads 0 --chunk-threads 8'),
                                    (dimg, 'difference: %.3f %% of pixels' % pct))):
        x = pad + i * (w + pad)
        out.paste(img, (x, head))
        d.text((x, head + h + 12), lab, font=f12, fill=FG if i < 2 else
               ((120, 220, 140) if pct == 0.0 else (230, 110, 110)))
    p = os.path.join(OUT, 'library_cells_1_vs_8.png')
    out.save(p)
    print('wrote %s  instances %d/%d  cells %d/%d  difference %.3f %%'
          % (p, na, nb, ca, cb, pct))
    return pct


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    if sys.argv[1] == 'cells':
        cells(sys.argv[2], sys.argv[3])
    else:
        import json
        spec = json.load(open(os.path.join(HERE, 'bars.json')))
        for b in spec:
            bars(b['title'], b['sub'], [tuple(r) for r in b['rows']],
                 os.path.join(OUT, b['file']))
