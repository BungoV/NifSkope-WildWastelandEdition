#!/usr/bin/env python3
"""HORIZON4 control 1 -- the ceiling engine IS the left panel's geometry.

Known-answer: for a set of receivers, the ceiling horizon at azimuth A must
predict lit/dark at elevation E exactly as `render.SunShadow(A, E).lit` does,
because the two read the same grids with the same shear.  Any disagreement
larger than the ladder rung (0.5 deg) is a fault in the ceiling engine, not a
property of anything.  A red control is included: the SAME receivers scored
against the horizon of a DIFFERENT azimuth.
"""
import numpy as np
import time
import h4core as H
import render as RD

t0 = time.time()
ter, ob, sh = H.load_scene(verbose=True)
print('scene %.1fs' % (time.time() - t0))

rng = np.random.default_rng(4)
x0, y0, x1, y1 = H.CHUNK
# half on the ground, half on object triangles -- the same mix control.py used
n = 4000
px = rng.uniform(x0, x1, n); py = rng.uniform(y0, y1, n)
pz = ter.atf(px, py)
gnd = np.stack([px, py, pz], 1)
ti = rng.integers(0, len(ob.tri), n)
w = rng.random((n, 3)); w /= w.sum(1, keepdims=True)
tv = ob.v[ob.tri[ti]]
obp = (tv * w[:, :, None]).sum(1).astype(np.float64)
pts = np.concatenate([gnd, obp])
kind = np.concatenate([np.zeros(n, int), np.ones(n, int)])

for AZ in (120.0, 240.0):
	t0 = time.time()
	sg = H.ShearGrid(ter, ob, AZ, verbose=True)
	idx = sg.index(pts)
	hz = sg.horizon(idx)
	print('  horizon %.1fs  mean %.2f deg  zero %.1f%%'
		  % (time.time() - t0, hz.mean(), 100.0 * (hz <= 0).mean()))
	for EL in (5.0, 15.0, 30.0):
		ss = RD.SunShadow(ter, ob, AZ, EL)
		lit_ss = ss.lit(pts)
		lit_hz = hz < EL
		for nm, m in (('terrain', kind == 0), ('objects', kind == 1)):
			ag = 100.0 * (lit_ss[m] == lit_hz[m]).mean()
			print('    az %3.0f el %2.0f %-8s agree %6.2f%%   ss lit %5.1f%%  hz lit %5.1f%%'
				  % (AZ, EL, nm, ag, 100.0 * lit_ss[m].mean(), 100.0 * lit_hz[m].mean()))
	# RED control: score against the horizon 90 deg away
	sg2 = H.ShearGrid(ter, ob, (AZ + 90.0) % 360.0)
	hz2 = sg2.horizon(sg2.index(pts))
	for EL in (15.0,):
		ss = RD.SunShadow(ter, ob, AZ, EL)
		lit_ss = ss.lit(pts)
		print('    RED  az %3.0f el %2.0f vs horizon at az %3.0f: agree %6.2f%%'
			  % (AZ, EL, (AZ + 90.0) % 360.0, 100.0 * (lit_ss == (hz2 < EL)).mean()))
print('done %.1f min' % ((time.time() - t0) / 60.0))
