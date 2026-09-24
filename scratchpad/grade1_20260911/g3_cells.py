"""GRADE1 -- the fingerprint, per land cell.

g2 showed the whole-sheet gain is not one number (0.864..1.198 over six tiles)
but that four cells of (-20,24) fit k = 0.813..0.824 -- ROADS1's measured
vanilla exposure of x0.82 to the letter.  The hypothesis this script tests:

   where we and vanilla agree on WHAT is painted, we are exactly 1/0.82 too
   bright; where the painted mix differs, the mix swamps the exposure.

So every land cell of the six baked tiles (96 cells, 128x128 texels each) gets

   k     the fitted gain ours -> vanilla
   r     the texel-level correlation between the two inside that cell, and the
         same at an 8x8 box average (fine detail is known to disagree, so the
         box-averaged r is the honest measure of "same content")
   sat   mean HSV saturation each side
   sd    each side's own spread

and the cells are then binned by r8.  If k collapses on one number as r8 rises,
that number is the missing constant and its scatter at high r8 is the error bar.
A phase-twin floor is reported for r8 as a structure statistic.

Usage: python g3_cells.py
"""

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gradelib as G                                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TILES = [(-20, 24), (-20, 20), (-24, 24), (-16, 24), (-20, 28), (-16, 20)]


def box(a, n):
    h, w = a.shape
    return a[:h // n * n, :w // n * n].reshape(h // n, n, w // n, n).mean((1, 3))


cells = []
for cx, cy in TILES:
    o = G.rgb(G.ours('def', cx, cy))
    v = G.rgb(G.van(cx, cy))
    lo, lv = G.lum(o), G.lum(v)
    so, sv = G.saturation(o), G.saturation(v)
    for gy in range(4):
        for gx in range(4):
            sl = (slice(gy * 128, gy * 128 + 128),
                  slice(gx * 128, gx * 128 + 128))
            a, b = lo[sl], lv[sl]
            p = G.fit_gain(a, b)
            tw = G.phase_twin(a, seed=7)
            cells.append({
                'tile': '(%d,%d)' % (cx, cy),
                'cell': '(%d,%d)' % (cx + gx, cy + 3 - gy),
                'k': p['k'], 'r1': G.pearson(a, b),
                'r8': G.pearson(box(a, 8), box(b, 8)),
                'r8_twin': G.pearson(box(tw, 8), box(b, 8)),
                'o': float(a.mean()), 'v': float(b.mean()),
                'sdo': float(a.std()), 'sdv': float(b.std()),
                'so': float(so[sl].mean()), 'sv': float(sv[sl].mean())})

cells.sort(key=lambda c: -c['r8'])
print('%-10s %-10s %6s %7s %7s %7s %7s %7s %6s %6s %6s %6s' % (
    'tile', 'cell', 'k', 'r1', 'r8', 'r8twin', 'ours', 'van',
    'sdO', 'sdV', 'satO', 'satV'))
for c in cells:
    print('%-10s %-10s %6.3f %+7.3f %+7.3f %+7.3f %7.2f %7.2f %6.2f %6.2f '
          '%6.3f %6.3f' % (c['tile'], c['cell'], c['k'], c['r1'], c['r8'],
                           c['r8_twin'], c['o'], c['v'], c['sdo'], c['sdv'],
                           c['so'], c['sv']))

print('\n--- k binned by content agreement r8')
print('%-14s %4s %8s %8s %8s %8s %8s' % (
    'r8 band', 'n', 'k mean', 'k sd', 'k min', 'k max', 'twin mean'))
bands = [(-1.0, 0.0), (0.0, 0.2), (0.2, 0.4), (0.4, 0.6), (0.6, 0.8), (0.8, 1.01)]
summary = []
for lo_, hi in bands:
    sel = [c for c in cells if lo_ <= c['r8'] < hi]
    if not sel:
        continue
    kk = np.array([c['k'] for c in sel])
    tw = np.mean([c['r8_twin'] for c in sel])
    print('%-14s %4d %8.4f %8.4f %8.4f %8.4f %8.4f'
          % ('%.1f..%.1f' % (lo_, hi), len(sel), kk.mean(), kk.std(),
             kk.min(), kk.max(), tw))
    summary.append({'band': [lo_, hi], 'n': len(sel), 'mean': float(kk.mean()),
                    'sd': float(kk.std()), 'min': float(kk.min()),
                    'max': float(kk.max()), 'twin': float(tw)})

top = [c for c in cells if c['r8'] >= 0.6]
if top:
    kk = np.array([c['k'] for c in top])
    print('\n  cells with r8 >= 0.6 (n=%d): k = %.4f +- %.4f  (1/k = %.4f)'
          % (len(top), kk.mean(), kk.std(), 1.0 / kk.mean()))
    print('  their saturation: ours %.4f  vanilla %.4f  ratio %.4f'
          % (np.mean([c['so'] for c in top]), np.mean([c['sv'] for c in top]),
             np.mean([c['so'] for c in top]) / np.mean([c['sv'] for c in top])))
    print('  their spread:     ours %.3f  vanilla %.3f'
          % (np.mean([c['sdo'] for c in top]), np.mean([c['sdv'] for c in top])))
    # does a gain explain them?  compare residual after k to residual after 1
    print('  a single k on those cells reduces |ours-van| from %.3f to %.3f'
          % (np.mean([abs(c['o'] - c['v']) for c in top]),
             np.mean([abs(c['o'] * kk.mean() - c['v']) for c in top])))
allk = np.array([c['k'] for c in cells])
print('\n  all 96 cells: k = %.4f +- %.4f  min %.4f max %.4f'
      % (allk.mean(), allk.std(), allk.min(), allk.max()))

with open(os.path.join(HERE, 'g3_cells.json'), 'w') as f:
    json.dump({'cells': cells, 'bands': summary}, f, indent=1, default=float)
print('wrote g3_cells.json')
