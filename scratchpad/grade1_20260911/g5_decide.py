"""GRADE1 -- the road/ground split, and the arithmetic that refuses the gate.

Part 1  the region split the brief asks for, on the tile that has roads.
Part 2  the refusal, proved from constants and not from a new build: the RGB
        error of a constant gain k on a tile is a PARABOLA in k with its vertex
        at that tile's own k_opt = <o,v>/<o,o>.  Error is therefore strictly
        increasing away from k_opt in both directions.  The two brief tiles have
        k_opt on OPPOSITE sides of 1, so every k != 1 raises the error on one of
        them, and k = 1 is the switch's off value.  No constant grade can meet
        "reduced on both tiles".  Evaluated on a k grid to show the parabola.
Part 3  the pooled-optimal k over the census (the one number to offer if a
        single grade is ever wanted), with the count of tiles it helps.

Usage: python g5_decide.py
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
out = {}

# ------------------------------------------------------------------ part 1
print('--- 1 region split (road mask = default bake vs --no-roads bake)')
print('%-10s %-8s %8s %9s %9s %8s %9s %9s' % (
    'tile', 'region', 'n', 'ours', 'vanilla', 'k_opt', 'rgbRMS', 'lumBias'))
for cx, cy in [(-20, 24), (-20, 20)]:
    tag = '(%d,%d)' % (cx, cy)
    o = G.rgb(G.ours('def', cx, cy))
    v = G.rgb(G.van(cx, cy))
    nr = G.rgb(G.ours('noroads', cx, cy))
    road = np.abs(nr - o).max(2) > 2.0
    cover, have = G.cover_plane(G.ours('def', cx, cy, '_data'))
    regions = {'whole': np.ones(road.shape, bool), 'road': road,
               'ground': (~road) & (cover <= 0), 'cover>0': cover > 0}
    for nm, m in regions.items():
        if m.sum() == 0:
            print('%-10s %-8s %8d  --' % (tag, nm, 0))
            out.setdefault(tag, {})[nm] = {'n': 0}
            continue
        lo, lv = G.lum(o)[m], G.lum(v)[m]
        k = G.fit_gain(lo, lv)['k']
        rr = G.resid(o[m], v[m])
        print('%-10s %-8s %8d %9.3f %9.3f %8.4f %9.3f %+9.3f'
              % (tag, nm, int(m.sum()), lo.mean(), lv.mean(), k, rr['rms'],
                 lo.mean() - lv.mean()))
        out.setdefault(tag, {})[nm] = {
            'n': int(m.sum()), 'ours': float(lo.mean()), 'van': float(lv.mean()),
            'k': k, 'rgb_rms': rr['rms'], 'lum_bias': float(lo.mean() - lv.mean())}

# ------------------------------------------------------------------ part 2
print('\n--- 2 the parabola: whole-tile RGB RMS against a constant gain k')
grid = [0.80, 0.85, 0.8755, 0.90, 0.95, 1.00, 1.05, 1.10, 1.1180, 1.15, 1.20]
sheets = {}
for cx, cy in [(-20, 24), (-20, 20)]:
    sheets['(%d,%d)' % (cx, cy)] = (G.rgb(G.ours('def', cx, cy)),
                                    G.rgb(G.van(cx, cy)))
print('%-8s ' % 'k' + ' '.join('%9s' % t for t in sheets))
for k in grid:
    print('%-8.4f ' % k + ' '.join(
        '%9.3f' % G.resid(o * k, v)['rms'] for o, v in sheets.values()))
print('k_opt   ' + ' '.join(
    '%9.4f' % G.fit_gain(G.lum(o), G.lum(v))['k'] for o, v in sheets.values()))
out['parabola'] = {'k': grid, 'rms': {t: [G.resid(o * k, v)['rms'] for k in grid]
                                      for t, (o, v) in sheets.items()}}
ko = {t: G.fit_gain(G.lum(o), G.lum(v))['k'] for t, (o, v) in sheets.items()}
out['k_opt'] = ko
print('  k_opt sits on opposite sides of 1 (%.4f and %.4f), so any k<1 raises '
      'the error on the second tile and any k>1 raises it on the first.'
      % tuple(ko.values()))

# ------------------------------------------------------------------ part 3
print('\n--- 3 the pooled-optimal gain over the census')
rows = []
seen = set()
for p in sorted(glob.glob(os.path.join(HERE, 'out', 'def', '*', 'tex',
                                       'Commonwealth.4.*.DDS'))):
    m = re.search(r'Commonwealth\.4\.(-?\d+)\.(-?\d+)\.DDS$', p)
    if not m:
        continue
    cx, cy = int(m.group(1)), int(m.group(2))
    if (cx, cy) in seen or not os.path.exists(G.van(cx, cy)):
        continue
    seen.add((cx, cy))
    rows.append((cx, cy, p))
so = sv = sov = soo = 0.0
for cx, cy, p in rows:
    o, v = G.rgb(p), G.rgb(G.van(cx, cy))
    sov += float(np.sum(o * v))
    soo += float(np.sum(o * o))
kp = sov / soo
print('  %d unique tiles, pooled k = %.4f' % (len(rows), kp))
b = a = 0.0
better = 0
per = []
for cx, cy, p in rows:
    o, v = G.rgb(p), G.rgb(G.van(cx, cy))
    r0 = G.resid(o, v)['rms']
    r1 = G.resid(o * kp, v)['rms']
    better += r1 < r0
    b += r0 * r0
    a += r1 * r1
    per.append({'tile': '(%d,%d)' % (cx, cy), 'before': r0, 'after': r1})
print('  pooled RGB RMS %.3f -> %.3f  (%.1f%%), reduced on %d of %d tiles'
      % (np.sqrt(b / len(rows)), np.sqrt(a / len(rows)),
         100.0 * (np.sqrt(a / len(rows)) / np.sqrt(b / len(rows)) - 1),
         better, len(rows)))
out['pooled'] = {'k': kp, 'n': len(rows), 'better': better,
                 'rms_before': float(np.sqrt(b / len(rows))),
                 'rms_after': float(np.sqrt(a / len(rows))), 'per': per}
for r in per:
    print('    %-10s %8.3f -> %8.3f  %s' % (r['tile'], r['before'], r['after'],
                                            'better' if r['after'] < r['before']
                                            else 'WORSE'))

with open(os.path.join(HERE, 'g5_decide.json'), 'w') as f:
    json.dump(out, f, indent=1, default=float)
print('\nwrote g5_decide.json')
