#!/usr/bin/env python3
"""THE QUESTION G3 ACTUALLY ASKS, put to the third witness.

The gate scores "is this texel lit by a sun at (azimuth, elevation)" -- the
sheet against the in-bake reference. Both read the same lattice, so section 1
says that score cannot tell anyone whether the sheet is right. Here the same
question goes to the third witness instead, over a grid of real texels of chunk
4.4.-12, at the four pre-registered elevations and both pre-registered azimuths.

The sheet is sampled the way the VIEWER samples it (`src/btdterrain.cpp`): the
sun's azimuth falls between two bins and the two stored bytes are blended.
"""
import json
import math
import sys
import time
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from wit import Land, Cast, true_skyline, reference_elev_fast, LANE
from sheet import HorizonSheet

A = 16
SUNS = [(az, el) for az in (120.0, 240.0) for el in (5.0, 15.0, 30.0, 60.0)]
N = 24                      # N x N texels across the chunk
land, ground, sky, _ = fields.build(verbose=False)
B = np.load(L + '/boxes.npy')
hs = HorizonSheet(LANE.replace('horizon2_20260918', 'horizon1_20260918')
				  + '/v8/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt')
k = Cast()

CW, CS, CE, CN = 4, -12, 7, -9
xs = np.linspace(CW * 4096.0 + 512.0, (CE + 1) * 4096.0 - 512.0, N)
ys = np.linspace(CS * 4096.0 + 512.0, (CN + 1) * 4096.0 - 512.0, N)

# one pencil per azimuth actually asked for, plus the bin sector around it
AZW = sorted(set(list(np.arange(108.0, 133.0, 1.0)) + list(np.arange(228.0, 253.0, 1.0))))
AZW = np.array(AZW)


def sheet_elev(bins, az):
	"""What the viewer reads at this azimuth: the two bins either side, blended."""
	f = az / (360.0 / A)
	k0 = int(math.floor(f)) % A
	k1 = (k0 + 1) % A
	t = f - math.floor(f)
	return (bins[k0] * (1.0 - t) + bins[k1] * t) * 90.0 / 255.0


t0 = time.time()
rec = []
for y in ys:
	for x in xs:
		g = hs.bins_at(float(x), float(y))
		if not g:
			continue
		cx, cy = g[2]
		gz = ground.sample_bilinear(cx, cy)
		z0 = gz + k.rise
		tru = true_skyline(land, cx, cy, z0, AZW, boxes=B, skip_containing=True)
		row = {'x': cx, 'y': cy, 'bins': list(g[0])}
		for az in (120.0, 240.0):
			sel = np.abs(AZW - az) < 0.5
			row['true%d' % int(az)] = float(tru[sel][0])
			row['ref%d' % int(az)] = float(reference_elev_fast(k, None, sky, cx, cy, gz, az, rays=1))
			row['sheet%d' % int(az)] = float(sheet_elev(g[0], az))
		rec.append(row)
print('%d texels, %.0fs' % (len(rec), time.time() - t0))
json.dump(rec, open(L + '/lit.json', 'w'))

print()
print('%-14s %8s %8s %8s   %s' % ('sun', 'TRUE', 'SHEET', 'REF', 'agreement of SHEET / REF with the third witness'))
for az, el in SUNS:
	T = np.array([r['true%d' % int(az)] for r in rec]) < el
	S = np.array([r['sheet%d' % int(az)] for r in rec]) < el
	R = np.array([r['ref%d' % int(az)] for r in rec]) < el

	def bal(a):
		tp = float((a & T).sum()); fn = float((~a & T).sum())
		tn = float((~a & ~T).sum()); fp = float((a & ~T).sum())
		sens = tp / (tp + fn) if tp + fn else 1.0
		spec = tn / (tn + fp) if tn + fp else 1.0
		return 0.5 * (sens + spec)
	print('az %3.0f el %2.0f    %7.1f%% %7.1f%% %7.1f%%   sheet %5.1f%%   ref %5.1f%%'
		  % (az, el, 100.0 * T.mean(), 100.0 * S.mean(), 100.0 * R.mean(),
			 100.0 * bal(S), 100.0 * bal(R)))

print()
for az in (120.0, 240.0):
	T = np.array([r['true%d' % int(az)] for r in rec])
	S = np.array([r['sheet%d' % int(az)] for r in rec])
	R = np.array([r['ref%d' % int(az)] for r in rec])
	print('azimuth %3.0f  elevation deg: TRUE mean %5.1f  SHEET mean %5.1f (err mean %5.2f max %5.2f)'
		  '  REF mean %5.1f (err mean %5.2f max %5.2f)'
		  % (az, T.mean(), S.mean(), np.abs(S - T).mean(), np.abs(S - T).max(),
			 R.mean(), np.abs(R - T).mean(), np.abs(R - T).max()))
