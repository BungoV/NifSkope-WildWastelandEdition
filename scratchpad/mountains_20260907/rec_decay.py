"""Does colour matching hold up with distance, where geography cannot?

rec_alt.py measured that simply copying the nearest painted cell's material
(29.4%) matches or beats colour matching (28.4%).  But that test is rigged in
geography's favour: the OUTWARD holdout's test cells sit on the rim of the
painted blob, one or two cells from a training sample.  The real targets are up
to ~60 cells from any painted cell, where copying a neighbour is meaningless.

Colour matching's one structural advantage is that it should not care about
distance.  This measures whether that is true: score both methods against
distance-to-nearest-training-cell, using a deep 50% rim so a range of distances
exists.  If colour holds flat while geography collapses, colour is still the
better of two inadequate options at real range; if colour ALSO collapses, the
far terrain simply does not resemble the painted terrain and the premise fails.
"""
import os
import sys
from collections import Counter

import numpy as np

from rec_common import HERE, MINX, MINY, ltex_names
from rec_holdout import TRANSFER, load, knn_predict
from rec_alt import family_of


def main():
    Feat, dom, XY, W, ltex = load()
    names = ltex_names()
    fam = {int(f): family_of(names.get(int(f), ('',))[0]) for f in np.unique(dom)}

    cx, cy = XY[:, 0].astype(float), XY[:, 1].astype(float)
    r = np.hypot(cx - cx.mean(), cy - cy.mean())
    thr = np.percentile(r, 50)
    te, tr = (r > thr), (r <= thr)
    print('deep holdout: train %d (inner half), test %d (outer half)'
          % (tr.sum(), te.sum()))

    trXY, teXY = XY[tr], XY[te]
    truth = dom[te]
    tf = np.array([fam[int(t)] for t in truth])

    mu, sd = Feat.mean(0), Feat.std(0) + 1e-6
    Xtr = (Feat[tr][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    Xte = (Feat[te][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    predC, conf = knn_predict(Xtr, dom[tr], Xte)

    # geography + distance to nearest training cell
    tp = np.unique(trXY[:, :2], axis=0).astype(np.float32)
    predG = np.empty(len(teXY), dtype=np.int64)
    dist = np.empty(len(teXY), dtype=np.float32)
    trcells = trXY[:, :2].astype(np.float32)
    for s in range(0, len(teXY), 512):
        e = min(s + 512, len(teXY))
        d = np.linalg.norm(teXY[s:e, None, :2].astype(np.float32) - trcells[None, :, :], axis=2)
        idx = np.argsort(d, axis=1)[:, :8]
        dist[s:e] = d.min(axis=1)
        for i in range(e - s):
            predG[s + i] = Counter(dom[tr][idx[i]].tolist()).most_common(1)[0][0]

    hc, hg = (predC == truth), (predG == truth)
    fc = np.array([fam[int(p)] for p in predC]) == tf
    fg = np.array([fam[int(p)] for p in predG]) == tf
    print('\noverall:  colour+ LTEX %.1f%% family %.1f%%   |   geography LTEX %.1f%% family %.1f%%'
          % (100 * hc.mean(), 100 * fc.mean(), 100 * hg.mean(), 100 * fg.mean()))

    print('\n%-14s %7s | %8s %8s | %8s %8s' %
          ('dist to train', 'n', 'col LTEX', 'col fam', 'geo LTEX', 'geo fam'))
    bands = [(0, 1), (1, 2), (2, 4), (4, 8), (8, 16), (16, 1e9)]
    for a, b in bands:
        m = (dist >= a) & (dist < b)
        if m.sum() < 30:
            continue
        print('%5.0f - %-6.0f %7d | %7.1f%% %7.1f%% | %7.1f%% %7.1f%%'
              % (a, b, m.sum(), 100 * hc[m].mean(), 100 * fc[m].mean(),
                 100 * hg[m].mean(), 100 * fg[m].mean()))
    print('\n(distance in cells; the real recovery targets sit up to ~60 cells '
          'from any painted cell)')


if __name__ == '__main__':
    sys.exit(main())
