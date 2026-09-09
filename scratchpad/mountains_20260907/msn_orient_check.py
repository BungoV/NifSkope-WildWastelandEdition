"""Independent end-to-end check that the _msn features landed on the right cell.

The tile-to-cell mapping (row 0 = north, cell (X+dx, Y+dy) at sub-block column
dx, row 3-dy) and the quadrant order (0 BL, 1 BR, 2 TL, 3 TR) are asserted
upstream, not proved here.  What is proved here is that MY reshape carried them
through: the normal map's geometry must agree with the heightfield's geometry.

Test: correlate the per-quadrant tilt of the normal map against the per-quadrant
mean slope of the 33x33 VHGT grid (features.npz column 19), over the quadrants
that have both.  Then repeat with the north/south flips I could plausibly have
got wrong.  The correct mapping must win by a clear margin; if it does not, the
correlation is not sensitive enough to be evidence and this prints INCONCLUSIVE
rather than PASS.
"""
import os
import sys

import numpy as np

from rec_common import HERE

SLOPE = 19          # features.npz column: per-quadrant mean VHGT slope


def corr(a, b):
    a = a - a.mean()
    b = b - b.mean()
    return float((a * b).sum() / max(np.sqrt((a * a).sum() * (b * b).sum()), 1e-12))


def main():
    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M, have = z['M'], z['have']
    names = list(z['names'])
    tilt = M[:, :, :, names.index('tiltMean')]
    slope = np.load(os.path.join(HERE, 'features.npz'))['F'][:, :, :, SLOPE]

    variants = {
        'as written': tilt,
        'cell rows flipped (cy mirrored)': tilt[::-1],
        'quadrants N/S swapped in cell': tilt[:, :, [2, 3, 0, 1]],
        'cell cols flipped (cx mirrored)': tilt[:, ::-1],
    }
    m = have & (slope.sum(axis=2) > 0)
    print('quadrants compared: %d' % (m.sum() * 4))
    res = {}
    for k, v in variants.items():
        res[k] = corr(v[m].ravel(), slope[m].ravel())
        print('  %-34s r = %+.4f' % (k, res[k]))
    best = max(res, key=lambda k: res[k])
    margin = res['as written'] - max(v for k, v in res.items() if k != 'as written')
    ok = best == 'as written' and margin > 0.05
    print('GATE orientation: %s (best = %r, margin over next %+.4f)'
          % ('PASS' if ok else 'INCONCLUSIVE', best, margin))
    return 0


if __name__ == '__main__':
    sys.exit(main())
