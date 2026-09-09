"""The test that would prove the _msn is more than spatial autocorrelation.

rec_decay.py measured that the colour model's accuracy falls from 43.9% one
cell from a training sample to 11.3% sixteen cells out.  That decay is the
proof colour was not identifying material: a colour feature does not know how
far away it is, so a genuine colour-material law would score flat in distance.
What the k-NN was really retrieving was nearby cells.

The same knife cuts the normal map.  If baked material detail is real, its
accuracy should be MUCH flatter in distance than colour's, because gravel is
gravel wherever it is.  If the _msn columns decay just as steeply, they are
another proxy for "which part of the map is this" and add nothing at the range
the real recovery targets sit at (up to ~60 cells out).

Same deep 50% rim holdout, same k-NN, same labels as rec_decay.py; only the
columns change.
"""
import os
import sys

import numpy as np

from rec_common import HERE, MINX, MINY, parse_layers
from rec_holdout import TRANSFER, knn_predict
from msn_holdout import transferable_msn


def main():
    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M, have, names = z['M'], z['have'], list(z['names'])
    Fc = np.load(os.path.join(HERE, 'features.npz'))['F']
    t = np.load(os.path.join(HERE, 'training.npz'))
    W, XY, ltex = t['W'], t['XY'], t['ltex']
    dom = ltex[np.argmax(W, axis=1)].astype(np.int64)

    keep = transferable_msn(M, have)
    r, c, q = XY[:, 1] - MINY, XY[:, 0] - MINX, XY[:, 2]
    A = np.concatenate([Fc[r, c, q][:, TRANSFER], M[r, c, q][:, keep]],
                       axis=1).astype(np.float32)
    nc = len(TRANSFER)
    A = (A - A.mean(0)) / (A.std(0) + 1e-6)

    cx, cy = XY[:, 0].astype(float), XY[:, 1].astype(float)
    rad = np.hypot(cx - cx.mean(), cy - cy.mean())
    thr = np.percentile(rad, 50)
    te, tr = rad > thr, rad <= thr
    print('deep holdout: train %d (inner half), test %d (outer half)'
          % (tr.sum(), te.sum()))
    print('msn columns used (%d): %s' % (len(keep), ', '.join(names[i] for i in keep)))

    trc = XY[tr][:, :2].astype(np.float32)
    dist = np.empty(te.sum(), dtype=np.float32)
    teXY = XY[te][:, :2].astype(np.float32)
    for s in range(0, len(teXY), 1024):
        e = min(s + 1024, len(teXY))
        d = np.linalg.norm(teXY[s:e, None, :] - trc[None, :, :], axis=2)
        dist[s:e] = d.min(axis=1)

    truth = dom[te]
    sets = {'colour+': list(range(nc)),
            'msn only': list(range(nc, A.shape[1])),
            'colour+msn': list(range(A.shape[1]))}
    hits = {}
    for nm, cols in sets.items():
        p, _ = knn_predict(A[tr][:, cols], dom[tr], A[te][:, cols])
        hits[nm] = (p == truth)
        print('overall %-11s LTEX top-1 %5.1f%%' % (nm, 100 * hits[nm].mean()))

    print()
    print('%-14s %7s %s' % ('dist to train', 'n',
                            ''.join('%12s' % k for k in sets)))
    for a, b in [(0, 1), (1, 2), (2, 4), (4, 8), (8, 16), (16, 1e9)]:
        m = (dist >= a) & (dist < b)
        if m.sum() < 30:
            continue
        print('%5.0f - %-6.0f %7d %s'
              % (a, b, m.sum(), ''.join('%11.1f%%' % (100 * hits[k][m].mean())
                                        for k in sets)))
    print('\n(distance in cells; the real targets sit up to ~60 cells out)')


if __name__ == '__main__':
    sys.exit(main())
