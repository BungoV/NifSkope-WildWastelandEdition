#!/usr/bin/env python3
"""Lane HORIZON2 -- the THIRD WITNESS and the bake model, in one module.

Three casters over the SAME raw inputs, so a disagreement is about the rule and
never about the data:

  * `true_skyline`  -- the third witness. Pencil rays over the RAW 128-unit LAND
    node grid (`--dump-land`, int16 units of 8) plus the placements' world
    boxes. No lattice, no mip, no `maxAlong`, no 2x2 tap. It is what a receiver
    actually sees.
  * `Field` + `cast_at` -- a PORT of src/lodghorizon.h: the same max-Z lattice,
    the same mip chain, the same `maxAlong` 2x2 tap, the same near-end rule. It
    exists so a candidate can be tested without a four-minute build, and it is
    only trustworthy once it reproduces the bytes the shipped bake wrote (the
    `--verify` gate).
  * `reference_elev` -- a PORT of src/lodghorizonrefute.h's cone reference, the
    witness HORIZON1 scored G3 against.

Every world unit here is a WORLD unit (root MISTAKES 2026-09-18 05:0x).
"""

import math
import os
import struct
import sys

import numpy as np

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/horizon2_20260918'

# ---------------------------------------------------------------- raw terrain

class Land(object):
	"""The worldspace's LAND nodes as ONE global grid at 128 units.

	`--dump-land` writes int32 minX, minY, cellsX, cellsY; then one uint8
	presence flag a cell (row-major from the south-west); then 33*33 int16
	heights a cell in units of 8, row 0 south, column 0 west. Cell (cx,cy)'s
	node (row r, col c) is the world position (cx*4096 + c*128, cy*4096 +
	r*128), and column 32 of (cx,cy) is column 0 of (cx+1,cy) -- the same world
	position stored twice -- so the two must agree when they land in one grid.
	That agreement is the loader's own control (`edge_control`)."""

	NODE = 128.0

	def __init__(self, path):
		b = open(path, 'rb').read()
		self.minX, self.minY, self.cw, self.ch = struct.unpack_from('<4i', b, 0)
		po = 16
		present = np.frombuffer(b, dtype=np.uint8, count=self.cw * self.ch, offset=po)
		go = po + self.cw * self.ch
		g = np.frombuffer(b, dtype='<i2', count=self.cw * self.ch * 33 * 33, offset=go)
		g = g.reshape(self.ch, self.cw, 33, 33)          # [cy][cx][row][col]
		self.present = present.reshape(self.ch, self.cw)
		self.nx = self.cw * 32 + 1
		self.ny = self.ch * 32 + 1
		# NONE is a sentinel, not a height: a cell with no LAND is a hole.
		self.h = np.full((self.ny, self.nx), np.nan, dtype=np.float32)
		self.mism = 0
		self.mismWorst = 0.0
		for cy in range(self.ch):
			for cx in range(self.cw):
				if not self.present[cy, cx]:
					continue
				blk = g[cy, cx].astype(np.float32) * 8.0
				y0, x0 = cy * 32, cx * 32
				old = self.h[y0:y0 + 33, x0:x0 + 33]
				m = ~np.isnan(old)
				if m.any():
					d = np.abs(old[m] - blk[m])
					self.mism += int((d > 0.5).sum())
					if d.size:
						self.mismWorst = max(self.mismWorst, float(d.max()))
				self.h[y0:y0 + 33, x0:x0 + 33] = blk
		self.ox = self.minX * 4096.0
		self.oy = self.minY * 4096.0

	def at(self, wx, wy):
		"""Bilinear over the TRUE node grid. NaN where there is no LAND."""
		fx = (np.asarray(wx, dtype=np.float64) - self.ox) / self.NODE
		fy = (np.asarray(wy, dtype=np.float64) - self.oy) / self.NODE
		ix = np.clip(np.floor(fx).astype(np.int64), 0, self.nx - 2)
		iy = np.clip(np.floor(fy).astype(np.int64), 0, self.ny - 2)
		tx = np.clip(fx - ix, 0.0, 1.0)
		ty = np.clip(fy - iy, 0.0, 1.0)
		h00 = self.h[iy, ix]
		h10 = self.h[iy, ix + 1]
		h01 = self.h[iy + 1, ix]
		h11 = self.h[iy + 1, ix + 1]
		return ((h00 * (1 - tx) + h10 * tx) * (1 - ty)
				+ (h01 * (1 - tx) + h11 * tx) * ty)

	def edge_control(self):
		"""The loader's own control: shared cell edges are stored twice."""
		return self.mism, self.mismWorst


