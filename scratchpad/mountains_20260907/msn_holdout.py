"""Does adding the _msn columns actually buy accuracy on the honest holdout?

The F-ratio screen in msn_probe.py says whether the normal map carries material
information at all.  It does not say whether that information is NEW -- a
feature can be strongly material-dependent and still be a restatement of
colour, in which case it adds nothing.  The only test that settles it is the
same one the colour baseline was scored on: rec_holdout.py's OUTWARD split,
train on the inner core of the painted blob and predict its outer rim, which is
the honest analogue of extrapolating into unpainted terrain.

Three predictors, identical k-NN, identical folds, only the columns differ:
  colour+       rec_shift.py's transferable colour columns (the 28.4% baseline)
  msn only      this lane's transferable _msn columns
  colour+ + msn both

The transferable _msn set is recomputed here from the same rule msn_shift.py
uses rather than pasted in, so this cannot drift out of step with the table.
"""
import os
import sys

import numpy as np

from rec_common import HERE, MINX, MINY, parse_layers, ltex_names
from rec_labels import build as build_groups
from rec_holdout import knn_predict, folds_blocked, folds_outward, score, TRANSFER


def transferable_msn(M, have):
    painted, _ = parse_layers()
    P = M[painted & have].reshape(-1, M.shape[-1])
    Q = M[(~painted) & have].reshape(-1, M.shape[-1])
    keep = []
    for i in range(M.shape[-1]):
        p, q = P[:, i], Q[:, i]
        lo, hi = np.percentile(p, [1, 99])
        ov = float(((q >= lo) & (q <= hi)).mean())
        sd = p.std() or 1.0
        if ov >= 0.90 and abs((q.mean() - p.mean()) / sd) <= 0.60:
            keep.append(i)
    return keep


def main():
    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M, have, names = z['M'], z['have'], list(z['names'])
    Fc = np.load(os.path.join(HERE, 'features.npz'))['F']
    t = np.load(os.path.join(HERE, 'training.npz'))
    W, XY, ltex = t['W'], t['XY'], t['ltex']
    dom = ltex[np.argmax(W, axis=1)].astype(np.int64)

    keep = transferable_msn(M, have)
    print('transferable _msn columns (%d): %s'
          % (len(keep), ', '.join(names[i] for i in keep)))

    r, c, q = XY[:, 1] - MINY, XY[:, 0] - MINX, XY[:, 2]
    Xc = Fc[r, c, q]
    Xm = M[r, c, q][:, keep]
    A = np.concatenate([Xc[:, TRANSFER], Xm], axis=1).astype(np.float32)
    ncol = len(TRANSFER)
    mu, sd = A.mean(0), A.std(0) + 1e-6
    A = (A - mu) / sd

    groups, gid, keys, gname = build_groups()
    gmap = {int(f): gid[groups.get(int(f), '#%08x' % int(f))] for f in np.unique(dom)}

    sets = {'colour+': list(range(ncol)),
            'msn only': list(range(ncol, A.shape[1])),
            'colour+msn': list(range(A.shape[1]))}

    for scheme, f, ids in (('OUTWARD (inner core -> outer rim)', folds_outward(XY), [1]),
                           ('BLOCKED (5 folds, 12-cell blocks)',
                            folds_blocked(XY), sorted(set(folds_blocked(XY).tolist())))):
        print('\n=== %s ===' % scheme)
        for nm, cols in sets.items():
            acc = gac = n = 0.0
            for fi in ids:
                te = (f == fi)
                p, _ = knn_predict(A[~te][:, cols], dom[~te], A[te][:, cols])
                a, g = score(p, dom[te], gmap)
                acc += a * te.sum(); gac += g * te.sum(); n += te.sum()
            print('  %-11s (%2d cols)  LTEX top-1 %5.1f%%   texture-group top-1 %5.1f%%'
                  % (nm, len(cols), 100 * acc / n, 100 * gac / n))


if __name__ == '__main__':
    sys.exit(main())
