"""Verify the _msn sub-lane's claim under the metric that governs this report.

The sub-lane reported that adding transferable _msn columns moves OUTWARD from
28.4% to 30.1%, and that the _msn decays more slowly with distance than colour
does -- beating it beyond 16 cells.  That would be a real finding: a feature
that describes the MATERIAL rather than its location is exactly what this lane
needed and failed to find in the diffuse.

Two reasons to re-run it here rather than take it as given:

  * it scored with NULL allowed as an answer (the 11.3% metric).  This report's
    governing metric excludes NULL, because "no base texture" is not a legal
    output for the mod, and that stricter metric is where colour fell to 0.8%.
  * the sub-lane itself flagged that its transferable nz family correlates
    +0.57..+0.67 with mean height -- a feature rec_shift.py REJECTED.  If the
    _msn gain is really smuggled elevation, it must not be counted.  So the
    clean-independent subset it identified (mean_nx, mean_nz, aniso, hpCorr_xz,
    all |r| <= 0.25 against anything colour knows) is scored separately.
"""
import os
import sys

import numpy as np

from rec_common import HERE, MINX, MINY, NULL
from rec_holdout import TRANSFER, load
from rec_apply import knn_fast, vote
from rec_robust import relabel, BANDS

MSN_TRANSFER = ['mean_nx', 'mean_nz', 'lap_nx', 'lap_nz', 'hp9_nx', 'hp9_nz',
                'hp5_nx', 'hp5_nz', 'dh_nx', 'dh_nz', 'dv_nx', 'dv_nz',
                'aniso', 'lapVec', 'hpTilt', 'hpCorr_xz']
MSN_CLEAN = ['mean_nx', 'mean_nz', 'aniso', 'hpCorr_xz']


def evaluate(name, X, lab, XY, te):
    tr = ~te
    mu, sd = X[tr].mean(0), X[tr].std(0) + 1e-6
    Xs = ((X - mu) / sd).astype(np.float32)
    I, D = knn_fast(Xs[tr], Xs[te])
    p, _ = vote(I, D, lab[tr])
    hit = (p == lab[te])
    trc, tec = XY[tr][:, :2].astype(np.float32), XY[te][:, :2].astype(np.float32)
    dmin = np.empty(len(tec), dtype=np.float32)
    for s in range(0, len(tec), 1024):
        e = min(s + 1024, len(tec))
        dmin[s:e] = np.linalg.norm(tec[s:e, None] - trc[None], axis=2).min(1)
    row = '%-30s %7.1f%% |' % (name, 100 * hit.mean())
    for a, b in BANDS:
        m = (dmin >= a) & (dmin < b)
        row += ('  %6.1f%%' % (100 * hit[m].mean())) if m.sum() >= 40 else '      n/a'
    print(row)
    return hit.mean()


def main():
    Feat, dom, XY, W, ltex = load()
    lab = relabel(dom, W, ltex)
    keep = lab != NULL
    Feat, lab, XY, dom = Feat[keep], lab[keep], XY[keep], dom[keep]

    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M, mnames = z['M'], list(z['names'])
    Msel = np.empty((len(XY), M.shape[-1]), dtype=np.float32)
    for i, (cx, cy, q) in enumerate(XY):
        Msel[i] = M[cy - MINY, cx - MINX, q]
    ti = [mnames.index(n) for n in MSN_TRANSFER]
    ci = [mnames.index(n) for n in MSN_CLEAN]

    cx, cy = XY[:, 0].astype(float), XY[:, 1].astype(float)
    r = np.hypot(cx - cx.mean(), cy - cy.mean())
    te = r > np.percentile(r, 50)
    print('deep holdout, NULL excluded as an answer (the governing metric)')
    print('%-30s %8s |%s' % ('features', 'overall',
                             ''.join('%9s' % ('%d-%d' % (a, b) if b < 1e9 else '16+')
                                     for a, b in BANDS)))
    print('-' * 82)
    evaluate('colour+ (14)', Feat[:, TRANSFER], lab, XY, te)
    evaluate('msn transferable (16)', Msel[:, ti], lab, XY, te)
    evaluate('msn clean-independent (4)', Msel[:, ci], lab, XY, te)
    evaluate('colour+ & msn transferable', np.hstack([Feat[:, TRANSFER], Msel[:, ti]]),
             lab, XY, te)
    evaluate('colour+ & msn clean (4)', np.hstack([Feat[:, TRANSFER], Msel[:, ci]]),
             lab, XY, te)
    print('-' * 82)
    print('null (always the commonest material): %.1f%%'
          % (100 * max((lab == v).mean() for v in np.unique(lab))))

    # is the msn gain smuggled elevation?  correlate each msn column with height
    F = np.load(os.path.join(HERE, 'features.npz'))['F']
    h = np.array([F[cy_ - MINY, cx_ - MINX, q, 17] for cx_, cy_, q in XY])
    print('\ncorrelation of each used msn column with mean terrain height')
    print('(height was REJECTED as a feature at 83.8%% overlap / +0.66 shift)')
    for n in MSN_TRANSFER:
        rr = np.corrcoef(Msel[:, mnames.index(n)], h)[0, 1]
        flag = '  <-- elevation proxy' if abs(rr) >= 0.45 else ''
        print('   %-12s r = %+0.3f%s' % (n, rr, flag))


if __name__ == '__main__':
    sys.exit(main())