# -------------------------------------------------- the third witness, raw

def true_skyline(land, px, py, z0, az_deg, reach=127561.0, step=32.0, boxes=None,
				 rise_from=32.0, skip_containing=False):
	"""The TRUE skyline elevation, in degrees, at each azimuth in `az_deg`.

	A pencil ray a azimuth, terrain read bilinear at its own position -- no
	lattice, no mip, no tap, no segment maximum. `boxes` is an (N,5) array of
	world-space placement boxes (x0,y0,x1,y1,top); a box contributes at the
	distance of its NEAREST point along the ray, which is the honest steepest
	angle it can subtend, and never at a shorter one.

	Returns degrees above the horizontal, 0 when nothing stands above z0."""
	az = np.asarray(az_deg, dtype=np.float64)
	a = np.radians(az)
	dx, dy = np.sin(a), np.cos(a)                 # bin 0 = +Y, clockwise to +X
	d = np.arange(rise_from, reach + step, step, dtype=np.float64)
	X = px + np.outer(dx, d)
	Y = py + np.outer(dy, d)
	Z = land.at(X, Y)
	e = np.degrees(np.arctan2(Z - z0, d[None, :]))
	e = np.where(np.isnan(Z), -90.0, e)
	best = e.max(axis=1)
	if boxes is not None and len(boxes):
		for k in range(len(az)):
			be = _box_elev(boxes, px, py, z0, dx[k], dy[k], reach, skip_containing)
			if be > best[k]:
				best[k] = be
	return np.maximum(best, 0.0)


def _box_elev(boxes, px, py, z0, dx, dy, reach, skip_containing=False):
	"""Steepest elevation any box in the ray's path subtends at its own near
	distance. A slab test on the XY box, exact, with no lattice anywhere."""
	x0, y0, x1, y1, top = (boxes[:, 0], boxes[:, 1], boxes[:, 2], boxes[:, 3], boxes[:, 4])
	inf = 1.0e30
	with np.errstate(divide='ignore', invalid='ignore'):
		# A zero component means the ray never leaves that slab: inside it the
		# pair must be (-inf, +inf), and OUTSIDE it must make the whole test
		# fail -- (+inf, +inf), so tnear is +inf and no box is ever hit. Writing
		# (+inf, -inf) there instead, which is the shape the other branch has,
		# makes min() -inf and max() +inf and every box in the world a hit; that
		# is what put 90.0 in bin 0 of all ten receivers on the first run.
		inx = (px >= x0) & (px <= x1)
		iny = (py >= y0) & (py <= y1)
		tx0 = (x0 - px) / dx if dx != 0 else np.where(inx, -inf, inf)
		tx1 = (x1 - px) / dx if dx != 0 else np.where(inx, inf, inf)
		ty0 = (y0 - py) / dy if dy != 0 else np.where(iny, -inf, inf)
		ty1 = (y1 - py) / dy if dy != 0 else np.where(iny, inf, inf)
	tnear = np.maximum(np.minimum(tx0, tx1), np.minimum(ty0, ty1))
	tfar = np.minimum(np.maximum(tx0, tx1), np.maximum(ty0, ty1))
	hit = (tfar >= np.maximum(tnear, 0.0)) & (tnear <= reach) & (top > z0)
	if not hit.any():
		return 0.0
	t = np.maximum(tnear[hit], 1.0)
	e = float(np.degrees(np.arctan2(top[hit] - z0, t)).max())
	if skip_containing:
		# a receiver standing inside a box's XY footprint has tnear <= 0 and
		# would read ~90 deg in EVERY bin. That is what an AABB says, not what
		# the mesh says, so it is reported apart from the rest.
		out = hit & (tnear > 0.0)
		e = float(np.degrees(np.arctan2(top[out] - z0, np.maximum(tnear[out], 1.0))).max()) if out.any() else 0.0
	return e


# ------------------------------------------- the bake's lattice, ported

NONE = -1.0e30
SENT = -1.0e29


