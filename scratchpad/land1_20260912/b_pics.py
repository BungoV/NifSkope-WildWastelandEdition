"""LAND1 Part B -- the picture (ww-texel-picture).

What `--incremental` promises is THE SAME BYTES, and bytes are not a thing you
can look at. So this figure shows the two halves of that promise as pixels:

  TOP ROW, five panels, every one a real DDS off disk written by the real exe:
    base        the region baked from the unedited plugin
    full        a FULL bake of the edited plugin -- the reference answer
    |full-base| the edit, amplified x8 so it is visible at all. NOT BLACK is the
                point: it proves the edit reached the output, which is the floor
                every gate arm carries. An arm whose floor is black proves
                nothing, because `equal` is free for a change that reached
                nothing.
    incr        the DIRTY REBAKE -- 9 of 25 chunks rebaked, the other 16 left
                exactly as the base bake wrote them
    |incr-full| the promise. BLACK, and black at x8 gain, and the sha1s printed
                under both panels are the real check: this panel could only ever
                be evidence, never proof.

  BOTTOM ROW: the dirty map for each arm of the gate -- which of the 25 chunks
  the diff marked, and why. The edited cell is marked. This is where the cost of
  the feature is legible: a one-cell edit dirties nine chunks, because the input
  digest already reads a one-cell ring and the diff then widens by one more.

    python b_pics.py   ->  images/b_incremental.png

Nothing here is modelled and no bake is made by this script: every pixel and
every number comes off the tree gate B3 wrote, with `--road-detail 1` on every
bake that made it.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
for p in (os.path.join(os.path.dirname(HERE), 'tiling4_20260912'),
          os.path.join(os.path.dirname(HERE), 'tiling3_20260911'),
          os.path.join(os.path.dirname(HERE), 'tiling2_20260911'),
          os.path.join(os.path.dirname(HERE), 'splat1_20260911')):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402

B3 = os.path.join(HERE, 'out', 'b3')
IMAGES = os.path.join(HERE, 'images')

# The chunk gate arm A/land moved, read off the gate's own floor listing.
CHUNK = (-20, 20)
GAIN = 8.0
SHEET = 384
PAD = 14

try:
    F = ImageFont.truetype('consola.ttf', 13)
    FB = ImageFont.truetype('consolab.ttf', 15)
    FH = ImageFont.truetype('consolab.ttf', 23)
except Exception:
    F = FB = FH = ImageFont.load_default()


def sheet(kind, sub):
    cx, cy = CHUNK
    p = os.path.join(B3, 'A', kind, sub, 'tex',
                     'Commonwealth.4.%d.%d.DDS' % (cx, cy)) if sub else \
        os.path.join(B3, 'A', kind, 'tex', 'Commonwealth.4.%d.%d.DDS' % (cx, cy))
    if not os.path.exists(p):
        raise SystemExit('REFUSED: %s is missing -- run gate B3 before drawing it' % p)
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64), p


def sha1_of(path):
    import hashlib
    return hashlib.sha1(open(path, 'rb').read()).hexdigest()


def u8(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def fit(img):
    return img.resize((SHEET, SHEET), Image.NEAREST)


# ---------------------------------------------------------------- dirty maps

# Read straight out of the gate's log so the picture cannot drift from the
# verdicts: (arm, dirty, total, moved, notInLedger, lost, byNeighbour).
def census():
    import re
    log = os.path.join(HERE, 'logs', 'b3.txt')
    rows = []
    pend = None
    pat = re.compile(r'incremental: (\d+) of (\d+) chunks dirty \((\d+) inputs moved, '
                     r'(\d+) not in the ledger, (\d+) output lost, (\d+) by neighbour\)')
    ver = re.compile(r'B3 (\S+)\s+(\w+)')
    for line in open(log, encoding='utf-8', errors='replace'):
        m = pat.search(line)
        if m:
            pend = tuple(int(x) for x in m.groups())
        m = ver.search(line)
        if m and pend:
            rows.append((m.group(1), m.group(2)) + pend)
            pend = None
    # the refs arms were re-run against a ref the bake actually draws; prefer
    # that log's verdicts where it exists.
    rerun = os.path.join(HERE, 'logs', 'b3_refs.txt')
    if os.path.exists(rerun):
        pend = None
        fresh = []
        for line in open(rerun, encoding='utf-8', errors='replace'):
            m = pat.search(line)
            if m:
                pend = tuple(int(x) for x in m.groups())
            m = ver.search(line)
            if m and pend:
                fresh.append((m.group(1), m.group(2)) + pend)
                pend = None
        names = {r[0] for r in fresh}
        rows = [r for r in rows if r[0] not in names] + fresh
    return rows


REGION = {'A': (-24, 16), 'B': (-16, 0)}   # the lowest (cx, cy) of each 5x5
DIM = 4


def seeds(arm):
    """The chunks the exe itself named as dirty-at-source, out of its own log."""
    import re
    tag, kind = arm.split('/')
    log = os.path.join(B3, tag, kind, 'incr', 'bake.log')
    out = set()
    if not os.path.exists(log):
        return out
    for line in open(log, encoding='utf-8', errors='replace'):
        m = re.match(r'\s+\((-?\d+),(-?\d+)\) ', line)
        if m:
            out.add((int(m.group(1)), int(m.group(2))))
        m = re.search(r'deleted Commonwealth\.\d+\.(-?\d+)\.(-?\d+)\.', line)
        if m:
            out.add((int(m.group(1)), int(m.group(2))))
    return out


def dirty_set(arm):
    """Seeds widened by ONE CHUNK STEP, clipped to the region -- the rule in
    nifcli.cpp:3791. Returned as grid indices."""
    tag = arm.split('/')[0]
    x0, y0 = REGION[tag]
    grid = set()
    for (sx, sy) in seeds(arm):
        for i in range(5):
            for j in range(5):
                cx, cy = x0 + i * DIM, y0 + j * DIM
                if abs(cx - sx) <= DIM and abs(cy - sy) <= DIM:
                    grid.add((i, j))
    return grid


def draw_map(d, x, y, arm, verdict, dirty, total, moved, notin, lost, nb, w=118):
    cell = w // 5
    col = {'PASS': (120, 210, 130), 'FAIL': (235, 110, 110)}.get(verdict, (230, 200, 120))
    d.text((x, y), '%-8s %s' % (arm, verdict), font=FB, fill=col)
    d.text((x, y + 18), '%d of %d dirty' % (dirty, total), font=F, fill=(235, 235, 240))
    d.text((x, y + 34), '%d moved  %d lost' % (moved, lost), font=F, fill=(170, 170, 180))
    d.text((x, y + 50), '%d by neighbour' % nb, font=F, fill=(170, 170, 180))
    gx, gy = x, y + 70

    sd = seeds(arm)
    on = dirty_set(arm)
    # THE PICTURE IS ALSO A CHECK. The set drawn here is computed from the seed
    # chunks and the widening rule; the count beside it came out of the exe. If
    # they disagree, one of the two is wrong and neither should be drawn.
    if len(on) != dirty:
        raise SystemExit('REFUSED: %s -- the widening rule gives %d dirty chunks, '
                         'the exe printed %d' % (arm, len(on), dirty))

    tag = arm.split('/')[0]
    x0, y0 = REGION[tag]
    for j in range(5):
        for i in range(5):
            cx, cy = x0 + i * DIM, y0 + j * DIM
            if (cx, cy) in sd:
                fill = (120, 200, 130)          # the chunk whose input moved
            elif (i, j) in on:
                fill = (62, 104, 72)            # dirtied by the widening
            else:
                fill = (44, 46, 52)             # left exactly as it was
            d.rectangle([gx + i * cell, gy + j * cell,
                         gx + i * cell + cell - 2, gy + j * cell + cell - 2],
                        fill=fill, outline=(28, 30, 34))
    return gy + 5 * cell + 8


def main():
    os.makedirs(IMAGES, exist_ok=True)
    base, pBase = sheet('base', None)
    full, pFull = sheet('land', 'full')
    incr, pIncr = sheet('land', 'incr')

    dEdit = np.abs(full - base) * GAIN
    dProm = np.abs(incr - full) * GAIN

    panels = [
        ('base', base, 'the region as it was', pBase),
        ('full bake', full, 'the edited plugin, ALL 25 chunks', pFull),
        ('|full - base| x8', dEdit, 'THE FLOOR: the edit reached the output', None),
        ('dirty rebake', incr, 'the edited plugin, 9 chunks of 25', pIncr),
        ('|incr - full| x8', dProm, 'THE PROMISE: black, and the sha1s agree', None),
    ]

    rows = census()
    W = PAD + len(panels) * (SHEET + PAD)
    H = 150 + SHEET + 70 + 250
    img = Image.new('RGB', (W, H), (24, 26, 30))
    d = ImageDraw.Draw(img)

    d.text((PAD, 14), 'LAND1 / INCR1  --  a dirty rebake is the same bytes as a full bake',
           font=FH, fill=(240, 240, 245))
    d.text((PAD, 46),
           'Commonwealth chunk %d,%d  dim 4  --  gate B3 arm A/land: a compressed LAND height '
           'raised by one gradient unit in cell (-20,20)' % CHUNK,
           font=F, fill=(180, 180, 190))
    d.text((PAD, 64),
           'Every panel is a real .DDS off disk, written by release/NifSkope.exe 08:42:33 '
           '(21,935,616 B, sha1 1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968), --road-detail 1.',
           font=F, fill=(150, 150, 160))
    d.text((PAD, 82),
           'x8 GAIN on both difference panels: a difference too small to see at 1x would '
           'still be a difference, so the gain is the honest way to look at one.',
           font=F, fill=(150, 150, 160))

    y0 = 118
    for k, (name, arr, note, path) in enumerate(panels):
        x = PAD + k * (SHEET + PAD)
        im = fit(u8(arr))
        img.paste(im, (x, y0 + 34))
        hot = (150, 230, 150) if 'PROMISE' in note else (
            (255, 200, 140) if 'FLOOR' in note else (235, 235, 240))
        d.text((x, y0), name, font=FB, fill=hot)
        d.text((x, y0 + 18), note, font=F, fill=(165, 165, 175))
        if path:
            d.text((x, y0 + 38 + SHEET), 'sha1 %s' % sha1_of(path)[:24],
                   font=F, fill=(150, 150, 160))
        else:
            mx = float(np.max(arr)) / GAIN
            d.text((x, y0 + 38 + SHEET), 'max |delta| = %.0f of 255' % mx,
                   font=F, fill=(150, 230, 150) if mx == 0 else (255, 200, 140))
        d.rectangle([x - 1, y0 + 33, x + SHEET, y0 + 34 + SHEET], outline=(60, 62, 70))

    y1 = y0 + SHEET + 76
    d.text((PAD, y1), 'THE DIRTY SET, every arm of the gate', font=FH, fill=(240, 240, 245))
    d.text((PAD, y1 + 30),
           'Two regions, five kinds of change. BRIGHT = the chunk whose input actually '
           'moved (or whose output was deleted); dim green = dirtied by the one-chunk '
           'widening; grey = left exactly as the previous bake wrote it.',
           font=F, fill=(150, 150, 160))
    x = PAD
    for r in rows:
        arm, verdict = r[0], r[1]
        draw_map(d, x, y1 + 56, arm, verdict, r[2], r[3], r[4], r[5], r[6], r[7])
        x += 150

    d.text((PAD, H - 26),
           'A one-cell edit dirties 9 chunks of 25 (36 % here, a few per cent of a real '
           'worldspace): the input digest reads a one-cell ring, and the diff widens by one '
           'more so a neighbour whose OUTPUT was lost is caught too.',
           font=F, fill=(165, 165, 175))

    out = os.path.join(IMAGES, 'b_incremental.png')
    img.save(out)
    print('wrote %s (%d x %d)' % (out, W, H))
    print('max |incr - full| = %.1f  (0 means the panel is black)'
          % (float(np.max(dProm)) / GAIN))
    return 0


if __name__ == '__main__':
    sys.exit(main())
