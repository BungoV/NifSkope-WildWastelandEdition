#!/usr/bin/env python3
"""HORIZON4 step 1 -- the ceiling table and its picture pairs.

Every row replaces ONE half of the RIGHT panel's input -- the object stream or
the terrain sheet -- and leaves the other half at the shipped bytes, so a row's
number is the effect of that one change and nothing else.  The LEFT panel is
never touched.

Rows, and what each one asks:

  O1  the stored `.lodi` v8 bytes                     (the baseline; must
                                                       reproduce SUNSIM1 s5.1)
  O2  the ceiling per VERTEX, no down-normal zero rule
  O2p the same, receiver pushed +16 u along the vertex normal
  O2m the same, TWO-SIDED: min(+16 u, -16 u), the lower skyline
  O3a O2 + a vertex on every drawn edge over 512 u
  O3b O2 + a vertex on every drawn edge over 256 u
  O4  the ceiling per PIXEL, snapped to a 64-u lattice on the face (tier 3)
  O4x the ceiling per PIXEL, unsnapped (the tier-3 ceiling at zero texel size)
  O5a O2 at 32 azimuth bins        O5b  O2 at 64 bins
  O5n O2 at 16 bins read NEAREST   O5m  O2 at 16 bins read as the pair MAX
  T1  the stored role-7 sheet                          (the baseline)
  T2a the ceiling sheet at 32 u a texel (the shipped size)
  T2b the ceiling sheet at 64 u a texel
  T3a T2a at 32 bins               T3b  T2a at 64 bins
  T4  T2a with the bin value = the MAX over the bin's 22.5 deg sector
  T5  T2a read with NO spatial interpolation (the texel the point lands in)
  T6a T2a baked with the ray starting AT the ground (rise 0)
  T6c T2a baked with the ray starting 48 u above it
  BEST the best O row and the best T row together
"""
import argparse
import json
import numpy as np
import os
import sys
import time

import h4core as H
import shade as SH

LANE = H.LANE
IMG = LANE + '/images'
SUNS = [(120.0, 5.0), (120.0, 15.0), (120.0, 30.0), (240.0, 15.0)]
CAMS = ['close', 'east', 'full', 'street']
PIC = [('street', 120.0, 5.0), ('east', 240.0, 15.0)]


