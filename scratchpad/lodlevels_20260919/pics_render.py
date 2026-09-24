#!/usr/bin/env python3
"""SUNSIM1 -- the offline sun renderer.  numpy only, CPU only.

PRIMARY VISIBILITY is solved once a camera and reused by every sun position and
by both panels, because the sun never moves the geometry:

  * terrain -- a per-pixel RAY MARCH against the bilinear 128-unit LAND field,
    guarded by a max-Z mip pyramid so a coarse step can be skipped only when
    nothing in its footprint could reach the ray, then bisected on the exact
    field.  No mesh, no LOD ring, no seam.
  * objects -- the 29,587 .lodo level-0 triangles projected and resolved in a
    depth buffer with perspective-correct barycentrics.  For PRIMARY rays that
    is the same answer a ray-triangle test gives, one triangle at a time,
    and it is thousands of times cheaper; the SHADOW test below is where the
    honesty actually has to be bought, and it is bought with a control.

THE SUN TEST, left panel (truth).  Sheared sun space: with the sun's horizontal
direction (sin az, cos az), put u along it and v across it, and give every
point the invariant

    s = z - u * tan(elevation)

A point is in shadow exactly when some geometry with a LARGER u has a LARGER s.
So one suffix-maximum of s along u over a (u, v) grid answers every shadow ray
in the picture at once, and it answers it for terrain and triangles together.
The grid is 16 u across the chunk (objects + terrain) and 256 u out to the
worldspace rim (terrain), and `control.py` scores it against brute-force
ray casts -- per-ray marching of the real bilinear field and real
ray-triangle intersection against all 29,587 triangles -- on random samples.

THE SUN TEST, right panel (baked data only).  Terrain: the role-7 sheet,
bilinear in space, the two bins the azimuth falls between blended by
`lodgenHorizonBinPair`'s own fraction.  Objects: the .lodi v8 per-vertex
horizon bytes, the same bin blend per vertex, then barycentric across the hit
triangle.  Nothing else is read.
"""
import numpy as np
import time

from pics_scene import Terrain, Objects, Sheet, CHUNK, sun_dir

DEG = 180.0 / np.pi


# --------------------------------------------------------------------- camera
class Camera(object):
	def __init__(self, eye, target, vfov_deg, w, h, up=(0.0, 0.0, 1.0), name=''):
		self.eye = np.array(eye, dtype=np.float64)
		self.target = np.array(target, dtype=np.float64)
		self.w, self.h, self.name = w, h, name
		f = self.target - self.eye
		f /= np.linalg.norm(f)
		up = np.array(up, dtype=np.float64)
		r = np.cross(f, up)
		r /= np.linalg.norm(r)
		u = np.cross(r, f)
		self.f, self.r, self.u = f, r, u
		self.vfov = np.radians(vfov_deg)
		self.ty = np.tan(self.vfov * 0.5)
		self.tx = self.ty * (w / float(h))

	def rays(self):
		"""Unit ray directions, shape (h, w, 3), pixel centres."""
		px = (np.arange(self.w) + 0.5) / self.w * 2.0 - 1.0
		py = 1.0 - (np.arange(self.h) + 0.5) / self.h * 2.0
		X, Y = np.meshgrid(px * self.tx, py * self.ty)
		d = (self.f[None, None, :] + X[..., None] * self.r[None, None, :]
			 + Y[..., None] * self.u[None, None, :])
		return d / np.linalg.norm(d, axis=-1, keepdims=True)

	def project(self, p):
		"""World points -> (sx, sy, depth-along-forward).  sx/sy in pixels."""
		d = p - self.eye[None, :]
		zc = d @ self.f
		xc = d @ self.r
		yc = d @ self.u
		zs = np.where(np.abs(zc) < 1e-9, 1e-9, zc)
		sx = (xc / (zs * self.tx) * 0.5 + 0.5) * self.w
		sy = (0.5 - yc / (zs * self.ty) * 0.5) * self.h
		return sx, sy, zc


