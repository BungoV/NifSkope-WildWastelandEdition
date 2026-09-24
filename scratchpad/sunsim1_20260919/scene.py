#!/usr/bin/env python3
"""SUNSIM1 -- the scene, loaded once from the raw bake and cached.

Everything here is READ-ONLY against the tree.  Three things come out:

  * `Terrain`  -- the worldspace LAND nodes as one 128-unit grid plus a max-Z
    mip pyramid, from scratchpad/horizon2_20260918/land.bin (the `--dump-land`
    blob).  The reader is HORIZON2's `wit.Land`, re-implemented here only so
    this lane has no import into a folder another lane may still be writing.
  * `Objects`  -- every .lodi placement's .lodo level-0 triangles carried to
    WORLD space, with per-vertex normals and per-vertex 16-bin horizon bytes.
  * `Sheet`    -- the role-7 terrain horizon sheet of Commonwealth.VT.4.lodt.

No trimesh, no embree, no GPU.  numpy only.
"""
import os
import struct
import sys
import numpy as np

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/sunsim1_20260919'
H2 = ROOT + '/scratchpad/horizon2_20260918'
BAKE = H2 + '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth'
LODT = H2 + '/dumpbake/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt'
LANDBIN = H2 + '/land.bin'

sys.path.insert(0, ROOT + '/tests/spells')

# chunk 4.4.-12 == cells x 4..7, y -12..-9
CHUNK = (4 * 4096.0, -12 * 4096.0, 8 * 4096.0, -8 * 4096.0)
DEG = 180.0 / np.pi


# ------------------------------------------------------------------ terrain
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
		# max-Z pyramid: mip[k] has node spacing 128 * 2^k, value = max of the block
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
		"""Bilinear over the true node grid; NaN where there is no LAND."""
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
		"""Same, over the hole-filled copy: never NaN, holes read -1e6."""
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
		"""Analytic normal of the bilinear surface, Z up."""
		e = 64.0
		hx = self.atf(wx + e, wy) - self.atf(wx - e, wy)
		hy = self.atf(wx, wy + e) - self.atf(wx, wy - e)
		nx = -hx / (2 * e)
		ny = -hy / (2 * e)
		n = np.stack([nx, ny, np.ones_like(nx)], axis=-1)
		return n / np.linalg.norm(n, axis=-1, keepdims=True)

	def mip_max(self, level, wx, wy):
		"""Max terrain height over the mip texel containing (wx, wy)."""
		s = self.NODE * (1 << level)
		m = self.mip[level]
		ix = np.clip(((np.asarray(wx) - self.ox) / s).astype(np.int64), 0, m.shape[1] - 1)
		iy = np.clip(((np.asarray(wy) - self.oy) / s).astype(np.int64), 0, m.shape[0] - 1)
		return m[iy, ix]


# ------------------------------------------------------------------ objects
def unpack_oct12(b0, b1, b2):
	"""`.lodo` v4 vertex normals are OCTAHEDRAL 12:12, not a signed byte triple:
	n = b0 | b1<<8 | b2<<16, octX = n & 0xFFF, octY = n >> 12 (src/lodofile.h
	line 222, codec lodoUnpackOct12 in src/lodofile.cpp).  Reading them as three
	signed bytes gave 66% of this chunk's vertices a normal pointing at or below
	the horizontal, which is not what a town looks like -- caught by control B,
	whose "face pointing down stores nothing" class did not match the file."""
	p = (np.asarray(b0, dtype=np.uint32)
		 | (np.asarray(b1, dtype=np.uint32) << 8)
		 | (np.asarray(b2, dtype=np.uint32) << 16))
	u = (p & 0xFFF).astype(np.float64) / 4095.0 * 2.0 - 1.0
	v = ((p >> 12) & 0xFFF).astype(np.float64) / 4095.0 * 2.0 - 1.0
	z = 1.0 - np.abs(u) - np.abs(v)
	m = z < 0.0
	if m.any():
		u2 = (1.0 - np.abs(v)) * np.where(u >= 0.0, 1.0, -1.0)
		v2 = (1.0 - np.abs(u)) * np.where(v >= 0.0, 1.0, -1.0)
		u = np.where(m, u2, u)
		v = np.where(m, v2, v)
	n = np.stack([u, v, z], axis=1)
	return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


