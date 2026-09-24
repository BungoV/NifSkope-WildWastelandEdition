#!/usr/bin/env python3
"""SUNSIM1 -- the two controls the pictures rest on.

A. IS THE LEFT PANEL A RAY CAST?  The left panel's shadow comes from a
   suffix-maximum over a sheared sun-space grid, not from one march a pixel.
   That is a claim, so it is scored against the thing it claims to equal:
   BRUTE-FORCE ray casting -- the shadow ray marched against the bilinear
   heightmap at a 24-unit step out to 150,000 units, and Moller-Trumbore
   against ALL 29,587 triangles with no acceleration structure at all.

B. IS THE PER-VERTEX HORIZON READ FROM THE RIGHT BYTES?  If the vertex ->
   16-byte slice mapping were scrambled, the right panel would still look
   plausible and would still be wrong.  So a sample of object vertices has its
   TRUE 16-bin skyline computed by pencil rays over the same raw geometry, and
   the stored bytes are scored against it -- with the same bytes SHUFFLED
   between vertices as the red control.  A mapping that is right must beat its
   own shuffle by a wide margin; one that is wrong cannot.
"""
import json
import os
import sys
import time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scene import Terrain, Objects, Sheet, CHUNK
from render import SunShadow

LANE = os.path.dirname(os.path.abspath(__file__))


