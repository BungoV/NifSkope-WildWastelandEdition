"""Lane LATTICE -- THE measurement: is our `_msn` rougher at the height-sample
lines than between them, and vanilla's not?

WHY NOT A SPECTRAL COMB.  A comb (lattice_run.py) only fires on a lattice whose
amplitude is the SAME everywhere.  The lattice here is a crease at every grid
line whose strength follows the terrain, so its amplitude is random from line to
line -- a train of impulses at spacing 4 with independent amplitudes has a FLAT
spectrum, no comb.  Measured: the known-blocky pre-fix sheet reads 0.19 on the
comb, BELOW a broadband field, because a zero-order hold has spectral ZEROS
exactly at the comb bins.  So the comb is the wrong instrument and is reported
only for the record.

THE RIGHT INSTRUMENT is a PHASE-CONDITIONAL statistic -- the same shape as the
x-mod-4 table in WW_CHANGES 2026-09-07.  Take the roughness of the slope field
column by column,

    c(x) = mean_y | F(x+1,y) - 2 F(x,y) + F(x-1,y) |

and group the columns by x mod p.  A field with no relation to our sample grid
spreads evenly over the classes; a field creased at every grid line puts its
roughness in one or two of them.

    MOD(p) = (max class - min class) / mean class

CONTROLS (ww-control-calibration):
  known answer   a fractal field reads MOD ~ 0.0x; the same field creased at
                 every 4th column reads high.  Printed above every real number.
  floor          VANILLA's own sheet on the same tile: it was baked by
                 Bethesda, not on our 129-sample grid, so its classes must be
                 flat whatever else is in it.
  ceiling        the PRE-FIX nearest-sampled sheet rebuilt from the same VHGT
                 heights through the same encoder and the same block codec --
                 the sheet that visibly had squares, so its MOD is what "a
                 lattice" reads on this data.
  gate           ceiling/floor >= 5x, pre-registered.

Usage:  python lattice_phase.py [--mip N]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lattice import (MARGIN, bc_roundtrip, chunk_hgt, comb, find_ours, load,
                     load_land, make_msn, residual, slopes, spec1d, VAN)

TILES = ['4.-60.36', '4.-12.44']
GATE = 5.0


def classes(F, p, axis=0):
    """Mean second-difference magnitude per residue class along `axis`."""
    F = np.asarray(F, dtype=np.float64)
    if axis == 1:
        F = F.T
    d2 = np.abs(F[:, 2:] - 2.0 * F[:, 1:-1] + F[:, :-2])   # columns 1..n-2
    c = d2.mean(axis=0)
    xs = np.arange(1, 1 + len(c))
    return np.array([c[xs % p == r].mean() for r in range(p)])


def mod(F, p, axis=0):
    c = classes(F, p, axis)
    m = c.mean()
    return float((c.max() - c.min()) / m) if m > 0 else float('nan')


def measure(P, R, p=4):
    """MOD along x and along y, on both slope components, worst case reported."""
    mx = max(mod(P, p, 0), mod(R, p, 0))
    my = max(mod(P, p, 1), mod(R, p, 1))
    return mx, my


def show(label, P, R, p=4, extra=''):
    mx, my = measure(P, R, p)
    n = P.shape[0]
    cx = classes(P, p, 0)
    print('%-30s MOD_x %6.3f  MOD_y %6.3f   classes(P,x) %s  %s'
          % (label, mx, my, ' '.join('%.4f' % v for v in cx), extra))
    return mx, my


def synth(n=512, seed=1, beta=2.5):
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


def crease(F, p=4, amp=0.15, seed=7):
    """Add a kink at every p-th column and row with a RANDOM amplitude -- the
    shape of the artefact under test, and the case a spectral comb cannot see."""
    rs = np.random.RandomState(seed)
    n = F.shape[0]
    G = F.copy()
    s = F.std()
    G[:, ::p] += rs.randn(n, G[:, ::p].shape[1]) * amp * s
    G[::p, :] += rs.randn(G[::p, :].shape[0], n) * amp * s
    return G


def main(argv):
    mip = 0
    if '--mip' in argv:
        mip = int(argv[argv.index('--mip') + 1])
    res = 512 >> mip

    print('=== 0. KNOWN ANSWERS ===')
    P0, R0 = synth(res)
    show('fractal, no crease', P0, R0)
    show('fractal + creases at p4', crease(P0), crease(R0, seed=8))
    print()

    land = load_land()
    rows = []
    for tile in TILES:
        dim, cx, cy = (int(v) for v in tile.split('.'))
        p = res // (dim * 32)
        print('=== Commonwealth.%s  mip %d  %dx%d  grid period %d texels ==='
              % (tile, mip, res, res, p))
        van = load(os.path.join(VAN, 'Commonwealth.%s_msn.DDS' % tile), mip)
        our = load(find_ours(tile), mip)
        hgt = chunk_hgt(land, cx, cy, dim)

        fv = show('VANILLA (floor)', *residual(*slopes(van)), p=p)
        fo = show('OURS (shipped)', *residual(*slopes(our)), p=p)
        near = bc_roundtrip(make_msn(hgt, dim, res=res, mode='nearest'))
        fc = show('PRE-FIX nearest (ceiling)', *residual(*slopes(near)), p=p)
        rep = bc_roundtrip(make_msn(hgt, dim, res=res, mode='bilinear'))
        fr = show('replica bilinear', *residual(*slopes(rep)), p=p)
        raw = make_msn(hgt, dim, res=res, mode='bilinear')
        show('replica bilinear, no codec', *residual(*slopes(raw)), p=p)
        rawn = make_msn(hgt, dim, res=res, mode='nearest')
        show('replica nearest, no codec', *residual(*slopes(rawn)), p=p)

        d = np.abs(rep.astype(np.int32) - our.astype(np.int32))
        print('   replica vs shipped: mean |d| %.2f bytes, 99th %d, max %d'
              % (d.mean(), np.percentile(d, 99), d.max()))
        sep = max(fc) / max(fv)
        print('   ceiling/floor %.2fx  %s     OURS/floor %.2fx'
              % (sep, 'PASS' if sep >= GATE else 'FAIL', max(fo) / max(fv)))
        print()
        rows.append((tile, max(fv), max(fo), max(fc)))

    print('=== summary (worst of MOD_x, MOD_y) ===')
    print('%-12s %8s %8s %8s %10s' % ('tile', 'vanilla', 'ours', 'pre-fix', 'ours/van'))
    for t, v, o, c in rows:
        print('%-12s %8.3f %8.3f %8.3f %9.2fx' % (t, v, o, c, o / v))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
