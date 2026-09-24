#!/usr/bin/env python3
"""First probe: does the PORT reproduce the shipped bytes, and what does the
third witness say at the same point?  Three receivers, terrain only."""

import sys
import time
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
from wit import Land, Field, Cast, cast_at, true_skyline, reference_elev, LANE
from sheet import HorizonSheet

land = Land(LANE + '/land.bin')

# the bake's own lattice rectangle: cw..ce grown by `grow`, clipped to the
# worldspace. grow = min(64, ceil(reach/4096)+1) = min(64, 33) = 33, which is
# the only value that gives the census's horizonLandCells 4900 = 70x70.
CW, CS, CE, CN = 4, -12, 7, -9
GROW = 33
lx0, lx1 = max(-96, CW - GROW), min(95, CE + GROW)
ly0, ly1 = max(-96, CS - GROW), min(95, CN + GROW)
print('lattice cells x %d..%d y %d..%d = %d cells (census says 4900)'
	  % (lx0, lx1, ly0, ly1, (lx1 - lx0 + 1) * (ly1 - ly0 + 1)))

X0, Y0 = lx0 * 4096.0, ly0 * 4096.0
X1, Y1 = (lx1 + 1) * 4096.0, (ly1 + 1) * 4096.0
t = time.time()
sky = Field(X0, Y0, X1, Y1, 128.0)
ground = Field(X0, Y0, X1, Y1, 128.0)
# every LAND node of every cell in the rectangle, exactly as build() does
gx0 = int((X0 - land.ox) / 128.0)
gy0 = int((Y0 - land.oy) / 128.0)
nx = (lx1 - lx0 + 1) * 32 + 1
ny = (ly1 - ly0 + 1) * 32 + 1
blk = land.h[gy0:gy0 + ny, gx0:gx0 + nx]
wx = X0 + np.arange(nx) * 128.0
wy = Y0 + np.arange(ny) * 128.0
WX, WY = np.meshgrid(wx, wy)
sky.raise_nodes(WX.ravel(), WY.ravel(), blk.ravel())
ground.raise_nodes(WX.ravel(), WY.ravel(), blk.ravel())
sky.build_mips()
print('lattice %dx%d squares, %d mips, %.1fs' % (sky.w, sky.h, len(sky.mip), time.time() - t))

hs = HorizonSheet(LANE.replace('horizon2_20260918', 'horizon1_20260918')
				  + '/v8/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt')
print('sheet: levelDim %d content %d border %d stored %d tiles %dx%d bins %d upt %.1f'
	  % (hs.v.levelDim, hs.v.content, hs.v.border, hs.v.stored, hs.v.tilesX, hs.v.tilesY,
		 hs.azimuths, hs.upt))

k = Cast()
AZ = np.arange(0, 360, 1.0)

for (px, py) in [(24900.0, -41300.0), (20000.0, -45000.0), (30000.0, -35000.0)]:
	gz = ground.sample_bilinear(px, py)
	tru = true_skyline(land, px, py, gz + k.rise, AZ)
	# the truth in the stored bins' own terms: the max over each 22.5 deg sector
	sect = []
	for b in range(16):
		c = b * 22.5
		lo, hi = c - 11.25, c + 11.25
		sel = ((AZ - lo) % 360 <= (hi - lo) % 360) if lo < hi else None
		idx = [i for i, a in enumerate(AZ) if min((a - c) % 360, (c - a) % 360) <= 11.25]
		sect.append(tru[idx].max())
	port = cast_at(k, None, sky, px, py, gz, out_deg=True)
	got = hs.bins_at(px, py)
	stored = [round(v * 90.0 / 255.0, 2) for v in got[0]] if got else None
	ref = [round(reference_elev(k, None, sky, px, py, gz, b * 22.5), 2) for b in range(0, 16, 4)]
	print('\n--- receiver (%.0f, %.0f)  ground %.1f  terrain-at %.1f' % (px, py, gz, land.at(px, py)))
	print('  TRUE sector max  %s' % ' '.join('%5.1f' % v for v in sect))
	print('  PORT (terrain)   %s' % ' '.join('%5.1f' % v for v in port))
	print('  STORED sheet     %s' % (' '.join('%5.1f' % v for v in stored) if stored else 'none'))
	print('  REF cone b0,4,8,12 %s' % ref)
	print('  texel %s' % (got[1] if got else '-'))
