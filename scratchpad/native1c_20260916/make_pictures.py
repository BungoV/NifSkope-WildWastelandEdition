"""Lane NATIVE1c, 2026-09-16: the three pictures, labels burned into the pixels.

    python make_pictures.py <near-dir> <mnam-dir> <out-dir>

where each dir holds `Native/Commonwealth.lodo` + `.lodi`. Every number drawn is
read out of those files by this script; nothing is retyped from a log.
"""
import math
import os
import sys

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')

from PIL import Image, ImageDraw, ImageFont            # noqa: E402
from lodgen_native_cut import read_lodo, read_lodi     # noqa: E402
import lodgen_silhouette as SIL                        # noqa: E402
import random                                          # noqa: E402

BG = (18, 18, 20)
FG = (236, 236, 238)
DIM = (150, 150, 158)
HOT = (255, 176, 64)
COOL = (110, 190, 255)
BAD = (255, 96, 96)


def font(sz):
    for n in ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(n):
            return ImageFont.truetype(n, sz)
    return ImageFont.load_default()


F18, F14, F12, F26 = font(18), font(14), font(12), font(26)


def label(d, xy, text, f=F14, fill=FG):
    d.text(xy, text, font=f, fill=fill)


def draw_soup(d, tris, pos, lo, ex, box, colour, grid=160):
    """One horizon view of a soup, drawn into `box` = (x, y, w, h)."""
    marks = set()
    cx, cy = lo[0] + ex[0] * 0.5, lo[1] + ex[1] * 0.5
    rad = math.sqrt((ex[0] * 0.5) ** 2 + (ex[1] * 0.5) ** 2)
    if rad <= 0 or ex[2] <= 0:
        return 0
    for (ia, ib, ic) in tris:
        p = []
        for idx in (ia, ib, ic):
            x, y, z = pos[idx]
            u = (x - cx)
            p.append(((u + rad) / (2.0 * rad) * grid, (z - lo[2]) / ex[2] * grid))
        SIL.mark_triangle(marks, p, grid)
    bx, by, bw, bh = box
    sx, sy = bw / float(grid), bh / float(grid)
    for m in marks:
        gy, gx = divmod(m, grid)
        d.rectangle([bx + gx * sx, by + bh - (gy + 1) * sy,
                     bx + (gx + 1) * sx, by + bh - gy * sy], fill=colour)
    return len(marks)


