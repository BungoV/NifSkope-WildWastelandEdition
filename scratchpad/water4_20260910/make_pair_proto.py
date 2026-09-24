# -*- coding: utf-8 -*-
"""make_pair_proto.py -- the Charles flow direction BEFORE (the disc fill, read
out of water3's charles_marked.lodl) and AFTER (the potential-flow solve,
the numpy prototype on the SAME mask with the SAME stroke out of that file's
own store), one framing, as a TEXEL picture (ww-texel-picture): the body's
bounding box, 2x, hue = direction.

This is the pre-build picture.  The render-hook pair at WATER2's framing needs
the exe and is taken by the build step (see the lane report).

Every number in a caption is measured here, by disc_metric.py's own code,
never typed.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from lodl_np import body_region, stroke_points      # noqa: E402
import flow_proto as fp                              # noqa: E402
import disc_metric as dm                             # noqa: E402

BODY = 3
MARKED = os.path.join(HERE, '..', 'water3_20260910', 'work', 'charles_marked.lodl')
OUT = os.path.join(HERE, 'images')
os.makedirs(OUT, exist_ok=True)

d, b, ids, flow, (px0, py0) = body_region(MARKED, BODY)
wet = ids == BODY
H, W = wet.shape
u = 4096.0 / d.bodyS                      # world units a texel
minWX = d.minX * 4096.0
minWY = d.minY * 4096.0

# ---- the stroke out of the file's own store --------------------------------
strokes = [s for s in stroke_points(d) if s['body'] == BODY and s['kind'] == 0]
assert strokes, 'no stroke on body %d in %s' % (BODY, MARKED)
s = strokes[0]
pts = [((x - minWX) / u - px0, (y - minWY) / u - py0) for (x, y) in s['pts']]
halfW = s['width'] * 0.5 / u

# ---- the prototype solve, the method as ported to the C++ ------------------
# (smooth_probe.py: a quartic bump under the stroke, DISC ends of the stroke
# width as source and sink, the bank-free continuation and 8 low-pass passes)
import smooth_probe as smp                            # noqa: E402
g, wet, ux, uy, it, res, pts = smp.solve_body(BODY, pts)
dirw = smp.direction_plane(g, wet, ux, uy, 8).astype(np.int32)
U, V = g.field(ux), g.field(uy)
speed = np.hypot(U, V)

# ---- the same instrument on both ------------------------------------------
before_dir = (flow & 0xFF).astype(np.int32)


def structure(dirw):
    hp = wet[:, :-1] & wet[:, 1:]
    vp = wet[:-1, :] & wet[1:, :]
    dd = np.concatenate([dm.angdiff(dirw[:, :-1], dirw[:, 1:])[hp],
                         dm.angdiff(dirw[:-1, :], dirw[1:, :])[vp]])
    p99 = float(np.percentile(dd, 99))
    seam = float((dd > 10.0).sum()) / len(dd)
    patches = 0
    for v in np.unique(dirw[wet]):
        m = wet & (dirw == v)
        if m.sum() < 64:
            continue
        l, kc = dm.label4(m)
        for c in range(1, kc + 1):
            comp = l == c
            if comp.sum() < 64:
                continue
            j = []
            sel = comp[:, :-1] & ~comp[:, 1:] & wet[:, 1:]; j.append(dm.angdiff(dirw[:, :-1], dirw[:, 1:])[sel])
            sel = comp[:, 1:] & ~comp[:, :-1] & wet[:, :-1]; j.append(dm.angdiff(dirw[:, 1:], dirw[:, :-1])[sel])
            sel = comp[:-1, :] & ~comp[1:, :] & wet[1:, :]; j.append(dm.angdiff(dirw[:-1, :], dirw[1:, :])[sel])
            sel = comp[1:, :] & ~comp[:-1, :] & wet[:-1, :]; j.append(dm.angdiff(dirw[1:, :], dirw[:-1, :])[sel])
            j = np.concatenate(j)
            if len(j) and float((j > 5.0).sum()) / len(j) > 0.5:
                patches += 1
    a = dirw[wet] / 256.0 * 2 * math.pi
    R = math.hypot(np.cos(a).mean(), np.sin(a).mean())
    mean = math.degrees(math.atan2(np.sin(a).mean(), np.cos(a).mean()))
    return dict(p99=p99, seam=seam, patches=patches, R=R, mean=mean, distinct=len(np.unique(dirw[wet])))


sb = structure(before_dir)
sa = structure(dirw)
print('before (disc fill):', sb)
print('after (potential flow, %d iterations, residual %.1e):' % (it, res), sa)
mouth = math.degrees(math.atan2(pts[-1][1] - pts[0][1], pts[-1][0] - pts[0][0]))
print('stroke start->end direction %.1f deg' % mouth)


def hue_image(dirw, bright=None):
    img = np.zeros((H, W, 3), np.uint8)
    img[:] = (24, 26, 30)
    hsv = Image.new('HSV', (W, H))
    hh = (dirw * 255 // 255).astype(np.uint8)
    vv = np.full((H, W), 200, np.uint8) if bright is None else bright
    arr = np.dstack([dirw.astype(np.uint8), np.full((H, W), 190, np.uint8), vv])
    rgb = np.array(Image.fromarray(arr, 'HSV').convert('RGB'))
    img[wet] = rgb[wet]
    return Image.fromarray(img[::-1])          # row 0 is SOUTH in the file


sp = np.zeros((H, W), np.uint8)
if speed[wet].max() > 0:
    sp[wet] = np.clip(120 + 135 * speed[wet] / np.percentile(speed[wet], 98), 120, 255).astype(np.uint8)
SCALE = 2
imB = hue_image(before_dir).resize((W * SCALE, H * SCALE), Image.NEAREST)
imA = hue_image(dirw, sp).resize((W * SCALE, H * SCALE), Image.NEAREST)


def font(sz, bold=False):
    for p in (r'C:\Windows\Fonts\segoeuib.ttf' if bold else r'C:\Windows\Fonts\segoeui.ttf',):
        try:
            return ImageFont.truetype(p, sz)
        except OSError:
            pass
    return ImageFont.load_default()


PAD, GAP, TOP, CAPH = 24, 24, 70, 150
Wt = PAD * 2 + imA.width * 2 + GAP
Ht = TOP + imA.height + CAPH + PAD
sheet = Image.new('RGB', (Wt, Ht), (32, 32, 34))
dr = ImageDraw.Draw(sheet)
dr.text((PAD, 18), 'The Charles (body 3), flow DIRECTION per texel, one mask, one stroke, two fills',
        fill=(230, 230, 230), font=font(26, True))
caps = [
    (imB, 'before - the harmonic fill with held discs (charles_marked.lodl)',
     ['%d distinct directions   R = %.3f   mean %.1f deg' % (sb['distinct'], sb['R'], sb['mean']),
      'seam-bounded patches %d   p99 adjacent jump %.2f deg   seams %.2f%%' % (sb['patches'], sb['p99'], sb['seam'] * 100),
      'each disc is one stroke segment held over its half-width (16 texels)']),
    (imA, 'after - the potential-flow solve (numpy prototype of the C++)',
     ['%d distinct directions   R = %.3f   mean %.1f deg' % (sa['distinct'], sa['R'], sa['mean']),
      'seam-bounded patches %d   p99 adjacent jump %.2f deg   seams %.2f%%' % (sa['patches'], sa['p99'], sa['seam'] * 100),
      'brightness = speed; %d CG iterations, residual %.0e; ends = discs, x4 bump, 8 passes' % (it, res)]),
]
x = PAD
for im, head, lines in caps:
    sheet.paste(im, (x, TOP))
    dr.text((x, TOP + im.height + 10), head, fill=(240, 170, 90), font=font(20, True))
    for i, ln in enumerate(lines):
        dr.text((x, TOP + im.height + 42 + i * 26), ln, fill=(150, 220, 150) if i < 2 else (190, 190, 190), font=font(17))
    x += im.width + GAP
out = os.path.join(OUT, 'charles_flow_proto_pair.png')
sheet.save(out)
print('wrote', out, sheet.size)
