"""The confidence is anti-correlated with accuracy, and that has to be fixed.

recovered_conf.png showed the model at its MOST confident in the far outer
margin -- precisely where rec_decay.py measured it to be least accurate.  The
cause is not subtle: far from the painted region the terrain is uniform, so all
15 k-NN neighbours agree and the vote share is high.  Unanimity among neighbours
is not evidence of correctness when every neighbour is wrong the same way.

A confidence number that rises as accuracy falls is worse than no confidence
number, because a consumer filtering on "only use high-confidence cells" would
select exactly the worst assignments.  So:

  1. measure the inversion,
  2. rebuild the calibration JOINTLY over (distance to painted terrain, vote
     share) instead of vote share alone,
  3. rewrite recovered.txt / recovered_blend.txt with the joint confidence.

Distance is known exactly for every real target, so this costs nothing and
removes the trap.
"""
import os
import sys

import numpy as np

from rec_common import HERE, MINX, MINY, N, NULL, parse_layers, ltex_names
from rec_holdout import TRANSFER, load
from rec_apply import knn_fast, vote

DBANDS = [(0, 2), (2, 4), (4, 8), (8, 16), (16, 1e9)]
CBANDS = [(0.0, 0.4), (0.4, 0.55), (0.55, 0.7), (0.7, 1.01)]


def main():
    Feat, dom, XY, W, ltex = load()
    lab = dom.copy()
    order = np.argsort(-W, axis=1)
    for i in np.where(dom == NULL)[0]:
        for j in order[i]:
            if ltex[j] != NULL and W[i, j] > 0:
                lab[i] = ltex[j]
                break
    keep = lab != NULL
    Feat, lab, XY = Feat[keep], lab[keep], XY[keep]
    mu, sd = Feat.mean(0), Feat.std(0) + 1e-6
    Xall = ((Feat[:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]).astype(np.float32)

    cx, cy = XY[:, 0].astype(float), XY[:, 1].astype(float)
    r = np.hypot(cx - cx.mean(), cy - cy.mean())
    te = r > np.percentile(r, 50)
    I, D = knn_fast(Xall[~te], Xall[te])
    p, c = vote(I, D, lab[~te])
    hit = (p == lab[te])
    trc = XY[~te][:, :2].astype(np.float32)
    tec = XY[te][:, :2].astype(np.float32)
    dmin = np.empty(len(tec), dtype=np.float32)
    for s in range(0, len(tec), 1024):
        e = min(s + 1024, len(tec))
        dmin[s:e] = np.linalg.norm(tec[s:e, None] - trc[None], axis=2).min(1)

    print('=== the inversion, measured ===')
    print('%-14s %7s %12s %10s' % ('distance', 'n', 'mean vote sh', 'accuracy'))
    for a, b in DBANDS:
        m = (dmin >= a) & (dmin < b)
        if m.sum() < 30:
            continue
        print('%5.0f - %-6.0f %7d %12.3f %9.1f%%'
              % (a, b, m.sum(), c[m].mean(), 100 * hit[m].mean()))
    cc = np.corrcoef(dmin, c)[0, 1]
    ca = np.corrcoef(dmin, hit.astype(float))[0, 1]
    print('corr(distance, vote share) = %+.3f   corr(distance, correct) = %+.3f'
          % (cc, ca))
    print('-> the vote share barely moves with distance while accuracy '
          'collapses, so raw\n   vote share massively overstates reliability far '
          'out: it is a trap either way.')

    print('\n=== joint calibration table: measured accuracy ===')
    print('%-14s' % 'distance' + ''.join('  vote %.2f-%.2f' % b for b in CBANDS))
    table = {}
    for a, b in DBANDS:
        row = '%5.0f - %-6.0f' % (a, b)
        for ca_, cb in CBANDS:
            m = (dmin >= a) & (dmin < b) & (c >= ca_) & (c < cb)
            if m.sum() >= 25:
                acc = float(hit[m].mean())
                table[(a, ca_)] = acc
                row += '        %5.1f%%' % (100 * acc)
            else:
                row += '            n/a'
        print(row)
    globacc = float(hit.mean())

    def lookup(dist, v):
        for a, b in DBANDS:
            if a <= dist < b:
                for ca_, cb in CBANDS:
                    if ca_ <= v < cb:
                        return table.get((a, ca_), globacc)
        return globacc

    # --- rewrite the deliverables with joint confidence ---
    painted, _ = parse_layers()
    z = np.load(os.path.join(HERE, 'recovered.npz'))
    grid, tgt = z['grid'], z['tgt']
    F = np.load(os.path.join(HERE, 'features.npz'))['F']
    Xte = ((F[tgt[:, 0], tgt[:, 1], tgt[:, 2]][:, TRANSFER] - mu[TRANSFER])
           / sd[TRANSFER]).astype(np.float32)
    I2, D2 = knn_fast(Xall, Xte)
    pred2, conf2 = vote(I2, D2, lab)

    pc = np.stack([tgt[:, 1] + MINX, tgt[:, 0] + MINY], axis=1).astype(np.float32)
    allp = np.array([[c_ + MINX, r_ + MINY] for r_ in range(N) for c_ in range(N)
                     if painted[r_, c_]], dtype=np.float32)
    dmin2 = np.empty(len(pc), dtype=np.float32)
    for s in range(0, len(pc), 2048):
        e = min(s + 2048, len(pc))
        dmin2[s:e] = np.linalg.norm(pc[s:e, None] - allp[None], axis=2).min(1)
    print('\ndistance of real targets to the nearest painted cell: '
          'median %.1f  p90 %.1f  max %.1f cells'
          % (np.median(dmin2), np.percentile(dmin2, 90), dmin2.max()))
    joint = np.array([lookup(dmin2[i], conf2[i]) for i in range(len(pc))],
                     dtype=np.float32)
    print('joint-calibrated confidence: mean %.3f  median %.3f  max %.3f'
          % (joint.mean(), np.median(joint), joint.max()))
    print('quadrants with calibrated confidence >= 0.30: %d (%.2f%%)'
          % (int((joint >= 0.30).sum()), 100.0 * (joint >= 0.30).mean()))

    np.savez(os.path.join(HERE, 'recovered_conf.npz'), conf=joint, dist=dmin2)
    print('wrote recovered_conf.npz')

    # patch the two text deliverables in place
    per = {}
    for i in range(len(tgt)):
        per.setdefault((int(tgt[i, 1]) + MINX, int(tgt[i, 0]) + MINY), []).append(joint[i])
    for fn in ('recovered.txt', 'recovered_blend.txt'):
        p = os.path.join(HERE, fn)
        out = []
        for line in open(p):
            if not line.startswith('R '):
                out.append(line)
                continue
            f = line.split()
            key = (int(f[1]), int(f[2]))
            cf = float(np.mean(per[key]))
            out.append(' '.join(f[:-1]) + ' conf=%.3f\n' % cf)
        open(p, 'w').write(''.join(out))
        print('rewrote %s with joint-calibrated confidence' % fn)


if __name__ == '__main__':
    sys.exit(main())
