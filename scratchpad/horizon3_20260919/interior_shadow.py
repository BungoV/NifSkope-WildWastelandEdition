#!/usr/bin/env python
"""HORIZON3 step 1(c) -- does a shadow ever land in the MIDDLE of a big face?

That is the only question that justifies tier 3 (a per-face horizon texture).
A shadow whose lit/dark boundary always touches an edge of the face can be
resolved by putting vertices on that edge, which is tier 2 and far cheaper.

The instrument is HORIZON2's THIRD WITNESS -- `scratchpad/horizon2_20260918/wit.py`'s
raw BTD heightmap (`land.bin`, bilinear, no lattice, no mip, no maximum-mipmap)
plus the placements as exact world AABBs decoded by
`tests/spells/lodgen_native_decode.py`.  It shares no code with the march.

TWO DEPARTURES FROM `wit.true_skyline`, both stated because both could hide a
bug:

 1. it is vectorised over RECEIVERS at one azimuth instead of over azimuths at
    one receiver.  CONTROL: `--control-wit` runs both on HORIZON2's ten
    receivers and prints the worst disagreement, which must be ~0.
 2. the march is CLIPPED at the distance beyond which nothing can reach the sun
    elevation -- `(max terrain z - z0)/tan(e)` for the terrain and
    `(max box top - z0)/tan(e)` for the boxes.  This is exact, not an
    approximation: past that distance the steepest possible angle is below the
    sun and the sample is lit whatever is there.

KNOWN-ANSWER CONTROL for the DETECTOR itself (`--control-synth`): a flat
4,096 x 4,096 deck with one synthetic tower, placed twice -- once at the deck's
south rim, where the shadow must run off the edge (interior = NO), and once in
the middle with the sun high enough that the shadow ends before the rim
(interior = YES).  A detector that cannot tell those two apart is not a
detector.

usage: interior_shadow.py --faces tiers_urban.json [--control-wit] [--control-synth]
"""
import argparse
import json
import math
import os
import sys
from collections import defaultdict

import numpy as np

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/horizon3_20260919'
H2 = ROOT + '/scratchpad/horizon2_20260918'
sys.path.insert(0, ROOT + '/tests/spells')
sys.path.insert(0, H2)
import lodgen_native_decode as D  # noqa: E402
import wit  # noqa: E402   (HORIZON2's third witness: Land, true_skyline)

NAT = (ROOT + '/scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/'
	   'Commonwealth/Commonwealth')
SAMPLE = 64.0
SUN_ELEV = [5.0, 15.0, 30.0]
SUN_AZIM = [120.0, 240.0]
STEP = 32.0
REACH_CAP = 127561.0        # HORIZON1 s1b: the tallest placement at a 5 deg sun
LOCAL_INDEX_BYTES = 48
LOCAL_INDEX_NONE = 0xFF


def dequant(v, mn, ext):
	return mn + (v / 65535.0) * ext


def corridor(pat, dx, dy, reach):
	"""The world rectangle every ray from the sample patch can touch."""
	x0, y0, x1, y1 = pat
	return (min(x0, x0 + dx * reach) - 128.0, min(y0, y0 + dy * reach) - 128.0,
			max(x1, x1 + dx * reach) + 128.0, max(y1, y1 + dy * reach) + 128.0)


def land_max_in(land, c):
	"""The tallest LAND node inside a world rectangle."""
	x0, y0, x1, y1 = c
	i0 = int(max(0, math.floor((x0 - land.ox) / land.NODE)))
	i1 = int(min(land.nx - 1, math.ceil((x1 - land.ox) / land.NODE)))
	j0 = int(max(0, math.floor((y0 - land.oy) / land.NODE)))
	j1 = int(min(land.ny - 1, math.ceil((y1 - land.oy) / land.NODE)))
	if i1 < i0 or j1 < j0:
		return float('nan')
	sub = land.h[j0:j1 + 1, i0:i1 + 1]
	if not np.isfinite(sub).any():
		return float('nan')
	return float(np.nanmax(sub))


# ------------------------------------------------------------------ the cast

