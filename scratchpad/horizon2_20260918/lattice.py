#!/usr/bin/env python3
"""Is the error in the MARCH RULE or in the LATTICE the march reads?

variants.py showed that no change to the march rule -- mip, tap, growth,
near-end, near-skip, reach -- moves the 8.5 degree error. So this splits the
chain at the lattice: the same 1-degree pencil the third witness uses, 32 u
step, one square per sample, no tap, no mip, no growth, no near-skip, run over
each candidate field. Whatever that pencil reads is the BEST any march over
that field could do.

  W_true     terrain bilinear + exact placement boxes        = the third witness
  W_sky      the bake's own `sky` lattice (LAND + objfield MAX plane)
  W_boxlat   terrain lattice + the boxes rasterised at 128 u
  W_slab     the bake's `sky` with the SLAB law from lane SLAB1 applied to the
             object half: a square occludes from the ground only where its
             LOWEST object surface reaches down to the receiver (a WALL); a
             square whose whole object span stands above the receiver (a
             CEILING) is left out.
"""
import json
import math
import struct
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from wit import Land, Field, LANE, NONE, SENT

A = 16
AZ = np.arange(0.0, 360.0, 1.0)
SECT = [np.array([i for i, a in enumerate(AZ)
				  if min((a - b * 22.5) % 360.0, (b * 22.5 - a) % 360.0) <= 11.25 + 1e-9])
		for b in range(A)]
rows = json.load(open(L + '/table.json'))
U = np.array([r['TRUEX'] if r['inBoxes'] else r['TRUE'] for r in rows])
S = np.array([r['STORED'] for r in rows])

land, ground, sky, ter0 = fields.build(verbose=False)

# --- the object dump, both planes
b = open(L + '/objfield.bin', 'rb').read()
ogx0, ogy0, ogw, ogh = struct.unpack_from('<4i', b, 4)
ocell = struct.unpack_from('<f', b, 20)[0]
omax = np.frombuffer(b, dtype='<f4', count=ogw * ogh, offset=24).reshape(ogh, ogw)
omin = np.frombuffer(b, dtype='<f4', count=ogw * ogh, offset=24 + 4 * ogw * ogh).reshape(ogh, ogw)
OX, OY = np.meshgrid((np.arange(ogw) + ogx0 + 0.5) * ocell, (np.arange(ogh) + ogy0 + 0.5) * ocell)
occ = omax > -1.0e29
print('object field %dx%d cell %.0f, occupied %d; MIN plane present %s'
	  % (ogw, ogh, ocell, int(occ.sum()), bool((omin > -1.0e29).any())))
span = np.where(occ, omax - omin, np.nan)
print('object span (max-min) over occupied squares: p25 %.0f p50 %.0f p75 %.0f p95 %.0f u'
	  % tuple(np.nanpercentile(span, q) for q in (25, 50, 75, 95)))

BOX = np.load(L + '/boxes.npy')


def field_from(nodes_fn):
	f = Field(fields.X0, fields.Y0, fields.X1, fields.Y1, 128.0)
	f.mip[0][:] = ter0
	nodes_fn(f)
	f.build_mips()
	return f


def raise_objfield(f, keep):
	f.raise_nodes(OX[keep], OY[keep], omax[keep].astype(np.float64))


def raise_boxes(f):
	for k in range(len(BOX)):
		x0, y0, x1, y1, top = BOX[k]
		i0 = int(math.floor((x0 - f.ox) / f.cell)); i1 = int(math.floor((x1 - f.ox) / f.cell))
		j0 = int(math.floor((y0 - f.oy) / f.cell)); j1 = int(math.floor((y1 - f.oy) / f.cell))
		i0, i1 = max(0, i0), min(f.w - 1, i1)
		j0, j1 = max(0, j0), min(f.h - 1, j1)
		if i1 >= i0 and j1 >= j0:
			np.maximum(f.mip[0][j0:j1 + 1, i0:i1 + 1], np.float32(top),
					   out=f.mip[0][j0:j1 + 1, i0:i1 + 1])


def pencil(f, px, py, z0, reach=127561.0, step=32.0):
	"""One square per sample -- the field read as honestly as a field can be."""
	a = np.radians(AZ)
	dx, dy = np.sin(a), np.cos(a)
	d = np.arange(step, reach + step, step)
	X = px + np.outer(dx, d)
	Y = py + np.outer(dy, d)
	m = f.mip[0]
	gx = np.floor((X - f.ox) / f.cell).astype(np.int64)
	gy = np.floor((Y - f.oy) / f.cell).astype(np.int64)
	ok = (gx >= 0) & (gy >= 0) & (gx < f.w) & (gy < f.h)
	Z = np.where(ok, m[np.clip(gy, 0, f.h - 1), np.clip(gx, 0, f.w - 1)], NONE)
	e = np.degrees(np.arctan2(Z - z0, d[None, :]))
	e = np.where(Z > SENT, e, -90.0)
	best = e.max(axis=1)
	return np.maximum(best, 0.0)


def report(name, f, slab=False):
	got = []
	for r in rows:
		z0 = r['gzLattice'] + 4.0
		if slab:
			f = build_slab(z0)
		p = pencil(f, r['x'], r['y'], z0)
		got.append([float(p[s].max()) for s in SECT])
	G = np.array(got)
	e = np.abs(G - U)
	print('%-58s mean %6.2f  max %6.2f  >2deg %5.1f%%' % (name, e.mean(), e.max(), 100.0 * (e > 2).mean()))
	return G


def build_slab(z0):
	"""SLAB1's law: a square is a WALL when its lowest object surface reaches
	down to the receiver's own height, and a CEILING when its whole span stands
	above. A ceiling does not occlude the ground under it."""
	keep = occ & (omin <= z0 + 128.0)
	f = Field(fields.X0, fields.Y0, fields.X1, fields.Y1, 128.0)
	f.mip[0][:] = ter0
	raise_objfield(f, keep)
	f.build_mips()
	return f


print()
print('one-degree pencil, one square per sample -- the CEILING on any march over that field')
print('-' * 100)
f_sky = sky
report('W_sky    the bake\'s own lattice (LAND + objfield MAX)', f_sky)
report('W_boxlat LAND lattice + the .lodi boxes rasterised at 128 u', field_from(lambda f: raise_boxes(f)))
report('W_slab   the bake\'s lattice, SLAB wall/ceiling law on the objects', None, slab=True)
report('W_land   LAND only, no objects at all', fields.terrain_only_field(ter0))
print('-' * 100)
print('for reference, the SHIPPED sheet against the same truth: mean %6.2f  max %6.2f'
	  % (np.abs(S - U).mean(), np.abs(S - U).max()))