# ------------------------------------------------------------------ helpers
def sub_bins(h, A):
	"""Take the A-bin subset out of the 64-azimuth ceiling block."""
	assert 64 % A == 0
	return np.ascontiguousarray(h[:, ::(64 // A)])


def sector_max(h, A):
	"""Bin k's value = the MAX over its own 22.5-deg sector, from the 64
	fine azimuths (5.625 deg apart, so +/-11.25 deg is +/-2 of them)."""
	step = 64 // A
	half = step // 2
	out = np.zeros((h.shape[0], A), dtype=h.dtype)
	for k in range(A):
		j = k * step
		idx = [(j + d) % 64 for d in range(-half, half + 1)]
		out[:, k] = h[:, idx].max(axis=1)
	return out


def edge_lookup(edges):
	key = edges[:, 0].astype(np.int64) * np.int64(1 << 32) + edges[:, 1]
	order = np.argsort(key)
	return key[order], order


def refine_hz(ob, gb, hzv, hzE, ekey, eorder, edges, elen, T):
	"""Row O3: the pixel's horizon over a triangle refined by inserting a point
	on every edge longer than T.  Red/green refinement, 1, 2 or 3 split edges,
	with the point relocated into its own sub-triangle -- so an inserted value
	is SHADED, not interpolated (HORIZON3 s2 'inserted vertices are shaded')."""
	oi = gb.objfirst
	tri = ob.tri[gb.tri[oi]]
	l = gb.bary[oi].copy()
	hv = hzv[tri]                                  # (P, 3) corner horizons
	P = len(tri)
	mid = np.full((P, 3), -1, dtype=np.int64)      # midpoint row per edge (ab, bc, ca)
	for c, (i, j) in enumerate(((0, 1), (1, 2), (2, 0))):
		a = np.minimum(tri[:, i], tri[:, j]).astype(np.int64)
		b = np.maximum(tri[:, i], tri[:, j]).astype(np.int64)
		k = a * np.int64(1 << 32) + b
		p = np.searchsorted(ekey, k)
		p = np.clip(p, 0, len(ekey) - 1)
		ok = ekey[p] == k
		r = np.where(ok, eorder[p], -1)
		long = ok & (elen[np.where(ok, r, 0)] > T)
		mid[:, c] = np.where(long, r, -1)
	hm = np.where(mid >= 0, hzE[np.clip(mid, 0, None)], 0.0)
	nsp = (mid >= 0).sum(1)
	out = (l * hv).sum(1)                          # the unsplit answer
	l0, l1, l2 = l[:, 0], l[:, 1], l[:, 2]
	hab, hbc, hca = hm[:, 0], hm[:, 1], hm[:, 2]
	ha, hb, hc = hv[:, 0], hv[:, 1], hv[:, 2]
	# ---- three edges split: the classic 1 -> 4
	m = nsp == 3
	if m.any():
		a0, a1, a2 = l0[m], l1[m], l2[m]
		r = np.empty(int(m.sum()))
		c0 = a0 >= 0.5
		c1 = a1 >= 0.5
		c2 = a2 >= 0.5
		cm = ~(c0 | c1 | c2)
		r[c0] = ((2 * a0[c0] - 1) * ha[m][c0] + 2 * a1[c0] * hab[m][c0] + 2 * a2[c0] * hca[m][c0])
		r[c1] = (2 * a0[c1] * hab[m][c1] + (2 * a1[c1] - 1) * hb[m][c1] + 2 * a2[c1] * hbc[m][c1])
		r[c2] = (2 * a0[c2] * hca[m][c2] + 2 * a1[c2] * hbc[m][c2] + (2 * a2[c2] - 1) * hc[m][c2])
		r[cm] = ((1 - 2 * a2[cm]) * hab[m][cm] + (1 - 2 * a0[cm]) * hbc[m][cm]
				 + (1 - 2 * a1[cm]) * hca[m][cm])
		out[m] = r
	# ---- one edge split
	m = nsp == 1
	if m.any():
		a0, a1, a2 = l0[m], l1[m], l2[m]
		w = mid[m] >= 0
		r = out[m].copy()
		for c, (I, J, K) in enumerate(((0, 1, 2), (1, 2, 0), (2, 0, 1))):
			s = w[:, c]
			if not s.any():
				continue
			hh = [ha[m][s], hb[m][s], hc[m][s]]
			hmm = [hab[m][s], hbc[m][s], hca[m][s]][c]
			ll = [a0[s], a1[s], a2[s]]
			# split edge (I,J); opposite corner K
			hi, hj, hk = hh[I], hh[J], hh[K]
			li, lj, lk = ll[I], ll[J], ll[K]
			near_i = li >= lj
			v = np.where(near_i,
						 (li - lj) * hi + 2 * lj * hmm + lk * hk,
						 (lj - li) * hj + 2 * li * hmm + lk * hk)
			rr = r.copy()
			rr[s] = v
			r = rr
		out[m] = r
	# ---- two edges split
	m = nsp == 2
	if m.any():
		a0, a1, a2 = l0[m], l1[m], l2[m]
		w = mid[m] >= 0
		hh = [ha[m], hb[m], hc[m]]
		hmm = [hab[m], hbc[m], hca[m]]
		ll = [a0, a1, a2]
		r = out[m].copy()
		for c in range(3):
			# edges (0,1)=ab, (1,2)=bc, (2,0)=ca ; the UNSPLIT one is c
			s = ~w[:, c] & w[:, (c + 1) % 3] & w[:, (c + 2) % 3]
			if not s.any():
				continue
			# name so that the split edges are (A,B) and (B,C) with B the shared corner
			B = (c + 2) % 3
			A = (B + 2) % 3
			C = (B + 1) % 3
			mAB = hmm[(c + 2) % 3][s]
			mBC = hmm[(c + 1) % 3][s]
			lA, lB, lC = ll[A][s], ll[B][s], ll[C][s]
			hA, hB, hC = hh[A][s], hh[B][s], hh[C][s]
			v = np.where(
				lB >= 0.5,
				2 * lA * mAB + (2 * lB - 1) * hB + 2 * lC * mBC,
				np.where(lB >= lC,
						 (lA - lB + lC) * hA + (2 * lB - 2 * lC) * mAB + 2 * lC * mBC,
						 lA * hA + 2 * lB * mBC + (lC - lB) * hC))
			rr = r.copy()
			rr[s] = v
			r = rr
		out[m] = r
	return out, nsp


# ------------------------------------------------------------------ the read
def right_hz(gb, ob, az, obj=None, ter=None):
	"""Horizon degrees a pixel, NaN where the source has nothing to say.

	`obj` is ('bins', bins, A, mode) or ('pix', per-pixel degrees);
	`ter` is ('plane', Plane, mode, spatial)."""
	n = len(gb.kind)
	hz = np.full(n, np.nan)
	ti = gb.terfirst
	if ti.any() and ter is not None:
		kind, pl, mode, spatial = ter
		f = pl.elev_at if spatial == 'bilinear' else pl.nearest_at
		hz[ti] = f(gb.pos[ti, 0], gb.pos[ti, 1], az, mode)
	oi = gb.objfirst
	if oi.any() and obj is not None:
		if obj[0] == 'bins':
			_, bins, A, mode = obj
			e = H.read_bins(bins, A, az, mode)
			ia = ob.tri[gb.tri[oi]]
			hz[oi] = (gb.bary[oi, 0] * e[ia[:, 0]] + gb.bary[oi, 1] * e[ia[:, 1]]
					  + gb.bary[oi, 2] * e[ia[:, 2]])
		else:
			hz[oi] = obj[1]
	return hz


def stats(gb, truth, hz, el):
	dec = (gb.kind != 0) & np.isfinite(hz)
	lit = hz < el
	out = {}
	for name, m in (('all', dec), ('terrain', dec & gb.terfirst), ('objects', dec & gb.objfirst)):
		k = int(m.sum())
		out[name] = (100.0 * float((truth[m] != lit[m]).sum()) / k if k else float('nan'), k)
	nod = int(((gb.kind != 0) & ~np.isfinite(hz)).sum())
	out['nodata'] = (100.0 * nod / max(1, int((gb.kind != 0).sum())), nod)
	return out, lit


def picture(gb, cam, truth, hz, lit, az, el, tag, title, note):
	nod = (gb.kind != 0) & ~np.isfinite(hz)
	tb = np.where(np.isfinite(hz), lit.astype(float), 1.0)
	L = SH.shade(gb, truth.astype(float), el, az)
	R = SH.shade(gb, tb, el, az, nodata=nod)
	st, _ = stats(gb, truth, hz, el)
	cap = ('sun azimuth %.0f deg, elevation %.0f deg   |   camera "%s" %dx%d\n'
		   'disagree lit/shadow  ALL %.2f%%  (terrain %.2f%% of %s px, objects %.2f%% of %s px)\n%s'
		   % (az, el, cam.name, cam.w, cam.h, st['all'][0], st['terrain'][0],
			  '{:,}'.format(st['terrain'][1]), st['objects'][0],
			  '{:,}'.format(st['objects'][1]), note))
	SH.pair(SH.to8(L, cam.w, cam.h), SH.to8(R, cam.w, cam.h), cam.w, cam.h,
			'LEFT  ray-cast sun (truth)', 'RIGHT  ' + title,
			cap, '%s/%s.png' % (IMG, tag),
			sub_l='heightmap + 29,587 .lodo triangles; shadow ray with a 16 u footprint',
			sub_r='the only change from the shipped RIGHT panel is this row')