def skyline_along(land, P, az_deg, boxes, box_own=None, terr_max=None,
				  box_max=None, sun_elev=0.0):
	"""Max skyline elevation, in degrees, at every receiver in P (N,3), along
	ONE azimuth.  `boxes` is (M,5) x0,y0,x1,y1,top.  `box_own` is an index whose
	box is excluded (a face does not shadow itself: the horizon stream never
	stores the receiver's own surface, §4.11)."""
	a = math.radians(az_deg)
	dx, dy = math.sin(a), math.cos(a)          # bin 0 = +Y, clockwise to +X
	z0 = P[:, 2]
	best = np.zeros(len(P))
	t = max(math.tan(math.radians(max(sun_elev, 0.05))), 1e-4)
	pat = (P[:, 0].min(), P[:, 1].min(), P[:, 0].max(), P[:, 1].max())

	# --- terrain, bilinear at the ray's own position, constant 32 u step
	if terr_max is not None:
		# The reach is refined DOWNWARD against the tallest thing actually
		# inside the corridor: each pass is still an upper bound, because a
		# shorter corridor can only contain a lower maximum.  Nothing beyond
		# `reach` can subtend the sun elevation, so this is exact, not a
		# tolerance.
		reach = REACH_CAP
		for _ in range(5):
			c = corridor(pat, dx, dy, reach)
			tm = land_max_in(land, c)
			if not np.isfinite(tm):
				reach = 0.0
				break
			r2 = float(np.clip((tm - z0.min()) / t, 0.0, REACH_CAP))
			if r2 >= reach - STEP:
				reach = r2
				break
			reach = r2
		if reach > STEP:
			d = np.arange(STEP, reach + STEP, STEP)
			for k in range(0, len(d), 512):
				dd = d[k:k + 512]
				X = P[:, 0][:, None] + dx * dd[None, :]
				Y = P[:, 1][:, None] + dy * dd[None, :]
				Z = land.at(X, Y)
				e = np.degrees(np.arctan2(Z - z0[:, None], dd[None, :]))
				e = np.where(np.isnan(Z), -90.0, e)
				best = np.maximum(best, e.max(axis=1))

	# --- boxes, exact slab test, pruned to the corridor the rays can reach
	if boxes is not None and len(boxes):
		reach = REACH_CAP
		keep = None
		for _ in range(5):
			cx0, cy0, cx1, cy1 = corridor(pat, dx, dy, reach)
			keep = ((boxes[:, 2] >= cx0) & (boxes[:, 0] <= cx1)
					& (boxes[:, 3] >= cy0) & (boxes[:, 1] <= cy1))
			if box_own is not None:
				keep[box_own] = False
			if not keep.any():
				break
			bm = float(boxes[keep, 4].max())
			r2 = float(np.clip((bm - z0.min()) / t, 0.0, REACH_CAP))
			if r2 >= reach - STEP:
				reach = r2
				break
			reach = r2
		B = boxes[keep] if keep is not None else boxes[:0]
		if len(B):
			for k in range(0, len(P), 256):
				p = P[k:k + 256]
				be = _box_elev_many(B, p, dx, dy, reach)
				best[k:k + 256] = np.maximum(best[k:k + 256], be)
	return np.maximum(best, 0.0)


def _box_elev_many(B, P, dx, dy, reach):
	"""Steepest elevation any box subtends at its own NEAREST distance along
	the ray, for every receiver at once.  Exact slab test, no lattice.
	Transcribed from wit._box_elev; a box CONTAINING the receiver reads 90 deg,
	which is what a box says and is why the own-placement box is excluded."""
	inf = 1.0e30
	px = P[:, 0][:, None]
	py = P[:, 1][:, None]
	z0 = P[:, 2][:, None]
	x0, y0, x1, y1, top = B[:, 0], B[:, 1], B[:, 2], B[:, 3], B[:, 4]
	with np.errstate(divide='ignore', invalid='ignore'):
		if dx != 0.0:
			tx0 = (x0[None, :] - px) / dx
			tx1 = (x1[None, :] - px) / dx
			txn = np.minimum(tx0, tx1)
			txf = np.maximum(tx0, tx1)
		else:
			inside = (px >= x0[None, :]) & (px <= x1[None, :])
			txn = np.where(inside, -inf, inf)
			txf = np.where(inside, inf, inf)
		if dy != 0.0:
			ty0 = (y0[None, :] - py) / dy
			ty1 = (y1[None, :] - py) / dy
			tyn = np.minimum(ty0, ty1)
			tyf = np.maximum(ty0, ty1)
		else:
			inside = (py >= y0[None, :]) & (py <= y1[None, :])
			tyn = np.where(inside, -inf, inf)
			tyf = np.where(inside, inf, inf)
	tnear = np.maximum(txn, tyn)
	tfar = np.minimum(txf, tyf)
	hit = (tfar >= np.maximum(tnear, 0.0)) & (tnear <= reach)
	d = np.maximum(tnear, 1.0)
	e = np.degrees(np.arctan2(top[None, :] - z0, d))
	e = np.where(hit & (top[None, :] > z0), e, -90.0)
	return e.max(axis=1)


# ------------------------------------------------------------- the detector

