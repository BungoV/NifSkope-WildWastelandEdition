#!/usr/bin/env python3
"""control_synth.py -- the KNOWN-ANSWER control for the water census.

ww-control-calibration step 1: run the metric on inputs whose answer is known
before the code runs, and print them above every real number.  The brief
pre-registers this gate: *a synthetic worldspace with a known lake, a stepped
river and a sea must classify correctly before the real numbers are believed.*

The synthetic worldspace, built here and nowhere else:

  24x24 cells, 32 samples per cell edge -> 768x768 texels, row 0 SOUTH.
  dry ground at +1000; worldspace default water 450, default type index 65535.

  SEA      texel columns 0..95 (cells x 0..2) cut to -100.  Inherits the
           default height and type.  Reaches the west edge.      -> sea
  RIVER    a channel 16 texels tall at y 400..415, x 96..607, cut to -50,
           water type index 1, water height stepping +50 per cell from 450
           at cell x=3 to 1200 at cell x=18.                     -> river, 16 surfaces
  LAKE     cells x 14..17, y 14..17, floor at +800, water height 1000,
           type index 2, touching no edge, one height.           -> lake, 1 surface
  PUDDLE   a 4x4-texel dip at (x 200, y 600), floor 0, height 450,
           type index 3.                                          -> lake, 1 surface

Expected, written before the run: FOUR bodies; classes sea / river / lake /
lake; the river monotone along its own axis (|r| = 1.00); the type-BLIND
segmentation collapsing sea+river into one body, which is the measurement that
justifies keying bodies on water type.
"""

import numpy as np

import water_model as WM

SPC = 32
CELLS = 24
N = CELLS * SPC
DEFAULT_H = 450.0
DEFAULT_T = 65535


def build():
    terrain = np.full((N, N), 1000.0, np.float32)
    wh = np.full((CELLS, CELLS), DEFAULT_H, np.float32)
    wt = np.full((CELLS, CELLS), DEFAULT_T, np.int64)
    hw = np.ones((CELLS, CELLS), bool)

    terrain[:, 0:96] = -100.0                       # sea

    terrain[400:416, 96:608] = -50.0                # river channel
    for cx in range(3, 19):
        wt[12, cx] = 1
        wh[12, cx] = 450.0 + 50.0 * (cx - 3)

    terrain[14 * SPC:18 * SPC, 14 * SPC:18 * SPC] = 800.0   # lake basin
    wh[14:18, 14:18] = 1000.0
    wt[14:18, 14:18] = 2

    terrain[600:604, 200:204] = 0.0                 # puddle
    wt[600 // SPC, 200 // SPC] = 3
    return terrain, wh, wt, hw


def main():
    terrain, wh, wt, hw = build()
    wet, whT, surf, nsurf, body, nbody = WM.segment(terrain, wh, hw, wt, SPC)
    tkey = WM.expand_cells(wt, SPC)
    stats = WM.body_stats(wet, whT, body, nbody, surf, tkey, DEFAULT_H)

    print('EXPECTED  4 bodies: sea / river(16 surfaces) / lake / lake')
    print('MEASURED  %d bodies, %d surfaces' % (nbody, nsurf))
    names = {DEFAULT_T: 'default', 1: 'river-type', 2: 'lake-type', 3: 'puddle-type'}
    got = {}
    for b, rec in sorted(stats.items(), key=lambda kv: -kv[1]['area']):
        cls = WM.classify(rec, DEFAULT_H)
        ys, xs = np.nonzero(body == b)
        hs = whT[ys, xs]
        r, aniso = WM.height_monotonicity(ys, xs, hs)
        got[names.get(rec['type'], rec['type'])] = cls
        print('  body %2d  type %-11s area %6d  surfaces %2d  edge %-5s  '
              'class %-5s  r %+0.2f aniso %.2f'
              % (b, names.get(rec['type'], rec['type']), rec['area'], rec['nsurf'],
                 rec['edge'], cls, r, aniso))

    ok = (nbody == 4
          and got.get('default') == 'sea'
          and got.get('river-type') == 'river'
          and got.get('lake-type') == 'lake'
          and got.get('puddle-type') == 'lake')
    riv = [rec for rec in stats.values() if rec['type'] == 1]
    ok = ok and riv and riv[0]['nsurf'] == 16

    # the refuter for keying on type: segment type-BLIND and watch sea+river merge
    import ccl
    blind, nblind = ccl.label(wet, np.zeros_like(tkey))
    print('REFUTER   type-blind segmentation -> %d bodies (expect 3: sea+river fused)'
          % nblind)
    ok = ok and nblind == 3

    print('control', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
