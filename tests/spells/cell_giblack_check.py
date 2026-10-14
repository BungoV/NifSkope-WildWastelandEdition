#!/usr/bin/env python3
"""Lane GIBLACK1 (2026-10-14) checker: a wall the probe GI must not black out.

  cell_giblack_check.py <on.png> <off.png> --rect x0,y0,x1,y1 [--bar 0.5] [--zero 3] [--expect pass|fail] [--label s]

on = the shot with the probe GI, off = the same camera with WW_CELL_GI=0 (the weather's flat ambient). Over the rect
(pixels of a 1920-wide frame; scaled to the shots' own width):
  M  the mean linear luminance with the GI on is at least (1 - bar) of the GI-off mean (a recess may read darker with
     the GI, that is the probe sky; it may not lose more than the bar)
  Z  no near-zero pixel: none whose brightest channel is <= zero (8-bit sRGB) where the GI-off shot's is above it
--expect fail: the red (the old search); the verdict is PASS when M or Z fails.
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
    ap.add_argument('off')
    ap.add_argument('--rect', required=True)
    ap.add_argument('--bar', type=float, default=0.5)
    ap.add_argument('--zero', type=int, default=3)
    ap.add_argument('--expect', default='pass')
    ap.add_argument('--label', default='')
    a = ap.parse_args()
    on = np.asarray(Image.open(a.on).convert('RGB')).astype(float)
    off = np.asarray(Image.open(a.off).convert('RGB')).astype(float)
    if on.shape != off.shape:
        print('FAIL  the two shots differ in size %s %s' % (on.shape, off.shape))
        return 1
    k = on.shape[1] / 1920.0
    x0, y0, x1, y1 = [int(round(int(v) * k)) for v in a.rect.split(',')]
    ro, rf = on[y0:y1, x0:x1], off[y0:y1, x0:x1]
    w = np.array([0.2126, 0.7152, 0.0722])
    mo, mf = float((lin(ro) @ w).mean()), float((lin(rf) @ w).mean())
    ratio = mo / mf if mf > 0 else 0.0
    zero = (ro.max(-1) <= a.zero) & (rf.max(-1) > a.zero)
    nz = int(zero.sum())
    okM = ratio >= 1.0 - a.bar
    okZ = nz == 0
    print('  %s rect %d,%d..%d,%d (%d px): mean luminance on %.5f off %.5f ratio %.3f (bar >= %.2f) %s' %
          (a.label, x0, y0, x1, y1, ro.shape[0] * ro.shape[1], mo, mf, ratio, 1.0 - a.bar, 'ok' if okM else 'LOW'))
    print('  %s near-zero pixels (max channel <= %d, GI off above it): %d (%.2f%%) %s' %
          (a.label, a.zero, nz, 100.0 * nz / zero.size, 'ok' if okZ else 'BLACK'))
    good = okM and okZ
    if a.expect == 'fail':
        print('%s  red %s: the wall %s' % ('PASS' if not good else 'FAIL', a.label, 'blacks out' if not good else 'does NOT black out'))
        return 0 if not good else 1
    print('%s  %s: the wall keeps its light with the GI on' % ('PASS' if good else 'FAIL', a.label) if good else
          'FAIL  %s: the wall blacks out with the GI on' % a.label)
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