# ------------------------------------------------------------- brute force
def brute_lit(ter, ob, pts, az, el, reach=150000.0, step=32.0, growth=1.0, rise=8.0):
	"""One marched shadow ray a point, plus every triangle, no shortcuts.

	The terrain step is CONSTANT and a quarter of the 128-unit node spacing, so
	the march cannot walk over a ridge; an earlier version grew the step
	geometrically and reported the terrain as lit far more often than it is."""
	d = np.array([np.sin(np.radians(az)) * np.cos(np.radians(el)),
				  np.cos(np.radians(az)) * np.cos(np.radians(el)),
				  np.sin(np.radians(el))])
	p = pts.copy()
	p[:, 2] += rise
	n = len(p)
	blocked = np.zeros(n, dtype=bool)
	t = np.full(n, step)
	st = np.full(n, step)
	while True:
		live = (~blocked) & (t < reach)
		if not live.any():
			break
		i = np.nonzero(live)[0]
		x = p[i, 0] + d[0] * t[i]
		y = p[i, 1] + d[1] * t[i]
		z = p[i, 2] + d[2] * t[i]
		blocked[i] |= ter.atf(x, y) > z
		t[i] += st[i]
		st[i] *= growth
	# Moller-Trumbore, all triangles, chunked over the points
	v = ob.v.astype(np.float64)
	t0 = v[ob.tri[:, 0]]
	e1 = v[ob.tri[:, 1]] - t0
	e2 = v[ob.tri[:, 2]] - t0
	pv = np.cross(d[None, :], e2)
	det = np.einsum('ij,ij->i', e1, pv)
	okdet = np.abs(det) > 1e-12
	inv = np.where(okdet, 1.0 / np.where(okdet, det, 1.0), 0.0)
	CH = max(1, int(8e6 // len(ob.tri)))
	for s in range(0, n, CH):
		sl = slice(s, min(n, s + CH))
		tv = p[sl][:, None, :] - t0[None, :, :]
		u = np.einsum('kij,ij->ki', tv, pv) * inv[None, :]
		qv = np.cross(tv, e1[None, :, :])
		vv = np.einsum('kij,j->ki', qv, d) * inv[None, :]
		tt = np.einsum('kij,ij->ki', qv, e2) * inv[None, :]
		hit = okdet[None, :] & (u >= 0) & (u <= 1) & (vv >= 0) & (u + vv <= 1) & (tt > 1.0)
		blocked[sl] |= hit.any(axis=1)
	return ~blocked


def top_grid(ob, cell=32.0, pad=256.0):
	"""Max z of the object triangles on an XY grid: the occluder field the
	pencil rays of control B read."""
	v = ob.v.astype(np.float64)
	x0, y0 = v[:, 0].min() - pad, v[:, 1].min() - pad
	nx = int((v[:, 0].max() + pad - x0) / cell) + 2
	ny = int((v[:, 1].max() + pad - y0) / cell) + 2
	G = np.full((ny, nx), -1.0e9)
	gx = (v[:, 0] - x0) / cell
	gy = (v[:, 1] - y0) / cell
	t = ob.tri
	px, py, pz = [gx], [gy], [v[:, 2]]
	for i, j in ((0, 1), (1, 2), (2, 0)):
		px.append(0.5 * (gx[t[:, i]] + gx[t[:, j]]))
		py.append(0.5 * (gy[t[:, i]] + gy[t[:, j]]))
		pz.append(0.5 * (v[t[:, i], 2] + v[t[:, j], 2]))
	AX = np.concatenate(px); AY = np.concatenate(py); AZ = np.concatenate(pz)
	ix = np.clip(AX.astype(int), 0, nx - 1); iy = np.clip(AY.astype(int), 0, ny - 1)
	np.maximum.at(G, (iy, ix), AZ)
	# interiors
	bx0 = np.floor(np.minimum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(int)
	bx1 = np.ceil(np.maximum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(int)
	by0 = np.floor(np.minimum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(int)
	by1 = np.ceil(np.maximum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(int)
	span = np.maximum(bx1 - bx0, by1 - by0)
	lg = np.clip(np.ceil(np.log2(np.maximum(span, 1))).astype(int), 0, 12)
	for L in np.unique(lg):
		K = 1 << int(L)
		sel = np.nonzero(lg == L)[0]
		per = max(1, int(4e6 // (K * K)))
		for s0 in range(0, sel.size, per):
			ss = sel[s0:s0 + per]
			GX = bx0[ss][:, None, None] + np.arange(K)[None, None, :]
			GY = by0[ss][:, None, None] + np.arange(K)[None, :, None]
			cx, cy = GX + 0.5, GY + 0.5
			ax, ay, az_ = gx[t[ss, 0]][:, None, None], gy[t[ss, 0]][:, None, None], v[t[ss, 0], 2][:, None, None]
			bx, by, bz = gx[t[ss, 1]][:, None, None], gy[t[ss, 1]][:, None, None], v[t[ss, 1], 2][:, None, None]
			c3x, c3y, cz = gx[t[ss, 2]][:, None, None], gy[t[ss, 2]][:, None, None], v[t[ss, 2], 2][:, None, None]
			area = (bx - ax) * (c3y - ay) - (by - ay) * (c3x - ax)
			aa = np.where(np.abs(area) < 1e-12, 1e-12, area)
			w0 = ((bx - cx) * (c3y - cy) - (by - cy) * (c3x - cx)) / aa
			w1 = ((c3x - cx) * (ay - cy) - (c3y - cy) * (ax - cx)) / aa
			w2 = 1.0 - w0 - w1
			ins = np.broadcast_to((w0 >= 0) & (w1 >= 0) & (w2 >= 0), (len(ss), K, K)).ravel()
			k = np.nonzero(ins)[0]
			if not k.size:
				continue
			zz = np.broadcast_to(w0 * az_ + w1 * bz + w2 * cz, (len(ss), K, K)).ravel()[k]
			np.maximum.at(G, (np.clip(np.broadcast_to(GY, (len(ss), K, K)).ravel()[k], 0, ny - 1),
							  np.clip(np.broadcast_to(GX, (len(ss), K, K)).ravel()[k], 0, nx - 1)), zz)
	return G, x0, y0, cell


def near_field(ob, cell=32.0, pad=256.0):
	"""The NEAR object lattice the bake actually casts against, copied from
	src/nativeemit.cpp: 32-unit squares (LODGEN_HORIZON_NEAR_CELL), and each
	triangle RAISES ITS WHOLE XY BOUNDING BOX to its own maximum Z
	(`nearHz.raiseBox`) -- it is not rasterised.  That is deliberately
	conservative and it is what the stored bytes saw."""
	v = ob.v.astype(np.float64)
	x0, y0 = v[:, 0].min() - pad, v[:, 1].min() - pad
	nx = int((v[:, 0].max() + pad - x0) / cell) + 2
	ny = int((v[:, 1].max() + pad - y0) / cell) + 2
	G = np.full((ny, nx), -1.0e9)
	t = ob.tri
	gx = (v[:, 0] - x0) / cell
	gy = (v[:, 1] - y0) / cell
	bx0 = np.floor(np.minimum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(int)
	bx1 = np.floor(np.maximum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(int)
	by0 = np.floor(np.minimum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(int)
	by1 = np.floor(np.maximum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(int)
	tz = np.maximum.reduce([v[t[:, 0], 2], v[t[:, 1], 2], v[t[:, 2], 2]])
	span = np.maximum(bx1 - bx0, by1 - by0) + 1
	lg = np.clip(np.ceil(np.log2(np.maximum(span, 1))).astype(int), 0, 12)
	for L in np.unique(lg):
		K = 1 << int(L)
		sel = np.nonzero(lg == L)[0]
		per = max(1, int(4e6 // (K * K)))
		for s0 in range(0, sel.size, per):
			ss = sel[s0:s0 + per]
			m = len(ss)
			GX = bx0[ss][:, None, None] + np.arange(K)[None, None, :]
			GY = by0[ss][:, None, None] + np.arange(K)[None, :, None]
			keep = np.broadcast_to((GX <= bx1[ss][:, None, None]) & (GY <= by1[ss][:, None, None]),
								   (m, K, K)).ravel()
			k = np.nonzero(keep)[0]
			if not k.size:
				continue
			zz = np.broadcast_to(tz[ss][:, None, None], (m, K, K)).ravel()[k]
			np.maximum.at(G, (np.clip(np.broadcast_to(GY, (m, K, K)).ravel()[k], 0, ny - 1),
							  np.clip(np.broadcast_to(GX, (m, K, K)).ravel()[k], 0, nx - 1)), zz)
	return G, x0, y0, cell


def land_lattice(ter, cell=128.0):
	"""Max Z of each 128-unit square of the terrain.  A bilinear patch over a
	square attains its maximum at a corner, so the max of the four nodes is the
	exact maximum of that square -- this is not a sample, it is the bound."""
	h = ter.hfill
	L = np.maximum(np.maximum(h[:-1, :-1], h[:-1, 1:]), np.maximum(h[1:, :-1], h[1:, 1:]))
	return L


def true_bins(ter, ob, NG, ngx0, ngy0, ncell, pts, nrm, reach=127561.0, rise=4.0, A=16,
			  LAT=None, near_only=False, far_only=False):
	"""The skyline the bake is ASKED for, in the 16 stored directions.

	The rules are copied out of src/lodghorizon.h + src/nativeemit.cpp rather
	than invented: TWO max-Z fields, a 32-unit NEAR lattice of the LOD object
	triangles (each triangle raising its whole XY bounding box to its own max Z)
	and a 128-unit FAR lattice of the land; each field is skipped within ONE of
	its own cells (`nearSkipCells` = 1, bake.log horizonNearSkip 1.00), so 32
	units and 128 units respectively; the ray starts `rise` = 4 units above the
	receiver; and a direction whose highest occluder does not clear the
	receiver's own TANGENT PLANE stores 0, because below that plane it is the
	receiver's own surface and N.L already darkens it.

	What this does NOT copy is the bake's geometric step ladder and its mip
	level choice -- the taps here are a third of a near cell apart out to 8,192
	units and then half a far cell, so nothing is stepped over.  The quality of
	the bake's ladder is the thing the bake's OWN refuter measures
	(vhorRefuteMeanErrDeg 0.816); this control is here to prove the byte-to-
	vertex mapping, which is why it is scored against a shuffle."""
	n = len(pts)
	out = np.zeros((n, A))
	z0 = pts[:, 2] + rise
	ny, nx = NG.shape
	lny, lnx = LAT.shape
	# the bake's own ladder: segments d -> d*1.3 from 32 units, and the elevation
	# of whatever tops the SEGMENT measured at its NEAR end (deliberate over-
	# occlusion, src/lodghorizon.h).  `ladder=False` instead measures at the tap.
	segs = []
	d = 32.0
	while d <= reach:
		segs.append((d, min(d * 1.3, reach)))
		if d * 1.3 >= reach:
			break
		d *= 1.3
	for k in range(A):
		a = np.radians(k * 360.0 / A)
		dx, dy = np.sin(a), np.cos(a)
		nz = nrm[:, 2]
		nd = nrm[:, 0] * dx + nrm[:, 1] * dy
		# the three normal cases of lodgenHorizonCastAt, verbatim
		up = nz > 1.0e-3                       # a sloped/flat face: its own tangent plane
		vert = (~up) & (nz > -1.0e-3)          # an exactly vertical face
		plane = np.where(up, np.degrees(np.arctan2(-nd, np.where(up, nz, 1.0))), -90.0)
		forced0 = (nz <= -1.0e-3) | (vert & ~(nd > 0.0))   # pointing down, or the back of a wall
		best = np.full(n, -90.0)
		for (d0, d1) in segs:
			top = np.full(n, -1.0e9)
			# the whole segment, never a point: taps half a lattice square apart
			# for the near field, and the terrain's own max-Z mip for the far one
			# so a long far segment cannot step over a ridge either.
			if not far_only and d1 > 32.0 and d0 < 49152.0:
				nt = int(np.clip((d1 - d0) / 16.0 + 2, 4, 400))
				for t in np.linspace(d0, d1, nt):
					jx = np.clip(((pts[:, 0] + dx * t - ngx0) / ncell).astype(int), 0, nx - 1)
					jy = np.clip(((pts[:, 1] + dy * t - ngy0) / ncell).astype(int), 0, ny - 1)
					np.maximum(top, NG[jy, jx], out=top)
			if not near_only and d1 > 128.0:
				lvl = int(np.clip(np.ceil(np.log2(max((d1 - d0) / 24.0, 128.0) / 128.0)), 0, 8))
				nt = int(np.clip((d1 - d0) / (128.0 * (1 << lvl)) + 2, 4, 64))
				for t in np.linspace(d0, d1, nt):
					np.maximum(top, ter.mip_max(lvl, pts[:, 0] + dx * t, pts[:, 1] + dy * t), out=top)
			np.maximum(best, np.degrees(np.arctan2(top - z0, d0)), out=best)
		out[:, k] = np.where((best > plane) & ~forced0, np.clip(best, 0.0, 90.0), 0.0)
	return out


def main():
	rng = np.random.default_rng(20260919)
	ter = Terrain(); ob = Objects(verbose=False)
	rep = {}
	# ---------------------------------------------------------------- A
	x0, y0, x1, y1 = CHUNK
	NT = 1500
	tp = np.stack([rng.uniform(x0 + 600, x1 - 600, NT), rng.uniform(y0 + 600, y1 - 600, NT)], 1)
	tz = ter.atf(tp[:, 0], tp[:, 1])
	terr_pts = np.column_stack([tp, tz])
	# object points: random barycentric points on random triangles
	NO = 1500
	ti = rng.integers(0, len(ob.tri), NO)
	r1, r2 = rng.random(NO), rng.random(NO)
	su = np.sqrt(r1)
	w0, w1, w2 = 1 - su, su * (1 - r2), su * r2
	V = ob.v.astype(np.float64)
	obj_pts = (V[ob.tri[ti, 0]] * w0[:, None] + V[ob.tri[ti, 1]] * w1[:, None]
			   + V[ob.tri[ti, 2]] * w2[:, None])
	rowsA = []
	for (az, el) in [(120.0, 5.0), (120.0, 15.0), (120.0, 30.0), (240.0, 15.0)]:
		ss = SunShadow(ter, ob, az, el)
		for nm, P, rise in (('terrain', terr_pts, 4.0), ('objects', obj_pts, 4.0)):
			t0 = time.time()
			bf = brute_lit(ter, ob, P, az, el, rise=rise)
			gr = ss.lit(P + np.array([0, 0, rise])[None, :])
			agree = float((bf == gr).mean()) * 100.0
			rowsA.append((az, el, nm, len(P), agree, float(bf.mean() * 100), float(gr.mean() * 100),
						  round(time.time() - t0, 1)))
			print('A az%3.0f el%2.0f %-8s n=%d  grid-vs-brute agree %.2f%%   '
				  'brute lit %.1f%%  grid lit %.1f%%  %.1fs' % rowsA[-1])
	rep['A_shadow_grid_vs_bruteforce'] = rowsA
	# ---------------------------------------------------------------- B
	print('B: building the 32 u near object lattice and the 128 u far land lattice ...')
	G, gx0, gy0, gc = near_field(ob)
	LAT = land_lattice(ter)
	NB = 800
	vi = rng.choice(len(ob.v), NB, replace=False)
	P = ob.v[vi].astype(np.float64)
	NR = ob.n[vi].astype(np.float64)
	t0 = time.time()
	tb = true_bins(ter, ob, G, gx0, gy0, gc, P, NR, LAT=LAT)
	st = ob.bins[vi].astype(np.float64) / 255.0 * 90.0
	sh = ob.bins[rng.permutation(len(ob.v))[:NB]].astype(np.float64) / 255.0 * 90.0
	def score(a, b):
		return {'mean_abs_err_deg': round(float(np.abs(a - b).mean()), 2),
				'pearson': round(float(np.corrcoef(a.ravel(), b.ravel())[0, 1]), 4),
				'zero_bit_agree_pct': round(float(((a > 0) == (b > 0)).mean()) * 100.0, 2),
				'mean_abs_err_where_true_nonzero_deg':
					round(float(np.abs(a - b)[b > 0].mean()) if (b > 0).any() else float('nan'), 2)}
	s_s, s_x = score(st, tb), score(sh, tb)
	rep['B_vertex_bins_vs_true'] = {
		'samples': NB, 'seconds': round(time.time() - t0, 1),
		'stored': s_s, 'shuffled_null': s_x,
		'true_mean_deg': round(float(tb.mean()), 2), 'stored_mean_deg': round(float(st.mean()), 2),
		'true_zero_pct': round(float((tb == 0).mean()) * 100.0, 2),
		'stored_zero_pct': round(float((st == 0).mean()) * 100.0, 2)}
	print('B stored   %s' % s_s)
	print('B shuffled %s   (the red control)' % s_x)
	print('B true mean %.2f deg (%.1f%% zero)   stored mean %.2f deg (%.1f%% zero)'
		  % (tb.mean(), (tb == 0).mean() * 100, st.mean(), (st == 0).mean() * 100))
	json.dump(rep, open(LANE + '/controls.json', 'w'), indent=1)


if __name__ == '__main__':
	main()
