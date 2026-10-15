#!/usr/bin/env python3
"""Lane GIBLACK1 (2026-10-14) checker: a wall the probe GI must not black out.

  cell_giblack_check.py <on.png> <off.png> --rect x0,y0,x1,y1 [--bar 0.5] [--zero 3] [--zeroshare 0] [--expect pass|fail] [--label s]

on = the shot with the probe GI, off = the same camera with WW_CELL_GI=0 (the weather's flat ambient). Over the rect
(pixels of a 1920-wide frame; scaled to the shots' own width):
  M  the mean linear luminance with the GI on is at least (1 - bar) of the GI-off mean (a recess may read darker with
     the GI, that is the probe sky; it may not lose more than the bar)
  Z  no near-zero pixel: none whose brightest channel is <= zero (8-bit sRGB) where the GI-off shot's is above it
     (lane GIBLACK2: --zeroshare, at most that share of the rect: a recess now draws its measured dark, and a lone
     pixel of its noise can touch the floor; a blackout covers percents)
--expect fail: the red (the old search); the verdict is PASS when M or Z fails.

  cell_giblack_check.py <p95.png> --path --rect x0,y0,x1,y1 [--pathmin 0.8] [--label s]

Lane GIBLACK2: a WW_CELL_LIT_PROBE=95 shot (r = cellGiSample's path / 16: 1 the room blend, 2 the room fill, 3 the
seen fill, 4 the plain trilinear, 5 stranded with nothing reached (the trilinear); a stranded surface's evidence
search: 6 its room's blend, 7 the room's fill, 8 the seen fill, 9 a sealed room's dark). P: at least pathmin of the
rect's pixels decode (round(r x 16 / 255)) to a room's blend (1 or 6). --expect fail: the red (the rooms over the
soup's box, GIBLACK1's law): PASS when P fails.
One verdict line last; exit 0 on PASS, 1 on FAIL.
"""
import argparse
import sys

import numpy as np
from PIL import Image


def lin(a):
    a = a / 255.0
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('on')
    ap.add_argument('off', nargs='?')
    ap.add_argument('--path', action='store_true')
    ap.add_argument('--pathmin', type=float, default=0.8)
    ap.add_argument('--rect', required=True)
    ap.add_argument('--bar', type=float, default=0.5)
    ap.add_argument('--zero', type=int, default=3)
    ap.add_argument('--zeroshare', type=float, default=0.0)
    ap.add_argument('--expect', default='pass')
    ap.add_argument('--label', default='')
    a = ap.parse_args()
    on = np.asarray(Image.open(a.on).convert('RGB')).astype(float)
    if a.path:
        k = on.shape[1] / 1920.0
        x0, y0, x1, y1 = [int(round(int(v) * k)) for v in a.rect.split(',')]
        code = np.rint(on[y0:y1, x0:x1, 0] * 16.0 / 255.0).astype(int)
        h = np.bincount(code.ravel(), minlength=17)[:10] / code.size
        room = h[1] + h[6]
        ok = room >= a.pathmin
        print('  %s rect %d,%d..%d,%d: path share 0 (no GI) %.3f  1 room blend %.3f  2 room fill %.3f  3 seen fill %.3f  '
              '4 trilinear %.3f  5 stranded, none %.3f  6 stranded room blend %.3f  7 room fill %.3f  8 seen fill %.3f  '
              '9 sealed dark %.3f' % ((a.label, x0, y0, x1, y1) + tuple(h)))
        if a.expect == 'fail':
            print('%s  red %s: a room blend decides only %.1f%% of the wall (bar %.0f%%)' % (
                'PASS' if not ok else 'FAIL', a.label, 100 * room, 100 * a.pathmin))
            return 0 if not ok else 1
        print('%s  %s: a room blend decides %.1f%% of the wall (bar %.0f%%)' % ('PASS' if ok else 'FAIL', a.label,
                                                                               100 * room, 100 * a.pathmin))
        return 0 if ok else 1
    off = np.asarray(Image.open(a.off).convert('RGB')).astype(float)
    if on.shape != off.shape:
        # the window can come up a few rows short of the size asked for (the screen's work area): the camera's
        # vertical field spans each frame's height, so a centre crop to the common height lines the two up
        if on.shape[1] != off.shape[1] or abs(on.shape[0] - off.shape[0]) > 8:
            print('FAIL  the two shots differ in size %s %s' % (on.shape, off.shape))
            return 1
        h, d = min(on.shape[0], off.shape[0]), abs(on.shape[0] - off.shape[0])
        on = on[(on.shape[0] - h) // 2:][:h]
        off = off[(off.shape[0] - h) // 2:][:h]
        print('  heights differ by %d rows: both centre-cropped to %d' % (d, h))
    k = on.shape[1] / 1920.0
    x0, y0, x1, y1 = [int(round(int(v) * k)) for v in a.rect.split(',')]
    ro, rf = on[y0:y1, x0:x1], off[y0:y1, x0:x1]
    w = np.array([0.2126, 0.7152, 0.0722])
    mo, mf = float((lin(ro) @ w).mean()), float((lin(rf) @ w).mean())
    ratio = mo / mf if mf > 0 else 0.0
    zero = (ro.max(-1) <= a.zero) & (rf.max(-1) > a.zero)
    nz = int(zero.sum())
    okM = ratio >= 1.0 - a.bar
    okZ = nz <= a.zeroshare * zero.size
    print('  %s rect %d,%d..%d,%d (%d px): mean luminance on %.5f off %.5f ratio %.3f (bar >= %.2f) %s' %
          (a.label, x0, y0, x1, y1, ro.shape[0] * ro.shape[1], mo, mf, ratio, 1.0 - a.bar, 'ok' if okM else 'LOW'))
    print('  %s near-zero pixels (max channel <= %d, GI off above it): %d (%.2f%%, allowed %.2f%%) %s' %
          (a.label, a.zero, nz, 100.0 * nz / zero.size, 100.0 * a.zeroshare, 'ok' if okZ else 'BLACK'))
    good = okM and okZ
    if a.expect == 'fail':
        print('%s  red %s: the wall %s' % ('PASS' if not good else 'FAIL', a.label, 'blacks out' if not good else 'does NOT black out'))
        return 0 if not good else 1
    print('%s  %s: the wall keeps its light with the GI on' % ('PASS' if good else 'FAIL', a.label) if good else
          'FAIL  %s: the wall blacks out with the GI on' % a.label)
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
