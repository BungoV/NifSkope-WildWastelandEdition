"""Is the _msn high-pass real material detail, or BC3 compression noise?

The whole case for these features rests on one claim: the geometric normal is
band-limited to about 4 texels (the VHGT grid is 4 texels per vertex at mip 0),
so anything finer must be baked material detail.  That claim has a rival
explanation.  These tiles are BC3, whose colour endpoints are 5 bits on R and B
and 6 bits on G, quantised per 4x4 block -- exactly the scale the argument
calls "material".  A staircase at block boundaries would masquerade as gravel.

Two measurements separate them.

1. Radial power spectrum of 64x64 patches.  The geometry cutoff sits at 16
   cycles per 64 texels (one cycle per 4 texels).  Compression noise is roughly
   flat to Nyquist and shows a spike at 16 cycles per 64 -- the block period is
   also 4 texels, which is unhelpfully the same number, so the spectrum alone
   cannot settle it.  It is reported for shape, not for the verdict.

2. The verdict measurement: does msn detail live in the same PLACES as diffuse
   detail?  The diffuse tiles unarguably carry material texture, and
   features.npz already holds a per-quadrant mean |laplacian| of diffuse
   luminance.  If the _msn high-pass is baked from the same source art, the two
   correlate across the worldspace's 147,456 quadrants.  If it is compression
   noise, it correlates with the tile's own contrast and not with the diffuse's
   speckle.  Noise cannot know where the gravel is.
"""
import os
import re
import sys

import numpy as np

from dds import DDS
from bcnp import decode_rgb
from rec_common import HERE
from msn_features import VAN, boxmean

SAMPLE = ['0.0', '-40.-40', '60.60', '16.-24', '-88.28', '-20.40',
          '32.-52', '-64.-8', '8.64', '44.20']
DIFF_LAP = 16      # features.npz column: per-quadrant mean |laplacian| of luminance


def radial(power):
    """(64,64) power -> mean power in each integer radial frequency bin."""
    n = power.shape[0]
    f = np.fft.fftfreq(n) * n
    rr = np.rint(np.hypot(f[:, None], f[None, :])).astype(int)
    out = np.zeros(n // 2 + 1)
    for k in range(1, n // 2 + 1):
        m = rr == k
        out[k] = power[m].mean() if m.any() else 0.0
    return out


def spectrum(a):
    """mean radial power over the 64 non-overlapping 64x64 patches of a tile."""
    p = a.reshape(8, 64, 8, 64).transpose(0, 2, 1, 3).reshape(-1, 64, 64)
    p = p - p.mean(axis=(1, 2), keepdims=True)
    f = np.abs(np.fft.fft2(p)) ** 2
    return np.stack([radial(x) for x in f]).mean(0)


def main():
    print('=== 1. radial power spectra, mean over %d tiles ===' % len(SAMPLE))
    print('    (16 cycles/64 = the 4-texel VHGT vertex spacing AND the BC block period)')
    acc = {}
    nt = 0
    for s in SAMPLE:
        pm = os.path.join(VAN, 'Commonwealth.4.%s_msn.DDS' % s)
        pd = os.path.join(VAN, 'Commonwealth.4.%s.DDS' % s)
        if not (os.path.exists(pm) and os.path.exists(pd)):
            continue
        m = (decode_rgb(DDS(pm), 0).astype(np.float64) / 255.0 - 0.5) * 2.0
        d = decode_rgb(DDS(pd), 0).astype(np.float64) / 255.0
        lum = 0.2126 * d[:, :, 0] + 0.7152 * d[:, :, 1] + 0.0722 * d[:, :, 2]
        for nm, a in (('msn nx', m[:, :, 0]), ('msn ny', m[:, :, 1]),
                      ('msn nz', m[:, :, 2]), ('diffuse lum', lum)):
            acc[nm] = acc.get(nm, 0.0) + spectrum(a)
        nt += 1
    print('%-13s %s' % ('band (cyc/64)', ''.join('%9d' % k for k in (1, 2, 4, 8, 16, 24, 32))))
    for nm in ('msn nx', 'msn ny', 'msn nz', 'diffuse lum'):
        v = acc[nm] / nt
        print('%-13s %s   ratio P(32)/P(8) = %.4f'
              % (nm, ''.join('%9.2e' % v[k] for k in (1, 2, 4, 8, 16, 24, 32)),
                 v[32] / max(v[8], 1e-30)))

    print()
    print('=== 2. does msn detail sit where diffuse detail sits? ===')
    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M, have, names = z['M'], z['have'], list(z['names'])
    dl = np.load(os.path.join(HERE, 'features.npz'))['F'][:, :, :, DIFF_LAP]
    m = have
    y = dl[m].ravel()
    print('quadrants correlated: %d' % len(y))
    print('%-12s %9s' % ('msn feature', 'r vs diffuse |laplacian|'))
    rows = []
    for i, nm in enumerate(names):
        x = M[m][:, :, i].ravel()
        a = x - x.mean()
        b = y - y.mean()
        r = float((a * b).sum() / max(np.sqrt((a * a).sum() * (b * b).sum()), 1e-30))
        rows.append((abs(r), nm, r))
    for _, nm, r in sorted(rows, reverse=True):
        print('%-12s %+9.3f' % (nm, r))


if __name__ == '__main__':
    sys.exit(main())
