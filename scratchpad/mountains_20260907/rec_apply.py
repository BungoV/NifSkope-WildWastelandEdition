"""Produce assignments for all 32,909 unpainted cells, with honest confidence.

The measurements in report_recover.md say this recovery is not accurate enough
to ship as a material recovery (11.3% at >16 cells from painted terrain, against
an 18.5% null).  The file is produced anyway because it was asked for and
because it is the artefact any judgement has to be made against -- but two
things are done to keep it from being mistaken for something better than it is:

  * the header carries the measured accuracy, so it cannot be consumed blind;
  * `conf` is CALIBRATED, not a raw vote share.  Raw k-NN vote share is wildly
    optimistic here.  The calibration maps vote share to the accuracy actually
    observed in the deep holdout restricted to quadrants >= 8 cells from any
    training cell, which is the regime the real targets live in.

The NULL pseudo-material (dominant in 16.4% of painted quadrants: ATXT layers
with no BTXT under them) is never emitted.  Writing "no base texture" into a mod
whose whole purpose is to supply a base texture would be actively harmful, so
where NULL is dominant the label falls back to that quadrant's best real
material.

Writes recovered.txt (BTXT-only, one flat material per quadrant) and
recovered_blend.txt (the full predicted blend), plus recovered.npz for the map.
"""
import os
import sys

import numpy as np

from rec_common import (HERE, MINX, MINY, N, NULL, parse_layers, ltex_names)
from rec_holdout import TRANSFER, load, folds_outward
from rec_labels import build as build_groups

K = 15


def knn_fast(Xtr, Xte, k=K, chunk=1024):
    """indices and distances of the k nearest training rows, via BLAS."""
    tn = (Xtr ** 2).sum(1)
    I = np.empty((len(Xte), k), dtype=np.int32)
    D = np.empty((len(Xte), k), dtype=np.float32)
    for s in range(0, len(Xte), chunk):
        e = min(s + chunk, len(Xte))
        q = Xte[s:e]
        d2 = tn[None, :] - 2.0 * (q @ Xtr.T) + (q ** 2).sum(1)[:, None]
        np.maximum(d2, 0, out=d2)
        idx = np.argpartition(d2, k, axis=1)[:, :k]
        I[s:e] = idx
        D[s:e] = np.sqrt(np.take_along_axis(d2, idx, axis=1))
    return I, D


def vote(I, D, labels):
    w = 1.0 / (D + 1e-3)
    pred = np.empty(len(I), dtype=np.int64)
    conf = np.empty(len(I), dtype=np.float32)
    lab = labels[I]
    for r in range(len(I)):
        acc = {}
        for j in range(I.shape[1]):
            acc[lab[r, j]] = acc.get(lab[r, j], 0.0) + w[r, j]
        tot = sum(acc.values())
        b = max(acc.items(), key=lambda kv: kv[1])
        pred[r] = b[0]
        conf[r] = b[1] / tot
    return pred, conf


