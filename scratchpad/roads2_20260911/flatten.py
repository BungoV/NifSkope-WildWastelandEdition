"""Section 1g: what a DETAIL control would land on, predicted from the bakes
themselves before a single line of C++ is written.

Our sheet's road luminance is 0.85 correlated, within a material, with the
diffuse's footprint sample T8 (flat.py); vanilla's is 0.016 correlated.  So the
candidate change is to lerp the sampled colour toward the texture's own
whole-texture average:

    c = lerp( texAverage, sample, detail )        detail 1 = ROADS1 exactly

Its effect on the sheet is predictable without rebaking: within a material, fit
our own sheet against T8 by least squares (slope B, the effective gain of the
diffuse through coverage, grading and the ground lerp), then

    predicted(detail) = ours - B * (1 - detail) * ( l8 - mean(l8 | material) )

and measure that field the way the seam was measured -- local 5x5 SD on the
road, gradient magnitude at piece boundaries, and the texture-phase R2.

usage: flatten.py <ourColour.DDS> <ourNoRoads.DDS>
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from roads2lib import Dds                                   # noqa: E402
import seam                                                 # noqa: E402
import flat                                                 # noqa: E402

N = seam.N


def main():
    z = np.load(os.path.join(HERE, 'flat_m20_20.npz'), allow_pickle=True)
    TI, MP, U, V, idb = z['TI'], z['MP'], z['U'], z['V'], z['idb']
    ff = np.load(os.path.join(HERE, 'flat_fields.npz'))
    T8, road = ff['T8'], ff['road']
    l8 = flat.lum(T8)

    van = Dds(seam.VAN).lum()
    ours = Dds(sys.argv[1]).lum()
    nor = Dds(sys.argv[2]).lum()

    uv = np.load(os.path.join(HERE, 'uv_m20_20.npz'))
    uu, vv = uv['u'], uv['v']

    pz = np.load(os.path.join(HERE, 'proj_m20_20.npz'))
    ramp = pz['ramped']
    diff, feath, solid = seam.boundaries(pz['idbuf'], ramp)

    # within-material least squares: ours ~ A + B*l8
    B = np.zeros((N, N))
    resid8 = np.zeros((N, N))
    print('within-material fit of our sheet against the footprint sample')
    for ti in np.unique(TI[road]):
        m = road & (TI == ti)
        if m.sum() < 200:
            continue
        x, y = l8[m], ours[m]
        b = np.cov(x, y)[0, 1] / max(x.var(), 1e-9)
        B[m] = b
        resid8[m] = x - x.mean()
        print('   material %2d  %6d texels  slope B = %6.3f' % (ti, int(m.sum()), b))

    g = seam.grad_mag

    def locsd(L):
        return np.array([[L[max(0, j - 2):j + 3, max(0, i - 2):i + 3].std()
                          for i in range(N)] for j in range(N)])

    def r2phase(L):
        return flat.__dict__ and _r2(L)

    def _r2(L):
        import uvphase
        return uvphase.r2_by_phase(L, road, uu, vv)

    print('')
    print('%-22s %9s %9s %9s %9s' % ('field', 'SD road', 'grad bnd',
                                     'grad road', 'phase R2'))
    gv = g(van)
    print('%-22s %9.3f %9.3f %9.3f %9.4f'
          % ('vanilla (the gate)', locsd(van)[road].mean(), gv[diff].mean(),
             gv[road].mean(), _r2(van)))
    print('%-22s %9.3f %9.3f %9.3f %9.4f'
          % ('ours --no-roads', locsd(nor)[road].mean(), g(nor)[diff].mean(),
             g(nor)[road].mean(), _r2(nor)))
    for detail in (1.0, 0.75, 0.5, 0.25, 0.0):
        P = ours - B * (1.0 - detail) * resid8
        gp = g(P)
        print('%-22s %9.3f %9.3f %9.3f %9.4f'
              % ('detail %.2f' % detail, locsd(P)[road].mean(), gp[diff].mean(),
                 gp[road].mean(), _r2(P)))
    print('')
    print('mean luminance on the road: vanilla %.2f  ours %.2f  '
          '(a detail change cannot move this: the residual is mean-zero)'
          % (van[road].mean(), ours[road].mean()))
    print('off-road ground: vanilla %.2f  ours %.2f'
          % (van[~road].mean(), ours[~road].mean()))


if __name__ == '__main__':
    main()
