"""Which _msn features are allowed to be used at all?

Same test as rec_shift.py, applied to the normal-map features.  A feature is
only usable if the unpainted cells look like the painted cells in it.  If the
unpainted terrain occupies a range of some feature that the painted training
set never covers, the model extrapolates blind there and invents confident
nonsense -- the specific way a recovery goes "wrong in a plausible-looking
way", which is worse than no recovery at all.

Measured per feature:
  * the two means and standard deviations,
  * overlap = the fraction of far quadrants whose value lies inside the
    painted set's 1st..99th percentile band,
  * a normalised mean shift (difference of means over the painted std).

Cells with no tile are excluded from both sides; a zero-filled row is not a
measurement and would drag the far mean toward the origin.
"""
import os
import sys

import numpy as np

from rec_common import HERE, parse_layers


def main():
    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M = z['M']
    have = z['have']
    names = list(z['names'])
    painted, _ = parse_layers()
    P = M[painted & have].reshape(-1, M.shape[-1])
    Q = M[(~painted) & have].reshape(-1, M.shape[-1])
    print('cells with features %d   painted cells %d   painted&have %d'
          % (have.sum(), painted.sum(), (painted & have).sum()))
    print('painted quadrants %d   far quadrants %d' % (len(P), len(Q)))
    print()
    print('%-11s %9s %8s | %9s %8s | %8s %8s' %
          ('feature', 'paint mu', 'paint sd', 'far mu', 'far sd', 'overlap', 'shift/sd'))
    verdict = []
    for i, nm in enumerate(names):
        p, q = P[:, i], Q[:, i]
        lo, hi = np.percentile(p, [1, 99])
        ov = float(((q >= lo) & (q <= hi)).mean())
        sd = p.std() or 1.0
        sh = float((q.mean() - p.mean()) / sd)
        print('%-11s %9.4f %8.4f | %9.4f %8.4f | %7.1f%% %8.2f'
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
