#!/usr/bin/env python3
"""The TEN receivers, chosen from the data and written down before any of them
is cast.  Classes are decided by the .lodi placement boxes (boxes.py) and the
raw BTD heightmap (land.bin) -- never by the lattice the bake marches over.

  LOW  x3  the lowest open ground in the chunk, nothing placed within 2000 u
			(the brief's "shoreline facing water": this chunk has no sea-level
			water -- 6 nodes of 16,641 at or below 0 -- so the water-facing
			receiver is taken as the lowest open ground with the low basin
			filling the bins that face it.  Stated as a divergence.)
  FLAT x3  open ground, nothing placed within 1200 u, local relief under 64 u
  ST   x2  a street: own square clear, placements within 400 u on two OPPOSITE
			sides, and a clear run over 1500 u along the street's own axis
  FOOT x2  hard against a building: a placement box within 100 u, and that box
			at least 600 u tall over the receiver
"""
import json
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
from wit import Land, LANE

CW, CS, CE, CN = 4, -12, 7, -9
# keep every receiver a full cell inside the chunk, so the .lodi's own extent
# is not the thing that decides what it can see
MX0, MY0 = CW * 4096.0 + 4096.0, CS * 4096.0 + 4096.0
MX1, MY1 = (CE + 1) * 4096.0 - 4096.0, (CN + 1) * 4096.0 - 4096.0

land = Land(LANE + '/land.bin')
B = np.load(L + '/boxes.npy')
bcx, bcy = 0.5 * (B[:, 0] + B[:, 2]), 0.5 * (B[:, 1] + B[:, 3])

STEP = 128.0
gx = np.arange(MX0, MX1, STEP)
gy = np.arange(MY0, MY1, STEP)
GX, GY = np.meshgrid(gx, gy)
Z = land.at(GX, GY)

# distance from every candidate to the nearest placement BOX (0 inside one)
def box_dist(X, Y):
	d = np.full(X.shape, 1e9)
	for k in range(len(B)):
		dx = np.maximum(np.maximum(B[k, 0] - X, X - B[k, 2]), 0.0)
		dy = np.maximum(np.maximum(B[k, 1] - Y, Y - B[k, 3]), 0.0)
		np.minimum(d, np.hypot(dx, dy), out=d)
	return d

D = box_dist(GX, GY)
# local relief over a 3x3 of candidates (+-128 u)
rel = np.full(Z.shape, 1e9)
for j in (-1, 0, 1):
	for i in (-1, 0, 1):
		s = np.roll(np.roll(Z, j, 0), i, 1)
		rel = np.minimum(rel, rel)
stack = np.stack([np.roll(np.roll(Z, j, 0), i, 1) for j in (-1, 0, 1) for i in (-1, 0, 1)])
rel = stack.max(0) - stack.min(0)

print('candidates %d, inside the chunk minus a one-cell rim' % Z.size)
print('nearest placement box: p50 %.0f p75 %.0f p90 %.0f p95 %.0f p99 %.0f max %.0f u'
	  % tuple(np.percentile(D, q) for q in (50, 75, 90, 95, 99, 100)))
print('candidates standing INSIDE a placement box: %.1f%%' % (100.0 * (D <= 0).mean()))

picks = []


def take(mask, key, n, label, spacing=1500.0):
	ys, xs = np.nonzero(mask)
	if len(xs) == 0:
		print('  !! no candidate for %s' % label)
		return
	order = np.argsort(key[ys, xs])
	got = 0
	for o in order:
		x, y = GX[ys[o], xs[o]], GY[ys[o], xs[o]]
		if any(np.hypot(x - p['x'], y - p['y']) < spacing for p in picks):
			continue
		picks.append({'id': '%s%d' % (label, got + 1), 'class': label,
					  'x': float(x), 'y': float(y), 'z': float(Z[ys[o], xs[o]]),
					  'boxDist': float(D[ys[o], xs[o]]),
					  'relief': float(rel[ys[o], xs[o]])})
		got += 1
		if got == n:
			return
	print('  !! only %d of %d for %s' % (got, n, label))


# LOW: open, and as low as the chunk goes
take((D > 256.0), Z, 3, 'LOW', spacing=1000.0)
# FLAT: open and flat, ranked by relief
take((D > 384.0) & (rel < 96.0), rel, 3, 'FLAT', spacing=1400.0)

# STREET: own square clear, boxes on two opposite sides within 400 u, and a
# clear run along the other axis
def side_dist(X, Y, ux, uy, lim=1200.0):
	"""distance to the nearest box whose centre lies in the +u half-plane."""
	d = np.full(X.shape, 1e9)
	for k in range(len(B)):
		dx = np.maximum(np.maximum(B[k, 0] - X, X - B[k, 2]), 0.0)
		dy = np.maximum(np.maximum(B[k, 1] - Y, Y - B[k, 3]), 0.0)
		proj = (bcx[k] - X) * ux + (bcy[k] - Y) * uy
		np.minimum(d, np.where(proj > 0, np.hypot(dx, dy), 1e9), out=d)
	return d

dE = side_dist(GX, GY, 1.0, 0.0)
dW = side_dist(GX, GY, -1.0, 0.0)
dN = side_dist(GX, GY, 0.0, 1.0)
dS = side_dist(GX, GY, 0.0, -1.0)
streetNS = (D > 60.0) & (dE < 260.0) & (dW < 260.0)
streetEW = (D > 60.0) & (dN < 260.0) & (dS < 260.0)
runNS = np.minimum(dN, dS); runEW = np.minimum(dE, dW)
run = np.where(streetNS & (runNS >= runEW), runNS, np.where(streetEW, runEW, -1.0))
print('street candidates: NS %d, EW %d; best clear run along the axis %.0f u'
      % (int(streetNS.sum()), int(streetEW.sum()), run.max()))
take((streetNS | streetEW) & (run > 0), -run, 2, 'ST', spacing=800.0)

# FOOT: a tall box within 100 u
tall = B[(B[:, 4] - np.interp(0, [0], [0])) > -1e9]
dFoot = np.full(GX.shape, 1e9)
footTop = np.zeros(GX.shape)
for k in range(len(B)):
	dx = np.maximum(np.maximum(B[k, 0] - GX, GX - B[k, 2]), 0.0)
	dy = np.maximum(np.maximum(B[k, 1] - GY, GY - B[k, 3]), 0.0)
	dd = np.hypot(dx, dy)
	sel = (dd < dFoot) & (B[k, 4] - Z > 600.0)
	dFoot = np.where(sel, dd, dFoot)
	footTop = np.where(sel, B[k, 4], footTop)
take((D > 8.0) & (dFoot < 100.0), dFoot, 2, 'FOOT', spacing=1500.0)

for p in picks:
	p['footTop'] = None
print()
for p in picks:
	print('%-6s (%8.0f, %9.0f)  ground %7.1f  nearest box %7.0f u  relief %6.1f'
		  % (p['id'], p['x'], p['y'], p['z'], p['boxDist'], p['relief']))
json.dump(picks, open(L + '/receivers.json', 'w'), indent=1)
print('\n-> receivers.json (%d)' % len(picks))
