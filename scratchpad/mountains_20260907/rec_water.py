"""Is the vanilla LOD diffuse really plain albedo everywhere?

The brief lists "the vanilla diffuse is plain albedo -- no baked sun, no baked
AO" as a settled fact, and says to speak up if one looks wrong.  One does.  The
palette fit in rec_model.py put LOceanFloor01 at RGB 39.7/39.5/37.0 while the
actual OceanFloor01_d.dds averages 82.4/75.4/62.2 -- a factor of about 0.48.
Its kelp variants land in the same place.  Every Glowing Sea material is fitted
dark too.

The obvious candidate is that the LOD bake darkens SUBMERGED terrain.  That is
testable without any new data: split painted quadrants by whether their terrain
sits below the worldspace water level and compare the fit residual.  If
submerged quadrants are systematically darker than the material model predicts,
"plain albedo" holds only above water, and anyone matching colours down there is
matching a tinted colour.
"""
import os
import sys

import numpy as np

from rec_common import HERE, MINX, MINY, ltex_names
from rec_model import fit_palette

# Fallout 4 Commonwealth default water height. VHGT is in units of 8, and
# lens2/land3C.npz stores the accumulated heights already scaled; we only need a
# relative split, so the exact constant is not load-bearing -- the sweep below
# reports several thresholds.
THRESHOLDS = [-2000.0, -1000.0, -500.0, 0.0, 500.0]


def main():
    z = np.load(os.path.join(HERE, 'training.npz'))
    W, C, XY, ltex = z['W'].astype(np.float64), z['C'].astype(np.float64), z['XY'], z['ltex']
    names = ltex_names()
    P, pred = fit_palette(W, C)
    resid = C - pred
    lum = 0.2126 * resid[:, 0] + 0.7152 * resid[:, 1] + 0.0722 * resid[:, 2]

    F = np.load(os.path.join(HERE, 'features.npz'))['F']
    h = np.array([F[cy - MINY, cx - MINX, q, 17] for cx, cy, q in XY])

    print('quadrant mean height over painted terrain: min %.0f  p10 %.0f  '
          'median %.0f  max %.0f' % (h.min(), np.percentile(h, 10),
                                     np.median(h), h.max()))
    print()
    print('%-14s %8s %14s %14s' % ('height <', 'n', 'mean resid lum', 'above: resid'))
    for t in THRESHOLDS:
        below = h < t
        if below.sum() < 50 or (~below).sum() < 50:
            continue
        print('%-14.0f %8d %14.2f %14.2f'
              % (t, below.sum(), lum[below].mean(), lum[~below].mean()))

    r = np.corrcoef(h, lum)[0, 1]
    print('\ncorr(quadrant height, residual luminance) = %+.3f' % r)

    # the specific materials that motivated this
    print('\nresidual luminance for the materials whose fit missed their texture:')
    for fid, nm in ((0x0014bf47, 'LOceanFloor01'), (0x0014d270, 'LOceanFloor01ShortKelp'),
                    (0x000dedc8, 'LGlowingSeaCorrosion01'), (0x000dedc4, 'LGlowingSeaMud01'),
                    (0x000ab72e, 'LCoastSandWet01'), (0x0001f78c, 'LRubbleRock01'),
                    (0x00021336, 'LDirtGravel01')):
        j = int(np.where(ltex == fid)[0][0]) if (ltex == fid).any() else None
        if j is None:
            continue
        m = W[:, j] > 0.5
        if m.sum() < 20:
            continue
        print('  %-26s n=%5d  mean height %8.0f  mean resid lum %+7.2f'
              % (nm, m.sum(), h[m].mean(), lum[m].mean()))

    # does a submerged-darkening term improve the fit?
    for t in (-1000.0, 0.0):
        below = (h < t).astype(np.float64)
        if below.sum() < 50:
            continue
        W2 = np.hstack([W, W * below[:, None]])
        P2, pred2 = fit_palette(W2, C, ridge=1.0)
        ss = ((C - pred) ** 2).sum()
        ss2 = ((C - pred2) ** 2).sum()
        tot = ((C - C.mean(0)) ** 2).sum()
        print('\nadding a separate palette for quadrants below %.0f: '
              'R^2 %.4f -> %.4f' % (t, 1 - ss / tot, 1 - ss2 / tot))


if __name__ == '__main__':
    sys.exit(main())
