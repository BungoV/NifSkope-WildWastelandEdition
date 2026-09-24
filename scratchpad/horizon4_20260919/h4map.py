#!/usr/bin/env python3
"""HORIZON4 rows M1-M3 -- a simulated RUNTIME FAR SHADOW MAP keyed on identity.

bungo, 2026-09-19 03:5x: *"Identity was a far better idea, but it relied on each
lod object being one thing, so a house, a single thing, a tower, a single
thing."*

THE LIGHT SPACE, and why it is the same shear the rest of this lane uses.
With the sun's horizontal direction (sin az, cos az), put `u` along it, `v`
across it, and

    s = z - u * tan(elevation)

`s` is CONSTANT along a sun ray (ds = -sin(el) dl + cos(el) tan(el) dl = 0), so
(v, s) is an orthographic light-space parameterisation and `u` is the depth: the
surface with the LARGEST u on a ray is the one the sun sees.  The map is
therefore a (v, s) raster storing, per texel, the largest `u` and the SHADOW
IDENTITY of whatever achieved it.  That is a plain orthographic shadow map
written in the coordinates the ceiling engine already speaks, so a map row and a
horizon row cannot differ because of a coordinate convention.

THE IDENTITY is the `.lodi` v7 `u16 group[]` (format doc s4.9): a SCOL is one
group, touching architecture kit pieces are one group, everything else is its
own.  Terrain carries the reserved identity `TERRAIN`.

THE RULE, as the director stated it:
  * an OBJECT receiver is dark when the map's depth is nearer AND the map's
    identity DIFFERS from the receiver's own group -- self-shadow excluded by
    identity;
  * a TERRAIN receiver is dark on depth alone, with a slope bias, whatever the
    caster is.

BIAS.  A constant depth bias is useless at a 5-degree sun: a horizontal surface
crossing one texel of `s` spans texel/tan(el) = 11.4 texels of `u`, so the bias
that stops acne at 5 degrees is a 730-unit peter-pan at 64 u a texel.  This uses
the NORMAL-OFFSET bias instead -- the receiver is moved along its own surface
normal by `normalBias` texels before the lookup -- plus a small constant depth
bias.  Both are stated with every number and neither was tuned against the
answer: `normalBias` 1.0 and `depthBias` 0.5 texels were set before the first
row was measured, and s3 of the report shows what a sweep of them does.
"""
import numpy as np
import sys

import h4core as H

TERRAIN_ID = np.int64(-1)


