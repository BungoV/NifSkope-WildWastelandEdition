#!/usr/bin/env python3
"""Pick the ten receivers from the DATA, not from a screenshot.

Shoreline = a node at or just above sea level with open water (height <= 0)
filling most of a half-plane; flat = low local relief with no placement within
600 units; street / building's foot come from the .lodi placements."""

import sys
import numpy as np

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918')
from wit import Land, LANE

land = Land(LANE + '/land.bin')
print('land %dx%d cells from (%d,%d), nodes %dx%d' % (land.cw, land.ch, land.minX, land.minY, land.nx, land.ny))
m, w = land.edge_control()
print('CONTROL shared cell edges disagreeing by >0.5 u: %d, worst %.1f u' % (m, w))

# chunk 4.4.-12 = cells 4..7 x -12..-9
CW, CS, CE, CN = 4, -12, 7, -9
x0, y0 = CW * 4096.0, CS * 4096.0
x1, y1 = (CE + 1) * 4096.0, (CN + 1) * 4096.0
gx0 = int((x0 - land.ox) / 128.0)
gy0 = int((y0 - land.oy) / 128.0)
sub = land.h[gy0:gy0 + 129, gx0:gx0 + 129]
print('chunk world x %.0f..%.0f y %.0f..%.0f, node block %s' % (x0, x1, y0, y1, sub.shape))
print('height min %.0f max %.0f mean %.0f ; <=0 nodes %d of %d'
	  % (np.nanmin(sub), np.nanmax(sub), np.nanmean(sub), int((sub <= 0).sum()), sub.size))
for q in (0, 5, 25, 50, 75, 95, 100):
	print('  p%-3d %8.1f' % (q, np.nanpercentile(sub, q)))

# water share per node row/col, so the shoreline can be found rather than guessed
water = (sub <= 0)
print('water share by 16-node band (north at the top):')
for j in range(0, 129, 16):
	row = water[j:j + 16]
	print('   y=%8.0f  water %5.1f%%' % (y0 + (128 - j) * 128.0 if False else y0 + j * 128.0,
										 100.0 * row.mean()))
