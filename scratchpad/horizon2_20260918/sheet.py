#!/usr/bin/env python3
"""Read the role-7 horizon bins the bake WROTE, at a world position.

Nothing here computes a horizon: it reads the bytes out of the container the
same way `LodtSheets` does, so what the report calls "the sheet's stored
elevation" is the byte on disk and not a second opinion about it."""

import os
import sys

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from lodgen_vt_check import Lodv

ROLE_HORIZON = 7
BINS_PER_SHEET = 4
SHIFT = (0, 8, 16, 24)          # the FILE is RGBA byte order: bin j is byte j.
#  The bake packs a Qt 0xAARRGGBB u32 (bin 0 at bits 16..23) and the sheet
#  writer swizzles it to R8G8B8A8 on the way out, so bin 0 lands in byte 0.
#  Reading the file's u32 with the C++ shifts swaps bins 0 and 2 of every four;
#  the in-bake refuter cannot see that, because it reads its bytes back out of the in-memory planes, never out of the file.


class HorizonSheet(object):
	def __init__(self, path):
		self.v = Lodv(path)
		v = self.v
		self.hz = [s for s in range(v.sheetCount) if v.sheets[s]['role'] == ROLE_HORIZON]
		if not self.hz:
			raise RuntimeError('%s carries no role-7 sheet' % path)
		# role 7 is last and contiguous -- checked, not assumed
		if self.hz != list(range(v.sheetCount - len(self.hz), v.sheetCount)):
			raise RuntimeError('role-7 sheets %r are not the last %d of %d'
							   % (self.hz, len(self.hz), v.sheetCount))
		self.azimuths = len(self.hz) * BINS_PER_SHEET
		self.upt = v.levelDim * 4096.0 / v.content

	def tile_of_cell(self, cx, cy):
		v = self.v
		if cx < v.west or cx > v.east or cy < v.south or cy > v.north:
			return None
		tx = (cx - v.west) // v.levelDim
		ty = (v.north - cy) // v.levelDim
		if tx < 0 or tx >= v.tilesX or ty < 0 or ty >= v.tilesY:
			return None
		return int(tx), int(ty)

	def bins_at(self, wx, wy):
		"""The `azimuths` stored bytes under a world position, or None."""
		v = self.v
		cx, cy = int(wx // 4096), int(wy // 4096)
		t = self.tile_of_cell(cx, cy)
		if t is None:
			return None
		tx, ty = t
		idx = ty * v.tilesX + tx
		p = v.payload(idx)
		if p is None:
			return None
		cellX0 = v.west + tx * v.levelDim
		cellY0 = v.north - (ty + 1) * v.levelDim + 1
		tileW = cellX0 * 4096.0
		tileN = (cellY0 + v.levelDim) * 4096.0
		i = int(round((wx - tileW) / self.upt + v.border - 0.5))
		j = int(round((tileN - wy) / self.upt + v.border - 0.5))
		S = v.stored
		if i < 0 or j < 0 or i >= S or j >= S:
			return None
		cover = bool(v.table[idx]['flags'] & 2)
		out = []
		for b in range(self.azimuths):
			s = self.hz[b // BINS_PER_SHEET]
			o = v.sheetOffset(cover, s, 0) + (j * S + i) * 4
			px = int.from_bytes(p[o:o + 4], 'little')
			out.append((px >> SHIFT[b % BINS_PER_SHEET]) & 0xFF)
		# the texel's OWN world centre -- the point the bake cast from, which is
		# not the point asked for: upt is 32 u here, so a receiver named by
		# world position sits up to 16 u from the sample that was stored.
		cwx = tileW + (float(i) - v.border + 0.5) * self.upt
		cwy = tileN - (float(j) - v.border + 0.5) * self.upt
		return out, (i, j, tx, ty), (cwx, cwy)
