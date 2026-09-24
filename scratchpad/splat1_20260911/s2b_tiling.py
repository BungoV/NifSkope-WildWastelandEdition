"""SPLAT1 section 2b -- bungo's question, as a measurement.

"the terrain textures here for the terrain bakes do use their correct scale,
right? So they'd be very tiny repeating pixel sized patterns"

TILE (world units per texture repeat) is a bare constant in the bake with no
source behind it. Vary it, re-bake the chunk OFFLINE at each value with the
bake's own footprint-mip rule, and ask two questions of each candidate:

  1. does it reproduce VANILLA'S local variance (the speckle), and
  2. does its high-passed field CORRELATE with vanilla's, i.e. is the
     landscape texture's own pattern present in Bethesda's sheet at that
     repeat?

Controls: the correlation of vanilla against a phase-randomised twin of each
candidate (same spectrum, no registration) is the floor, and vanilla against
itself is the ceiling.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import splatlib as S                                          # noqa: E402
import offline_bake as B                                      # noqa: E402

CANDS = [170.667, 341.333, 512.0, 682.667, 1024.0, 2048.0]
# 341.333 = 4096/12 = 128/0.375, THE ENGINE'S OWN REPEAT (section 2c).
# 2048 = the constant the bake ships. The rest bracket them.


def highpass(a, r=8):
    return a - S._box(a, r)


def corr(a, b):
    a = a - a.mean()
    b = b - b.mean()
    d = a.std() * b.std()
    return float((a * b).mean() / d) if d > 0 else 0.0


for (cx0, cy0) in ((-20, 24), (-20, 20)):
    print('=== chunk (%d,%d) ===' % (cx0, cy0))
    van = S.Dds(S.van_sheet(cx0, cy0)).level(0)
    vanL = S.lum(van)
    ourL = S.lum(S.Dds(S.OURS[(cx0, cy0)]).level(0))
    hv = highpass(vanL)
    print('  vanilla   local var %7.2f   high-passed sd %5.2f' %
          (S.local_var(vanL).mean(), hv.std()))
    print('  ours      local var %7.2f' % S.local_var(ourL).mean())
    print('')
    print('  %-9s %6s %6s  %10s  %10s  %10s  %8s' %
          ('TILE(u)', 'src', 'mip', 'u/texel@mip', 'local var', 'corr(van)',
           'twin floor'))
    for t in CANDS:
        sheet = B.bake(cx0, cy0, 4, mip='code', tile=t)
        L = S.lum(np.dstack([sheet, np.full(sheet.shape[:2], 255.0)]))
        d = B.code_mip(0x0001D1D3, t, 32.0)
        hc = highpass(L)
        c = corr(hv, hc)
        floors = [corr(hv, highpass(S.phase_twin(L, seed=1000 + k)))
                  for k in range(3)]
        print('  %-9.0f %6d %6.2f  %10.2f  %10.2f  %+10.4f  %+8.4f'
              % (t, d['width'], d['mip'], d['mipTexelWorld'],
                 S.local_var(L).mean(), c, max(abs(x) for x in floors)))
    # THE CORRELATION INSTRUMENT'S OWN CEILING: our OWN sheet against the
    # offline re-bake at the tiling our own bake used. If this does not read
    # near 1, the correlation cannot detect a landscape texture in ANY sheet
    # and none of the rows above may be read as a negative.
    ref = B.bake(cx0, cy0, 4, mip='code', tile=2048.0)
    refL = S.lum(np.dstack([ref, np.full(ref.shape[:2], 255.0)]))
    print('  CEILING  corr(OUR sheet, offline re-bake at TILE=2048) = %+0.4f'
          % corr(highpass(ourL), highpass(refL)))
    print('  ceiling: corr(vanilla, vanilla) = %+0.4f' % corr(hv, hv))
    print('')
