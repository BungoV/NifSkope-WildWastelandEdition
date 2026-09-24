#!/usr/bin/env python3
"""THE GATE for the blended ground (lane CELLVIEW4).

It starts NOTHING. It takes three pictures of the SAME cell from the same
camera and decides which of two known answers the viewer's ground matches:

    MOSAIC    scratchpad/cellview4_20260919/images/sim_m20_7_mosaic.png
              today's rule, rebuilt from the LAND record by splat_sim.py --
              every 128-unit quad takes ONE texture at its own SW corner.
    BLENDED   scratchpad/cellview4_20260919/images/sim_m20_7_blended.png
              the engine's rule: BTXT, then every ATXT layer composited over
              it in ATXT paint order with its own per-vertex opacity.  THE
              TARGET.
    SHOT      the picture the build lane shoots of the same cell.

WHY IT IS A RELATIVE TEST, NOT A COLOUR MATCH.  The simulation composites raw
diffuse texels.  The viewer lights the ground, multiplies the VCLR vertex
colour into it and tone-maps the frame, so the SHOT is never going to equal
either target in absolute colour, and a gate that demanded that would fail on
a correct build.  What it can demand is the thing that actually distinguishes
the two rules: on the quads where mosaic and blended DISAGREE, the shot has to
side with BLENDED.  That is scale- and exposure-robust, and it fails loudly on
a build that quietly kept the mosaic -- which is the only failure this gate
exists to catch.

THE PRE-REGISTERED ROW.  Measured on Sanctuary -20,7 at 512px BEFORE any C++
was written, so it cannot be tuned to a result.  Two reductions of the same
pair, and they are NOT the same number -- keep them apart:

    per PIXEL, mean |blended - mosaic| over the cell      0.0345
    per pixel, quads with a mean difference over 0.02     509 / 1024
    per pixel, over 0.10                                   45 / 1024
    per pixel, identical (under 0.001)                    348 / 1024

    per QUAD (what THIS gate reduces to first -- the difference of the two
    quad MEANS, not the mean of the differences, so it is the smaller figure):
    mean |blended - mosaic| over the cell                 0.0183
    worst quad                                            0.5026
    quads where the rules disagree by more than 0.02      249 / 1024

The gate uses the 249: at least 90% of them must be closer to BLENDED than to
MOSAIC.  The quads the two rules agree on are EXCLUDED because they carry no
information either way.

WHAT THE THRESHOLD IS MADE OF -- both ends measured, not assumed, by feeding
the gate each known answer as the SHOT:

    shot = blended (a correct build)    249/249   100.0%   PASS
    shot = mosaic  (today's build)      187/249    75.1%   FAIL

So the separation is 25 points and the 90% line sits inside it with 15 points
of margin above the failing case.  A mosaic build does NOT score near zero:
the two pictures agree over most of the cell, and after the gain+offset fit
three quarters of even the disagreeing quads still land nearer the blended
target by chance.  That is why the line is at 90% and not at 50%, and why
moving it below 76% would make this gate unable to fail.

Usage:
  python cell_splat_compare.py <shot.png> <mosaic.png> <blended.png> [--quads N]
Exit 0 = PASS, 1 = FAIL, 2 = could not run.
"""
import sys

import numpy as np

try:
    from PIL import Image
except ImportError:                                  # noqa: BLE001
    sys.stderr.write('cell_splat_compare: needs Pillow\n')
    sys.exit(2)

DIFF_MIN = 0.02          # a quad below this is "the two rules agree here"
NEEDED = 0.90            # fraction of the disagreeing quads that must side blended


def load(path, n):
    """-> float array (n, n, 3) in 0..1, the picture reduced to per-quad means."""
    im = Image.open(path).convert('RGB')
    a = np.asarray(im, dtype=np.float32) / 255.0
    h, w = a.shape[:2]
    if h < n or w < n:
        raise SystemExit('cell_splat_compare: %s is %dx%d, smaller than the '
                         '%dx%d quad grid' % (path, w, h, n, n))
    # exact block means: crop to a multiple of n rather than resampling, so the
    # number a quad reports is the mean of its own pixels and nothing else
    a = a[:h // n * n, :w // n * n]
    bh, bw = a.shape[0] // n, a.shape[1] // n
    return a.reshape(n, bh, n, bw, 3).mean(axis=(1, 3))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    n = 32
    for i, a in enumerate(sys.argv):
        if a == '--quads' and i + 1 < len(sys.argv):
            n = int(sys.argv[i + 1])
    if len(args) < 3:
        sys.stderr.write(__doc__)
        return 2
    shot, mosaic, blended = args[0], args[1], args[2]

    S = load(shot, n)
    M = load(mosaic, n)
    B = load(blended, n)

    # the two known answers, and where they disagree at all
    sep = np.abs(B - M).mean(axis=2)
    live = sep > DIFF_MIN
    nlive = int(live.sum())
    print('cell_splat_compare: %dx%d quads' % (n, n))
    print('  mean |blended - mosaic| over the cell : %.4f' % float(sep.mean()))
    print('  worst quad                            : %.4f' % float(sep.max()))
    print('  quads where the two rules disagree    : %d / %d' % (nlive, n * n))
    if nlive == 0:
        print('  REFUSED: the two targets are the same picture -- nothing to '
              'tell apart, so this gate cannot pass or fail honestly')
        return 2

    """The SHOT is compared after removing the one difference that is expected
    and uninteresting: the viewer's overall exposure.  A single scale and
    offset per channel, fitted over the WHOLE cell, is the least generous
    correction that can be made -- it cannot move any individual quad towards
    either target, only the frame as a whole."""
    fit = np.zeros_like(S)
    for c in range(3):
        s = S[..., c].ravel()
        t = B[..., c].ravel()
        A = np.vstack([s, np.ones_like(s)]).T
        k, b = np.linalg.lstsq(A, t, rcond=None)[0]
        fit[..., c] = S[..., c] * k + b
    print('  (the shot was fitted to the target by one gain+offset per channel)')

    dB = np.abs(fit - B).mean(axis=2)
    dM = np.abs(fit - M).mean(axis=2)
    sides_blended = (dB < dM) & live
    got = int(sides_blended.sum())
    frac = got / float(nlive)
    print('  of the disagreeing quads, closer to BLENDED : %d (%.1f%%)'
          % (got, 100.0 * frac))
    print('  needed                                      : %.0f%%'
          % (100.0 * NEEDED))
    ok = frac >= NEEDED
    print('%s' % ('PASS' if ok else 'FAIL'))
    if not ok:
        print('  a build still drawing the MOSAIC scores 75.1% on this cell '
              '(measured, by feeding the mosaic in as the shot); a score near '
              'that means the blend did not happen at all. A score between '
              '76% and 90% means the layers ARE composited but in the wrong '
              'ORDER -- check that WW_CELLSPLAT_LAYER_INDEX is defined and '
              'that the ground legend does not say "WITHOUT THE PAINT ORDER".')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
