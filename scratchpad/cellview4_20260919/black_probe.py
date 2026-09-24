#!/usr/bin/env python3
"""CELLVIEW4 item 3 -- THE DISCRIMINATOR, part 2.

tangent_census.py says 12 of the 395 distinct models in cell block 5,-11 have
a shape with no VF_TANGENT.  The candidate mechanism for the solid-black arrow
is "no VF_TANGENT -> lodgen still appends a ZERO tangent per row
(src/lodgen.cpp:2175) -> cellview's short-array fallback (src/cellview.cpp:1036)
never fires -> degenerate TBN -> black".

THE TEST: that mechanism predicts EVERY unoccluded tangent-less instance is
black.  One tangent-less instance that is plainly NOT black refutes it.  So
project each instance's bounding box into the delivered picture and measure.

Camera (src/glview.cpp:6417-6432, WW_RENDER_ORTHO is half-WIDTH):
  picture   scratchpad/cellview3_20260919/images/after_downtown.png, 1822x925
  centre    world (22528, -43008) = the exact centre of cells 4..6 / -12..-10
  scale     2.69704 world units per pixel
Calibrated against the arrow itself: predicted (1368.9, 548.2) vs measured
near-black bbox centre (1364.5, 547.5).

CAVEAT THIS DOES NOT REMOVE: the view is top-down, so an instance can be dark
because something is ON TOP OF IT, not because it drew black.  Read the
"cover" column -- it is the fraction of the box that is near-black -- together
with the model.  This measures the picture; it does not read the depth buffer.
"""
import os
import sys

import numpy as np
from PIL import Image

import tangent_census as tc

IMG = '../cellview3_20260919/images/after_downtown.png'
DUMP = '../cellview3_20260919/dump_downtown.txt'
ROOT = r'E:/Tools/Fallout 4/DataUnpacked/Data'
CX, CY = 22528.0, -43008.0
UPP = 2.69704
BLACK = 0.06          # luminance below this counts as "black"


def main():
    im = np.asarray(Image.open(os.path.join(os.path.dirname(__file__) or '.',
                                            IMG)).convert('RGB'),
                    dtype=np.float32) / 255.0
    h, w = im.shape[:2]
    lum = im.mean(axis=2)
    ox, oy = w / 2.0, h / 2.0

    rows = []
    for ln in open(os.path.join(os.path.dirname(__file__) or '.', DUMP)):
        if ln.startswith('#'):
            continue
        f = ln.split()
        if len(f) < 21:
            continue
        rows.append((' '.join(f[20:]), [float(x) for x in f[14:20]], f[0]))

    # which models have a tangent-less shape
    noTan = set()
    for m in {r[0] for r in rows}:
        p = os.path.join(ROOT, 'meshes', m.replace('\\', os.sep))
        if not os.path.exists(p):
            d, n = os.path.split(p)
            for fn in os.listdir(d):
                if fn.lower() == n.lower():
                    p = os.path.join(d, fn)
                    break
        ds = tc.descs(p)
        if not ds or any(why for _f, _s, why in ds):
            continue
        if any(not (fl & tc.VF_TANGENT) for fl, _s, _w in ds):
            noTan.add(m)

    print('tangent-less models placed in the cell: %d' % len(noTan))
    print()
    print('%-8s %-6s %-6s %-5s %-5s %-6s %-6s  %s' % (
        'ref', 'px', 'py', 'w', 'h', 'meanL', 'black', 'model'))
    for m, bb, ref in sorted(rows, key=lambda r: r[0].lower()):
        if m not in noTan:
            continue
        x0 = ox + (bb[0] - CX) / UPP
        x1 = ox + (bb[3] - CX) / UPP
        y0 = oy - (bb[4] - CY) / UPP
        y1 = oy - (bb[1] - CY) / UPP
        a, b = int(max(0, min(x0, x1))), int(min(w, max(x0, x1) + 1))
        c, d = int(max(0, min(y0, y1))), int(min(h, max(y0, y1) + 1))
        if b <= a or d <= c:
            print('%-8s %-6s %-6s %-5s %-5s %-6s %-6s  %s' % (
                ref, '-', '-', '-', '-', 'offscreen', '-', m))
            continue
        patch = lum[c:d, a:b]
        print('%-8s %-6d %-6d %-5d %-5d %-6.3f %-6.2f  %s' % (
            ref[-6:], (a + b) // 2, (c + d) // 2, b - a, d - c,
            float(patch.mean()), float((patch < BLACK).mean()), m))
    return 0


if __name__ == '__main__':
    sys.exit(main())