def interior_components(inside, lit):
	"""`inside` and `lit` are 2-D grids over the face's own (u,v) sample
	lattice.  A BOUNDARY sample is an inside sample with an inside 4-neighbour
	of the other lit value.  A boundary sample is a RIM sample if any of its
	four neighbours is outside the face or off the grid.  Returns
	(components, interior_components, largest_interior)."""
	H, W = inside.shape
	bnd = np.zeros_like(inside)
	rim = np.zeros_like(inside)
	for (dy, dx) in ((0, 1), (0, -1), (1, 0), (-1, 0)):
		sh = np.roll(np.roll(inside, dy, 0), dx, 1)
		sl = np.roll(np.roll(lit, dy, 0), dx, 1)
		if dy == 1:
			sh[0, :] = False
		elif dy == -1:
			sh[-1, :] = False
		if dx == 1:
			sh[:, 0] = False
		elif dx == -1:
			sh[:, -1] = False
		bnd |= inside & sh & (lit != sl)
		rim |= inside & ~sh
	comps = 0
	inter = 0
	largest = 0
	seen = np.zeros_like(inside)
	for j in range(H):
		for i in range(W):
			if not bnd[j, i] or seen[j, i]:
				continue
			stack = [(j, i)]
			seen[j, i] = True
			touches = False
			n = 0
			while stack:
				(y, x) = stack.pop()
				n += 1
				if rim[y, x]:
					touches = True
				for ddy in (-1, 0, 1):
					for ddx in (-1, 0, 1):
						yy, xx = y + ddy, x + ddx
						if 0 <= yy < H and 0 <= xx < W and bnd[yy, xx] and not seen[yy, xx]:
							seen[yy, xx] = True
							stack.append((yy, xx))
			comps += 1
			if not touches:
				inter += 1
				largest = max(largest, n)
	return comps, inter, largest


def point_in_tri(pu, pv, tri):
	(ax, ay), (bx, by), (cx, cy) = tri
	d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
	if abs(d) < 1e-12:
		return np.zeros(pu.shape, dtype=bool)
	l1 = ((by - cy) * (pu - cx) + (cx - bx) * (pv - cy)) / d
	l2 = ((cy - ay) * (pu - cx) + (ax - cx) * (pv - cy)) / d
	l3 = 1.0 - l1 - l2
	return (l1 >= -1e-9) & (l2 >= -1e-9) & (l3 >= -1e-9)


# ------------------------------------------------------------------ controls

def control_wit(land, boxes):
	"""Departure 1: my cast is vectorised over receivers.  It must agree with
	wit.true_skyline, which is vectorised over azimuths, on the same data."""
	rec = json.load(open(H2 + '/receivers.json'))
	rows = rec['receivers'] if isinstance(rec, dict) else rec
	worst = 0.0
	n = 0
	for r in rows:
		x, y = float(r['x']), float(r['y'])
		z = float(r.get('z', r.get('ground', 0.0)))
		for az in (0.0, 45.0, 120.0, 240.0, 315.0):
			a = wit.true_skyline(land, x, y, z, np.array([az]), boxes=boxes)[0]
			b = skyline_along(land, np.array([[x, y, z]]), az, boxes,
							  terr_max=float(np.nanmax(land.h)),
							  box_max=float(boxes[:, 4].max()), sun_elev=0.05)[0]
			worst = max(worst, abs(a - b))
			n += 1
	return worst, n


