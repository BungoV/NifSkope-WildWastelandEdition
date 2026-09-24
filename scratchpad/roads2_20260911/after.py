"""Gate S1/S3/S4 in one table: every sheet this lane baked, measured the way the
seam was measured before any code was written.

Rows are the same sets seam.py used -- all piece boundaries, the solid-boundary
control, all road texels, the off-road ground, and the five displaced floors --
plus the two numbers that named the defect: local 5x5 luminance SD on the road
and the variance of the road luminance explained by the diffuse's own UV phase.

usage: after.py <name=path.DDS> ...
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from roads2lib import Dds                                   # noqa: E402
import seam                                                 # noqa: E402
import uvphase                                              # noqa: E402

N = seam.N


def locsd(L):
    return np.array([[L[max(0, j - 2):j + 3, max(0, i - 2):i + 3].std()
                      for i in range(N)] for j in range(N)])


def main():
    sheets = [('vanilla', Dds(seam.VAN).lum())]
    for a in sys.argv[1:]:
        name, path = a.split('=', 1)
        sheets.append((name, Dds(path).lum()))
    pz = np.load(os.path.join(HERE, 'proj_m20_20.npz'))
    idbuf, ramp = pz['idbuf'], pz['ramped']
    diff, feath, solid = seam.boundaries(idbuf, ramp)
    road = idbuf >= 0
    uv = np.load(os.path.join(HERE, 'uv_m20_20.npz'))
    uu, vv = uv['u'], uv['v']

    grads = {n: seam.grad_mag(L) for n, L in sheets}
    hdr = '%-34s %7s' % ('set', 'texels')
    for n, _ in sheets:
        hdr += ' %11s' % n[:11]
    print('GRADIENT MAGNITUDE')
    print(hdr)

    def row(label, m):
        s = '%-34s %7d' % (label, int(m.sum()))
        for n, _ in sheets:
            s += ' %11.3f' % grads[n][m].mean()
        print(s)

    row('all piece boundaries', diff)
    row('FEATHERED boundaries', feath)
    row('SOLID boundaries (control)', solid)
    row('all road texels', road)
    row('off-road ground', ~road)
    for dj, di in seam.DISPLACE:
        row('floor: displaced %+d%+d' % (dj, di),
            np.roll(np.roll(diff, dj, axis=0), di, axis=1))

    print('')
    print('RATIO TO VANILLA on the two rows the gate names')
    for label, m in (('all piece boundaries', diff),
                     ('SOLID boundaries (control)', solid),
                     ('all road texels', road),
                     ('off-road ground', ~road)):
        s = '%-34s' % label
        v = grads['vanilla'][m].mean()
        for n, _ in sheets:
            s += ' %11.3f' % (grads[n][m].mean() / max(v, 1e-9))
        print(s)

    print('')
    print('%-34s %7s' % ('local 5x5 luminance SD', ''))
    for where, m in (('on the road', road), ('off the road', ~road)):
        s = '%-34s %7s' % (where, '')
        for n, L in sheets:
            s += ' %11.3f' % locsd(L)[m].mean()
        print(s)

    print('')
    print('%-34s %7s' % ('road luminance variance explained by the UV phase', ''))
    s = '%-34s %7s' % ('R2 (16x16 phase bins)', '')
    for n, L in sheets:
        s += ' %11.4f' % uvphase.r2_by_phase(L, road, uu, vv)
    print(s)

    print('')
    print('%-34s %7s' % ('mean luminance', ''))
    for where, m in (('on the road', road), ('off the road', ~road)):
        s = '%-34s %7s' % (where, '')
        for n, L in sheets:
            s += ' %11.2f' % L[m].mean()
        print(s)

    print('')
    print('MEAN ABSOLUTE LUMINANCE ERROR AGAINST VANILLA')
    van = dict(sheets)['vanilla']
    for where, m in (('on the road', road), ('off the road', ~road),
                     ('whole sheet', np.ones_like(road))):
        s = '%-34s %7d' % (where, int(m.sum()))
        for n, L in sheets:
            s += ' %11.3f' % np.abs(L[m] - van[m]).mean()
        print(s)


if __name__ == '__main__':
    main()
