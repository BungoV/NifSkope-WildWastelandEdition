"""Does spatial coherence rescue the accuracy?

Per-quadrant prediction is noisy, but landscape material is painted in patches,
not per quadrant.  If the noise is roughly independent between neighbours, a
majority vote over a neighbourhood should recover far more than 28% -- this is
the strongest remaining idea for making the recovery usable, so it gets measured
properly rather than assumed.

Three things here:
  1. how spatially coherent the TRUTH actually is (an upper bound on what
     smoothing can buy);
  2. the OUTWARD holdout re-scored after neighbourhood voting at several radii;
  3. a block-constant ORACLE -- if we somehow assigned each NxN block of cells
     its single best material, what would we score?  Nothing that predicts one
     material per block can beat this, so it bounds the whole approach.
"""
import os
import sys
from collections import Counter

import numpy as np

from rec_common import HERE, MINX, MINY, N, parse_layers, ltex_names
from rec_holdout import (TRANSFER, COLOUR3, load, knn_predict, folds_outward,
                         score)
from rec_labels import build as build_groups


def main():
    Feat, dom, XY, W, ltex = load()
    names = ltex_names()
    groups, gid, keys, gname = build_groups()
    gmap = {int(f): gid[groups.get(int(f), '#%08x' % int(f))] for f in np.unique(dom)}

    # index by (cell row, cell col, quadrant)
    lab = np.full((N, N, 4), -1, dtype=np.int64)
    for i, (cx, cy, q) in enumerate(XY):
        lab[cy - MINY, cx - MINX, q] = dom[i]

    # ---- 1. how coherent is the truth? ----
    print('=== spatial coherence of the true dominant material ===')
    ok = lab >= 0
    same_e = ((lab[:, :-1] == lab[:, 1:]) & ok[:, :-1] & ok[:, 1:]).sum()
    tot_e = (ok[:, :-1] & ok[:, 1:]).sum()
    same_n = ((lab[:-1] == lab[1:]) & ok[:-1] & ok[1:]).sum()
    tot_n = (ok[:-1] & ok[1:]).sum()
    print('adjacent CELLS share a dominant material (same quadrant): '
          '%.1f%% east-west, %.1f%% north-south'
          % (100.0 * same_e / tot_e, 100.0 * same_n / tot_n))
    q = lab.reshape(N, N, 4)
    inter = 0
    intert = 0
    for a in range(4):
        for b in range(a + 1, 4):
            m = (q[:, :, a] >= 0) & (q[:, :, b] >= 0)
            inter += (q[:, :, a][m] == q[:, :, b][m]).sum()
            intert += m.sum()
    print('two quadrants of the SAME cell agree: %.1f%%' % (100.0 * inter / intert))
    for blk in (2, 4, 8, 16):
        hits = tot = 0
        for r0 in range(0, N, blk):
            for c0 in range(0, N, blk):
                v = lab[r0:r0 + blk, c0:c0 + blk]
                v = v[v >= 0]
                if len(v) == 0:
                    continue
                hits += Counter(v.tolist()).most_common(1)[0][1]
                tot += len(v)
        print('ORACLE, one material per %2dx%-2d cell block: %.1f%% of quadrants correct'
              % (blk, blk, 100.0 * hits / tot))

    # ---- 2. smoothing the OUTWARD holdout ----
    print('\n=== OUTWARD holdout, with neighbourhood voting ===')
    mu, sd = Feat.mean(0), Feat.std(0) + 1e-6
    f = folds_outward(XY)
    te, tr = (f == 1), (f == 0)
    Xtr = (Feat[tr][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    Xte = (Feat[te][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    pred, conf = knn_predict(Xtr, dom[tr], Xte)
    a, g = score(pred, dom[te], gmap)
    print('  raw per-quadrant          LTEX %5.1f%%   group %5.1f%%' % (100 * a, 100 * g))

    # scatter predictions back onto the grid, then vote in a (2r+1) cell window
    pg = np.full((N, N, 4), -1, dtype=np.int64)
    teXY = XY[te]
    for i, (cx, cy, qq) in enumerate(teXY):
        pg[cy - MINY, cx - MINX, qq] = pred[i]
    truth = dom[te]
    for r in (1, 2, 3, 5):
        out = np.empty(len(teXY), dtype=np.int64)
        for i, (cx, cy, qq) in enumerate(teXY):
            rr, cc = cy - MINY, cx - MINX
            win = pg[max(0, rr - r):rr + r + 1, max(0, cc - r):cc + r + 1]
            v = win[win >= 0]
            out[i] = Counter(v.tolist()).most_common(1)[0][0] if len(v) else pred[i]
        a, g = score(out, truth, gmap)
        print('  vote over +-%d cells (%2dx%-2d) LTEX %5.1f%%   group %5.1f%%'
              % (r, 2 * r + 1, 2 * r + 1, 100 * a, 100 * g))


if __name__ == '__main__':
    sys.exit(main())