class ShadowMap(object):

	def __init__(self, ter, ob, gtri, az, el, texel, margin=2048.0,
				 far_reach=150000.0, far_cell=256.0, near_step=None, verbose=False):
		self.az, self.el, self.texel = az, el, texel
		a = np.radians(az)
		self.sx, self.sy = np.sin(a), np.cos(a)
		self.tan = np.tan(np.radians(el))
		x0, y0, x1, y1 = H.CHUNK
		x0 -= margin; y0 -= margin; x1 += margin; y1 += margin
		# ---- extent: v from the chunk's own corners, s from the chunk's own
		#      geometry.  A caster that can shadow the chunk lies on a ray THROUGH
		#      the chunk, so its (v, s) is inside this box by construction.
		cor = np.array([[x0, y0], [x1, y0], [x0, y1], [x1, y1]])
		V = -cor[:, 0] * self.sy + cor[:, 1] * self.sx
		U = cor[:, 0] * self.sx + cor[:, 1] * self.sy
		self.v0 = V.min() - texel
		self.v1 = V.max() + texel
		zs = ob.v[:, 2]
		zlo = float(min(zs.min(), np.nanmin(ter.at(cor[:, 0], cor[:, 1])) if True else 0.0))
		zhi = float(zs.max())
		# terrain inside the box can be higher/lower than the corners
		gx = np.linspace(x0, x1, 65)
		gy = np.linspace(y0, y1, 65)
		GX, GY = np.meshgrid(gx, gy)
		GZ = ter.atf(GX, GY)
		zlo = min(zlo, float(np.nanmin(GZ[GZ > -1e5])))
		zhi = max(zhi, float(np.nanmax(GZ)))
		self.s0 = zlo - U.max() * self.tan - texel * 4
		self.s1 = zhi - U.min() * self.tan + texel * 4
		self.nv = int((self.v1 - self.v0) / texel) + 1
		self.ns = int((self.s1 - self.s0) / texel) + 1
		self._tris(ob, gtri)
		self._terrain(ter, x0, y0, x1, y1, near_step or texel * 0.5,
					  far_reach, far_cell, U)
		if verbose:
			print('  map az %.0f el %.0f texel %.0f: %d x %d = %s texels, %.2f MB at 4+2 B'
				  % (az, el, texel, self.nv, self.ns, '{:,}'.format(self.nv * self.ns),
					 self.nv * self.ns * 6 / 1e6))

	# ---------------------------------------------------------------- casters
	def _key(self, u, ident):
		"""Pack so one np.maximum.at keeps the largest u AND its identity."""
		q = np.clip(np.rint((u + 1.0e6) * 8.0), 0, 2 ** 40 - 1).astype(np.int64)
		return (q << np.int64(20)) | (np.asarray(ident, dtype=np.int64) & np.int64(0xFFFFF))

	def _unkey(self, k):
		u = (k >> np.int64(20)).astype(np.float64) / 8.0 - 1.0e6
		i = k & np.int64(0xFFFFF)
		i = np.where(i == 0xFFFFF, TERRAIN_ID, i)
		return u, i

	def _tris(self, ob, gtri):
		"""Rasterise every drawn triangle into (v, s), the largest u winning."""
		v = ob.v.astype(np.float64)
		U = v[:, 0] * self.sx + v[:, 1] * self.sy
		V = -v[:, 0] * self.sy + v[:, 1] * self.sx
		S = v[:, 2] - U * self.tan
		gv = (V - self.v0) / self.texel
		gs = (S - self.s0) / self.texel
		t = ob.tri
		KEY = np.full(self.nv * self.ns, np.int64(-1), dtype=np.int64)
		# point samples first: vertices and edge midpoints, so a triangle thinner
		# than a texel still casts (the same rule the ceiling grid uses)
		vid = np.zeros(len(v), dtype=np.int64)
		for c in range(3):
			vid[t[:, c]] = gtri
		px, py, pu, pi = [gv], [gs], [U], [vid]
		for i, j in ((0, 1), (1, 2), (2, 0)):
			px.append(0.5 * (gv[t[:, i]] + gv[t[:, j]]))
			py.append(0.5 * (gs[t[:, i]] + gs[t[:, j]]))
			pu.append(0.5 * (U[t[:, i]] + U[t[:, j]]))
			pi.append(gtri)
		AX = np.floor(np.concatenate(px)).astype(np.int64)
		AY = np.floor(np.concatenate(py)).astype(np.int64)
		AU = np.concatenate(pu)
		AI = np.concatenate(pi)
		self._put(KEY, AX, AY, AU, AI)
		# then the interiors
		x0 = np.floor(np.minimum.reduce([gv[t[:, 0]], gv[t[:, 1]], gv[t[:, 2]]])).astype(np.int64)
		x1 = np.ceil(np.maximum.reduce([gv[t[:, 0]], gv[t[:, 1]], gv[t[:, 2]]])).astype(np.int64)
		y0 = np.floor(np.minimum.reduce([gs[t[:, 0]], gs[t[:, 1]], gs[t[:, 2]]])).astype(np.int64)
		y1 = np.ceil(np.maximum.reduce([gs[t[:, 0]], gs[t[:, 1]], gs[t[:, 2]]])).astype(np.int64)
		span = np.maximum(x1 - x0, y1 - y0)
		lg = np.clip(np.ceil(np.log2(np.maximum(span, 1))).astype(int), 0, 14)
		for L in np.unique(lg):
			K = 1 << int(L)
			sel = np.nonzero(lg == L)[0]
			per = max(1, int(6e6 // (K * K)))
			for s0 in range(0, sel.size, per):
				ss = sel[s0:s0 + per]
				GX = x0[ss][:, None, None] + np.arange(K)[None, None, :]
				GY = y0[ss][:, None, None] + np.arange(K)[None, :, None]
				cx = GX + 0.5
				cy = GY + 0.5
				ax, ay, au = gv[t[ss, 0]][:, None, None], gs[t[ss, 0]][:, None, None], U[t[ss, 0]][:, None, None]
				bx, by, bu = gv[t[ss, 1]][:, None, None], gs[t[ss, 1]][:, None, None], U[t[ss, 1]][:, None, None]
				c3x, c3y, cu = gv[t[ss, 2]][:, None, None], gs[t[ss, 2]][:, None, None], U[t[ss, 2]][:, None, None]
				area = (bx - ax) * (c3y - ay) - (by - ay) * (c3x - ax)
				aa = np.where(np.abs(area) < 1e-12, 1e-12, area)
				w0 = ((bx - cx) * (c3y - cy) - (by - cy) * (c3x - cx)) / aa
				w1 = ((c3x - cx) * (ay - cy) - (c3y - cy) * (ax - cx)) / aa
				w2 = 1.0 - w0 - w1
				ins = ((w0 >= 0) & (w1 >= 0) & (w2 >= 0)
					   & (GX < x1[ss][:, None, None]) & (GY < y1[ss][:, None, None]))
				ins = np.broadcast_to(ins, (len(ss), K, K))
				k = np.nonzero(ins.ravel())[0]
				if k.size == 0:
					continue
				uv = np.broadcast_to(w0 * au + w1 * bu + w2 * cu, ins.shape).ravel()[k]
				iv = np.broadcast_to(gtri[ss][:, None, None], ins.shape).ravel()[k]
				self._put(KEY, np.broadcast_to(GX, ins.shape).ravel()[k],
						  np.broadcast_to(GY, ins.shape).ravel()[k], uv, iv)
		self.KEY = KEY

	def _put(self, KEY, iv, isv, u, ident):
		ok = (iv >= 0) & (iv < self.nv) & (isv >= 0) & (isv < self.ns)
		if not ok.any():
			return
		pix = iv[ok] * self.ns + isv[ok]
		np.maximum.at(KEY, pix, self._key(u[ok], ident[ok] if np.ndim(ident) else ident))

	def _terrain(self, ter, x0, y0, x1, y1, step, far_reach, far_cell, U):
		"""Terrain as a point splat, near at `step` and far on the 256-unit
		lattice out to the reach, both with the reserved TERRAIN identity."""
		nx = int((x1 - x0) / step) + 1
		ny = int((y1 - y0) / step) + 1
		gx = x0 + np.arange(nx) * step
		gy = y0 + np.arange(ny) * step
		for chunk in range(0, ny, max(1, int(4e6 // max(1, nx)))):
			GY, GX = np.meshgrid(gy[chunk:chunk + max(1, int(4e6 // max(1, nx)))], gx, indexing='ij')
			GZ = ter.atf(GX, GY)
			self._splat_pts(GX.ravel(), GY.ravel(), GZ.ravel(), 0xFFFFF)
		# far: the same lattice `ShearGrid` uses, so the two agree about the rim
		fu0 = U.min() - far_reach
		fv0 = self.v0 - far_reach
		fw = int((U.max() + far_reach - fu0) / far_cell) + 1
		fh = int((self.v1 + far_reach - fv0) / far_cell) + 1
		fu = fu0 + (np.arange(fw) + 0.5) * far_cell
		fv = fv0 + (np.arange(fh) + 0.5) * far_cell
		for b in range(0, fh, max(1, int(4e6 // max(1, fw)))):
			FV, FU = np.meshgrid(fv[b:b + max(1, int(4e6 // max(1, fw)))], fu, indexing='ij')
			FX = FU * self.sx - FV * self.sy
			FY = FU * self.sy + FV * self.sx
			FZ = ter.atf(FX, FY)
			self._splat_pts(FX.ravel(), FY.ravel(), FZ.ravel(), 0xFFFFF)

	def _splat_pts(self, x, y, z, ident):
		good = z > -1e5
		x, y, z = x[good], y[good], z[good]
		u = x * self.sx + y * self.sy
		v = -x * self.sy + y * self.sx
		s = z - u * self.tan
		iv = np.floor((v - self.v0) / self.texel).astype(np.int64)
		isv = np.floor((s - self.s0) / self.texel).astype(np.int64)
		ok = (iv >= 0) & (iv < self.nv) & (isv >= 0) & (isv < self.ns)
		if not ok.any():
			return
		pix = iv[ok] * self.ns + isv[ok]
		np.maximum.at(self.KEY, pix, self._key(u[ok], ident))

	# --------------------------------------------------------------- receivers
	def query(self, pos, nrm, ident, normal_bias=1.0, depth_bias=0.5,
			  use_identity=True):
		"""Dark/lit per receiver, plus the caster identity the map held.

		`ident` is the receiver's shadow identity (a group id, or TERRAIN_ID).
		A terrain receiver is judged on depth alone whatever the caster, which
		is the director's rule."""
		p = pos + nrm * (normal_bias * self.texel)
		u = p[:, 0] * self.sx + p[:, 1] * self.sy
		v = -p[:, 0] * self.sy + p[:, 1] * self.sx
		s = p[:, 2] - u * self.tan
		iv = np.floor((v - self.v0) / self.texel).astype(np.int64)
		isv = np.floor((s - self.s0) / self.texel).astype(np.int64)
		inside = (iv >= 0) & (iv < self.nv) & (isv >= 0) & (isv < self.ns)
		pix = np.where(inside, iv * self.ns + np.clip(isv, 0, self.ns - 1), 0)
		k = self.KEY[pix]
		mu, mi = self._unkey(np.maximum(k, 0))
		has = inside & (k >= 0)
		nearer = has & (mu > u + depth_bias * self.texel)
		if use_identity:
			same = (mi == ident) & (ident != TERRAIN_ID)
			dark = nearer & ~same
		else:
			dark = nearer
		return dark, mi, nearer, has


def group_of_vertex(ob, T):
	"""Shadow identity per .lodo vertex of the scene, from the v7 group table.
	Ids are dense PER CHUNK, so they are made globally unique the same way
	`groups.py` does before they are used as a key."""
	g = np.array(T['group'], dtype=np.int64)
	ch = np.zeros(len(g), dtype=np.int64)
	for ci, c in enumerate(T['chunks']):
		ch[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
	gid = ch * 100000 + g + 1        # +1: 0 is the map's "empty" key
	return gid[ob.inst]