# ------------------------------------------------------- terrain ray marching
def march_terrain(ter, eye, dirs, tmax=131072.0, step0=48.0, growth=1.03,
				  refine=18, verbose=True, tstart=1.0):
	"""First terrain hit per ray.  Returns (t, hit) with t = NaN where no hit."""
	n = dirs.shape[0]
	ox, oy, oz = eye
	dx, dy, dz = dirs[:, 0], dirs[:, 1], dirs[:, 2]
	t = np.full(n, float(tstart))
	hit = np.zeros(n, dtype=bool)
	thit = np.full(n, np.nan)
	step = np.full(n, step0)
	alive = np.ones(n, dtype=bool)
	rounds = 0
	while alive.any() and rounds < 600:
		rounds += 1
		idx = np.nonzero(alive)[0]
		t0 = t[idx]
		st = step[idx]
		t1 = t0 + st
		lvl = int(np.clip(np.ceil(np.log2(max(st.max(), 128.0) / 128.0)), 0, 8))
		hmax = ter.mip_max(lvl, ox + dx[idx] * t0, oy + dy[idx] * t0)
		for fr in (0.5, 1.0):
			tt = t0 + st * fr
			hmax = np.maximum(hmax, ter.mip_max(lvl, ox + dx[idx] * tt, oy + dy[idx] * tt))
		zmin = np.minimum(oz + dz[idx] * t0, oz + dz[idx] * t1)
		cand = zmin <= hmax
		ci = idx[cand]
		if ci.size:
			a = t[ci]
			b = a + step[ci]
			fa = (oz + dz[ci] * a) - ter.atf(ox + dx[ci] * a, oy + dy[ci] * a)
			fb = (oz + dz[ci] * b) - ter.atf(ox + dx[ci] * b, oy + dy[ci] * b)
			cross = (fa > 0) & (fb <= 0)
			# start already under the surface -> hit at t0
			under = fa <= 0
			lo = a.copy()
			hi = b.copy()
			for _ in range(refine):
				mid = 0.5 * (lo + hi)
				fm = (oz + dz[ci] * mid) - ter.atf(ox + dx[ci] * mid, oy + dy[ci] * mid)
				gt = fm > 0
				lo = np.where(gt, mid, lo)
				hi = np.where(gt, hi, mid)
			got = cross | under
			th = np.where(under, a, 0.5 * (lo + hi))
			hit[ci[got]] = True
			thit[ci[got]] = th[got]
		t[idx] = t1
		step[idx] = st * growth
		alive = (~hit) & (t < tmax)
	if verbose:
		print('  terrain march: %d rounds, %d/%d rays hit' % (rounds, hit.sum(), n))
	return thit, hit


