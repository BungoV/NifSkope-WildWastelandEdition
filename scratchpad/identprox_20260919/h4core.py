#!/usr/bin/env python3
"""HORIZON4 -- the ceiling engine.

SUNSIM1 asked "how far apart are the baked bytes and a ray cast".  This asks
the next question: **if the bytes held the best value the representation can
hold, how far apart would they still be?**  Every row of the ceiling table is
therefore the SAME simulator with the RIGHT panel's input replaced by a horizon
computed here, in the file's own quantisation, one rule changed at a time.

THE CEILING HORIZON, and why it is not the bake's own march.
------------------------------------------------------------
`lodgenHorizonCastAt` walks a geometric segment ladder over a max-Z lattice and
takes the elevation at each segment's NEAR end.  That march has a measured
residual bias (+0.86 deg at growth 1.3, HORIZON2) and it is an APPROXIMATION of
a quantity: the elevation of the highest thing along the bin's direction.  The
ceiling must be that quantity itself, not a better approximation of it, or the
table measures the march rather than the representation.

So the ceiling is computed by the SAME sheared suffix-maximum the LEFT panel's
shadow test uses -- the identical geometry, the identical 16-unit footprint, the
identical far grid -- run at a LADDER of elevations.  For a fixed azimuth put
`u` along the sun's horizontal direction and `v` across it; then

    s = z - u * tan(elevation)

is constant along a sun ray, so a point is blocked at elevation `e` exactly when
some geometry with a larger `u` has a larger `s`.  One suffix maximum per ladder
rung answers that for every receiver at once, and the receiver's horizon is the
LARGEST rung at which it is still blocked.  Because the LEFT panel's `lit()` is
the same comparison at one elevation, a ceiling row and the truth panel cannot
disagree because they disagree about what geometry is: they can only disagree
because the REPRESENTATION (bins, bytes, texels, a tangent-plane rule) threw
something away.  That is the whole point of the table.

Ladder resolution: 0.5 deg from 0 to 50, then 4 deg to 90.  The file's own
quantiser is 0.3529 deg a step, so the ladder is within 1.5 byte steps of the
quantisation it feeds, and the residual is stated in the report rather than
hidden.  A receiver's reported horizon is the largest rung at which it is
blocked, so the ceiling UNDER-states the true horizon by up to one rung: it errs
toward LESS shadow, the opposite side from the LEFT panel's 16-unit footprint.
Both directions are stated beside every number.
"""
import numpy as np
import os
import sys
import time

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/horizon4_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
sys.path.insert(0, SUNSIM)
sys.path.insert(0, ROOT + '/tests/spells')

from scene import Terrain, Objects, Sheet, CHUNK          # noqa: E402
import render as RD                                        # noqa: E402

DEG = 180.0 / np.pi

# The ladder.  Dense where the suns of this lane stand (5, 15, 30).
LADDER_DEG = np.concatenate([np.arange(0.0, 50.0001, 0.5), np.arange(54.0, 90.0001, 4.0)])
LADDER_TAN = np.tan(np.radians(LADDER_DEG))


