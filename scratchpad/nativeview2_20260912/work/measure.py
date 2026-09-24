#!/usr/bin/env python
"""Lane NATIVEVIEW2's numbers, off the PNGs one `shots.sh` run wrote.

  python measure.py <dir> [<dir2> ...]

Per frame: the covered mask, its luma mean and standard deviation, and the dark
fraction (luma < 40, the threshold NATIVEVIEW1 measured 20.0 percent with).
Then, per directory, the own-vs-flat pair that gate (b) is about: the dark-mask
IoU, and the fraction of covered pixels whose luma differs by more than 8.

Luma is Rec.601  0.299 R + 0.587 G + 0.114 B  on the 8-bit frame, stated here
because a different weighting moves every number below.

Background is the viewport's clear colour (43,45,49) with WW_RENDER_CLEAN=1,
read back per frame rather than assumed; a TOP frame of this chunk has none.
The mask a PAIR is compared over is the INTERSECTION of the two arms' covered
masks, so no arm can win by covering less.
"""
import os
import sys

import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], np.int16)
DARK = 40.0


def luma(a):
    return 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]


def load(p):
    a = np.array(Image.open(p).convert("RGB")).astype(np.int16)
    cov = (np.abs(a - BG).sum(axis=2) > 0)
    return a, cov, luma(a.astype(np.float64))


def row(name, a, cov, Y):
    n = int(cov.sum())
    if n == 0:
        return "%-22s  EMPTY" % name
    y = Y[cov]
    return ("%-22s %5dx%-4d cov %7d (%5.1f%%)  luma mean %6.2f sd %6.2f  dark<40 %6.2f%%"
            % (name, a.shape[1], a.shape[0], n, 100.0 * n / cov.size,
               y.mean(), y.std(), 100.0 * (y < DARK).mean()))


def pair(tag, pa, pb, an, bn):
    aa, ca, Ya = load(pa)
    ab, cb, Yb = load(pb)
    m = ca & cb
    if m.sum() == 0:
        print("  %-28s no common coverage" % tag)
        return
    da = (Ya < DARK) & m
    db = (Yb < DARK) & m
    inter = int((da & db).sum())
    union = int((da | db).sum())
    iou = inter / union if union else float("nan")
    d = np.abs(Ya - Yb)[m]
    print("  %-28s common %7d  dark %s %5.2f%%  dark %s %5.2f%%  IoU %.3f  |dY|>8 %5.2f%%  mean|dY| %5.2f"
          % (tag, int(m.sum()), an, 100.0 * da.sum() / m.sum(), bn,
             100.0 * db.sum() / m.sum(), iou, 100.0 * (d > 8).mean(), d.mean()))


def main():
    for d in sys.argv[1:]:
        print("=== %s" % d)
        names = sorted(f for f in os.listdir(d) if f.endswith(".png"))
        for f in names:
            a, cov, Y = load(os.path.join(d, f))
            print(" " + row(f[:-4], a, cov, Y))
        print(" --- pairs")
        for view in ("top", "obl"):
            for a, b in (("own", "flat"), ("own", "tilt"), ("flat", "tilt")):
                pa = os.path.join(d, "t_%s_%s.png" % (a, view))
                pb = os.path.join(d, "t_%s_%s.png" % (b, view))
                if os.path.exists(pa) and os.path.exists(pb):
                    pair("terrain %s: %s vs %s" % (view, a, b), pa, pb, a, b)
        print()


if __name__ == "__main__":
    main()