class Objects(object):
	"""Every placement's level-0 triangles in world space, plus the bins.

	v : (V,3)   world vertex positions
	n : (V,3)   world vertex normals (unit; face normal where the byte triple
	            decodes to something shorter than half a unit)
	bins:(V,16) the per-vertex horizon bytes, bin 0 = north, clockwise
	tri : (T,3) vertex indices
	"""

	def __init__(self, cache=LANE + '/scene_objects.npz', verbose=True):
		if cache and os.path.exists(cache):
			z = np.load(cache)
			self.v, self.n, self.bins, self.tri = z['v'], z['n'], z['bins'], z['tri']
			self.inst = z['inst']
			if verbose:
				print('objects: cache %d verts %d tris' % (len(self.v), len(self.tri)))
			return
		import lodgen_native_decode as ND
		L = ND.read_lodo(BAKE + '.lodo')
		T = ND.read_lodi(BAKE + '.lodi')
		h = T['header']
		A = h['horizonAzimuths']
		assert A == 16, A
		lv = np.array([(x['px'], x['py'], x['pz'], x['n0'], x['n1'], x['n2'])
					   for x in L['vertices']], dtype=np.float64)
		# each mesh's own vertex range and its level-0 triangle list (mesh-local)
		meshRange, meshTris = {}, {}
		for mi, m in enumerate(L['meshes']):
			cs = L['clusters'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
			ls = L['clusterLods'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
			if not cs:
				continue
			vb = min(c['vertexBase'] for c in cs)
			ve = max(c['vertexBase'] + c['vertexCount'] for c in cs)
			meshRange[mi] = (vb, ve)
			tl = []
			for ci, (c, l) in enumerate(zip(cs, ls)):
				if l['level'] != 0:
					continue
				gi = m['clusterFirst'] + ci
				li = L['localIndices'][gi * 48:(gi + 1) * 48]
				off = c['vertexBase'] - vb
				for t in range(c['triangleCount']):
					tl.append((li[t * 3] + off, li[t * 3 + 1] + off, li[t * 3 + 2] + off))
			meshTris[mi] = np.array(tl, dtype=np.int64) if tl else np.zeros((0, 3), np.int64)
		hor = np.frombuffer(bytes(T['vertexHorizon']), dtype=np.uint8)
		hf = T['vertexHorizonFirst']
		V, N, B, TR, INST = [], [], [], [], []
		base = 0
		for i, r in enumerate(T['instances']):
			bse = L['bases'][r['baseId']]
			mid = bse['rep0']
			if mid == 0xFFFF or mid not in meshRange:
				continue
			vb, ve = meshRange[mid]
			nv = ve - vb
			hs, he = hf[i], hf[i + 1]
			if he - hs != nv * A:
				continue
			m = L['meshes'][mid]
			q = lv[vb:ve]
			lo = np.array(m['aabbMin'], dtype=np.float64)
			ex = np.array(m['aabbExtent'], dtype=np.float64)
			p = lo + q[:, 0:3] / 65535.0 * ex
			nn = unpack_oct12(q[:, 3], q[:, 4], q[:, 5])
			M = np.array(r['m'], dtype=np.float64).reshape(3, 3)
			s = r['scaleF']
			w = (M @ p.T).T * s + np.array([r['x'], r['y'], r['z']])
			wn = (M @ nn.T).T
			V.append(w)
			N.append(wn)
			B.append(hor[hs:he].reshape(nv, A))
			TR.append(meshTris[mid] + base)
			INST.append(np.full(nv, i, dtype=np.int32))
			base += nv
		self.v = np.concatenate(V).astype(np.float32)
		self.n = np.concatenate(N).astype(np.float32)
		self.bins = np.concatenate(B).astype(np.uint8)
		self.tri = np.concatenate(TR).astype(np.int32)
		self.inst = np.concatenate(INST)
		# face normal where the stored triple is degenerate
		ln = np.linalg.norm(self.n, axis=1)
		bad = ln < 0.5
		a, b, c = self.v[self.tri[:, 0]], self.v[self.tri[:, 1]], self.v[self.tri[:, 2]]
		fn = np.cross(b - a, c - a)
		fl = np.linalg.norm(fn, axis=1, keepdims=True)
		fn = fn / np.where(fl == 0, 1, fl)
		for k in range(3):
			idx = self.tri[:, k]
			m = bad[idx]
			self.n[idx[m]] = fn[m]
		ln = np.maximum(np.linalg.norm(self.n, axis=1, keepdims=True), 1e-9)
		self.n = (self.n / ln).astype(np.float32)
		if cache:
			np.savez_compressed(cache, v=self.v, n=self.n, bins=self.bins,
								tri=self.tri, inst=self.inst)
		if verbose:
			print('objects: %d placements -> %d verts %d tris, world z %.0f..%.0f'
				  % (len(T['instances']), len(self.v), len(self.tri),
					 self.v[:, 2].min(), self.v[:, 2].max()))


# -------------------------------------------------------------------- sheet
class Sheet(object):
	"""The role-7 terrain horizon sheet, as one (H, W, 16) byte plane in world
	space, so a per-pixel bilinear tap is one gather instead of a container
	walk.  Built through HORIZON2's own `HorizonSheet` reader, texel by texel
	once, then cached."""

	def __init__(self, cache=LANE + '/scene_sheet.npz', verbose=True):
		if cache and os.path.exists(cache):
			z = np.load(cache)
			self.plane, self.ox, self.oy, self.upt = z['plane'], float(z['ox']), float(z['oy']), float(z['upt'])
			if verbose:
				print('sheet: cache %s upt %.0f' % (self.plane.shape, self.upt))
			return
		from lodgen_horizon_witness import HorizonSheet
		s = HorizonSheet(LODT)
		v = s.v
		self.upt = s.upt
		x0, y0, x1, y1 = CHUNK
		nx = int((x1 - x0) / self.upt)
		ny = int((y1 - y0) / self.upt)
		self.ox, self.oy = x0, y0
		plane = np.zeros((ny, nx, 16), dtype=np.uint8)
		# read the tile payload once and index it directly (bins_at() per texel
		# would re-decompress the payload 262,144 times)
		idx = 0
		p = v.payload(idx)
		cover = bool(v.table[idx]['flags'] & 2)
		S = v.stored
		buf = np.frombuffer(p, dtype=np.uint8)
		tileW = (v.west + 0 * v.levelDim) * 4096.0
		tileN = (v.north - (0 + 1) * v.levelDim + 1 + v.levelDim) * 4096.0
		for b in range(16):
			o = v.sheetOffset(cover, s.hz[b // 4], 0)
			img = buf[o:o + S * S * 4].reshape(S, S, 4)[:, :, b % 4]
			# world -> texel: i = (wx - tileW)/upt + border - 0.5
			wx = self.ox + (np.arange(nx) + 0.5) * self.upt
			wy = self.oy + (np.arange(ny) + 0.5) * self.upt
			ii = np.round((wx - tileW) / self.upt + v.border - 0.5).astype(int)
			jj = np.round((tileN - wy) / self.upt + v.border - 0.5).astype(int)
			ii = np.clip(ii, 0, S - 1)
			jj = np.clip(jj, 0, S - 1)
			plane[:, :, b] = img[jj[:, None], ii[None, :]]
		self.plane = plane
		if cache:
			np.savez_compressed(cache, plane=plane, ox=self.ox, oy=self.oy, upt=self.upt)
		if verbose:
			print('sheet: %s upt %.0f, byte mean %.1f (%.2f deg)'
				  % (plane.shape, self.upt, plane.mean(), plane.mean() / 255.0 * 90.0))

	def elev_at(self, wx, wy, az_deg):
		"""Bilinear in space over the two azimuth bins the sun falls between,
		then linear between the two bins -- `lodgenHorizonBinPair` exactly.
		Returns degrees, and NaN outside the sheet."""
		A = 16
		step = 360.0 / A
		t = az_deg / step
		t -= np.floor(t / A) * A
		k0 = int(t) % A
		k1 = (k0 + 1) % A
		f = t - np.floor(t)
		fx = (np.asarray(wx, dtype=np.float64) - self.ox) / self.upt - 0.5
		fy = (np.asarray(wy, dtype=np.float64) - self.oy) / self.upt - 0.5
		H, W = self.plane.shape[0], self.plane.shape[1]
		inside = (fx >= -0.5) & (fx <= W - 0.5) & (fy >= -0.5) & (fy <= H - 0.5)
		ix = np.clip(np.floor(fx).astype(np.int64), 0, W - 2)
		iy = np.clip(np.floor(fy).astype(np.int64), 0, H - 2)
		tx = np.clip(fx - ix, 0.0, 1.0)
		ty = np.clip(fy - iy, 0.0, 1.0)
		out = 0.0
		for k, wgt in ((k0, 1.0 - f), (k1, f)):
			pl = self.plane[:, :, k]
			a = pl[iy, ix] * (1 - tx) + pl[iy, ix + 1] * tx
			c = pl[iy + 1, ix] * (1 - tx) + pl[iy + 1, ix + 1] * tx
			out = out + wgt * (a * (1 - ty) + c * ty)
		deg = out / 255.0 * 90.0
		return np.where(inside, deg, np.nan)


def bin_pair(az_deg, A=16):
	step = 360.0 / A
	t = az_deg / step
	t -= np.floor(t / A) * A
	k0 = int(t) % A
	return k0, (k0 + 1) % A, t - np.floor(t)


def sun_dir(az_deg, el_deg):
	"""Unit vector TOWARD the sun.  dx = sin(az), dy = cos(az), Z up."""
	a = np.radians(az_deg)
	e = np.radians(el_deg)
	return np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])


if __name__ == '__main__':
	import time
	t = time.time(); ter = Terrain(); print('terrain %.1fs %s' % (time.time() - t, ter.h.shape))
	t = time.time(); ob = Objects(); print('objects %.1fs' % (time.time() - t))
	t = time.time(); sh = Sheet(); print('sheet %.1fs' % (time.time() - t))
	x0, y0, x1, y1 = CHUNK
	gx = np.linspace(x0, x1, 9); gy = np.linspace(y0, y1, 9)
	X, Y = np.meshgrid(gx, gy)
	print('chunk terrain z %.0f..%.0f' % (np.nanmin(ter.at(X, Y)), np.nanmax(ter.at(X, Y))))
	print('sheet elev at chunk centre, az 120: %.2f deg'
		  % sh.elev_at(np.array([(x0 + x1) / 2]), np.array([(y0 + y1) / 2]), 120.0)[0])
