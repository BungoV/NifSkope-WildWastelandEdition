#!/usr/bin/env python3
"""THE ELIMINATION, scored against the right quantity.

variants.py scored every march rule against the third witness's MAXIMUM OVER
EACH BIN'S 22.5 DEGREE SECTOR, and found that no rule change helped. That is
because the sector maximum is itself what the shipped march computes -- it
widens its own footprint to the bin (`wantCell = binWidthFraction * d`) and
takes segment maxima, so it was being scored against its own convention.

The viewer does not read a sector maximum. `src/btdterrain.cpp` takes the sun's
azimuth, finds the two bins either side of it and BLENDS them -- which is only
meaningful if each stored byte is the skyline IN ITS OWN DIRECTION. So the
truth each stored byte should be measured against is the third witness's
skyline at the bin's own azimuth, a single pencil, and that is what this scores.
"""
import json
import math
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from wit import Land, Cast, bin_dir, true_skyline, NONE, SENT
from variants import march

A = 16
land, ground, sky, ter0 = fields.build(verbose=False)
terF = fields.terrain_only_field(ter0)
rows = json.load(open(L + '/table.json'))
S = np.array([r['STORED'] for r in rows])
B = np.load(L + '/boxes.npy')

# the DIRECTIONAL truth: one pencil per bin, at the bin's own azimuth
AZB = np.array([b * (360.0 / A) for b in range(A)])
D = []
for r in rows:
	z0 = r['gzLattice'] + 4.0
	D.append(true_skyline(land, r['x'], r['y'], z0, AZB, boxes=B,
						  skip_containing=bool(r['inBoxes'])))
D = np.array(D)
SEC = np.array([r['TRUEX'] if r['inBoxes'] else r['TRUE'] for r in rows])
print('third witness, ten receivers x 16 bins:')
print('  directional (pencil at the bin azimuth) mean %5.2f deg' % D.mean())
print('  sector maximum over the bin             mean %5.2f deg' % SEC.mean())
print('  the sector convention alone costs       mean %5.2f deg, max %5.2f'
	  % ((SEC - D).mean(), (SEC - D).max()))
print()
print('score = |march - DIRECTIONAL third witness|, degrees')
print('-' * 112)


def score(name, **kw):
	got = [march(sky, r['x'], r['y'], r['gzLattice'], **kw) for r in rows]
	G = np.array(got)
	e = np.abs(G - D)
	print('%-46s mean %6.2f  max %6.2f  >2deg %5.1f%%  bias %+6.2f  |v-shipped| %6.2f'
		  % (name, e.mean(), e.max(), 100.0 * (e > 2).mean(), (G - D).mean(), np.abs(G - S).mean()))
	return G


score('V0 shipped rule')
score('V1  mip off (wantCell 1, always level 0)', mip=False)
score('V2  tap 1x1', tap=1)
score('V3  tap 1x1 by low corner', tap=1, centre=False)
score('V4  mip off + tap 1x1', mip=False, tap=1)
score('V5  mip off + tap 1x1 + growth 1.10', mip=False, tap=1, growth=1.10)
score('V6  mip off + tap 1x1 + growth 1.02', mip=False, tap=1, growth=1.02)
score('V7  mip off + tap 1x1 + growth 1.02 + far end', mip=False, tap=1, growth=1.02, at_far=True)
score('V8  mip off + tap 1x1 + growth 1.02 + skip 0', mip=False, tap=1, growth=1.02, near_skip=0.0)
score('V9  growth 1.02 only', growth=1.02)
score('V10 mip off + growth 1.02', mip=False, growth=1.02)
print('-' * 112)
print('for reference, the SHIPPED sheet against the directional truth: mean %6.2f  max %6.2f  bias %+6.2f'
	  % (np.abs(S - D).mean(), np.abs(S - D).max(), (S - D).mean()))
