#!/usr/bin/env python3
"""CELLVIEW4 item 3 -- who is actually under the black pixels?

black_probe.py refuted "no VF_TANGENT" as the cause (18 tangent-less instances
on screen, none black).  Before blaming any other property of markerxheading,
check the assumption underneath: that the arrow is the only thing there.  This
prints (a) the exact colours of the black region and (b) EVERY reference in the
dump whose bounding box covers it, tallest last -- because in a top-down view
the thing you see is the one with the highest zmax.
"""
import os
import sys

import numpy as np
from PIL import Image

IMG = '../cellview3_20260919/images/after_downtown.png'
DUMP = '../cellview3_20260919/dump_downtown.txt'
CX, CY = 22528.0, -43008.0
UPP = 2.69704


def main():
    here = os.path.dirname(__file__) or '.'
    im = np.asarray(Image.open(os.path.join(here, IMG)).convert('RGB'))
    h, w = im.shape[:2]
    lum = im.astype(np.float32).mean(axis=2) / 255.0

    # the black blob, found rather than assumed: darkest connected area in the
    # neighbourhood of the measured bbox centre
    y0, y1, x0, x1 = 520, 580, 1335, 1400
    sub = lum[y0:y1, x0:x1]
    mask = sub < 0.06
    ys, xs = np.nonzero(mask)
    print('black pixels (<0.06) in the search window: %d' % len(ys))
    print('bbox in picture coords: x %d..%d  y %d..%d'
          % (x0 + xs.min(), x0 + xs.max(), y0 + ys.min(), y0 + ys.max()))
    cols = im[y0:y1, x0:x1][mask]
    uniq, cnt = np.unique(cols.reshape(-1, 3), axis=0, return_counts=True)
    order = np.argsort(-cnt)[:8]
    print('most common colours in the blob:')
    for i in order:
        print('   rgb(%3d,%3d,%3d)  x%d' % (*uniq[i], cnt[i]))

    # world coords of the blob centre
    bx = x0 + (xs.min() + xs.max()) / 2.0
    by = y0 + (ys.min() + ys.max()) / 2.0
    wx = CX + (bx - w / 2.0) * UPP
    wy = CY - (by - h / 2.0) * UPP
    print('\nblob centre: picture (%.1f, %.1f) -> world (%.1f, %.1f)'
          % (bx, by, wx, wy))

    hits = []
    for ln in open(os.path.join(here, DUMP)):
        if ln.startswith('#'):
            continue
        f = ln.split()
        if len(f) < 21:
            continue
        bb = [float(v) for v in f[14:20]]
        if bb[0] <= wx <= bb[3] and bb[1] <= wy <= bb[4]:
            hits.append((bb[5], f[0], f[1], f[2], f[13], bb,
                         ' '.join(f[20:])))
    print('\nreferences whose box covers that point: %d' % len(hits))
    for zmax, ref, base, typ, tris, bb, m in sorted(hits):
        print('   zmax %9.1f  zmin %9.1f  %s %s %-5s tris %-6s %s'
              % (zmax, bb[2], ref, base, typ, tris, m))
    return 0


if __name__ == '__main__':
    sys.exit(main())