def control_synth():
	"""Known-answer control for the DETECTOR: the same deck, the same tower,
	moved.  One must come back interior = NO, the other interior = YES."""
	out = []
	for (name, tx, ty, top, elev, expect) in (
			# a TALL tower at the rim: its shadow runs off the deck's own edge,
			# so tier 2 (vertices on that edge) would resolve it -> interior NO
			('tall tower at the SOUTH RIM, sun 15', 2048.0, 120.0, 3000.0, 15.0, 0),
			# a LOW tower in the middle: 500/tan(30) = 866 u of shadow, which
			# ends 926 u short of the rim -> interior YES, and no edge of the
			# face can carry it
			('low tower in the MIDDLE, sun 30', 2048.0, 2048.0, 500.0, 30.0, 1)):
		W = int(4096 // SAMPLE)
		u = (np.arange(W) + 0.5) * SAMPLE
		U, V = np.meshgrid(u, u)
		inside = np.ones(U.shape, dtype=bool)
		# one synthetic tower, 512 wide, 3,000 units above the deck
		box = np.array([[tx - 256, ty - 256, tx + 256, ty + 256, top]])
		P = np.stack([U.ravel(), V.ravel(), np.zeros(U.size)], axis=1)
		sky = np.zeros(U.size)
		for k in range(0, len(P), 4096):
			sky[k:k + 4096] = _box_elev_many(box, P[k:k + 4096], 0.0, 1.0,
											 200000.0)
		lit = (sky < elev).reshape(U.shape)
		c, i, lg = interior_components(inside, lit)
		out.append((name, expect, c, i, lg, int(lit.sum()), int(lit.size)))
	return out


# ---------------------------------------------------------------------- main

def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('--faces', default=LANE + '/tiers_urban.json')
	ap.add_argument('--control-wit', action='store_true')
	ap.add_argument('--control-synth', action='store_true')
	ap.add_argument('--json', default=LANE + '/interior_urban.json')
	a = ap.parse_args()

	if a.control_synth:
		print('--- DETECTOR known-answer control ---')
		ok = True
		for (name, expect, c, i, lg, nl, nt) in control_synth():
			got = 1 if i > 0 else 0
			print('  %-36s expect interior=%d  got components %d interior %d '
				  'largest %d  lit %d/%d  %s'
				  % (name, expect, c, i, lg, nl, nt,
					 'OK' if got == expect else 'CONTROL FAILED'))
			ok = ok and got == expect
		print('  detector control: %s' % ('GREEN' if ok else 'RED'))
		if not ok:
			return 1

	land = wit.Land(H2 + '/land.bin')
	mism, worstm = land.edge_control()
	print('land: %d x %d cells, shared-edge control %d mismatches worst %.1f u'
		  % (land.cw, land.ch, mism, worstm))
	terr_max = float(np.nanmax(land.h))

	L = D.read_lodo(NAT + '.lodo')
	T = D.read_lodi(NAT + '.lodi')
	inst = T['instances']
	meshes, bases = L['meshes'], L['bases']
	bx = []
	for r in inst:
		b = bases[r['baseId']] if r['baseId'] < len(bases) else None
		ms = ([meshes[b['rep%d' % q]] for q in range(4)
			   if b and b['rep%d' % q] != 0xFFFF and b['rep%d' % q] < len(meshes)]
			  if b else [])
		if not ms:
			bx.append((0, 0, -1, -1, -1e9))
			continue
		lo = np.array([min(m['aabbMin'][i] for m in ms) for i in range(3)])
		hi = np.array([max(m['aabbMin'][i] + m['aabbExtent'][i] for m in ms)
					   for i in range(3)])
		cn = np.array([[[lo[0], hi[0]][(k >> 0) & 1], [lo[1], hi[1]][(k >> 1) & 1],
						[lo[2], hi[2]][(k >> 2) & 1]] for k in range(8)])
		M = np.array(r['m'], dtype=np.float64).reshape(3, 3)
		w = (M @ cn.T).T * r['scaleF'] + np.array([r['x'], r['y'], r['z']])
		bx.append((w[:, 0].min(), w[:, 1].min(), w[:, 0].max(), w[:, 1].max(),
				   w[:, 2].max()))
	boxes = np.array(bx, dtype=np.float64)
	box_max = float(boxes[:, 4].max())
	print('boxes: %d, tops %.0f .. %.0f, terrain max %.0f'
		  % (len(boxes), boxes[:, 4].min(), box_max, terr_max))

	if a.control_wit:
		worst, n = control_wit(land, boxes)
		print('--- CAST control: |mine - wit.true_skyline| over %d casts: worst '
			  '%.4f deg ---' % (n, worst))

	top = json.load(open(a.faces))['top_faces']
	rows = []
	for fi, f in enumerate(top):
		mid, ii = f['mesh'], f['instance']
		mesh = meshes[mid]
		r = inst[ii]
		M = np.array(r['m'], dtype=np.float64).reshape(3, 3)
		orig = np.array([r['x'], r['y'], r['z']])
		# rebuild the mesh's level-0 triangles, exactly as measure_tiers did
		lo = min(L['clusters'][c]['vertexBase']
				 for c in range(mesh['clusterFirst'],
								mesh['clusterFirst'] + mesh['clusterCount']))
		tris = []
		for c in range(mesh['clusterFirst'], mesh['clusterFirst'] + mesh['clusterCount']):
			if c < len(L['clusterLods']) and L['clusterLods'][c]['level'] != 0:
				continue
			cl = L['clusters'][c]
			li = L['localIndices'][c * LOCAL_INDEX_BYTES:(c + 1) * LOCAL_INDEX_BYTES]
			for t in range(cl['triangleCount']):
				q = (li[t * 3], li[t * 3 + 1], li[t * 3 + 2])
				if LOCAL_INDEX_NONE in q:
					continue
				tris.append([cl['vertexBase'] + k - lo for k in q])
		tris = np.asarray(tris)
		need = np.unique(tris[f['members']].ravel())
		pos = {}
		for v in need:
			lv = L['vertices'][int(v) + lo]
			pos[int(v)] = np.array([
				dequant(lv['px'], mesh['aabbMin'][0], mesh['aabbExtent'][0]),
				dequant(lv['py'], mesh['aabbMin'][1], mesh['aabbExtent'][1]),
				dequant(lv['pz'], mesh['aabbMin'][2], mesh['aabbExtent'][2])])
		W = {v: (M @ p) * r['scaleF'] + orig for v, p in pos.items()}
		n3 = np.array(f['normal'], dtype=np.float64)
		nw = M @ n3
		nw /= np.linalg.norm(nw)
		t1 = np.array([1.0, 0.0, 0.0])
		if abs(float(nw @ t1)) > 0.9:
			t1 = np.array([0.0, 1.0, 0.0])
		t1 = t1 - nw * float(nw @ t1)
		t1 /= np.linalg.norm(t1)
		t2 = np.cross(nw, t1)
		P0 = W[int(tris[f['members'][0]][0])]
		tri2 = []
		for m in f['members']:
			pts = [((W[int(v)] - P0) @ t1, (W[int(v)] - P0) @ t2) for v in tris[m]]
			tri2.append(pts)
		allu = np.array([p[0] for t in tri2 for p in t])
		allv = np.array([p[1] for t in tri2 for p in t])
		nu = max(2, int(math.ceil((allu.max() - allu.min()) / SAMPLE)))
		nv = max(2, int(math.ceil((allv.max() - allv.min()) / SAMPLE)))
		uu = allu.min() + (np.arange(nu) + 0.5) * (allu.max() - allu.min()) / nu
		vv = allv.min() + (np.arange(nv) + 0.5) * (allv.max() - allv.min()) / nv
		U, V = np.meshgrid(uu, vv)
		inside = np.zeros(U.shape, dtype=bool)
		for t in tri2:
			inside |= point_in_tri(U, V, t)
		if inside.sum() < 9:
			continue
		XYZ = (P0[None, :] + U.ravel()[:, None] * t1[None, :]
			   + V.ravel()[:, None] * t2[None, :])
		# lift a hair off the surface so the plane itself is never the occluder
		XYZ = XYZ + nw[None, :] * 4.0
		Pin = XYZ[inside.ravel()]
		row = dict(rank=fi + 1, model=f['model'], area=f['area'],
				   instance=ii, samples=int(inside.sum()),
				   grid=[int(nv), int(nu)], copies=f.get('copies', 1),
				   normal=[float(x) for x in nw], suns={})
		for elev in SUN_ELEV:
			for az in SUN_AZIM:
				sky = skyline_along(land, Pin, az, boxes, box_own=ii,
									terr_max=terr_max, box_max=box_max,
									sun_elev=elev)
				litflat = np.zeros(U.size, dtype=bool)
				litflat[inside.ravel()] = sky < elev
				lit = litflat.reshape(U.shape)
				c, i, lg = interior_components(inside, lit)
				row['suns']['e%02d_a%03d' % (int(elev), int(az))] = dict(
					components=c, interior=i, largest_interior=lg,
					lit=int((lit & inside).sum()), samples=int(inside.sum()))
		rows.append(row)
		best = max(v['interior'] for v in row['suns'].values())
		print('%2d %-46s A %10.0f  %4d samples  interior components, worst sun: %d'
			  % (fi + 1, f['model'][-46:], f['area'], row['samples'], best))
		for k, v in sorted(row['suns'].items()):
			print('     %s  components %3d  interior %3d  largest %4d  lit %5d/%d'
				  % (k, v['components'], v['interior'], v['largest_interior'],
					 v['lit'], v['samples']))

	nface_any = sum(1 for r in rows
					if any(v['interior'] > 0 for v in r['suns'].values()))
	nface_big = sum(1 for r in rows
					if any(v['largest_interior'] >= 3 for v in r['suns'].values()))
	print('=== %d of %d faces carry a lit/dark boundary that touches NO edge, '
		  'at some sun; %d with a component of 3+ samples ==='
		  % (nface_any, len(rows), nface_big))
	json.dump(dict(faces=rows, faces_with_interior=nface_any,
				   faces_with_interior_3plus=nface_big, total=len(rows),
				   sample_u=SAMPLE, elevations=SUN_ELEV, azimuths=SUN_AZIM),
			  open(a.json, 'w'), indent=1)
	return 0


if __name__ == '__main__':
	sys.exit(main())