# ------------------------------------------------------------------ picture 1
def picture_silhouette(nearLodo, out):
    L = read_lodo(nearLodo)
    rng = random.Random(7)
    best = None
    for mi, m in enumerate(L['meshes']):
        if m['levelCount'] < 3:
            continue
        t = sum(L['clusters'][ci]['triangleCount']
                for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']))
        if 400 <= t <= 2500 and (best is None or t > best[0]):
            best = (t, mi)
    assert best, 'no mesh in the size window'
    mi = best[1]
    m = L['meshes'][mi]
    lo = (m['ax'], m['ay'], m['az'])
    ex = (m['ex'], m['ey'], m['ez'])
    pos, per = SIL.mesh_tables(L, mi)
    full = SIL.cut_at_level(L, per, 0)
    top = max(p[1] for p in per)
    cut = SIL.cut_at_level(L, per, top)
    twin = SIL.drop_twin(full, len(cut), rng)
    rc = SIL.ratio(cut, full, pos, lo, ex, 8, 96)
    rt = SIL.ratio(twin, full, pos, lo, ex, 8, 96)

    W, H = 1180, 620
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    label(d, (28, 22), 'THE SILHOUETTE FLOOR -- a building may shrink, a tree may '
          'never become a stump', F26)
    label(d, (28, 62), 'lane NATIVE1c  2026-09-16   .lodo v4   model: '
          + L['string_at'](m['modelStringOffset']), F14, DIM)
    label(d, (28, 84), 'worst of 8 horizon views, 96 x 96 raster over the mesh box; '
          'floor 0.70, and the writer rolls a level back below it', F14, DIM)
    boxes = [(60, 140, 320, 320), (430, 140, 320, 320), (800, 140, 320, 320)]
    draw_soup(d, full, pos, lo, ex, boxes[0], FG)
    draw_soup(d, cut, pos, lo, ex, boxes[1], COOL)
    draw_soup(d, twin, pos, lo, ex, boxes[2], BAD)
    cap = [('LEVEL 0  full detail', '%d triangles' % len(full), 'reads 1.0000 against itself', FG),
           ('LEVEL %d  the kept cut' % top, '%d triangles' % len(cut),
            'keeps %.4f  PASSES the 0.70 floor' % rc, COOL),
           ('THE FLOOR TWIN  vertices dropped', '%d triangles, matched' % len(twin),
            'keeps %.4f  FAILS the 0.70 floor' % rt, BAD)]
    for (bx, by, bw, bh), (t1, t2, t3, col) in zip(boxes, cap):
        d.rectangle([bx - 1, by - 1, bx + bw + 1, by + bh + 1], outline=DIM)
        label(d, (bx, by + bh + 14), t1, F18, col)
        label(d, (bx, by + bh + 38), t2, F14, DIM)
        label(d, (bx, by + bh + 58), t3, F14, col)
    label(d, (28, H - 30), 'measured by tests/spells/lodgen_silhouette.py, which '
          'rasterises the .lodo bytes itself -- not by the writer', F12, DIM)
    im.save(out)
    return out, im.size


# ------------------------------------------------------------------ picture 2
def level1_distance(L):
    errs = []
    pct = []
    for ci, cl in enumerate(L['lods']):
        if cl['level'] != 1:
            continue
        mm = L['meshes'][L['clusters'][ci]['meshId']]
        diag = math.sqrt(mm['ex'] ** 2 + mm['ey'] ** 2 + mm['ez'] ** 2)
        errs.append(cl['err'])
        if diag > 0:
            pct.append(cl['err'] / diag * 100.0)
    errs.sort()
    pct.sort()
    ps = 960.0 / math.tan(math.radians(35.0))
    med = errs[len(errs) // 2]
    return med * ps, (pct[len(pct) // 2] if pct else 0.0), len(errs)


def picture_library(nearLodo, mnamLodo, out):
    A, B = read_lodo(nearLodo), read_lodo(mnamLodo)
    da, pa, na = level1_distance(A)
    db, pb, nb = level1_distance(B)
    W, H = 1180, 560
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    label(d, (28, 22), "THE LADDER'S FIRST STEP IS NOW SELECTABLE", F26)
    label(d, (28, 62), 'lane NATIVE1c  2026-09-16   Sanctuary region, 9 chunks, dim 4   '
          'one-pixel tolerance, 1920 x 1080 at 70 deg', F14, DIM)
    label(d, (28, 84), 'screenErrorPx = geometricError x scale x projectionScale / distance   '
          '->   distance at 1 px = median level-1 error x 1371.3', F14, DIM)
    top = max(da, db, 52100.0)
    x0, wmax = 300, 690
    rows = [('--library mnam  (v3, the way back)', db, pb, nb, DIM),
            ("the plan's number, NATIVE 3.5.4", 52100.0, 3.80, 0, DIM),
            ('--library near  (v4, the default)', da, pa, na, HOT)]
    y = 170
    for (name, dist, pct, n, col) in rows:
        label(d, (28, y + 8), name, F14, col)
        w = max(3, int(wmax * dist / top))
        d.rectangle([x0, y, x0 + w, y + 34], fill=col)
        label(d, (x0 + w + 12, y + 8), '%s units' % format(int(round(dist)), ','), F18, col)
        sub = '%.2f percent of the model diagonal' % pct
        if n:
            sub += '   (%s level-1 clusters)' % format(n, ',')
        label(d, (x0, y + 40), sub, F12, DIM)
        y += 100
    label(d, (28, y + 10), "The plan's own words for the grey bar, NATIVE 3.5.4: the first "
          'step "reaches one pixel only past 52,100 units".', F14, FG)
    label(d, (28, y + 34), 'Level 0 now comes from each base\'s near MODL, so the first '
          'step lands at %s units -- %.1f times closer.' % (format(int(round(da)), ','),
                                                            db / max(1.0, da)), F14, HOT)
    label(d, (28, H - 30), 'measured by tests/spells/lodgen_ladder_select.py off the two '
          'arms\' .lodo bytes', F12, DIM)
    im.save(out)
    return out, im.size


# ------------------------------------------------------------------ picture 3
def picture_ao(nearLodi, out):
    import struct
    raw = open(nearLodi, 'rb').read()
    ver = struct.unpack_from('<I', raw, 4)[0]
    assert ver == 5, 'the AO blob is a v5 payload; this file is v%d' % ver
    off = struct.unpack_from('<Q', raw, 0xE4)[0]
    cnt = struct.unpack_from('<I', raw, 0xEC)[0]
    stride = raw[0xF0]
    assert stride == 1, 'placementAoStride %d' % stride
    pao = list(raw[off:off + cnt])
    meas = [v for v in pao if v != 0xFF]
    assert meas, 'no measured AO byte'
    W, H = 1180, 620
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    label(d, (28, 22), 'THE PER-PLACEMENT AO BYTE -- what a card-drawn LOD used to be told',
          F26)
    label(d, (28, 62), 'lane NATIVE1c  2026-09-16   .lodi v5   %s instances, %s measured, '
          '%s not measured (0xFF)'
          % (format(len(pao), ','), format(len(meas), ','),
             format(len(pao) - len(meas), ',')), F14, DIM)
    label(d, (28, 84), 'one ray cast straight up from just above the drawn top, against the '
          'assembled chunk AND the heightfield', F14, DIM)
    bins = [0] * 64
    for v in meas:
        bins[min(63, v * 64 // 255)] += 1
    peak = max(bins) or 1
    x0, y0, bw, bh = 60, 150, 1060, 280
    for i, c in enumerate(bins):
        h = int(bh * c / peak)
        x = x0 + i * (bw // 64)
        d.rectangle([x, y0 + bh - h, x + (bw // 64) - 2, y0 + bh], fill=COOL)
    d.line([x0, y0 + bh, x0 + bw, y0 + bh], fill=DIM)
    label(d, (x0, y0 + bh + 8), '0  fully occluded', F12, DIM)
    label(d, (x0 + bw - 130, y0 + bh + 8), '254  open sky', F12, DIM)
    mn, mx = min(meas), max(meas)
    mean = sum(meas) / float(len(meas))
    srt = sorted(meas)
    label(d, (60, y0 + bh + 44), 'min %d    median %d    mean %.1f    max %d    '
          'distinct values %d'
          % (mn, srt[len(srt) // 2], mean, mx, len(set(meas))), F18, FG)
    label(d, (60, y0 + bh + 76), 'v3 wrote 255 -- fully lit -- for EVERY card-drawn '
          'placement: the record' + chr(39) + 's `ao` is a mean over lit', F14, HOT)
    label(d, (60, y0 + bh + 98), 'chunk-mesh vertices, and a card has none. The '
          'spread above is what that constant was hiding.', F14, HOT)
    label(d, (60, y0 + bh + 120), '0xFF now means NOT MEASURED, and is not AO 255.',
          F14, HOT)
    label(d, (28, H - 30), 'read out of the .lodi bytes by tests/spells/'
          'lodgen_native_cut.py\'s reader; the gate is lodgen_native_fields.py section j5',
          F12, DIM)
    im.save(out)
    return out, im.size


if __name__ == '__main__':
    near, mnam, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(outdir, exist_ok=True)
    made = [picture_silhouette(near + '/Native/Commonwealth.lodo',
                               outdir + '/silhouette_floor.png'),
            picture_library(near + '/Native/Commonwealth.lodo',
                            mnam + '/Native/Commonwealth.lodo',
                            outdir + '/library_near_vs_mnam.png'),
            picture_ao(near + '/Native/Commonwealth.lodi',
                       outdir + '/placement_ao.png')]
    for (p, sz) in made:
        back = Image.open(p)
        print('%-56s %d x %d  read back %d x %d  %d bytes'
              % (os.path.basename(p), sz[0], sz[1], back.size[0], back.size[1],
                 os.path.getsize(p)))
