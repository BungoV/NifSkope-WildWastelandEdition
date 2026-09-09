"""Lane LATTICE step 2 -- ONE CHANGE PER VARIANT, which candidate does the
number pick?

The three candidates named in HANDOFF.md, and the intervention that separates
each of them:

  (a) the bilinear reconstruction is C0 -- the gradient it produces is
      CONTINUOUS BUT KINKED at every height-sample line, so the sheet is creased
      on a 4-texel grid.  INTERVENTION: re-derive the normal from a
      reconstruction whose gradient is C1/C2 and see whether MOD falls.
        - `smoothstep` / `quintic`: the same four taps, the interpolation
          parameter eased.  Passes through every VHGT sample exactly, so it
          cannot blur and cannot ring.
        - `bspline`: C2 but APPROXIMATING -- it does blur, and is here to show
          what buying smoothness with blur costs.
        - `catmull`: the basis already tried and reverted; the ringing control.
  (b) the 8-unit VHGT staircase.  INTERVENTION: dequantise the heights before
      differencing (Gaussian over the grid), bilinear otherwise unchanged.  Also
      run as a SYNTHETIC 2x2 (smooth vs quantised) x (bilinear vs quintic), so
      the staircase's own contribution is isolated from the reconstruction's.
  (c) the texel/sample phase from the x-mod-4 table.  INTERVENTION: shift the
      texel->grid mapping by half a grid step.  If the raised residue classes
      MOVE with the phase the roughness is locked to the SAMPLE LINES; if they
      stay put it is locked to the TEXEL grid, i.e. the block codec.

Gates, pre-registered: MOD must fall to vanilla's level or below, and the
high-frequency residual rms must NOT fall below the current bilinear's.

Usage:  python lattice_variants.py
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lattice import (bc_roundtrip, boxmean, chunk_hgt, find_ours,
                     gauss_blur_grid, load, load_land, make_msn, residual,
                     slopes, VAN)
from lattice_phase import classes, measure

TILES = ['4.-60.36', '4.-12.44']


def hf_energy(rgb, k=9):
    """The 6.6-8.2x metric: mean absolute deviation of the sheet's bytes from a
    local blur.  Kept in BYTES so it is the same number the thread has quoted."""
    a = np.asarray(rgb, dtype=np.float64)
    d = 0.0
    for c in range(3):
        d += np.abs(a[:, :, c] - boxmean(a[:, :, c], k)).mean()
    return d / 3.0


def stats(rgb, p=4):
    P, R = residual(*slopes(rgb))
    mx, my = measure(P, R, p)
    return {
        'mod': max(mx, my),
        'mod_x': mx,
        'mod_y': my,
        'rms': float(np.sqrt((P ** 2 + R ** 2).mean())),
        'hf': hf_energy(rgb),
        'cls': classes(P, p, 0),
    }


def row(label, s, base=None, extra=''):
    rel = ''
    if base is not None:
        rel = '  rms %+6.1f%%  hf %+6.1f%%' % (
            100.0 * (s['rms'] / base['rms'] - 1.0),
            100.0 * (s['hf'] / base['hf'] - 1.0))
    print('  %-28s MOD %6.3f   rms %7.5f  hf %6.3f%s  cls %s %s'
          % (label, s['mod'], s['rms'], s['hf'], rel,
             ' '.join('%.4f' % v for v in s['cls']), extra))


def synthetic_2x2():
    """The staircase, isolated.  A smooth analytic height field on the same
    129-sample grid, with and without the 8-unit quantisation, reconstructed
    bilinearly and quintically.  Nothing here touches vanilla or our output, so
    it separates the two candidates with no codec and no terrain in the way."""
    print('\n=== (b) THE STAIRCASE, ISOLATED (synthetic, no codec) ===')
    hn, dim, res = 129, 4, 512
    rs = np.random.RandomState(11)
    ky = np.fft.fftfreq(hn)[:, None]
    kx = np.fft.fftfreq(hn)[None, :]
    k = np.sqrt(kx ** 2 + ky ** 2)
    k[0, 0] = 1e-9
    amp = k ** (-1.5)
    amp[0, 0] = 0
    h = np.fft.ifft2(amp * np.fft.fft2(rs.randn(hn, hn))).real
    h = h / h.std() * 900.0                       # ~ the tile's own height sd
    for hname, hh in (('exact heights', h),
                      ('quantised to 8 units', np.round(h / 8.0) * 8.0)):
        for mode in ('bilinear', 'quintic'):
            s = stats(make_msn(hh, dim, res=res, mode=mode))
            row('%-22s %-10s' % (hname, mode), s)


def main(argv):
    land = load_land()
    print('=== 0. what the candidates are measured against ===')
    for tile in TILES:
        van = load(os.path.join(VAN, 'Commonwealth.%s_msn.DDS' % tile), 0)
        print('  vanilla %-12s' % tile, end=' ')
        s = stats(van)
        print('MOD %6.3f  rms %7.5f  hf %6.3f  cls %s'
              % (s['mod'], s['rms'], s['hf'], ' '.join('%.4f' % v for v in s['cls'])))

    for tile in TILES:
        dim, cx, cy = (int(v) for v in tile.split('.'))
        hgt = chunk_hgt(land, cx, cy, dim)
        vs = stats(load(os.path.join(VAN, 'Commonwealth.%s_msn.DDS' % tile), 0))
        print('\n=== Commonwealth.%s  (vanilla MOD %.3f, hf %.3f) ==='
              % (tile, vs['mod'], vs['hf']))

        print(' (a) the reconstruction -- one basis per row, everything else equal')
        base = None
        for mode in ('bilinear', 'quintic', 'smoothstep', 'bspline', 'catmull'):
            s = stats(bc_roundtrip(make_msn(hgt, dim, mode=mode)))
            if mode == 'bilinear':
                base = s
            row(mode, s, base)

        print(' (b) the 8-unit staircase -- heights smoothed first, bilinear otherwise')
        for sg in (0.25, 0.5, 1.0):
            s = stats(bc_roundtrip(make_msn(hgt, dim, mode='bilinear', gsigma=sg)))
            row('bilinear, gauss sigma %.2f' % sg, s, base)

        print(' (c) the phase -- the same bilinear sheet, sample grid shifted')
        for ph in (0.0, 0.25, 0.5):
            s = stats(bc_roundtrip(make_msn(hgt, dim, mode='bilinear', phase=ph)))
            row('bilinear, phase %+0.2f step' % ph, s, base)
        for ph in (0.0, 0.5):
            s = stats(bc_roundtrip(make_msn(hgt, dim, mode='quintic', phase=ph)))
            row('quintic,  phase %+0.2f step' % ph, s, base)

    synthetic_2x2()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
