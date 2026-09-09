"""Does the _msn actually SEPARATE materials, or only survive the shift test?

Transferability is necessary, not sufficient.  A feature can have a perfect
overlap between painted and far and still be pure noise -- noise transfers
beautifully.  Before anyone spends model capacity on these columns, measure
whether they carry material identity at all.

Method: take painted quadrants that are dominated by ONE material (composite
weight >= 0.80, so the quadrant is nearly single-material and its texture is
that material's texture, not a blend), keep materials with enough such
quadrants, and compute the one-way F-ratio per feature -- between-material
variance of the class means over the pooled within-material variance.  F = 1 is
what a feature carrying nothing about material scores.  The same statistic is
computed for the COLOUR features so the two are on one scale and "the normal
map beats colour" is a comparison, not an impression.

This is a screen, not a classifier.  It says which columns hold signal; the
holdout in rec_holdout.py is what says whether the signal is usable.
"""
import os
import sys

import numpy as np

from rec_common import HERE, MINX, MINY, ltex_names

DOM = 0.80        # a quadrant counts only if one material owns this much of it
MINN = 40         # a material counts only with this many such quadrants
if len(sys.argv) > 2:                      # widen the screen without editing it
    DOM, MINN = float(sys.argv[1]), int(sys.argv[2])
COLNAMES = ['meanR', 'meanG', 'meanB', 'stdR', 'stdG', 'stdB',
            'R-G', 'G-B', 'R-B', 'r_norm', 'g_norm',
            'lumP10', 'lumP50', 'lumP90', 'gradH', 'gradV', 'laplace',
            'hMean', 'hStd', 'slopeMean', 'slopeMax', 'cellSlope']


def fratio(X, lab):
    """one-way F per column of X under integer labels lab."""
    out = np.empty(X.shape[1])
    gm = X.mean(0)
    ks = np.unique(lab)
    for j in range(X.shape[1]):
        num = 0.0
        den = 0.0
        n = 0
        for k in ks:
            v = X[lab == k, j]
            num += len(v) * (v.mean() - gm[j]) ** 2
            den += ((v - v.mean()) ** 2).sum()
            n += len(v)
        num /= max(len(ks) - 1, 1)
        den /= max(n - len(ks), 1)
        out[j] = num / max(den, 1e-30)
    return out


def main():
    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M, have, names = z['M'], z['have'], list(z['names'])
    Fc = np.load(os.path.join(HERE, 'features.npz'))['F']
    t = np.load(os.path.join(HERE, 'training.npz'))
    W, XY, ltex = t['W'], t['XY'], t['ltex']

    dom = ltex[np.argmax(W, axis=1)]
    domw = W.max(axis=1)
    sel = domw >= DOM
    cx, cy, q = XY[sel, 0], XY[sel, 1], XY[sel, 2]
    r, c = cy - MINY, cx - MINX
    ok = have[r, c]
    r, c, q, lab = r[ok], c[ok], q[ok], dom[sel][ok]

    keep = np.zeros(len(lab), dtype=bool)
    uq, cnt = np.unique(lab, return_counts=True)
    for u, n in zip(uq, cnt):
        if n >= MINN:
            keep |= (lab == u)
    r, c, q, lab = r[keep], c[keep], q[keep], lab[keep]
    nm = ltex_names()
    print('single-material quadrants (dom >= %.2f): %d over %d materials with >= %d each'
          % (DOM, len(lab), len(np.unique(lab)), MINN))

    Xm = M[r, c, q].astype(np.float64)
    Xc = Fc[r, c, q].astype(np.float64)
    fm = fratio(Xm, lab)
    fc = fratio(Xc, lab)

    print()
    print('%-11s %8s   |  %-11s %8s' % ('msn feature', 'F', 'colour feat', 'F'))
    o1 = np.argsort(-fm)
    o2 = np.argsort(-fc)
    for i in range(max(len(fm), len(fc))):
        a = ('%-11s %8.1f' % (names[o1[i]], fm[o1[i]])) if i < len(fm) else ' ' * 20
        b = ('%-11s %8.1f' % (COLNAMES[o2[i]], fc[o2[i]])) if i < len(fc) else ''
        print('%s   |  %s' % (a, b))
    print()
    print('best msn F = %.1f (%s)    best colour F = %.1f (%s)'
          % (fm.max(), names[int(np.argmax(fm))], fc.max(), COLNAMES[int(np.argmax(fc))]))

    # do the top msn columns say anything colour does not?  correlate them.
    top = o1[:5]
    print()
    print('correlation of the 5 strongest msn columns with every colour column '
          '(largest |r| shown):')
    for j in top:
        best, bn = 0.0, ''
        for i in range(Xc.shape[1]):
            a = Xm[:, j] - Xm[:, j].mean()
            b = Xc[:, i] - Xc[:, i].mean()
            rr = float((a * b).sum() / max(np.sqrt((a * a).sum() * (b * b).sum()), 1e-12))
            if abs(rr) > abs(best):
                best, bn = rr, COLNAMES[i]
        print('  %-11s F %7.1f   max |r| with colour = %+.3f (%s)'
              % (names[j], fm[j], best, bn))

    print()
    print('class means of the 3 strongest msn columns, 12 largest materials:')
    big = sorted(np.unique(lab), key=lambda u: -(lab == u).sum())[:12]
    hdr = '  %-34s %6s' % ('material', 'n')
    for j in o1[:3]:
        hdr += ' %11s' % names[j]
    print(hdr)
    for u in big:
        m = lab == u
        line = '  %-34s %6d' % (nm.get(int(u), ('?',))[0][:34], m.sum())
        for j in o1[:3]:
            line += ' %11.4f' % Xm[m, j].mean()
        print(line)


if __name__ == '__main__':
    sys.exit(main())
