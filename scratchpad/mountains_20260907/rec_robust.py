"""Is the distance decay real, or an artefact of how the holdout was cut?

The deep holdout in rec_decay.py trains on the inner half of the painted blob
and tests on the outer half.  That confounds two things: the test cells are far
from training data AND they sit at the extremes of the painted region, where the
terrain may simply be odd.  If the decay is really about "odd terrain" rather
than "distance", the headline conclusion is overstated.

So the same measurement is repeated under splits where that confound does not
apply -- a west/east cut and a north/south cut, in which the test cells span the
full range of distances and are not selected for being extreme, and a
large-block checkerboard.  If accuracy still falls off with distance under every
split, the decay is a property of the problem and not of the cut.

NULL-dominant quadrants are relabelled to their best real material throughout,
because "no base texture" is not a legal answer for this mod (see report section 7).
"""
import os
import sys

import numpy as np

from rec_common import HERE, NULL, ltex_names
from rec_holdout import TRANSFER, load
from rec_apply import knn_fast, vote

BANDS = [(0, 2), (2, 4), (4, 8), (8, 16), (16, 1e9)]


def relabel(dom, W, ltex):
    lab = dom.copy()
    order = np.argsort(-W, axis=1)
    for i in np.where(dom == NULL)[0]:
        for j in order[i]:
            if ltex[j] != NULL and W[i, j] > 0:
                lab[i] = ltex[j]
                break
    return lab


def run(name, te, Feat, lab, XY):
    tr = ~te
    if te.sum() < 200 or tr.sum() < 200:
        return
    mu, sd = Feat[tr].mean(0), Feat[tr].std(0) + 1e-6
    X = ((Feat[:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]).astype(np.float32)
    I, D = knn_fast(X[tr], X[te])
    p, c = vote(I, D, lab[tr])
    hit = (p == lab[te])
    trc = XY[tr][:, :2].astype(np.float32)
    tec = XY[te][:, :2].astype(np.float32)
    dmin = np.empty(len(tec), dtype=np.float32)
    for s in range(0, len(tec), 1024):
        e = min(s + 1024, len(tec))
        dmin[s:e] = np.linalg.norm(tec[s:e, None] - trc[None], axis=2).min(1)
    row = '%-22s %6d %7.1f%% |' % (name, te.sum(), 100 * hit.mean())
    for a, b in BANDS:
        m = (dmin >= a) & (dmin < b)
        row += ('  %6.1f%%' % (100 * hit[m].mean())) if m.sum() >= 40 else '      n/a'
    print(row)


def main():
    Feat, dom, XY, W, ltex = load()
    lab = relabel(dom, W, ltex)
    keep = lab != NULL
    Feat, lab, XY = Feat[keep], lab[keep], XY[keep]
    cx, cy = XY[:, 0], XY[:, 1]

    print('accuracy on LTEX identity, NULL excluded as an answer')
    print('%-22s %6s %8s |%s' % ('split', 'n test', 'overall',
                                 ''.join('%9s' % ('%d-%d' % (a, b) if b < 1e9 else '16+')
                                         for a, b in BANDS)))
    print('-' * 78)
    run('west trains / east', cx > np.median(cx), Feat, lab, XY)
    run('east trains / west', cx <= np.median(cx), Feat, lab, XY)
    run('south trains / north', cy > np.median(cy), Feat, lab, XY)
    run('north trains / south', cy <= np.median(cy), Feat, lab, XY)
    r = np.hypot(cx - cx.mean(), cy - cy.mean())
    run('inner trains / outer', r > np.percentile(r, 50), Feat, lab, XY)
    bx, by = (cx + 96) // 16, (cy + 96) // 16
    run('block checkerboard', ((bx + by) % 2) == 0, Feat, lab, XY)
    print('-' * 78)
    print('(columns are distance in cells from the test quadrant to the nearest '
          'training quadrant)')


if __name__ == '__main__':
    sys.exit(main())
