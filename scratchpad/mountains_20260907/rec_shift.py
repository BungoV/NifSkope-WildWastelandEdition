"""Which features are allowed to be used at all?

A feature is only usable if the out-of-bounds cells look like the painted cells
in it.  If unpainted terrain occupies a range of some feature that the painted
training set never covers, the model extrapolates blind there and will invent
confident nonsense.  This is the specific way a recovery goes "wrong in a
plausible-looking way", which the brief says is worse than none.

Measured per feature:
  * the two means and standard deviations,
  * overlap = the fraction of far quadrants whose value lies inside the
    painted set's 1st..99th percentile band,
  * a normalised mean shift (difference of means over the painted std).
"""
import os
import sys

import numpy as np

from rec_common import HERE, parse_layers

NAMES = ['meanR', 'meanG', 'meanB', 'stdR', 'stdG', 'stdB',
         'R-G', 'G-B', 'R-B', 'r_norm', 'g_norm',
         'lumP10', 'lumP50', 'lumP90', 'gradH', 'gradV', 'laplace',
         'hMean', 'hStd', 'slopeMean', 'slopeMax', 'cellSlope']


def main():
    z = np.load(os.path.join(HERE, 'features.npz'))
    F = z['F']
    painted, _ = parse_layers()
    P = F[painted].reshape(-1, F.shape[-1])
    Q = F[~painted].reshape(-1, F.shape[-1])
    print('painted quadrants %d   far quadrants %d' % (len(P), len(Q)))
    print()
    print('%-11s %9s %8s | %9s %8s | %8s %8s' %
          ('feature', 'paint mu', 'paint sd', 'far mu', 'far sd', 'overlap', 'shift/sd'))
    verdict = []
    for i, nm in enumerate(NAMES):
        p, q = P[:, i], Q[:, i]
        lo, hi = np.percentile(p, [1, 99])
        ov = float(((q >= lo) & (q <= hi)).mean())
        sd = p.std() or 1.0
        sh = float((q.mean() - p.mean()) / sd)
        print('%-11s %9.3f %8.3f | %9.3f %8.3f | %7.1f%% %8.2f'
              % (nm, p.mean(), sd, q.mean(), q.std(), 100 * ov, sh))
        verdict.append((nm, ov, abs(sh)))
    print()
    good = [v for v in verdict if v[1] >= 0.90 and v[2] <= 0.60]
    bad = [v for v in verdict if not (v[1] >= 0.90 and v[2] <= 0.60)]
    print('TRANSFERABLE (overlap >= 90%% and |shift| <= 0.60 sd): %s'
          % ', '.join(v[0] for v in good))
    print('REJECTED: %s' % ', '.join('%s(ov %.0f%%, shift %.2f)' % (v[0], 100 * v[1], v[2])
                                     for v in bad))


if __name__ == '__main__':
    sys.exit(main())
