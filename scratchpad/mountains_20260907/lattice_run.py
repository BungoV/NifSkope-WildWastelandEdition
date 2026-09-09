"""Lane LATTICE step 1 -- the measurement, with its controls, on the sheets.

    python lattice_run.py            # full run, mip 0 and mip 1
    python lattice_run.py --quick    # controls + the two tiles at mip 0 only
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lattice import (BLUR, MARGIN, bc_roundtrip, chunk_hgt, comb, find_ours,
                     lattice_report, load, load_land, make_msn, prominence,
                     residual, slopes, spec1d, VAN)

PERIODS = [4, 8, 16, 32]
TILES = ['4.-60.36', '4.-12.44']
GATE = 5.0


def hr(t):
    print('\n' + t)
    print('-' * len(t))


def synth(n=512, seed=1, beta=2.5):
    """A broadband fractal slope field: the metric must read ~1 on it."""
    rs = np.random.RandomState(seed)
    ky = np.fft.fftfreq(n)[:, None]
    kx = np.fft.fftfreq(n)[None, :]
    k = np.sqrt(kx ** 2 + ky ** 2)
    k[0, 0] = 1e-9
    amp = k ** (-beta / 2.0)
    amp[0, 0] = 0
    out = []
    for s in range(2):
        f = np.fft.ifft2(amp * np.fft.fft2(rs.randn(n, n))).real
        out.append(f / f.std())
    return out


def comb_pair(P, R, periods=PERIODS):
    n = P.shape[0]
    px = spec1d(P, 0) + spec1d(R, 0)
    py = spec1d(P, 1) + spec1d(R, 1)
    return {p: (comb(px, p, n), comb(py, p, n)) for p in periods}


def show(label, P, R, extra=''):
    c = comb_pair(P, R)
    rms = float(np.sqrt((P ** 2 + R ** 2).mean()))
    cells = '   '.join('p%-2d %6.2f/%-6.2f' % (p, c[p][0], c[p][1]) for p in PERIODS)
    print('%-30s rms %7.4f   %s %s' % (label, rms, cells, extra))
    return c, rms


def sheet(rgb):
    return residual(*slopes(rgb))


def main(argv):
    quick = '--quick' in argv
    mips = [0] if quick else [0, 1]

    hr('0. KNOWN ANSWERS -- does the metric fire only on a lattice?')
    P0, R0 = synth()
    show('fractal, no lattice', P0[MARGIN:-MARGIN, MARGIN:-MARGIN],
         R0[MARGIN:-MARGIN, MARGIN:-MARGIN])
    n = P0.shape[0]
    ii = np.arange(n)
    for amp, per in ((0.05, 4), (0.05, 16)):
        lat = (np.cos(2 * np.pi * ii / per)[None, :]
               + np.cos(2 * np.pi * ii / per)[:, None])
        show('fractal + %d%% p%d lattice' % (int(amp * 100), per),
             (P0 + amp * lat)[MARGIN:-MARGIN, MARGIN:-MARGIN],
             (R0 + amp * lat)[MARGIN:-MARGIN, MARGIN:-MARGIN])

    land = load_land()

    for mip in mips:
        for tile in TILES:
            dim, cx, cy = (int(v) for v in tile.split('.'))
            res = 512 >> mip
            hr('Commonwealth.%s   mip %d   %dx%d   (height-sample period = %d texels)'
               % (tile, mip, res, res, res // (dim * 32)))

            vp = os.path.join(VAN, 'Commonwealth.%s_msn.DDS' % tile)
            op = find_ours(tile)
            print('vanilla %s' % vp)
            print('ours    %s' % op)
            van = load(vp, mip)
            our = load(op, mip)

            cv, _ = show('VANILLA (floor)', *sheet(van))
            co, _ = show('OURS (shipped)', *sheet(our))

            # positive control: the PRE-FIX nearest sheet, same heights, same
            # encoder, same block codec, at this mip's resolution
            hgt = chunk_hgt(land, cx, cy, dim)
            near = bc_roundtrip(make_msn(hgt, dim, res=res, mode='nearest'))
            cn, _ = show('PRE-FIX nearest (ceiling)', *sheet(near))

            # the replica, to prove the model reproduces the shipped sheet
            rep = bc_roundtrip(make_msn(hgt, dim, res=res, mode='bilinear'))
            cr, _ = show('replica bilinear', *sheet(rep))
            d = np.abs(rep.astype(np.int32) - our.astype(np.int32))
            print('   replica vs shipped: mean |d| %.2f bytes, 99th pct %d, max %d'
                  % (d.mean(), np.percentile(d, 99), d.max()))

            for p in PERIODS:
                sep = max(cn[p]) / max(cv[p]) if max(cv[p]) else float('inf')
                print('   p%-3d  ceiling/floor %6.2fx  %s   ours/floor %6.2fx'
                      % (p, sep, 'PASS' if sep >= GATE else 'fail',
                         max(co[p]) / max(cv[p]) if max(cv[p]) else float('nan')))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
