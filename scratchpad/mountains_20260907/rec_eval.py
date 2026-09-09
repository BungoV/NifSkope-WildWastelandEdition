"""The two evaluations that decide what, if anything, can be shipped.

1. CONFIDENCE STRATIFICATION.  A flat 28% is useless, but if the model knows
   which of its calls are good then the confident subset may be shippable on its
   own and the rest can be left alone.  The brief says a confidence number per
   assignment is worth more than a clean-looking table; this measures whether
   the confidence means anything.

2. COLOUR FIDELITY vs MATERIAL IDENTITY.  The brief's sharpest warning is that
   getting the colour right with the wrong LTEX moves a texture replacer's
   distant terrain the WRONG way.  So: when the model picks the wrong material,
   how wrong is the resulting colour?  If colour error stays small while
   identity is wrong, the recovery looks good in a screenshot and is wrong in
   exactly the way that matters -- and that has to be stated plainly.

3. Failure cases by LTEX, which the brief asks for by name.
"""
import os
import sys
from collections import Counter

import numpy as np

from rec_common import HERE, MINX, MINY, N, parse_layers, ltex_names
from rec_holdout import TRANSFER, load, knn_predict, folds_outward, folds_blocked
from rec_labels import build as build_groups


def main():
    Feat, dom, XY, W, ltex = load()
    names = ltex_names()
    groups, gid, keys, gname = build_groups()
    gmap = {int(f): gid[groups.get(int(f), '#%08x' % int(f))] for f in np.unique(dom)}
    pal = np.load(os.path.join(HERE, 'palette.npz'))
    P, pltex = pal['P'], pal['ltex']
    pidx = {int(f): i for i, f in enumerate(pltex)}

    mu, sd = Feat.mean(0), Feat.std(0) + 1e-6
    f = folds_outward(XY)
    te, tr = (f == 1), (f == 0)
    Xtr = (Feat[tr][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    Xte = (Feat[te][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    pred, conf = knn_predict(Xtr, dom[tr], Xte)
    truth = dom[te]
    hit = (pred == truth)
    pg = np.array([gmap[int(p)] for p in pred])
    tg = np.array([gmap[int(t)] for t in truth])
    hitg = (pg == tg)

    print('=== 1. does the confidence mean anything? (OUTWARD holdout) ===')
    print('%-16s %8s %9s %9s' % ('confidence band', 'n', 'LTEX', 'group'))
    edges = [0.0, 0.2, 0.3, 0.4, 0.5, 0.7, 1.01]
    for a, b in zip(edges[:-1], edges[1:]):
        m = (conf >= a) & (conf < b)
        if m.sum() == 0:
            continue
        print('%.2f - %.2f      %8d %8.1f%% %8.1f%%'
              % (a, b, m.sum(), 100 * hit[m].mean(), 100 * hitg[m].mean()))
    print()
    order = np.argsort(-conf)
    for frac in (0.05, 0.10, 0.20, 0.30, 0.50):
        k = int(len(order) * frac)
        m = order[:k]
        print('  most-confident %3.0f%% (n=%5d): LTEX %5.1f%%   group %5.1f%%'
              % (100 * frac, k, 100 * hit[m].mean(), 100 * hitg[m].mean()))

    print('\n=== 2. colour fidelity vs material identity ===')
    tcol = Feat[te][:, :3]
    pcol = np.array([P[pidx[int(p)]] if int(p) in pidx else np.full(3, np.nan)
                     for p in pred])
    err = np.linalg.norm(pcol - tcol, axis=1)
    good = ~np.isnan(err)
    print('predicted material colour vs the cell\'s true LOD colour:')
    for t in (2, 5, 10, 20):
        print('   within %2d RGB units: %5.1f%% of quadrants' % (t, 100 * (err[good] < t).mean()))
    print('   median colour error %.1f' % np.median(err[good]))
    print('   ... while LTEX identity is right only %.1f%% of the time.' % (100 * hit.mean()))
    w = hit[good] == False
    print('   among the %d WRONG-material calls, median colour error is %.1f'
          % (w.sum(), np.median(err[good][w])))
    print('   among the correct ones,                median colour error is %.1f'
          % np.median(err[good][~w]))

    print('\n=== 3. failure cases by true material (OUTWARD holdout) ===')
    print('%-34s %6s %7s   %s' % ('true material', 'n', 'recall', 'most common wrong answer'))
    cnt = Counter(truth.tolist())
    for fid, n in cnt.most_common(18):
        m = (truth == fid)
        rec = hit[m].mean()
        wrong = Counter(pred[m & ~hit].tolist())
        w1 = wrong.most_common(1)
        wn = ('%s %.0f%%' % (names.get(int(w1[0][0]), ('?',))[0],
                             100.0 * w1[0][1] / m.sum())) if w1 else '-'
        print('%-34s %6d %6.1f%%   %s'
              % (names.get(int(fid), ('?',))[0], n, 100 * rec, wn))


if __name__ == '__main__':
    sys.exit(main())