# --------------------------------------------------------------------------
class ShearGrid(object):
	"""The LEFT panel's shadow geometry for ONE azimuth, kept as raw max Z.

	`render.SunShadow` bakes `tan(elevation)` into the cell value, so it has to
	be rebuilt for every sun.  Here the cells hold the max Z over the cell and
	the shear is applied per ladder rung, so ONE build serves the whole ladder.
	The cell sizes, the reach, the triangle splat and the terrain sampling are
	character for character the ones `SunShadow` uses; only the stored quantity
	differs.
	"""

	NEAR_CELL = RD.SunShadow.NEAR_CELL       # 16 u
	FAR_CELL = RD.SunShadow.FAR_CELL         # 256 u
	FAR_REACH = RD.SunShadow.FAR_REACH       # 150,000 u

	def __init__(self, ter, ob, az, margin=2048.0, verbose=False):
		t0 = time.time()
		self.az = az
		a = np.radians(az)
		self.sx, self.sy = np.sin(a), np.cos(a)
		x0, y0, x1, y1 = CHUNK
		x0 -= margin; y0 -= margin; x1 += margin; y1 += margin
		cor = np.array([[x0, y0], [x1, y0], [x0, y1], [x1, y1]])
		U = cor[:, 0] * self.sx + cor[:, 1] * self.sy
		V = -cor[:, 0] * self.sy + cor[:, 1] * self.sx
		self.nu0, self.nu1 = U.min() - 64, U.max() + 64
		self.nv0, self.nv1 = V.min() - 64, V.max() + 64
		nw = int((self.nu1 - self.nu0) / self.NEAR_CELL) + 1
		nh = int((self.nv1 - self.nv0) / self.NEAR_CELL) + 1
		uu = self.nu0 + (np.arange(nw) + 0.5) * self.NEAR_CELL
		vv = self.nv0 + (np.arange(nh) + 0.5) * self.NEAR_CELL
		UU, VV = np.meshgrid(uu, vv)
		WX = UU * self.sx - VV * self.sy
		WY = UU * self.sy + VV * self.sx
		near = ter.atf(WX, WY).astype(np.float32)
		self._splat(near, ob)
		self.near = near
		self.nu = uu.astype(np.float32)
		self.nw, self.nh = nw, nh
		# --- far grid, terrain only, the bilinear surface at the cell centre
		self.fu0 = self.nu0 - self.FAR_REACH
		self.fv0 = self.nv0 - self.FAR_REACH
		fw = int((self.nu1 + self.FAR_REACH - self.fu0) / self.FAR_CELL) + 1
		fh = int((self.nv1 + self.FAR_REACH - self.fv0) / self.FAR_CELL) + 1
		fu = self.fu0 + (np.arange(fw) + 0.5) * self.FAR_CELL
		fv = self.fv0 + (np.arange(fh) + 0.5) * self.FAR_CELL
		FU, FV = np.meshgrid(fu, fv)
		self.far = ter.atf(FU * self.sx - FV * self.sy,
						   FU * self.sy + FV * self.sx).astype(np.float32)
		self.fu = fu.astype(np.float32)
		self.fw, self.fh = fw, fh
		self._sn = np.empty_like(self.near)
		self._sf = np.empty_like(self.far)
		self.build_s = time.time() - t0
		if verbose:
			print('  shear az %.1f: near %dx%d far %dx%d  %.1fs'
				  % (az, nh, nw, fh, fw, self.build_s))

	# the splat is `SunShadow._splat_tris` with the shear taken out of the value
	def _splat(self, near, ob):
		v = ob.v.astype(np.float64)
		U = v[:, 0] * self.sx + v[:, 1] * self.sy
		V = -v[:, 0] * self.sy + v[:, 1] * self.sx
		S = v[:, 2]
		gx = (U - self.nu0) / self.NEAR_CELL
		gy = (V - self.nv0) / self.NEAR_CELL
		t = ob.tri
		px, py, ps = [gx], [gy], [S]
		for i, j in ((0, 1), (1, 2), (2, 0)):
			px.append(0.5 * (gx[t[:, i]] + gx[t[:, j]]))
			py.append(0.5 * (gy[t[:, i]] + gy[t[:, j]]))
			ps.append(0.5 * (S[t[:, i]] + S[t[:, j]]))
		self._scatter(near, np.concatenate(px), np.concatenate(py), np.concatenate(ps))
		x0 = np.floor(np.minimum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(np.int64)
		x1 = np.ceil(np.maximum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(np.int64)
		y0 = np.floor(np.minimum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(np.int64)
		y1 = np.ceil(np.maximum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(np.int64)
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
				ax, ay, asv = gx[t[ss, 0]][:, None, None], gy[t[ss, 0]][:, None, None], S[t[ss, 0]][:, None, None]
				bx, by, bs = gx[t[ss, 1]][:, None, None], gy[t[ss, 1]][:, None, None], S[t[ss, 1]][:, None, None]
				c3x, c3y, cs = gx[t[ss, 2]][:, None, None], gy[t[ss, 2]][:, None, None], S[t[ss, 2]][:, None, None]
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
				sv = np.broadcast_to(w0 * asv + w1 * bs + w2 * cs, ins.shape).ravel()[k]
				GXb = np.broadcast_to(GX, ins.shape).ravel()
				GYb = np.broadcast_to(GY, ins.shape).ravel()
				self._scatter(near, GXb[k].astype(np.float64), GYb[k].astype(np.float64),
							  sv, already_cell=True)

	def _scatter(self, near, gx, gy, s, already_cell=False):
		ix = gx.astype(np.int64) if already_cell else np.floor(gx).astype(np.int64)
		iy = gy.astype(np.int64) if already_cell else np.floor(gy).astype(np.int64)
		ok = (ix >= 0) & (ix < near.shape[1]) & (iy >= 0) & (iy < near.shape[0])
		np.maximum.at(near, (iy[ok], ix[ok]), s[ok])

	# ---------------------------------------------------------------- receivers
	def index(self, pts, skip_near=1, skip_far=1):
		"""Pre-compute everything about a receiver set that the ladder reuses."""
		x, y, z = pts[:, 0], pts[:, 1], pts[:, 2]
		u = x * self.sx + y * self.sy
		v = -x * self.sy + y * self.sx
		ixn = np.floor((u - self.nu0) / self.NEAR_CELL).astype(np.int64) + skip_near
		iyn = np.floor((v - self.nv0) / self.NEAR_CELL).astype(np.int64)
		okn = (ixn >= 0) & (ixn < self.nw) & (iyn >= 0) & (iyn < self.nh)
		ixf = np.floor((u - self.fu0) / self.FAR_CELL).astype(np.int64) + skip_far
		iyf = np.floor((v - self.fv0) / self.FAR_CELL).astype(np.int64)
		okf = (ixf >= 0) & (ixf < self.fw) & (iyf >= 0) & (iyf < self.fh)
		return dict(u=u, z=z,
					fn=np.where(okn, iyn * self.nw + np.clip(ixn, 0, self.nw - 1), -1),
					ff=np.where(okf, iyf * self.fw + np.clip(ixf, 0, self.fw - 1), -1))

	def horizon(self, idx, rise=12.0, ladder_deg=LADDER_DEG, ladder_tan=LADDER_TAN):
		"""Degrees.  The largest ladder rung at which the receiver is blocked.

		`rise` is `SunShadow.lit`'s own bias, so a ceiling row and the truth
		panel start the ray from the same place."""
		u, z = idx['u'], idx['z']
		fn, ff = idx['fn'], idx['ff']
		gn = fn >= 0
		gf = ff >= 0
		fnc = np.where(gn, fn, 0)
		ffc = np.where(gf, ff, 0)
		out = np.zeros(len(u))
		nearf = self.near.ravel()
		farf = self.far.ravel()
		for d, t in zip(ladder_deg, ladder_tan):
			np.subtract(self.near, self.nu[None, :] * t, out=self._sn)
			acc = np.maximum.accumulate(self._sn[:, ::-1], axis=1)[:, ::-1].ravel()
			np.subtract(self.far, self.fu[None, :] * t, out=self._sf)
			accf = np.maximum.accumulate(self._sf[:, ::-1], axis=1)[:, ::-1].ravel()
			s = z + rise - u * t
			blocked = (gn & (acc[fnc] > s)) | (gf & (accf[ffc] > s))
			out[blocked] = d
		return out

	def horizon_multi(self, idx, rises, ladder_deg=LADDER_DEG, ladder_tan=LADDER_TAN):
		"""The same walk for several receiver RISES at once -- the suffix maxima
		are the expensive part and they do not depend on the rise."""
		u, z = idx['u'], idx['z']
		fn, ff = idx['fn'], idx['ff']
		gn, gf = fn >= 0, ff >= 0
		fnc, ffc = np.where(gn, fn, 0), np.where(gf, ff, 0)
		out = [np.zeros(len(u)) for _ in rises]
		for d, t in zip(ladder_deg, ladder_tan):
			np.subtract(self.near, self.nu[None, :] * t, out=self._sn)
			acc = np.maximum.accumulate(self._sn[:, ::-1], axis=1)[:, ::-1].ravel()[fnc]
			np.subtract(self.far, self.fu[None, :] * t, out=self._sf)
			accf = np.maximum.accumulate(self._sf[:, ::-1], axis=1)[:, ::-1].ravel()[ffc]
			base = z - u * t
			for o, r in zip(out, rises):
				s = base + r
				o[(gn & (acc > s)) | (gf & (accf > s))] = d
		return out
	# NB `nearf`/`farf` are kept only so a caller can inspect the raw field.
	_unused = (0,)


# --------------------------------------------------------------------------
def quantise(deg):
	"""`lodgenHorizonQuantise`: round(deg/90*255), clamped, negatives -> 0."""
	d = np.clip(np.asarray(deg, dtype=np.float64), 0.0, 90.0)
	b = np.rint(d / 90.0 * 255.0)
	return np.clip(b, 0, 255).astype(np.uint8)


def dequantise(b):
	return np.asarray(b, dtype=np.float64) / 255.0 * 90.0


def bin_dirs(A):
	"""Bin k's centre azimuth: bin 0 = north, clockwise toward east."""
	return np.arange(A) * (360.0 / A)


def read_bins(bins, A, az, mode='lerp'):
	"""The runtime read, `lodgenHorizonElevAt` and its two alternatives.

	bins is (N, A) bytes; returns degrees per row."""
	step = 360.0 / A
	t = az / step
	t -= np.floor(t / A) * A
	k0 = int(t) % A
	k1 = (k0 + 1) % A
	f = t - np.floor(t)
	if mode == 'lerp':
		return dequantise(bins[:, k0]) * (1.0 - f) + dequantise(bins[:, k1]) * f
	if mode == 'nearest':
		return dequantise(bins[:, k1 if f >= 0.5 else k0])
	if mode == 'max':
		return dequantise(np.maximum(bins[:, k0], bins[:, k1]))
	raise ValueError(mode)


# --------------------------------------------------------------------------
class Plane(object):
	"""A terrain horizon SHEET: (H, W, A) bytes on a regular world lattice,
	read exactly the way `scene.Sheet.elev_at` reads the shipped one."""

	def __init__(self, plane, ox, oy, upt, A):
		self.plane, self.ox, self.oy, self.upt, self.A = plane, ox, oy, upt, A

	def elev_at(self, wx, wy, az, mode='lerp'):
		A = self.A
		step = 360.0 / A
		t = az / step
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
		if mode == 'lerp':
			ks = ((k0, 1.0 - f), (k1, f))
		elif mode == 'nearest':
			ks = ((k1 if f >= 0.5 else k0, 1.0),)
		elif mode == 'max':
			ks = None
		else:
			raise ValueError(mode)

		def tap(k):
			pl = self.plane[:, :, k].astype(np.float64)
			a = pl[iy, ix] * (1 - tx) + pl[iy, ix + 1] * tx
			c = pl[iy + 1, ix] * (1 - tx) + pl[iy + 1, ix + 1] * tx
			return a * (1 - ty) + c * ty

		if ks is None:
			out = np.maximum(tap(k0), tap(k1))
		else:
			out = sum(w * tap(k) for k, w in ks)
		return np.where(inside, out / 255.0 * 90.0, np.nan)

	def nearest_at(self, wx, wy, az, mode='lerp'):
		"""No spatial interpolation -- the texel the point lands in.  This is
		what T5's 'no interpolation at read time' control needs."""
		A = self.A
		step = 360.0 / A
		t = az / step
		t -= np.floor(t / A) * A
		k0 = int(t) % A
		k1 = (k0 + 1) % A
		f = t - np.floor(t)
		ix = np.floor((np.asarray(wx) - self.ox) / self.upt).astype(np.int64)
		iy = np.floor((np.asarray(wy) - self.oy) / self.upt).astype(np.int64)
		H, W = self.plane.shape[0], self.plane.shape[1]
		inside = (ix >= 0) & (ix < W) & (iy >= 0) & (iy < H)
		ixc = np.clip(ix, 0, W - 1)
		iyc = np.clip(iy, 0, H - 1)
		v0 = self.plane[iyc, ixc, k0].astype(np.float64)
		v1 = self.plane[iyc, ixc, k1].astype(np.float64)
		if mode == 'lerp':
			out = v0 * (1 - f) + v1 * f
		elif mode == 'nearest':
			out = v1 if f >= 0.5 else v0
		else:
			out = np.maximum(v0, v1)
		return np.where(inside, out / 255.0 * 90.0, np.nan)


def texel_grid(upt):
	"""Texel centres over chunk 4.4.-12, and the (ox, oy) a Plane wants."""
	x0, y0, x1, y1 = CHUNK
	nx = int(round((x1 - x0) / upt))
	ny = int(round((y1 - y0) / upt))
	wx = x0 + (np.arange(nx) + 0.5) * upt
	wy = y0 + (np.arange(ny) + 0.5) * upt
	X, Y = np.meshgrid(wx, wy)
	return X, Y, nx, ny, x0, y0


# --------------------------------------------------------------------------
def load_scene(verbose=True):
	ter = Terrain()
	ob = Objects(verbose=verbose)
	sh = Sheet(verbose=verbose)
	return ter, ob, sh


def gbuffers(ter, ob, names=('close', 'east', 'full', 'street'), w=1600, h=900,
			 cache=LANE + '/gb_%s.npz', verbose=True):
	"""The four cameras' G-buffers, cached: the sun never moves the geometry."""
	import cams as CAMS
	cs = CAMS.build(ter, w, h)
	out = {}
	for nm in names:
		cam = cs[nm]
		p = cache % nm
		if os.path.exists(p):
			z = np.load(p)
			gb = _GB(cam, z)
			if verbose:
				print('gbuffer %s: cache' % nm)
		else:
			gb = RD.GBuffer(cam, ter, ob, verbose=verbose)
			np.savez_compressed(p, kind=gb.kind, t=gb.t, pos=gb.pos.astype(np.float32),
								dir=gb.dir.astype(np.float32), nrm=gb.nrm.astype(np.float32),
								tri=gb.tri.astype(np.int32), bary=gb.bary.astype(np.float32),
								dropped=gb.dropped)
		out[nm] = (cam, gb)
	return out


class _GB(object):
	def __init__(self, cam, z):
		self.cam = cam
		self.kind = z['kind']
		self.t = z['t']
		self.pos = z['pos'].astype(np.float64)
		self.dir = z['dir'].astype(np.float64)
		self.nrm = z['nrm'].astype(np.float64)
		self.tri = z['tri'].astype(np.int64)
		self.bary = z['bary'].astype(np.float64)
		self.dropped = int(z['dropped'])
		self.objfirst = self.kind == 2
		self.terfirst = self.kind == 1


def truth_lit(ter, ob, gb, az, el, cache=LANE + '/truth_%s_az%03d_el%02d.npy'):
	"""The LEFT panel, cached per camera and sun."""
	p = cache % (gb.cam.name, int(round(az)), int(round(el)))
	if os.path.exists(p):
		return np.load(p)
	ss = RD.SunShadow(ter, ob, az, el)
	lit = np.zeros(len(gb.kind), dtype=bool)
	m = gb.kind != 0
	lit[m] = ss.lit(gb.pos[m])
	np.save(p, lit)
	return lit