def main():
    Feat, dom, XY, W, ltex = load()
    names = ltex_names()

    # relabel: never emit the NULL pseudo-material
    lab = dom.copy()
    nullmask = (dom == NULL)
    order = np.argsort(-W, axis=1)
    for i in np.where(nullmask)[0]:
        for j in order[i]:
            if ltex[j] != NULL and W[i, j] > 0:
                lab[i] = ltex[j]
                break
    print('relabelled %d NULL-dominant quadrants to their best real material '
          '(%d had no real material at all and keep NULL)'
          % (nullmask.sum(), int((lab == NULL).sum())))
    keep = lab != NULL
    Feat, lab, XY = Feat[keep], lab[keep], XY[keep]

    mu, sd = Feat.mean(0), Feat.std(0) + 1e-6
    Xtr = ((Feat[:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]).astype(np.float32)

    # ---- calibration: vote share -> real accuracy at realistic range ----
    cx, cy = XY[:, 0].astype(float), XY[:, 1].astype(float)
    r = np.hypot(cx - cx.mean(), cy - cy.mean())
    te = r > np.percentile(r, 50)
    I, D = knn_fast(Xtr[~te], Xtr[te])
    p, c = vote(I, D, lab[~te])
    trc = XY[~te][:, :2].astype(np.float32)
    dmin = np.empty(te.sum(), dtype=np.float32)
    tec = XY[te][:, :2].astype(np.float32)
    for s in range(0, len(tec), 1024):
        e = min(s + 1024, len(tec))
        dmin[s:e] = np.linalg.norm(tec[s:e, None] - trc[None], axis=2).min(1)
    far = dmin >= 8.0
    hit = (p == lab[te])
    bins = [0.0, 0.3, 0.4, 0.5, 0.6, 0.75, 1.01]
    cal = []
    print('\ncalibration (deep holdout, only quadrants >= 8 cells from training):')
    for a, b in zip(bins[:-1], bins[1:]):
        m = far & (c >= a) & (c < b)
        acc = float(hit[m].mean()) if m.sum() >= 30 else float('nan')
        cal.append((a, b, acc, int(m.sum())))
        print('  vote share %.2f-%.2f  n=%5d  measured accuracy %.1f%%'
              % (a, b, m.sum(), 100 * acc))
    print('  overall at >=8 cells: %.1f%% (n=%d)' % (100 * hit[far].mean(), far.sum()))
    overall_far = float(hit[far].mean())

    def calibrate(v):
        for a, b, acc, n in cal:
            if a <= v < b:
                return overall_far if np.isnan(acc) else acc
        return overall_far

    # ---- predict every unpainted quadrant ----
    painted, _ = parse_layers()
    F = np.load(os.path.join(HERE, 'features.npz'))['F']
    tgt = []
    for rr in range(N):
        for cc in range(N):
            if not painted[rr, cc]:
                for q in range(4):
                    tgt.append((rr, cc, q))
    tgt = np.array(tgt, dtype=np.int32)
    print('\nunpainted cells %d -> %d quadrants to assign'
          % ((~painted).sum(), len(tgt)))
    Xte = ((F[tgt[:, 0], tgt[:, 1], tgt[:, 2]][:, TRANSFER] - mu[TRANSFER])
           / sd[TRANSFER]).astype(np.float32)
    I, D = knn_fast(Xtr, Xte)
    pred, conf = vote(I, D, lab)
    print('predicted; distinct materials assigned: %d' % len(np.unique(pred)))

    # blended variant: weighted mean of the neighbours' full blends
    Wk = W[keep]
    Bl = np.zeros((len(tgt), Wk.shape[1]), dtype=np.float32)
    ww = 1.0 / (D + 1e-3)
    ww /= ww.sum(1, keepdims=True)
    for j in range(I.shape[1]):
        Bl += ww[:, j:j + 1] * Wk[I[:, j]]
    nz = ltex != NULL
    Bl[:, ~nz] = 0.0
    Bl /= np.maximum(Bl.sum(1, keepdims=True), 1e-6)

    cal_conf = np.array([calibrate(v) for v in conf], dtype=np.float32)
    hdr = ('# recovered.txt -- RECOVERED LANDSCAPE MATERIALS, OUT-OF-BOUNDS '
           'COMMONWEALTH CELLS\n'
           '# R cx cy q0=<ltex:w;...> q1=... q2=... q3=... conf=<0..1>\n'
           '# quadrant order 0 BL, 1 BR, 2 TL, 3 TR\n'
           '#\n'
           '# *** MEASURED ACCURACY WARNING -- READ report_recover.md BEFORE '
           'SHIPPING THIS ***\n'
           '# Holdout top-1 accuracy on LTEX identity falls from 43.9%% (1-2 '
           'cells from painted\n'
           '# terrain) to 11.3%% (>16 cells).  The real targets are up to ~60 '
           'cells out.  A null\n'
           '# model that always guesses the most common material scores 18.5%%.'
           '  Simply copying\n'
           '# the nearest painted cell, using no colour at all, scores 29.4%% '
           'vs this model\'s\n'
           '# 28.4%% on the same split.  conf below is CALIBRATED against the '
           '>=8-cell regime\n'
           '# (overall %.3f there), NOT a raw vote share.\n' % overall_far)

    lines = {}
    bl_lines = {}
    for i, (rr, cc, q) in enumerate(tgt):
        key = (cc + MINX, rr + MINY)
        lines.setdefault(key, {})[q] = ('%08x:1.000' % pred[i], cal_conf[i])
        top = np.argsort(-Bl[i])[:4]
        top = [t for t in top if Bl[i, t] > 0.02]
        bl_lines.setdefault(key, {})[q] = (
            ';'.join('%08x:%.3f' % (ltex[t], Bl[i, t]) for t in top), cal_conf[i])

    for fn, src in (('recovered.txt', lines), ('recovered_blend.txt', bl_lines)):
        with open(os.path.join(HERE, fn), 'w') as f:
            f.write(hdr)
            if fn.startswith('recovered_blend'):
                f.write('# (blended variant: up to 4 layers per quadrant)\n')
            for (cx_, cy_) in sorted(src, key=lambda k: (k[1], k[0])):
                qs = src[(cx_, cy_)]
                cf = float(np.mean([v[1] for v in qs.values()]))
                f.write('R %d %d %s conf=%.3f\n'
                        % (cx_, cy_,
                           ' '.join('q%d=%s' % (q, qs[q][0]) for q in sorted(qs)),
                           cf))
        print('wrote %s (%d cells)' % (fn, len(src)))

    grid = np.full((N, N, 4), -1, dtype=np.int64)
    for i, (rr, cc, q) in enumerate(tgt):
        grid[rr, cc, q] = pred[i]
    np.savez(os.path.join(HERE, 'recovered.npz'), grid=grid,
             conf=cal_conf, tgt=tgt)
    print('wrote recovered.npz')


if __name__ == '__main__':
    sys.exit(main())