class Field(object):
	"""A port of `LodgenHorizonField` -- max-Z squares plus a max mip chain."""

	def __init__(self, x0, y0, x1, y1, cell):
		self.cell = cell
		self.ox = math.floor(x0 / cell) * cell
		self.oy = math.floor(y0 / cell) * cell
		hx = math.ceil(x1 / cell) * cell
		hy = math.ceil(y1 / cell) * cell
		self.w = max(1, int((hx - self.ox) / cell))
		self.h = max(1, int((hy - self.oy) / cell))
		self.mip = [np.full((self.h, self.w), NONE, dtype=np.float32)]

	def raise_nodes(self, wx, wy, z):
		gx = np.floor((wx - self.ox) / self.cell).astype(np.int64)
		gy = np.floor((wy - self.oy) / self.cell).astype(np.int64)
		ok = (gx >= 0) & (gy >= 0) & (gx < self.w) & (gy < self.h) & ~np.isnan(z)
		np.maximum.at(self.mip[0], (gy[ok], gx[ok]), z[ok].astype(np.float32))

	def build_mips(self):
		del self.mip[1:]
		while self.mip[-1].shape[0] > 1 or self.mip[-1].shape[1] > 1:
			s = self.mip[-1]
			ph, pw = s.shape
			nh, nw = max(1, (ph + 1) // 2), max(1, (pw + 1) // 2)
			pad = np.full((nh * 2, nw * 2), NONE, dtype=np.float32)
			pad[:ph, :pw] = s
			self.mip.append(pad.reshape(nh, 2, nw, 2).max(axis=(1, 3)))

	def at(self, level, gx, gy):
		if level < 0 or level >= len(self.mip):
			return NONE
		m = self.mip[level]
		if gx < 0 or gy < 0 or gy >= m.shape[0] or gx >= m.shape[1]:
			return NONE
		return float(m[gy, gx])

	def max_along(self, x0, y0, x1, y1, want_cell):
		level, c = 0, self.cell
		while level + 1 < len(self.mip) and c * 4.0 <= want_cell:
			level += 1
			c *= 2.0
		dx, dy = x1 - x0, y1 - y0
		ln = math.hypot(dx, dy)
		taps = min(64, int(ln / c))
		m = NONE
		for t in range(taps + 1):
			f = (t / taps) if taps else 0.0
			px, py = x0 + dx * f, y0 + dy * f
			gx = int(math.floor((px - self.ox) / c - 0.5))
			gy = int(math.floor((py - self.oy) / c - 0.5))
			for j in range(2):
				for i in range(2):
					v = self.at(level, gx + i, gy + j)
					if v > m:
						m = v
		return m

	def sample_bilinear(self, wx, wy):
		fx = (wx - self.ox) / self.cell - 0.5
		fy = (wy - self.oy) / self.cell - 0.5
		gx, gy = int(math.floor(fx)), int(math.floor(fy))
		tx, ty = fx - gx, fy - gy
		wgt = ((1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty)
		acc = wsum = 0.0
		for i in range(4):
			v = self.at(0, gx + (i & 1), gy + (i >> 1))
			if v > SENT:
				acc += v * wgt[i]
				wsum += wgt[i]
		return acc / wsum if wsum > 1e-6 else NONE


class Cast(object):
	def __init__(self, azimuths=16, reach=127561.0, first=32.0, growth=1.5,
				 rise=4.0, near_skip=1.0, foot_invariant=False):
		self.azimuths = azimuths
		self.reach = reach
		self.first = first
		self.growth = growth
		self.rise = rise
		self.near_skip = near_skip
		self.bin_width = 2.0 * math.sin(math.pi / azimuths)
		# THE CANDIDATE FIX, off by default so the port reproduces the SHIPPED
		# bytes first: a field is not consulted where its own 2x2 tap is wider
		# than the azimuth bin (the invariant src/lodghorizon.h states).
		self.foot_invariant = foot_invariant

	def skip(self, f):
		if f is None or not (self.near_skip > 0.0):
			s = 0.0
		else:
			s = self.near_skip * f.cell
		if f is not None and self.foot_invariant:
			s = max(s, 2.0 * f.cell / self.bin_width)
		return s


def bin_dir(k, A):
	a = k * 2.0 * math.pi / A
	return math.sin(a), math.cos(a)


def quantise(deg):
	if not deg > 0.0:
		return 0
	return max(0, min(255, int(round(min(deg, 90.0) / 90.0 * 255.0))))


def cast_at(k, near, far, px, py, pz, out_deg=False):
	"""A port of `lodgenHorizonCastAt` for a FLAT receiver (normal = null),
	which is what the terrain sheet casts."""
	A = k.azimuths
	z0 = pz + k.rise
	sk_n, sk_f = k.skip(near), k.skip(far)
	res = []
	for b in range(A):
		dx, dy = bin_dir(b, A)
		best, any_ = 0.0, False
		d = k.first
		for _ in range(4096):
			if d > k.reach:
				break
			dEnd = min(d * k.growth, k.reach)
			x0, y0 = px + dx * d, py + dy * d
			x1, y1 = px + dx * dEnd, py + dy * dEnd
			want = max(k.bin_width * d, 1.0)
			top = NONE
			if near is not None and dEnd > sk_n:
				top = max(top, near.max_along(x0, y0, x1, y1, want))
			if far is not None and dEnd > sk_f:
				top = max(top, far.max_along(x0, y0, x1, y1, want))
			if top > SENT:
				e = math.degrees(math.atan2(top - z0, d))
				if not any_ or e > best:
					best, any_ = e, True
			if dEnd >= k.reach:
				break
			d = dEnd
		v = best if (any_ and best > -90.0) else 0.0
		res.append(v if out_deg else quantise(v))
	return res


def reference_pencil(k, near, far, px, py, pz, az_deg, step=32.0):
	"""A port of `lodgenHorizonReferencePencil` -- constant step, exact
	azimuth, `wantCell` 1. NOTE what that last one does NOT buy: `maxAlong`
	still takes a 2x2 tap of the BASE square, so the reference's footprint at
	level 0 is 2*cell wide, exactly the march's."""
	a = math.radians(az_deg)
	dx, dy = math.sin(a), math.cos(a)
	z0 = pz + k.rise
	sk_n, sk_f = k.skip(near), k.skip(far)
	best, any_ = 0.0, False
	d = step
	while d <= k.reach:
		dEnd = min(d + step, k.reach)
		x0, y0 = px + dx * d, py + dy * d
		x1, y1 = px + dx * dEnd, py + dy * dEnd
		top = NONE
		if near is not None and dEnd > sk_n:
			top = max(top, near.max_along(x0, y0, x1, y1, 1.0))
		if far is not None and dEnd > sk_f:
			top = max(top, far.max_along(x0, y0, x1, y1, 1.0))
		if top > SENT:
			e = math.degrees(math.atan2(top - z0, d))
			if not any_ or e > best:
				best, any_ = e, True
		d += step
	return best if any_ else 0.0


def reference_elev(k, near, far, px, py, pz, az_deg, rays=9):
	half = 180.0 / k.azimuths
	best = None
	for r in range(rays):
		t = (r / (rays - 1)) if rays > 1 else 0.5
		off = (-half + 2.0 * half * t) if rays > 1 else 0.0
		v = reference_pencil(k, near, far, px, py, pz, az_deg + off)
		if best is None or v > best:
			best = v
	return best


def block_max(f, X, Y, level=0):
	"""The 2x2 tap `maxAlong` takes at level `level`, for many points at once."""
	c = f.cell * (2.0 ** level)
	m = f.mip[level]
	gx = np.floor((X - f.ox) / c - 0.5).astype(np.int64)
	gy = np.floor((Y - f.oy) / c - 0.5).astype(np.int64)
	out = np.full(X.shape, NONE, dtype=np.float64)
	for j in range(2):
		for i in range(2):
			ax, ay = gx + i, gy + j
			ok = (ax >= 0) & (ay >= 0) & (ax < m.shape[1]) & (ay < m.shape[0])
			v = np.where(ok, m[np.clip(ay, 0, m.shape[0] - 1), np.clip(ax, 0, m.shape[1] - 1)], NONE)
			np.maximum(out, v, out=out)
	return out


def reference_pencil_fast(k, near, far, px, py, pz, az_deg, step=32.0):
	"""`reference_pencil` vectorised. A 32 u segment at wantCell 1 gives
	taps = int(32/128) = 0, so maxAlong reads exactly ONE 2x2 block, the one at
	the segment's near end -- which is what this computes for every step at
	once. Checked against the scalar port by ref_fast_control()."""
	a = math.radians(az_deg)
	dx, dy = math.sin(a), math.cos(a)
	z0 = pz + k.rise
	d = np.arange(step, k.reach + 1e-6, step)
	dEnd = np.minimum(d + step, k.reach)
	X, Y = px + dx * d, py + dy * d
	top = np.full(d.shape, NONE, dtype=np.float64)
	for f in (near, far):
		if f is None or not f.mip[0].size:
			continue
		v = np.where(dEnd > k.skip(f), block_max(f, X, Y), NONE)
		np.maximum(top, v, out=top)
	e = np.degrees(np.arctan2(top - z0, d))
	e = np.where(top > SENT, e, -1.0e30)
	best = float(e.max())
	return best if best > -1.0e29 else 0.0


def reference_elev_fast(k, near, far, px, py, pz, az_deg, rays=9):
	half = 180.0 / k.azimuths
	return max(reference_pencil_fast(k, near, far, px, py, pz,
									 az_deg + (-half + 2.0 * half * (r / (rays - 1)) if rays > 1 else 0.0))
			   for r in range(rays))
