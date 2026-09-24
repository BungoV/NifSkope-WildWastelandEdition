#!/usr/bin/env python3
"""THE CANDIDATE FIX, measured before a line of C++ is touched.

Two changes to `lodgenHorizonCastAt` / `LodgenHorizonField::maxAlong`, both in
`src/lodghorizon.h`, and nothing else:

  1. `maxAlong` reads ONE square per tap -- the square the sample lands in --
     instead of a 2x2 block. The 2x2 block is a 2*c wide dilation of every
     occluder; at level 0 that is 256 u, and at the level `wantCell` picks it is
     twice the bin width. Taps are spaced half a cell instead of a whole one so
     nothing is stepped over.
  2. `wantCell` comes from the SEGMENT and the tap budget -- `len / maxTaps` --
     instead of from the azimuth bin's width. Choosing the mip by the bin width
     is what makes the march a maximum over the whole 22.5 degree sector; the
     viewer blends two stored bytes as if each were the skyline in its own
     direction (`src/btdterrain.cpp`), so a sector maximum is the wrong
     quantity. Choosing it by the segment keeps the tap budget honest at long
     range without widening the footprint to the bin.
"""
import json
import math
import sys
import time
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from wit import Land, Cast, true_skyline, LANE, NONE, SENT
from sheet import HorizonSheet

A = 16
MAXTAPS = 64
BW = 2.0 * math.sin(math.pi / A)
land, ground, sky, ter0 = fields.build(verbose=False)
B = np.load(L + '/boxes.npy')


def max_along(f, x0, y0, x1, y1, want_cell, shipped):
	level, c = 0, f.cell
	while level + 1 < len(f.mip) and c * 4.0 <= want_cell:
		level += 1
		c *= 2.0
	dx, dy = x1 - x0, y1 - y0
	ln = math.hypot(dx, dy)
	m = f.mip[level]
	if shipped:
		taps = min(MAXTAPS, int(ln / c))
		t = np.arange(taps + 1, dtype=np.float64) / (taps if taps else 1)
		px, py = x0 + dx * t, y0 + dy * t
		gx = np.floor((px - f.ox) / c - 0.5).astype(np.int64)
		gy = np.floor((py - f.oy) / c - 0.5).astype(np.int64)
		best = NONE
		for j in range(2):
			for i in range(2):
				ax, ay = gx + i, gy + j
				ok = (ax >= 0) & (ay >= 0) & (ax < m.shape[1]) & (ay < m.shape[0])
				v = np.where(ok, m[np.clip(ay, 0, m.shape[0] - 1), np.clip(ax, 0, m.shape[1] - 1)], NONE)
				w = float(v.max())
				if w > best:
					best = w
		return best
	taps = min(2 * MAXTAPS, int(2.0 * ln / c))
	t = np.arange(taps + 1, dtype=np.float64) / (taps if taps else 1)
	px, py = x0 + dx * t, y0 + dy * t
	gx = np.floor((px - f.ox) / c).astype(np.int64)
	gy = np.floor((py - f.oy) / c).astype(np.int64)
	ok = (gx >= 0) & (gy >= 0) & (gx < m.shape[1]) & (gy < m.shape[0])
	v = np.where(ok, m[np.clip(gy, 0, m.shape[0] - 1), np.clip(gx, 0, m.shape[1] - 1)], NONE)
	return float(v.max())


def cast(f, px, py, pz, shipped, growth=1.5, first=32.0, reach=127561.0,
		 rise=4.0, near_skip=1.0):
	z0 = pz + rise
	sk = near_skip * f.cell
	out = []
	for b in range(A):
		a = b * 2.0 * math.pi / A
		dx, dy = math.sin(a), math.cos(a)
		best, any_ = 0.0, False
		d = first
		while d <= reach:
			dEnd = min(d * growth, reach)
			ln = dEnd - d
			want = max(BW * d, 1.0) if shipped else max(ln / MAXTAPS, 1.0)
			if dEnd > sk:
				top = max_along(f, px + dx * d, py + dy * d,
								px + dx * dEnd, py + dy * dEnd, want, shipped)
				if top > SENT:
					e = math.degrees(math.atan2(top - z0, d))
					if not any_ or e > best:
						best, any_ = e, True
			if dEnd >= reach:
				break
			d = dEnd
		out.append(best if any_ else 0.0)
	return out


def sheet_elev(deg, az):
	f = az / (360.0 / A)
	k0 = int(math.floor(f)) % A
	k1 = (k0 + 1) % A
	t = f - math.floor(f)
	return deg[k0] * (1.0 - t) + deg[k1] * t


if __name__ == '__main__':
	rows = json.load(open(L + '/table.json'))
	S = np.array([r['STORED'] for r in rows])
	AZB = np.array([b * (360.0 / A) for b in range(A)])
	D = np.array([true_skyline(land, r['x'], r['y'], r['gzLattice'] + 4.0, AZB, boxes=B,
							   skip_containing=bool(r['inBoxes'])) for r in rows])
	for nm, sh in (('shipped', True), ('candidate', False)):
		G = np.array([cast(sky, r['x'], r['y'], r['gzLattice'], sh) for r in rows])
		e = np.abs(G - D)
		print('%-10s |march - directional truth| mean %6.2f max %6.2f bias %+6.2f ; |march - STORED| %6.2f'
			  % (nm, e.mean(), e.max(), (G - D).mean(), np.abs(G - S).mean()))

	# the lit question, over the same 576 texels lit.py used
	rec = json.load(open(L + '/lit.json'))
	print('\n576 texels of chunk 4.4.-12, the lit question put to each witness')
	t0 = time.time()
	cand = []
	for r in rec:
		cand.append(cast(sky, r['x'], r['y'], ground.sample_bilinear(r['x'], r['y']), False))
	print('candidate cast %.0fs' % (time.time() - t0))
	json.dump(cand, open(L + '/cand_bins.json', 'w'))
	print('%-14s %8s %8s %8s %8s' % ('sun', 'TRUE', 'SHEET', 'REF', 'CAND'))
	for az in (120.0, 240.0):
		T = np.array([r['true%d' % int(az)] for r in rec])
		Sh = np.array([r['sheet%d' % int(az)] for r in rec])
		Rf = np.array([r['ref%d' % int(az)] for r in rec])
		Cd = np.array([sheet_elev(c, az) for c in cand])
		for el in (5.0, 15.0, 30.0, 60.0):
			print('az %3.0f el %2.0f    %7.1f%% %7.1f%% %7.1f%% %7.1f%%'
				  % (az, el, 100.0 * (T < el).mean(), 100.0 * (Sh < el).mean(),
					 100.0 * (Rf < el).mean(), 100.0 * (Cd < el).mean()))
		print('  az %3.0f elevation: TRUE mean %5.1f | SHEET %5.1f (err %5.2f bias %+5.2f)'
			  ' | REF %5.1f | CAND %5.1f (err %5.2f bias %+5.2f)'
			  % (az, T.mean(), Sh.mean(), np.abs(Sh - T).mean(), (Sh - T).mean(),
				 Rf.mean(), Cd.mean(), np.abs(Cd - T).mean(), (Cd - T).mean()))
