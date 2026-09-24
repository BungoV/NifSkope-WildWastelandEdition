#!/usr/bin/env python
"""The two red rows' statistics, computed on every calibration arm.

  python measure.py <framedir> [<pair> ...]     pair = A:B

Reuses native_lighting_check.py's own arithmetic verbatim (BG, DARK, BLK=32,
the darkest-fifth IoU and the block-averaged SD of the luma difference) so the
numbers are comparable with the gate's.  Lane GATEFIX2, 2026-09-19.
"""
import os
import sys

import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], np.int16)
BLK = 32


def load(d, n):
    a = np.array(Image.open(os.path.join(d, n + ".png")).convert("RGB")).astype(np.int16)
    cov = (np.abs(a - BG).sum(axis=2) > 0)
    Y = (0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]).astype(np.float64)
    return cov, Y


def blocks(Y, cov):
    h, w = Y.shape
    bh, bw = h // BLK, w // BLK
    ys = Y[:bh * BLK, :bw * BLK].reshape(bh, BLK, bw, BLK)
    cs = cov[:bh * BLK, :bw * BLK].reshape(bh, BLK, bw, BLK)
    full = cs.all(axis=(1, 3))
    return ys.mean(axis=(1, 3))[full], int(full.sum())


def stats(d, a, b, view="obl"):
    c1, Y1 = load(d, "t_%s_%s" % (a, view))
    c2, Y2 = load(d, "t_%s_%s" % (b, view))
    m = c1 & c2
    q1, q2 = np.quantile(Y1[m], 0.20), np.quantile(Y2[m], 0.20)
    A = (Y1 <= q1) & m
    B = (Y2 <= q2) & m
    iou = int((A & B).sum()) / max(1, int((A | B).sum()))
    D = Y1 - Y2
    bm, nb = blocks(D, m)
    return iou, float(bm.std()), float(np.abs(D[m]).mean()), nb, float(np.abs(D[m]).std())


def main():
    d = sys.argv[1]
    pairs = [p.split(":") for p in sys.argv[2:]] or [["own", "flat"]]
    print("%-14s %-8s %-9s %-10s %-8s" % ("pair", "IoU", "blockSD", "mean|dY|", "blocks"))
    for a, b in pairs:
        iou, bsd, md, nb, sd = stats(d, a, b)
        print("%-14s %-8.3f %-9.2f %-10.2f %-8d (pixel sd of dY %.2f)"
              % ("%s vs %s" % (a, b), iou, bsd, md, nb, sd))


if __name__ == "__main__":
    main()
