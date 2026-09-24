"""Known-answer controls for the F1 instruments, run BEFORE any vanilla number
is looked at. An instrument that cannot be shown to fail is not a measurement.

Every control is a synthetic gradient field on a tilted plane whose answer is
known from how it was built:

  C1  rills RUNNING DOWNHILL on a plane tilted to the east.  The channels are
      parallel to the flow, so the fine gradient swings ACROSS the slope.
      Expect A >> 1, and the measured spacing to be the spacing that was built.
  C2  the SAME field rotated 90 degrees -- contour terraces, corrugations that
      run across the flow. Expect A << 1. This is the control that fails if the
      across/along axes are swapped, which is the one mistake that would make
      every vanilla number mean its opposite.
  C3  isotropic white noise. Expect A = 1 to within sampling error.
  C4  a smooth field with no fine content at all. Expect the fine share near 0.
  C5  rills whose amplitude grows with the slope, on a plane whose tilt varies.
      Expect S4's correlation clearly positive; C1's uniform-amplitude rills
      expect it near zero.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/splat1_20260911')
import b1_vanilla as B                                          # noqa: E402

N = 512
FAIL = []


def check(name, got, lo, hi):
    ok = (got == got) and lo <= got <= hi
    print('  %-46s %10.4f  want %.3f..%.3f  %s'
          % (name, got, lo, hi, 'ok' if ok else 'FAIL'))
    if not ok:
        FAIL.append(name)


def run(gx, gy):
    ccx, ccy, fx, fy = B.split(gx, gy)
    dx, dy, ax, ay, m = B.frame(ccx, ccy)
    A, _, _ = B.s2_anisotropy(fx, fy, ax, ay, dx, dy)
    spac, wide, nb = B.s3_channels(fx * ax + fy * ay, ax, ay)
    rp, k = B.s4_slope(fx, fy, gx, gy)
    gm = np.sqrt(gx * gx + gy * gy)
    _, fine = B.s1_bands(gm)
    return A, spac, wide, nb, rp, fine


def plane(tilt_e=0.20, tilt_n=0.0):
    """A constant-gradient plane: gx = tilt_e everywhere (uphill to the east)."""
    return (np.full((N, N), tilt_e, float), np.full((N, N), tilt_n, float))


def main():
    y, x = np.mgrid[0:N, 0:N].astype(float)

    # C1: plane tilted east; rills parallel to the flow, i.e. running east-west,
    # so they are periodic in y (across the slope). Their gradient is in y.
    period = 8.0
    amp = 0.10
    # The tilt VARIES east-west so S4's driver has variance to correlate
    # against, while the rill amplitude is deliberately CONSTANT: r must read
    # near zero, and C5 (amplitude proportional to tilt) must not.
    tiltC1 = 0.05 + 0.35 * (x / float(N))
    gx, gy = tiltC1, np.zeros((N, N))
    rill = amp * np.cos(2.0 * np.pi * y / period)
    print('C1 rills DOWN the slope (periodic across it), CONSTANT amplitude')
    A, spac, wide, nb, rp, fine = run(gx, gy + rill)
    check('C1 anisotropy A (across/along), want large', A, 20.0, 1e9)
    check('C1 measured spacing in texels (built %.1f)' % period, spac,
          period * 0.75, period * 1.25)
    check('C1 blocks that agreed a direction', float(nb), 30.0, 64.0)
    check('C1 amplitude-vs-slope r, constant rill amplitude', abs(rp), 0.0, 0.30)

    # C2: the same corrugation turned 90 degrees -- it now runs across the flow.
    print('C2 the same field rotated 90 degrees (terraces across the slope)')
    rill2 = amp * np.cos(2.0 * np.pi * x / period)
    A2, _, _, _, _, _ = run(gx + rill2, gy)
    check('C2 anisotropy A, want SMALL (the axis test)', A2, 0.0, 0.05)

    # C3: isotropic noise
    print('C3 isotropic white noise')
    rng = np.random.default_rng(4)
    A3, _, _, _, _, _ = run(gx + rng.standard_normal((N, N)) * amp,
                            gy + rng.standard_normal((N, N)) * amp)
    check('C3 anisotropy A, want 1', A3, 0.90, 1.10)

    # C4: no fine content
    print('C4 a smooth field, no relief finer than the coarse blur')
    sm = 0.05 * np.cos(2.0 * np.pi * y / 160.0)
    _, _, _, _, _, fine4 = run(gx, gy + sm)
    check('C4 share of gradient variance under 4 texels', fine4, 0.0, 0.05)

    # C5: rill amplitude proportional to the local slope
    print('C5 rills whose amplitude grows with the slope')
    tilt = 0.05 + 0.35 * (x / float(N))
    growing = tilt * np.cos(2.0 * np.pi * y / period)
    _, _, _, _, rp5, _ = run(tilt, growing)
    check('C5 amplitude-vs-slope r, want clearly positive', rp5, 0.50, 1.00)

    print('\n%d control(s) failed: %s' % (len(FAIL), ', '.join(FAIL) or 'none'))
    return 1 if FAIL else 0


if __name__ == '__main__':
    sys.exit(main())
