#!/usr/bin/env python3
"""The bake's own two lattices, rebuilt in Python exactly as
`LodgenVtHorizon::build()` builds them: `ground` from the LAND nodes only,
`sky` from the LAND nodes and then raised by the object height field's top over
the same 128 u squares. Cached to fields.npz so every later probe reads the
same bytes."""
import os
import struct
import sys
import time
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
from wit import Land, Field, LANE, NONE

CW, CS, CE, CN = 4, -12, 7, -9
GROW = 33
LX0, LX1 = max(-96, CW - GROW), min(95, CE + GROW)
LY0, LY1 = max(-96, CS - GROW), min(95, CN + GROW)
X0, Y0 = LX0 * 4096.0, LY0 * 4096.0
X1, Y1 = (LX1 + 1) * 4096.0, (LY1 + 1) * 4096.0
CACHE = L + '/fields.npz'


def build(verbose=True):
	land = Land(LANE + '/land.bin')
	sky = Field(X0, Y0, X1, Y1, 128.0)
	ground = Field(X0, Y0, X1, Y1, 128.0)
	gx0 = int((X0 - land.ox) / 128.0)
	gy0 = int((Y0 - land.oy) / 128.0)
	nx = (LX1 - LX0 + 1) * 32 + 1
	ny = (LY1 - LY0 + 1) * 32 + 1
	blk = land.h[gy0:gy0 + ny, gx0:gx0 + nx]
	wx = X0 + np.arange(nx) * 128.0
	wy = Y0 + np.arange(ny) * 128.0
	WX, WY = np.meshgrid(wx, wy)
	sky.raise_nodes(WX.ravel(), WY.ravel(), blk.ravel())
	ground.raise_nodes(WX.ravel(), WY.ravel(), blk.ravel())
	ground.build_mips()
	terrain_only = sky.mip[0].copy()

	# the object half: `of.topAt(wx, wy)` over objectMarginCells, raised into sky
	b = open(L + '/objfield.bin', 'rb').read()
	ogx0, ogy0, ogw, ogh = struct.unpack_from('<4i', b, 4)
	ocell = struct.unpack_from('<f', b, 20)[0]
	og = np.frombuffer(b, dtype='<f4', count=ogw * ogh, offset=24).reshape(ogh, ogw)
	ox = (np.arange(ogw) + ogx0 + 0.5) * ocell
	oy = (np.arange(ogh) + ogy0 + 0.5) * ocell
	OX, OY = np.meshgrid(ox, oy)
	occ = og > -1.0e29
	sky.raise_nodes(OX[occ], OY[occ], og[occ].astype(np.float64))
	sky.build_mips()
	if verbose:
		print('lattice %d x %d squares of 128 u, %d mips; object squares raised %d'
			  % (sky.w, sky.h, len(sky.mip), int(occ.sum())))
	return land, ground, sky, terrain_only


def terrain_only_field(terrain_only):
	f = Field(X0, Y0, X1, Y1, 128.0)
	f.mip[0] = terrain_only.copy()
	f.build_mips()
	return f


if __name__ == '__main__':
	t = time.time()
	land, ground, sky, ter = build()
	print('%.1fs' % (time.time() - t))