# ------------------------------------------------------ triangle depth buffer
def raster_objects(cam, ob, verbose=True):
	"""Depth buffer over the object triangles.

	Returns tri (h,w) int32 -1 where none, depth (h,w) along the camera
	forward axis, and bary (h,w,3) perspective-correct."""
	h, w = cam.h, cam.w
	sx, sy, zc = cam.project(ob.v.astype(np.float64))
	tri = ob.tri
	a, b, c = tri[:, 0], tri[:, 1], tri[:, 2]
	NEAR = 16.0
	ok = (zc[a] > NEAR) & (zc[b] > NEAR) & (zc[c] > NEAR)
	dropped = int((~ok).sum())
	ti = np.nonzero(ok)[0]
	x0 = np.stack([sx[a[ti]], sx[b[ti]], sx[c[ti]]], 1)
	y0 = np.stack([sy[a[ti]], sy[b[ti]], sy[c[ti]]], 1)
	z0 = np.stack([zc[a[ti]], zc[b[ti]], zc[c[ti]]], 1)
	bx0 = np.clip(np.floor(x0.min(1)).astype(np.int64), 0, w)
	bx1 = np.clip(np.ceil(x0.max(1)).astype(np.int64), 0, w)
	by0 = np.clip(np.floor(y0.min(1)).astype(np.int64), 0, h)
	by1 = np.clip(np.ceil(y0.max(1)).astype(np.int64), 0, h)
	bw = bx1 - bx0
	bh = by1 - by0
	live = (bw > 0) & (bh > 0)
	KEY = np.full(h * w, np.uint64(0xFFFFFFFFFFFFFFFF), dtype=np.uint64)
	span = np.maximum(bw, bh)
	frags = 0
	sizes = np.unique(np.clip(np.ceil(np.log2(np.maximum(span[live], 1))).astype(int), 0, 20))
	for lg in sizes:
		K = 1 << int(lg)
		sel = np.nonzero(live & (np.clip(np.ceil(np.log2(np.maximum(span, 1))).astype(int), 0, 20) == lg))[0]
		if sel.size == 0:
			continue
		# chunk so no bucket allocates more than ~8M fragments at a time
		per = max(1, int(8e6 // (K * K)))
		for s0 in range(0, sel.size, per):
			ss = sel[s0:s0 + per]
			m = ss.size
			gx = bx0[ss][:, None, None] + np.arange(K)[None, None, :]
			gy = by0[ss][:, None, None] + np.arange(K)[None, :, None]
			inb = (gx < bx1[ss][:, None, None]) & (gy < by1[ss][:, None, None])
			px = gx + 0.5
			py = gy + 0.5
			X = x0[ss]; Y = y0[ss]; Z = z0[ss]
			ax, ay = X[:, 0][:, None, None], Y[:, 0][:, None, None]
			bx_, by_ = X[:, 1][:, None, None], Y[:, 1][:, None, None]
			cx_, cy_ = X[:, 2][:, None, None], Y[:, 2][:, None, None]
			area = (bx_ - ax) * (cy_ - ay) - (by_ - ay) * (cx_ - ax)
			w0 = (bx_ - px) * (cy_ - py) - (by_ - py) * (cx_ - px)
			w1 = (cx_ - px) * (ay - py) - (cy_ - py) * (ax - px)
			w2 = (ax - px) * (by_ - py) - (ay - py) * (bx_ - px)
			aa = np.where(np.abs(area) < 1e-12, 1e-12, area)
			l0, l1, l2 = w0 / aa, w1 / aa, w2 / aa
			inside = inb & (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
			iw = l0 / Z[:, 0][:, None, None] + l1 / Z[:, 1][:, None, None] + l2 / Z[:, 2][:, None, None]
			depth = 1.0 / np.where(np.abs(iw) < 1e-12, 1e-12, iw)
			sel_in = np.nonzero(inside.ravel())[0]
			if sel_in.size == 0:
				continue
			frags += sel_in.size
			GXb = np.broadcast_to(gx, inside.shape).ravel()
			GYb = np.broadcast_to(gy, inside.shape).ravel()
			pix = (GYb[sel_in] * w + GXb[sel_in]).astype(np.int64)
			d32 = depth.ravel()[sel_in].astype(np.float32)
			d32 = np.where(d32 > 0, d32, np.float32(1e30))
			key = (d32.view(np.uint32).astype(np.uint64) << np.uint64(32))
			tid = np.broadcast_to(ti[ss][:, None, None], inside.shape).ravel()[sel_in]
			key |= tid.astype(np.uint64)
			np.minimum.at(KEY, pix, key)
	tribuf = np.full(h * w, -1, dtype=np.int64)
	got = KEY != np.uint64(0xFFFFFFFFFFFFFFFF)
	tribuf[got] = (KEY[got] & np.uint64(0xFFFFFFFF)).astype(np.int64)
	dep = np.full(h * w, np.inf)
	dep[got] = (KEY[got] >> np.uint64(32)).astype(np.uint32).view(np.float32).astype(np.float64)
	# recover perspective-correct barycentrics for the winners only
	bary = np.zeros((h * w, 3))
	gi = np.nonzero(got)[0]
	if gi.size:
		tt = tribuf[gi]
		ia, ib, ic = tri[tt, 0], tri[tt, 1], tri[tt, 2]
		px = (gi % w) + 0.5
		py = (gi // w) + 0.5
		ax, ay, az_ = sx[ia], sy[ia], zc[ia]
		bx_, by_, bz = sx[ib], sy[ib], zc[ib]
		cx_, cy_, cz = sx[ic], sy[ic], zc[ic]
		area = (bx_ - ax) * (cy_ - ay) - (by_ - ay) * (cx_ - ax)
		area = np.where(np.abs(area) < 1e-12, 1e-12, area)
		l0 = ((bx_ - px) * (cy_ - py) - (by_ - py) * (cx_ - px)) / area
		l1 = ((cx_ - px) * (ay - py) - (cy_ - py) * (ax - px)) / area
		l2 = 1.0 - l0 - l1
		p0, p1, p2 = l0 / az_, l1 / bz, l2 / cz
		s = p0 + p1 + p2
		s = np.where(np.abs(s) < 1e-18, 1e-18, s)
		bary[gi, 0] = p0 / s
		bary[gi, 1] = p1 / s
		bary[gi, 2] = p2 / s
	if verbose:
		print('  objects: %d tris, %d dropped at the near plane, %.1fM fragments, %d px covered'
			  % (len(tri), dropped, frags / 1e6, int(got.sum())))
	return (tribuf.reshape(h, w), dep.reshape(h, w), bary.reshape(h, w, 3), dropped)


# ---------------------------------------------------------------- the G-buffer
class GBuffer(object):
	"""Everything about a camera that the sun does not change."""

	def __init__(self, cam, ter, ob, verbose=True):
		t0 = time.time()
		d = cam.rays().reshape(-1, 3)
		self.cam = cam
		tt, th = march_terrain(ter, cam.eye, d, verbose=verbose,
							   tmax=getattr(cam, 'tmax', 131072.0),
							   tstart=getattr(cam, 'tstart', 1.0))
		tri, dep, bary, dropped = raster_objects(cam, ob, verbose=verbose)
		n = cam.h * cam.w
		tri = tri.ravel(); dep = dep.ravel(); bary = bary.reshape(-1, 3)
		objfirst = (tri >= 0) & ((~th) | (dep < tt))
		terfirst = th & (~objfirst)
		self.kind = np.zeros(n, dtype=np.int8)      # 0 sky, 1 terrain, 2 object
		self.kind[terfirst] = 1
		self.kind[objfirst] = 2
		self.t = np.where(terfirst, tt, np.where(objfirst, dep, np.inf))
		self.pos = cam.eye[None, :] + d * np.where(np.isfinite(self.t), self.t, 0.0)[:, None]
		self.dir = d
		self.nrm = np.zeros((n, 3)); self.nrm[:, 2] = 1.0
		if terfirst.any():
			self.nrm[terfirst] = ter.normal(self.pos[terfirst, 0], self.pos[terfirst, 1])
		self.tri = tri
		self.bary = bary
		if objfirst.any():
			ia = ob.tri[tri[objfirst]]
			nn = (ob.n[ia[:, 0]] * bary[objfirst, 0:1] + ob.n[ia[:, 1]] * bary[objfirst, 1:2]
				  + ob.n[ia[:, 2]] * bary[objfirst, 2:3])
			ln = np.maximum(np.linalg.norm(nn, axis=1, keepdims=True), 1e-9)
			nn = nn / ln
			# LOD shells are not consistently wound; face the camera
			flip = np.sum(nn * d[objfirst], axis=1) > 0
			nn[flip] *= -1.0
			self.nrm[objfirst] = nn
		self.objfirst = objfirst
		self.terfirst = terfirst
		self.dropped = dropped
		if verbose:
			print('  gbuffer %.1fs: sky %.1f%% terrain %.1f%% object %.1f%%'
				  % (time.time() - t0, 100.0 * (self.kind == 0).mean(),
					 100.0 * terfirst.mean(), 100.0 * objfirst.mean()))


# --------------------------------------------------------- truth: sheared sun
class SunShadow(object):
	"""The suffix-maximum of s = z - u tan(el) along u, on two grids."""

	NEAR_CELL = 16.0
	FAR_CELL = 256.0
	FAR_REACH = 150000.0
	POINT_SPLAT = True

	def __init__(self, ter, ob, az, el, margin=2048.0):
		self.az, self.el = az, el
		a = np.radians(az)
		self.sx, self.sy = np.sin(a), np.cos(a)
		self.tan = np.tan(np.radians(el))
		x0, y0, x1, y1 = CHUNK
		x0 -= margin; y0 -= margin; x1 += margin; y1 += margin
		cor = np.array([[x0, y0], [x1, y0], [x0, y1], [x1, y1]])
		U = cor[:, 0] * self.sx + cor[:, 1] * self.sy
		V = -cor[:, 0] * self.sy + cor[:, 1] * self.sx
		self.nu0, self.nu1 = U.min() - 64, U.max() + 64
		self.nv0, self.nv1 = V.min() - 64, V.max() + 64
		nw = int((self.nu1 - self.nu0) / self.NEAR_CELL) + 1
		nh = int((self.nv1 - self.nv0) / self.NEAR_CELL) + 1
		# --- near grid: terrain sampled at the cell centre + the object triangles
		uu = self.nu0 + (np.arange(nw) + 0.5) * self.NEAR_CELL
		vv = self.nv0 + (np.arange(nh) + 0.5) * self.NEAR_CELL
		UU, VV = np.meshgrid(uu, vv)
		WX = UU * self.sx - VV * self.sy
		WY = UU * self.sy + VV * self.sx
		near = ter.atf(WX, WY) - UU * self.tan
		self._splat_tris(near, ob)
		self.near = np.maximum.accumulate(near[:, ::-1], axis=1)[:, ::-1].astype(np.float32)
		# --- far grid: terrain only, the BILINEAR surface at the cell centre
		self.fu0 = self.nu0 - self.FAR_REACH
		self.fv0 = self.nv0 - self.FAR_REACH
		fw = int((self.nu1 + self.FAR_REACH - self.fu0) / self.FAR_CELL) + 1
		fh = int((self.nv1 + self.FAR_REACH - self.fv0) / self.FAR_CELL) + 1
		fu = self.fu0 + (np.arange(fw) + 0.5) * self.FAR_CELL
		fv = self.fv0 + (np.arange(fh) + 0.5) * self.FAR_CELL
		FU, FV = np.meshgrid(fu, fv)
		FX = FU * self.sx - FV * self.sy
		FY = FU * self.sy + FV * self.sx
		# NOT the 256-unit max-Z mip.  With the mip this grid WAS the error
		# budget of the whole cast: against brute force on 900 random ground
		# points at azimuth 240 / elevation 15 the near grid alone agreed 96.7%
		# and the mip far grid dragged the pair to 89.6%, always by declaring
		# MORE shadow -- a block maximum shadows you with a hilltop the ray
		# never passes.  A bilinear tap every FAR_CELL along the sun line, then
		# the suffix maximum, is the march the brute force does.
		far = ter.atf(FX, FY) - FU * self.tan
		self.far = np.maximum.accumulate(far[:, ::-1], axis=1)[:, ::-1].astype(np.float32)
		self.nw, self.nh, self.fw, self.fh = nw, nh, fw, fh

	def _splat_tris(self, near, ob):
		"""Rasterise every triangle into the near grid, taking the max of s.

		Cell centres inside the triangle plus the three vertices and the three
		edge midpoints, so a triangle thinner than a cell still occludes."""
		v = ob.v.astype(np.float64)
		U = v[:, 0] * self.sx + v[:, 1] * self.sy
		V = -v[:, 0] * self.sy + v[:, 1] * self.sx
		S = v[:, 2] - U * self.tan
		gx = (U - self.nu0) / self.NEAR_CELL
		gy = (V - self.nv0) / self.NEAR_CELL
		t = ob.tri
		# point samples first (vertices and edge midpoints)
		pts_x = [gx]
		pts_y = [gy]
		pts_s = [S]
		for i, j in ((0, 1), (1, 2), (2, 0)):
			pts_x.append(0.5 * (gx[t[:, i]] + gx[t[:, j]]))
			pts_y.append(0.5 * (gy[t[:, i]] + gy[t[:, j]]))
			pts_s.append(0.5 * (S[t[:, i]] + S[t[:, j]]))
		if self.POINT_SPLAT:
			AX = np.concatenate(pts_x); AY = np.concatenate(pts_y); AS = np.concatenate(pts_s)
			self._scatter(near, AX, AY, AS)
		# then the interiors, bucketed by bounding-box size
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
				cx3, cy3, cs = gx[t[ss, 2]][:, None, None], gy[t[ss, 2]][:, None, None], S[t[ss, 2]][:, None, None]
				area = (bx - ax) * (cy3 - ay) - (by - ay) * (cx3 - ax)
				aa = np.where(np.abs(area) < 1e-12, 1e-12, area)
				w0 = ((bx - cx) * (cy3 - cy) - (by - cy) * (cx3 - cx)) / aa
				w1 = ((cx3 - cx) * (ay - cy) - (cy3 - cy) * (ax - cx)) / aa
				w2 = 1.0 - w0 - w1
				ins = ((w0 >= 0) & (w1 >= 0) & (w2 >= 0)
					   & (GX < x1[ss][:, None, None]) & (GY < y1[ss][:, None, None]))
				ins = np.broadcast_to(ins, (len(ss), K, K)) if ins.shape != (len(ss), K, K) else ins
				k = np.nonzero(ins.ravel())[0]
				if k.size == 0:
					continue
				sv = (w0 * asv + w1 * bs + w2 * cs).ravel()[k]
				GXb = np.broadcast_to(GX, ins.shape).ravel()
				GYb = np.broadcast_to(GY, ins.shape).ravel()
				self._scatter(near, GXb[k].astype(np.float64),
							  GYb[k].astype(np.float64), sv, already_cell=True)

	def _scatter(self, near, gx, gy, s, already_cell=False):
		ix = gx.astype(np.int64) if already_cell else np.floor(gx).astype(np.int64)
		iy = gy.astype(np.int64) if already_cell else np.floor(gy).astype(np.int64)
		ok = (ix >= 0) & (ix < near.shape[1]) & (iy >= 0) & (iy < near.shape[0])
		np.maximum.at(near, (iy[ok], ix[ok]), s[ok])

	def lit(self, pos, bias=12.0):
		"""True where the point can see the sun."""
		x, y, z = pos[:, 0], pos[:, 1], pos[:, 2]
		u = x * self.sx + y * self.sy
		v = -x * self.sy + y * self.sx
		s = z - u * self.tan + bias
		blocked = np.zeros(len(x), dtype=bool)
		# near grid, one cell forward along u so a surface cannot shade itself
		ix = np.floor((u - self.nu0) / self.NEAR_CELL).astype(np.int64) + 1
		iy = np.floor((v - self.nv0) / self.NEAR_CELL).astype(np.int64)
		ok = (ix >= 0) & (ix < self.nw) & (iy >= 0) & (iy < self.nh)
		if ok.any():
			blocked[ok] |= self.near[iy[ok], ix[ok]] > s[ok]
		fx = np.floor((u - self.fu0) / self.FAR_CELL).astype(np.int64) + 1
		fy = np.floor((v - self.fv0) / self.FAR_CELL).astype(np.int64)
		ok = (fx >= 0) & (fx < self.fw) & (fy >= 0) & (fy < self.fh)
		if ok.any():
			blocked[ok] |= self.far[fy[ok], fx[ok]] > s[ok]
		return ~blocked


# ------------------------------------------------------------- baked-data sun
def baked_lit(gb, ob, sheet, az, el, soft=1.0):
	"""Lit fraction from the BAKED bytes only.  NaN where the bake says nothing."""
	n = len(gb.kind)
	hz = np.full(n, np.nan)
	ti = gb.terfirst
	if ti.any():
		hz[ti] = sheet.elev_at(gb.pos[ti, 0], gb.pos[ti, 1], az)
	oi = gb.objfirst
	if oi.any():
		A = 16
		step = 360.0 / A
		t = az / step
		t -= np.floor(t / A) * A
		k0 = int(t) % A
		k1 = (k0 + 1) % A
		f = t - np.floor(t)
		ia = ob.tri[gb.tri[oi]]
		deg = np.zeros(int(oi.sum()))
		for k in range(3):
			b = ob.bins[ia[:, k]]
			e = (b[:, k0] * (1.0 - f) + b[:, k1] * f) / 255.0 * 90.0
			deg += gb.bary[oi, k] * e
		hz[oi] = deg
	return np.clip((el - hz) / soft + 0.5, 0.0, 1.0), hz
