"""GRADE1 -- the census: every baked tile, and what a GLOBAL change can do.

g2/g3 refuted the constant gain: the fitted k runs 0.86..1.20 across six tiles
and 0.70..1.39 across their 96 land cells, mean 1.00.  A single exposure cannot
reduce the error on every tile because the tiles disagree on its SIGN.

This script measures that over the whole census grid, and then asks the only
global question left with a consistent sign: CHROMA.  Ours is more saturated
than vanilla on every tile measured so far.  The candidate operator is a pull
of each texel toward its own luminance,

    c' = L + s * (c - L),        L = lum(c),  s in (0,1]

which is exactly "less saturated, same brightness" -- it leaves luminance
untouched, so it cannot borrow any of the exposure error, and s = 1 is the
identity (the off value is byte-identical by construction, not by luck).

Reported per tile: the fitted s and the whole-tile RGB RMS before and after,
using ONE global s (the census median), never the tile's own.

Usage: python g4_census.py
"""

import glob
import json
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gradelib as G                                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def tiles():
    got = []
    for p in sorted(glob.glob(os.path.join(HERE, 'out', 'def', '*', 'tex',
                                           'Commonwealth.4.*.DDS'))):
        m = re.search(r'Commonwealth\.4\.(-?\d+)\.(-?\d+)\.DDS$', p)
        if m:
            got.append((int(m.group(1)), int(m.group(2)), p))
    return got


def desat(img, s):
    L = G.lum(img)[:, :, None]
    return L + s * (img - L)


def fit_s(o, v):
    """least squares s for v ~ L + s*(o-L), over all three channels."""
    L = G.lum(o)[:, :, None]
    d = (o - L).ravel()
    t = (v - L).ravel()
    return float(np.sum(d * t) / np.sum(d * d))


rows = []
for cx, cy, p in tiles():
    vp = G.van(cx, cy)
    if not os.path.exists(vp):
        continue
    o, v = G.rgb(p), G.rgb(vp)
    lo, lv = G.lum(o), G.lum(v)
    k = G.fit_gain(lo, lv)['k']
    s = fit_s(o, v)
    rows.append({'tile': '(%d,%d)' % (cx, cy), 'cx': cx, 'cy': cy, 'path': p,
                 'o': float(lo.mean()), 'v': float(lv.mean()), 'k': k, 's': s,
                 'so': float(G.saturation(o).mean()),
                 'sv': float(G.saturation(v).mean()),
                 'rgb_rms': G.resid(o, v)['rms'],
                 'sd_o': float(lo.std()), 'sd_v': float(lv.std())})

print('%-11s %7s %7s %7s %7s %7s %7s %8s' % (
    'tile', 'ours', 'van', 'k', 'satO', 'satV', 's_fit', 'rgbRMS'))
for r in rows:
    print('%-11s %7.2f %7.2f %7.4f %7.4f %7.4f %7.4f %8.3f'
          % (r['tile'], r['o'], r['v'], r['k'], r['so'], r['sv'], r['s'],
             r['rgb_rms']))

k = np.array([r['k'] for r in rows])
s = np.array([r['s'] for r in rows])
print('\n  n = %d tiles' % len(rows))
print('  k (exposure)   mean %.4f  sd %.4f  min %.4f  max %.4f   '
      '%d tiles want k<1, %d want k>1'
      % (k.mean(), k.std(), k.min(), k.max(), int((k < 1).sum()),
         int((k > 1).sum())))
print('  s (chroma)     mean %.4f  sd %.4f  min %.4f  max %.4f   '
      '%d tiles want s<1, %d want s>1'
      % (s.mean(), s.std(), s.min(), s.max(), int((s < 1).sum()),
         int((s > 1).sum())))

S = float(np.median(s))
print('\n  ONE global chroma pull s = %.4f (the census median), applied to '
      'every tile:' % S)
print('%-11s %9s %9s %9s   %s' % ('tile', 'rgbRMS', 'after', 'delta', ''))
better = 0
for r in rows:
    o, v = G.rgb(r['path']), G.rgb(G.van(r['cx'], r['cy']))
    a = G.resid(desat(o, S), v)['rms']
    d = a - r['rgb_rms']
    better += d < 0
    print('%-11s %9.3f %9.3f %+9.3f   %s'
          % (r['tile'], r['rgb_rms'], a, d, 'better' if d < 0 else 'WORSE'))
    r['after_s'] = a
print('  reduced on %d of %d tiles' % (better, len(rows)))

print('\n  for contrast, ONE global exposure k = %.4f (the census median):'
      % float(np.median(k)))
K = float(np.median(k))
better_k = 0
for r in rows:
    o, v = G.rgb(r['path']), G.rgb(G.van(r['cx'], r['cy']))
    a = G.resid(o * K, v)['rms']
    d = a - r['rgb_rms']
    better_k += d < 0
    print('%-11s %9.3f %9.3f %+9.3f   %s'
          % (r['tile'], r['rgb_rms'], a, d, 'better' if d < 0 else 'WORSE'))
    r['after_k'] = a
print('  reduced on %d of %d tiles' % (better_k, len(rows)))

with open(os.path.join(HERE, 'g4_census.json'), 'w') as f:
    json.dump({'rows': rows, 's_global': S, 'k_global': K,
               'better_s': int(better), 'better_k': int(better_k)},
              f, indent=1, default=float)
print('\nwrote g4_census.json')
