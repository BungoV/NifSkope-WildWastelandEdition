# -*- coding: utf-8 -*-
"""make_dye.py -- the DYE plane at the Charles's mouth, at texel level.

Lane BUILD10, for lane WATER4's PENDING step 3 picture 2.  `src/btdterrain.*`
has no `dye` plane key, so this is NOT a render: it is a texel picture drawn
from the file's own bytes through lane WATER2's INDEPENDENT decoder
(`lodl_np.Lodl`, which shares no code with the writer or the marking tool).

The dye store is 4 bytes a sample at the flow rate; the word is
`source | weight << 16` (spec_water 3.7b).  `source` 1..32767 is the body id
whose water this is.

The PICTURE is the 5 L window round the mouth the harness printed, L = the
file's own dye half-distance (the kind-8 knob, default 8,192 world units).
The NUMBERS are measured over a 12 L window, because "the max beyond 3 L" has
nowhere to live inside a 5 L crop -- the crop's own corners reach 3.5 L at most.
Both windows are stated in the caption; the gate's own numbers are the 12 L
ones.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'
W4 = os.path.join(ROOT, 'scratchpad', 'water4_20260910')
sys.path.insert(0, W4)
from lodl_np import Lodl, stroke_points          # noqa: E402

FILE = os.path.join(W4, 'work', 'charles_marked_v4.lodl')
OUT = os.path.join(W4, 'images', 'charles_dye_mouth.png')
MOUTH = (2621, 2960)          # texels, printed by the harness (gate_water_flow.txt)
DEFAULT_L = 8192.0            # world units, the knob's default

d = Lodl(FILE)
assert d.dyeStore is not None, 'this file carries no dye plane'
assert d.bodyS == d.flowS, 'body and flow rates differ; the id crop would not line up'

TEXEL = 4096.0 / d.flowS      # a cell is 4,096 world units
L = DEFAULT_L
for s in stroke_points(d):
    if s['kind'] == 8:
        L = s['width']
Lt = L / TEXEL                       # the half-distance in texels
print('flow/dye rate %d, texel %.1f units, L = %.0f units = %.1f texels' % (d.flowS, TEXEL, L, Lt))


def window(halfTexels):
    r = int(round(halfTexels))
    px0, py0 = MOUTH[0] - r, MOUTH[1] - r
    n = 2 * r
    dye = d.store_region(d.dyeStore, px0, py0, n, n).astype(np.uint32)
    ids = d.store_region(d.idStore, px0, py0, n, n)
    yy, xx = np.mgrid[0:n, 0:n]
    return dye, ids, np.hypot(xx - r, yy - r), r


# ---- the numbers, over 12 L -------------------------------------------------
dyeB, idsB, distB, rB = window(6 * Lt)
wtB = ((dyeB >> 16) & 0xFF).astype(np.int32)
srcB = (dyeB & 0xFFFF).astype(np.int32)
inner = (wtB > 0) & (distB <= Lt / 2)
outer = (wtB > 0) & (distB > 3 * Lt)
meanIn = float(wtB[inner].mean()) if inner.any() else 0.0
maxOut = int(wtB[outer].max()) if outer.any() else 0
srcs = sorted(set(int(v) for v in np.unique(srcB[wtB > 0])))
print('12 L window (%d texels): %d dyed; mean within L/2 %.1f over %d; max beyond 3 L %d over %d'
      % (2 * rB, int((wtB > 0).sum()), meanIn, int(inner.sum()), maxOut, int(outer.sum())))
print('dye sources present:', srcs)

# ---- the picture, the 5 L crop ---------------------------------------------
dye, ids, dist, R = window(2.5 * Lt)
w = h = 2 * R
wt = ((dye >> 16) & 0xFF).astype(np.int32)
SCALE = 3


def hashcol(i):
    if i == 0:
        return (26, 28, 32)
    r = (int(i) * 2654435761) & 0xFFFFFFFF
    return (40 + ((r >> 16) & 0x3F), 40 + ((r >> 8) & 0x3F), 40 + (r & 0x3F))


img = Image.new('RGB', (w, h))
ip = img.load()
for y in range(h):
    for x in range(w):
        base = hashcol(int(ids[y, x]))
        k = int(wt[y, x])
        if k:
            t = k / 255.0
            ip[x, y] = (int(base[0] * (1 - t) + 40 * t),
                        int(base[1] * (1 - t) + 255 * t),
                        int(base[2] * (1 - t) + 90 * t))
        else:
            ip[x, y] = base
img = img.transpose(Image.FLIP_TOP_BOTTOM)         # row 0 is SOUTH in the file
img = img.resize((w * SCALE, h * SCALE), Image.NEAREST)
dr = ImageDraw.Draw(img)


def font(sz, bold=False):
    for p in (r'C:\Windows\Fonts\segoeuib.ttf' if bold else r'C:\Windows\Fonts\segoeui.ttf',):
        try:
            return ImageFont.truetype(p, sz)
        except OSError:
            pass
    return ImageFont.load_default()


fT, fN, fB = font(24, True), font(18, True), font(15)
cx, cy = R * SCALE, R * SCALE                     # the mouth, after the flip


def ring(rt, col, label):
    r = rt * SCALE
    dr.ellipse([cx - r, cy - r, cx + r, cy + r], outline=col)
    dr.text((cx + r * 0.70, cy - r * 0.70), label, font=fN, fill=col)


ring(Lt / 2, (255, 210, 90), 'L/2')
ring(2 * Lt, (255, 160, 120), '2 L')
dr.line([cx - 9, cy, cx + 9, cy], fill=(255, 255, 255))
dr.line([cx, cy - 9, cx, cy + 9], fill=(255, 255, 255))

# which body the dye is painted ON -- the receiving body, not the river
import collections
onB = collections.Counter(int(v) for v in idsB[wtB > 0]).most_common(3)
print('dyed texels by the body id UNDER them:', onB)

PAD, TOP, CAP = 24, 66, 240
sheet = Image.new('RGB', (img.width + PAD * 2, TOP + img.height + CAP), (24, 26, 30))
s = ImageDraw.Draw(sheet)
title = 'The DYE plane at the Charles mouth - %d x %d texels (5 L), 1 texel = %d units' % (w, h, TEXEL)
assert s.textlength(title, font=fT) <= sheet.width - 2 * PAD, title
s.text((PAD, 18), title, font=fT, fill=(230, 232, 235))
sheet.paste(img, (PAD, TOP))
s.rectangle([PAD - 1, TOP - 1, PAD + img.width, TOP + img.height], outline=(70, 74, 80))
y = TOP + img.height + 12
rows = [('mean weight within L/2   %.1f of 255  (%d texels)' % (meanIn, int(inner.sum())),
         meanIn > 128),
        ('max weight beyond 3 L    %d of 255   (gate: 1/8 of 255 = 32, slack 48)' % maxOut,
         maxOut <= 48),
        ('%d dyed texels in the 12 L window; every one names body %s as its source'
         % (int((wtB > 0).sum()), ','.join(str(v) for v in srcs)), True),
        ('and every one lies on body %d -- the body the river drains INTO, which is'
         ' what "the river tints the sea" means' % onB[0][0], len(onB) == 1)]
for line, good in rows:
    assert s.textlength(line, font=fN) <= sheet.width - 2 * PAD, line
    s.text((PAD, y), line, font=fN, fill=(150, 210, 150) if good else (230, 120, 120))
    y += 24
for ln in ('Green is the dye weight over the body-id hash; the white cross is the mouth the tool',
           'found in the file, not one assumed. The dark olive body below the cross is the Charles',
           'itself: it carries no dye words, because the plane is a field cut round the mouth of the',
           'body it drains into. L = %.0f units = %.0f texels; the numbers above are measured over' % (L, Lt),
           '12 L, because 3 L does not fit inside a 5 L crop.'):
    s.text((PAD, y + 4), ln, font=fB, fill=(174, 179, 186))
    y += 21
assert y + 4 <= sheet.height, 'the caption runs off the sheet: %d > %d' % (y + 4, sheet.height)
sheet.save(OUT)
print('%s  %dx%d' % (OUT, sheet.width, sheet.height))
