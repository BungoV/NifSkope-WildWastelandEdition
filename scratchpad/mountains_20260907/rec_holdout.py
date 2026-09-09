"""Holdout validation, on spatially contiguous regions.

Two schemes, because they answer different questions:

  BLOCKED  -- the painted area is cut into 12x12-cell blocks and the blocks are
              dealt round-robin into 5 folds.  A cell and its neighbours are
              always in the same fold, so nothing leaks, but every fold still
              spans the whole range of terrain types.  This is the fair estimate
              of "how well does this generalise".

  OUTWARD  -- train on the INNER core of the painted blob, test on its OUTER
              rim.  This is the honest analogue of the real task, which is
              extrapolation away from the painted region, and it will be worse.

Predictors compared, all k-NN in a standardised feature space:
  colour3   -- mean RGB only (the brief's baseline)
  colour+   -- every feature rec_shift.py passed as transferable
  palette   -- nearest fitted material colour (no training samples at all)
and against two null models:
  prior     -- always the material with the largest painted area
  chance    -- 1 / number of materials

Accuracy is reported on LTEX identity AND on texture group, because a texture
replacer only ever notices the latter.
"""
import os
import sys
from collections import Counter

import numpy as np

from rec_common import HERE, MINX, MINY, N, NULL, parse_layers, ltex_names
from rec_labels import build as build_groups

TRANSFER = [0, 1, 2, 3, 4, 5, 7, 10, 11, 12, 13, 14, 15, 16]   # rec_shift.py verdict
COLOUR3 = [0, 1, 2]
K = 15


def load():
    z = np.load(os.path.join(HERE, 'training.npz'))
    W, XY, ltex = z['W'], z['XY'], z['ltex']
    dom = ltex[np.argmax(W, axis=1)].astype(np.int64)
    f = np.load(os.path.join(HERE, 'features.npz'))['F']
    Feat = np.empty((len(XY), f.shape[-1]), dtype=np.float32)
    for i, (cx, cy, q) in enumerate(XY):
        Feat[i] = f[cy - MINY, cx - MINX, q]
    return Feat, dom, XY, W, ltex


def knn_predict(Xtr, ytr, Xte, k=K, chunk=512):
    """distance-weighted k-NN vote; returns (pred, confidence)."""
    pred = np.empty(len(Xte), dtype=ytr.dtype)
    conf = np.empty(len(Xte), dtype=np.float32)
    for s in range(0, len(Xte), chunk):
        e = min(s + chunk, len(Xte))
        d = np.linalg.norm(Xte[s:e, None, :] - Xtr[None, :, :], axis=2)
        idx = np.argpartition(d, k, axis=1)[:, :k]
        dd = np.take_along_axis(d, idx, axis=1)
        w = 1.0 / (dd + 1e-3)
        lab = ytr[idx]
        for r in range(e - s):
            acc = {}
            for j in range(k):
                acc[lab[r, j]] = acc.get(lab[r, j], 0.0) + w[r, j]
            tot = sum(acc.values())
            best = max(acc.items(), key=lambda kv: kv[1])
            pred[s + r] = best[0]
            conf[s + r] = best[1] / tot
    return pred, conf


def folds_blocked(XY, nfold=5, block=12):
    bx = (XY[:, 0] - MINX) // block
    by = (XY[:, 1] - MINY) // block
    return ((bx * 7 + by * 3) % nfold)


def folds_outward(XY, frac=0.30):
    """test = the outermost `frac` of painted cells by distance from the
    painted centroid; train = the inner core."""
    cx, cy = XY[:, 0].astype(float), XY[:, 1].astype(float)
    r = np.hypot(cx - cx.mean(), cy - cy.mean())
    thr = np.percentile(r, 100 * (1 - frac))
    return (r > thr).astype(int)      # 1 = test fold


def score(pred, truth, gmap):
    a = float((pred == truth).mean())
    pg = np.array([gmap[int(p)] for p in pred])
    tg = np.array([gmap[int(t)] for t in truth])
    return a, float((pg == tg).mean())


def main():
    Feat, dom, XY, W, ltex = load()
    names = ltex_names()
    groups, gid, keys, gname = build_groups()
    gmap = {int(f): gid[groups.get(int(f), '#%08x' % int(f))] for f in np.unique(dom)}

    mu, sd = Feat.mean(0), Feat.std(0) + 1e-6

    pal = np.load(os.path.join(HERE, 'palette.npz'))
    P, pltex, area = pal['P'], pal['ltex'], pal['area']

    prior = Counter(dom.tolist()).most_common(1)[0][0]
    print('null models: prior = %s (%.1f%% of painted quadrants), chance = %.2f%%'
          % (names.get(int(prior), ('?',))[0],
             100.0 * (dom == prior).mean(), 100.0 / len(np.unique(dom))))
    print('%d painted quadrants, %d distinct dominant materials, %d texture groups'
          % (len(dom), len(np.unique(dom)), len({gmap[int(d)] for d in np.unique(dom)})))

    for scheme, f in (('BLOCKED (5 folds, 12-cell blocks)', folds_blocked(XY)),
                      ('OUTWARD (inner core -> outer rim)', folds_outward(XY))):
        print('\n=== %s ===' % scheme)
        if scheme.startswith('OUTWARD'):
            fold_ids = [1]
        else:
            fold_ids = sorted(set(f.tolist()))
        res = {}
        for name, cols in (('colour3', COLOUR3), ('colour+', TRANSFER)):
            A = []
            G = []
            CONF = []
            for fi in fold_ids:
                te = (f == fi)
                tr = ~te
                Xtr = (Feat[tr][:, cols] - mu[cols]) / sd[cols]
                Xte = (Feat[te][:, cols] - mu[cols]) / sd[cols]
                p, cf = knn_predict(Xtr, dom[tr], Xte)
                a, g = score(p, dom[te], gmap)
                A.append(a * te.sum()); G.append(g * te.sum()); CONF.append(cf)
            n = sum((f == fi).sum() for fi in fold_ids)
            res[name] = (sum(A) / n, sum(G) / n)
            print('  %-9s  LTEX top-1 %5.1f%%   texture-group top-1 %5.1f%%'
                  % (name, 100 * res[name][0], 100 * res[name][1]))

        # palette nearest-colour, uses no training samples
        te = (f == fold_ids[0]) if len(fold_ids) == 1 else np.ones(len(f), bool)
        keep = area >= 20.0
        d = np.linalg.norm(Feat[te][:, None, :3] - P[None, keep, :], axis=2)
        p = pltex[keep][np.argmin(d, axis=1)].astype(np.int64)
        gm = dict(gmap)
        for fid in np.unique(p):
            gm.setdefault(int(fid), gid[groups.get(int(fid), '#%08x' % int(fid))])
        a, g = score(p, dom[te], gm)
        print('  %-9s  LTEX top-1 %5.1f%%   texture-group top-1 %5.1f%%'
              % ('palette', 100 * a, 100 * g))
        pa = float((np.full(te.sum(), prior) == dom[te]).mean())
        print('  %-9s  LTEX top-1 %5.1f%%' % ('prior', 100 * pa))


if __name__ == '__main__':
    sys.exit(main())
