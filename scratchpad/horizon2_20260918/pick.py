#!/usr/bin/env python3
"""Pick the ten receivers FROM THE DATA: the lowest open water-facing ground,
the flattest open ground, a street between placements, and a building's foot."""
import struct
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
from wit import Land, LANE

land = Land(LANE + '/land.bin')
b = open(L + '/objfield.bin', 'rb').read()
gx0, gy0, gw, gh = struct.unpack_from('<4i', b, 4)
og = np.frombuffer(b, dtype='<f4', count=gw * gh, offset=24).reshape(gh, gw)
occ = og > -1e29

CW, CS, CE, CN = 4, -12, 7, -9
# terrain on the object lattice's own 128-unit squares (square CENTRES)
sx = (np.arange(gw) + gx0 + 0.5) * 128.0
sy = (np.arange(gh) + gy0 + 0.5) * 128.0
SX, SY = np.meshgrid(sx, sy)
ter = land.at(SX, SY)

inside = ((SX > CW * 4096) & (SX < (CE + 1) * 4096)
		  & (SY > CS * 4096) & (SY < (CN + 1) * 4096))
print('object squares inside the chunk: %d occupied of %d (%.1f%%)'
	  % (int((occ & inside).sum()), int(inside.sum()), 100.0 * (occ & inside).mean() / inside.mean()))
print('terrain inside min %.0f max %.0f' % (np.nanmin(ter[inside]), np.nanmax(ter[inside])))

# clearance of the object above the ground, where there is one
cl = np.where(occ, og - ter, np.nan)
v = cl[inside & occ]
print('object top ABOVE ground, occupied squares inside the chunk:')
for q in (1, 5, 25, 50, 75, 95, 99):
	print('   p%-3d %8.1f u' % (q, np.nanpercentile(v, q)))
print('   share over 500 u: %.1f%%  over 1000 u: %.1f%%'
	  % (100.0 * np.nanmean(v > 500), 100.0 * np.nanmean(v > 1000)))

# the low ground -- the water body, if there is one
lowmask = inside & (ter < np.nanpercentile(ter[inside], 2))
ys, xs = np.nonzero(lowmask)
print('lowest 2%% of the chunk: %d squares, terrain %.0f..%.0f, centroid (%.0f, %.0f)'
	  % (len(xs), np.nanmin(ter[lowmask]), np.nanmax(ter[lowmask]),
		 sx[xs].mean(), sy[ys].mean()))

# openness: no object within R units

