#!/usr/bin/env python3
"""THE TEN-RECEIVER TABLE -- the report's centrepiece.

Five witnesses per receiver per stored bin, in degrees:

  TRUE   the third witness: 1-degree pencil rays over the RAW inputs -- the BTD
		 heightmap read bilinear at the ray's own position, plus the .lodi/.lodo
		 placements as exact world boxes. No lattice, no mip, no maxAlong, no
		 2x2 tap, no near-skip. Folded to the stored bins as the maximum over
		 each bin's own 22.5 degree sector.
  TER    the same ray, terrain only, with the placements left out.
  STORED what the shipped sheet holds at that texel.
  PORT   the Python transcription of `lodgenHorizonCastAt` over the bake's own
		 two lattices -- there to prove PORT == STORED, i.e. that the witness
		 chain is reading the same bake the C++ wrote.
  REF    the in-bake reference, `lodgenHorizonReferenceElev`, 9 pencils at a
		 32 u step over the SAME lattice. This is the witness the 97% floor is
		 scored against.
"""
import json
import sys
import time
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from wit import Cast, cast_at, true_skyline, reference_elev, reference_elev_fast, LANE
from sheet import HorizonSheet

A = 16
AZ = np.arange(0.0, 360.0, 1.0)
SECT = [np.array([i for i, a in enumerate(AZ)
				  if min((a - b * 22.5) % 360.0, (b * 22.5 - a) % 360.0) <= 11.25 + 1e-9])
		for b in range(A)]

land, ground, sky, ter0 = fields.build()
terF = fields.terrain_only_field(ter0)
B = np.load(L + '/boxes.npy')
R = json.load(open(L + '/receivers.json'))
hs = HorizonSheet(LANE.replace('horizon2_20260918', 'horizon1_20260918')
				  + '/v8/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt')
k = Cast()

# control: the fast reference must reproduce the scalar port it replaces
p = R[0]
gz = ground.sample_bilinear(p['x'], p['y'])
c0 = reference_elev(k, None, sky, p['x'], p['y'], gz, 0.0)
c1 = reference_elev_fast(k, None, sky, p['x'], p['y'], gz, 0.0)
print('CONTROL reference_elev scalar %.4f vs vectorised %.4f  (must match)' % (c0, c1))
assert abs(c0 - c1) < 1e-6

rows = []
t0 = time.time()
for p in R:
	# EVERY witness is evaluated at the texel centre the bake cast from, not at
	# the nominal pick position: upt is 32 u, so the two differ by up to 16 u and
	# beside a building 16 u is tens of degrees.
	g0 = hs.bins_at(p['x'], p['y'])
	x, y = (g0[2] if g0 else (p['x'], p['y']))
	gz = ground.sample_bilinear(x, y)
	gtrue = float(land.at(x, y))
	z0 = gz + k.rise
	tru = true_skyline(land, x, y, z0, AZ, boxes=B)
	trx = true_skyline(land, x, y, z0, AZ, boxes=B, skip_containing=True)
	ter = true_skyline(land, x, y, z0, AZ, boxes=None)
	nin = int(((B[:, 0] <= x) & (x <= B[:, 2]) & (B[:, 1] <= y) & (y <= B[:, 3])).sum())
	row = {
		'id': p['id'], 'class': p['class'], 'x': x, 'y': y, 'inBoxes': nin,
		'gzLattice': gz, 'gzTrue': gtrue,
		'TRUE': [float(tru[s].max()) for s in SECT],
		'TRUEX': [float(trx[s].max()) for s in SECT],
		'TER': [float(ter[s].max()) for s in SECT],
		'PORT': [float(v) for v in cast_at(k, None, sky, x, y, gz, out_deg=True)],
		'PORTTER': [float(v) for v in cast_at(k, None, terF, x, y, gz, out_deg=True)],
		'REF': [float(reference_elev_fast(k, None, sky, x, y, gz, b * 22.5)) for b in range(A)],
	}
	g = hs.bins_at(x, y)
	row['STORED'] = [float(v) * 90.0 / 255.0 for v in g[0]] if g else None
	row['texel'] = list(g[1]) if g else None
	rows.append(row)
	print('%-6s (%6.0f,%7.0f) done %.0fs' % (p['id'], x, y, time.time() - t0))

json.dump(rows, open(L + '/table.json', 'w'), indent=1)

print()
hdr = 'bin'.rjust(5) + ''.join('%7d' % b for b in range(A))
for r in rows:
	print('=== %-5s %-4s (%.0f, %.0f)  lattice ground %.1f  true ground %.1f  (%s)'
		  % (r['id'], r['class'], r['x'], r['y'], r['gzLattice'], r['gzTrue'],
			 ('texel ' + ','.join(str(q) for q in r['texel'])) if r['texel'] else 'no texel'))
	print(hdr)
	for nm in ('TRUE', 'TRUEX', 'TER', 'STORED', 'PORT', 'REF'):
		v = r[nm]
		print('%5s' % nm + (''.join('%7.1f' % q for q in v) if v else '   none'))
	if r['STORED']:
		d = np.array(r['STORED']) - np.array(r['TRUE'])
		dp = np.array(r['STORED']) - np.array(r['PORT'])
		dx_ = np.array(r['STORED']) - np.array(r['TRUEX'])
		print('  inside %d box footprints ; STORED-TRUE mean %+.1f max %+.1f ;'
			  ' STORED-TRUEX mean %+.1f max %+.1f ; CONTROL |STORED-PORT| max %.2f deg'
			  % (r['inBoxes'], d.mean(), d.max(), dx_.mean(), dx_.max(), np.abs(dp).max()))
	print()


print('=== SUMMARY over the ten receivers x 16 bins (degrees) ===')
T = np.array([r['TRUE'] for r in rows])
TX = np.array([r['TRUEX'] for r in rows])
TE = np.array([r['TER'] for r in rows])
S = np.array([r['STORED'] for r in rows])
PO = np.array([r['PORT'] for r in rows])
RE = np.array([r['REF'] for r in rows])
U = np.where(np.array([[r['inBoxes']] for r in rows]) > 0, TX, T)   # the usable truth
print('CONTROL  |STORED - PORT|            mean %5.2f  max %5.2f   (the port reads the shipped bake)'
	  % (np.abs(S - PO).mean(), np.abs(S - PO).max()))
for nm, a in (('SHEET  |STORED - TRUE|', S), ('REFER  |REF    - TRUE|', RE)):
	print('%s        mean %5.2f  max %5.2f  over 2 deg %4.1f%%  over 10 deg %4.1f%%'
		  % (nm, np.abs(a - U).mean(), np.abs(a - U).max(),
			 100.0 * (np.abs(a - U) > 2).mean(), 100.0 * (np.abs(a - U) > 10).mean()))
print('SHEET vs REFERENCE                  mean %5.2f  max %5.2f'
	  % (np.abs(S - RE).mean(), np.abs(S - RE).max()))
print('terrain alone (TER) mean %.2f deg, max %.2f -- the sky the LAND leaves is not the sheet'
	  % (TE.mean(), TE.max()))
print('bins where the SHEET stands OVER the third witness: %.1f%% ; UNDER: %.1f%%'
	  % (100.0 * (S - U > 2).mean(), 100.0 * (U - S > 2).mean()))
