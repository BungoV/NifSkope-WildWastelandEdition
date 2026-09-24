#!/usr/bin/env python3
"""THE ELIMINATION. One march rule changed at a time, scored against the third
witness on the same ten receivers x 16 bins.

The baseline is the shipped rule, and it must reproduce the shipped bytes --
that is the first row and it is a control, not a result.
"""
import json
import math
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from wit import Cast, bin_dir, NONE, SENT

land, ground, sky, ter0 = fields.build(verbose=False)
terF = fields.terrain_only_field(ter0)
rows = json.load(open(L + '/table.json'))
A = 16
# the usable truth: where an AABB contains the receiver, the box-only reading of
# ~90 deg in every bin is the box talking, not the mesh, so TRUEX is used there.
U = np.array([r['TRUEX'] if r['inBoxes'] else r['TRUE'] for r in rows])
S = np.array([r['STORED'] for r in rows])


def max_along(f, x0, y0, x1, y1, want_cell, tap=2, centre=True, cap=64):
	level, c = 0, f.cell
	while level + 1 < len(f.mip) and c * 4.0 <= want_cell:
		level += 1
		c *= 2.0
	dx, dy = x1 - x0, y1 - y0
	ln = math.hypot(dx, dy)
	taps = min(cap, int(ln / c))
	m = f.mip[level]
	t = np.arange(taps + 1, dtype=np.float64) / (taps if taps else 1)
	px, py = x0 + dx * t, y0 + dy * t
	off = 0.5 if centre else 0.0
	gx = np.floor((px - f.ox) / c - off).astype(np.int64)
	gy = np.floor((py - f.oy) / c - off).astype(np.int64)
	best = NONE
	for j in range(tap):
		for i in range(tap):
			ax, ay = gx + i, gy + j
			ok = (ax >= 0) & (ay >= 0) & (ax < m.shape[1]) & (ay < m.shape[0])
			v = np.where(ok, m[np.clip(ay, 0, m.shape[0] - 1), np.clip(ax, 0, m.shape[1] - 1)], NONE)
			w = float(v.max())
			if w > best:
				best = w
	return best


def march(f, px, py, pz, *, growth=1.5, first=32.0, reach=127561.0, rise=4.0,
		  near_skip=1.0, mip=True, tap=2, centre=True, at_far=False,
		  foot_invariant=False, bin_width=None):
	bw = bin_width if bin_width is not None else 2.0 * math.sin(math.pi / A)
	z0 = pz + rise
	sk = near_skip * f.cell
	if foot_invariant:
		sk = max(sk, float(tap) * f.cell / bw)
	out = []
	for b in range(A):
		dx, dy = bin_dir(b, A)
		best, any_ = 0.0, False
		d = first
		while d <= reach:
			dEnd = min(d * growth, reach)
			want = max(bw * d, 1.0) if mip else 1.0
			if dEnd > sk:
				top = max_along(f, px + dx * d, py + dy * d,
								px + dx * dEnd, py + dy * dEnd, want, tap, centre)
				if top > SENT:
					e = math.degrees(math.atan2(top - z0, dEnd if at_far else d))
					if not any_ or e > best:
						best, any_ = e, True
			if dEnd >= reach:
				break
			d = dEnd
		out.append(best if any_ else 0.0)
	return out


def score(name, **kw):
	got = []
	for r in rows:
		f = terF if kw.pop('terrain_only', False) else sky
		got.append(march(f, r['x'], r['y'], r['gzLattice'], **kw))
	G = np.array(got)
	e = np.abs(G - U)
	print('%-46s mean %6.2f  max %6.2f  >2deg %5.1f%%  >10deg %5.1f%%  |v-shipped| %6.2f'
		  % (name, e.mean(), e.max(), 100.0 * (e > 2).mean(), 100.0 * (e > 10).mean(),
			 np.abs(G - S).mean()))
	return G


print('score = |march - third witness| over 10 receivers x 16 bins, degrees')
print('-' * 118)
base = score('V0 shipped rule (CONTROL: must match STORED)')
print('-' * 118)
score('V1  mip off, always level 0', mip=False)
score('V2  tap 1x1 instead of 2x2', tap=1)
score('V3  tap 1x1, addressed by low corner', tap=1, centre=False)
score('V4  elevation at the FAR end of the segment', at_far=True)
score('V5  growth 1.5 -> 1.10', growth=1.10)
score('V6  growth 1.5 -> 1.02', growth=1.02)
score('V7  near-skip 1.0 -> 0.0', near_skip=0.0)
score('V8  near-skip 1.0 -> 4.0', near_skip=4.0)
score('V9  foot invariant (skip until 2c < bin width)', foot_invariant=True)
score('V10 reach 127,561 -> 16,384 u', reach=16384.0)
score('V11 reach 127,561 -> 4,096 u', reach=4096.0)
print('-' * 118)
score('V12 tap 1x1 + growth 1.02', tap=1, growth=1.02)
score('V13 tap 1x1 + growth 1.02 + mip off', tap=1, growth=1.02, mip=False)
score('V14 tap 1x1 + growth 1.10 + mip off', tap=1, growth=1.10, mip=False)
score('V15 V13 + first step 8 u', tap=1, growth=1.02, mip=False, first=8.0)
print('-' * 118)

# (e) unit or axis confusion: is the sheet's bin 0 where the witness's bin 0 is?
print('azimuth alignment -- mean |STORED - TRUE| with the stored bins rotated by k:')
for k in range(-2, 3):
	print('    rotate %+d  mean %6.2f' % (k, np.abs(np.roll(S, k, axis=1) - U).mean()), end='')
print()
print('   and mirrored (bin -> -bin): mean %6.2f' % np.abs(S[:, ::-1] - U).mean())

# (a) terrain alone: does the LAND half of the lattice over-occlude by itself?
TER = np.array([r['TER'] for r in rows])
PT = np.array([r['PORTTER'] for r in rows])
print('terrain-only march vs terrain-only third witness: mean %6.2f  max %6.2f'
	  % (np.abs(PT - TER).mean(), np.abs(PT - TER).max()))
print('   (third witness terrain mean %.2f, march terrain mean %.2f)' % (TER.mean(), PT.mean()))

# (f) are the boxes inflated relative to the object height field the bake marches?
print('object field top vs box top over the ten receivers\' own squares:')
for r in rows[:0]:
	pass
