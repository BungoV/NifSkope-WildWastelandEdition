#!/usr/bin/env python3
"""LODLEVELS pictures -- Terrain only, lifted verbatim from
scratchpad/sunsim1_20260919/scene.py so this lane never imports a folder
another lane is writing.  The Objects/Sheet classes of that file are the
.lodo/.lodi bake and are NOT wanted here: this lane builds its object set from
the STAT MNAM slot meshes itself.
"""
import struct
import numpy as np

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANDBIN = ROOT + '/scratchpad/horizon2_20260918/land.bin'

# chunk 4.4.-12 == cells x 4..7, y -12..-9
CHUNK = (4 * 4096.0, -12 * 4096.0, 8 * 4096.0, -8 * 4096.0)


class Terrain(object):
	"""LAND nodes on a global 128 u grid, plus max-Z mips for the ray march."""

	NODE = 128.0

	def __init__(self, path=LANDBIN, mips=9):
		b = np.fromfile(path, dtype=np.uint8)
		self.minX, self.minY, self.cw, self.ch = struct.unpack_from('<4i', b.tobytes()[:16], 0)
		po = 16
		present = b[po:po + self.cw * self.ch].reshape(self.ch, self.cw)
		go = po + self.cw * self.ch
		g = b[go:go + self.cw * self.ch * 33 * 33 * 2].view('<i2')
		g = g.reshape(self.ch, self.cw, 33, 33).astype(np.float32) * 8.0
		self.nx = self.cw * 32 + 1
		self.ny = self.ch * 32 + 1
		h = np.full((self.ny, self.nx), np.nan, dtype=np.float32)
		ys, xs = np.nonzero(present)
		for cy, cx in zip(ys, xs):
			h[cy * 32:cy * 32 + 33, cx * 32:cx * 32 + 33] = g[cy, cx]
		self.h = h
		self.ox = self.minX * 4096.0
		self.oy = self.minY * 4096.0
		self.hole = np.isnan(h)
		self.hfill = np.where(self.hole, -1.0e6, h).astype(np.float32)
		self.mip = [self.hfill]
		for k in range(1, mips):
			p = self.mip[-1]
			ny, nx = p.shape
			ny2, nx2 = (ny + 1) // 2, (nx + 1) // 2
			q = np.full((ny2, nx2), -1.0e6, dtype=np.float32)
			for dy in (0, 1):
				for dx in (0, 1):
					s = p[dy::2, dx::2]
					q[:s.shape[0], :s.shape[1]] = np.maximum(q[:s.shape[0], :s.shape[1]], s)
			self.mip.append(q)

	def at(self, wx, wy):
		fx = (np.asarray(wx, dtype=np.float64) - self.ox) / self.NODE
		fy = (np.asarray(wy, dtype=np.float64) - self.oy) / self.NODE
		ix = np.clip(np.floor(fx).astype(np.int64), 0, self.nx - 2)
		iy = np.clip(np.floor(fy).astype(np.int64), 0, self.ny - 2)
		tx = np.clip(fx - ix, 0.0, 1.0)
		ty = np.clip(fy - iy, 0.0, 1.0)
		h = self.h
		a = h[iy, ix] * (1 - tx) + h[iy, ix + 1] * tx
		c = h[iy + 1, ix] * (1 - tx) + h[iy + 1, ix + 1] * tx
		return a * (1 - ty) + c * ty

	def atf(self, wx, wy):
		fx = (np.asarray(wx, dtype=np.float64) - self.ox) / self.NODE
		fy = (np.asarray(wy, dtype=np.float64) - self.oy) / self.NODE
		ix = np.clip(np.floor(fx).astype(np.int64), 0, self.nx - 2)
		iy = np.clip(np.floor(fy).astype(np.int64), 0, self.ny - 2)
		tx = np.clip(fx - ix, 0.0, 1.0)
		ty = np.clip(fy - iy, 0.0, 1.0)
		h = self.hfill
		a = h[iy, ix] * (1 - tx) + h[iy, ix + 1] * tx
		c = h[iy + 1, ix] * (1 - tx) + h[iy + 1, ix + 1] * tx
		return a * (1 - ty) + c * ty

	def normal(self, wx, wy):
		e = 64.0
		hx = self.atf(wx + e, wy) - self.atf(wx - e, wy)
		hy = self.atf(wx, wy + e) - self.atf(wx, wy - e)
		nx = -hx / (2 * e)
		ny = -hy / (2 * e)
		n = np.stack([nx, ny, np.ones_like(nx)], axis=-1)
		return n / np.linalg.norm(n, axis=-1, keepdims=True)

	def mip_max(self, level, wx, wy):
		s = self.NODE * (1 << level)
		m = self.mip[level]
		ix = np.clip(((np.asarray(wx) - self.ox) / s).astype(np.int64), 0, m.shape[1] - 1)
		iy = np.clip(((np.asarray(wy) - self.oy) / s).astype(np.int64), 0, m.shape[0] - 1)
		return m[iy, ix]


class Objects(object):
	pass


class Sheet(object):
	pass


def sun_dir(az_deg, el_deg):
	a = np.radians(az_deg)
	e = np.radians(el_deg)
	return np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])
