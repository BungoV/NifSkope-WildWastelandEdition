#!/usr/bin/env python3
"""THE CONTROL the shipped C++ owes: it must implement the rule that was measured.

candidate.py cast its 576 texels with `growth` still at its default 1.5, because
the growth sweep in the same script had not yet chosen 1.3. So `cand_bins.json`
is the fixed FOOTPRINT at the old ladder, and comparing it to the shipped exe
mixes two changes. This re-casts the same 576 texels with BOTH changes -- the
footprint AND growth 1.3 -- which is exactly what the exe does, and compares.
"""
import json
import math
import sys
import time
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from candidate import cast, sheet_elev, A

land, ground, sky, _ = fields.build(verbose=False)
rec = json.load(open(L + '/lit.json'))
NEWB = json.load(open(L + '/new_bins.json'))

t0 = time.time()
P = []
for r in rec:
    P.append(cast(sky, r['x'], r['y'], ground.sample_bilinear(r['x'], r['y']), False, growth=1.3))
print('python candidate at growth 1.3, %d texels, %.0fs' % (len(P), time.time() - t0))
json.dump(P, open(L + '/cand13_bins.json', 'w'))

Pb = np.array(P)
Nb = np.array(NEWB, dtype=float) * 90.0 / 255.0
e = np.abs(Pb - Nb)
print('all %d texels x %d bins: |C++ - python| mean %.3f  max %.3f  >1 step %.1f%%  (step %.3f deg)'
      % (Pb.shape[0], A, e.mean(), e.max(), 100.0 * (e > 90.0 / 255.0).mean(), 90.0 / 255.0))
for az in (120.0, 240.0):
    a = np.array([sheet_elev(p, az) for p in P])
    b = np.array([sheet_elev(list(n), az) * 90.0 / 255.0 for n in NEWB])
    print('  azimuth %3.0f: python mean %.2f  C++ mean %.2f  |diff| mean %.3f max %.3f'
          % (az, a.mean(), b.mean(), np.abs(a - b).mean(), np.abs(a - b).max()))
