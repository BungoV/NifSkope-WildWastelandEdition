#!/usr/bin/env python3
"""THE KNOWN-ANSWER CONTROL for the mechanism, with no bake and no fixture.

A flat world at z = 0 with ONE square raised to `TOP`, `AT` units NORTH of the
receiver. The true skyline is that square's elevation in the bins that point at
it and 0 everywhere else. What do the march and the "independent" reference
say in the bins pointing the OTHER WAY?"""
import math
import sys

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
from wit import Field, Cast, cast_at, reference_elev

CELL = 128.0
TOP = 1500.0

def build(at_x, at_y, top, cell=CELL):
	f = Field(-20000.0, -20000.0, 20000.0, 20000.0, cell)
	f.mip[0][:] = 0.0                       # flat ground at z = 0 everywhere
	gx = int(math.floor((at_x - f.ox) / cell))
	gy = int(math.floor((at_y - f.oy) / cell))
	f.mip[0][gy, gx] = top
	f.build_mips()
	return f

k = Cast()
print('flat ground z=0, ONE square at z=%.0f, receiver at (0,0) z=0, rise %.0f' % (TOP, k.rise))
print('near-skip %g cell(s) = %.0f u ; first readable step d=128 u' % (k.near_skip, k.near_skip * CELL))
print()
print('%8s | %-58s | %s' % ('object', 'march bins 0(N) 4(E) 8(S) 12(W), deg', 'ref cone N/S'))
for dist in (128.0, 256.0, 512.0, 1024.0, 4096.0):
	f = build(0.0, dist, TOP)
	b = cast_at(k, None, f, 0.0, 0.0, 0.0, out_deg=True)
	# the TRUE elevation of that square, as seen from the receiver
	true_n = math.degrees(math.atan2(TOP - k.rise, dist))
	rN = reference_elev(k, None, f, 0.0, 0.0, 0.0, 0.0)
	rS = reference_elev(k, None, f, 0.0, 0.0, 0.0, 180.0)
	print('%6.0f u | N %5.1f  E %5.1f  S %5.1f  W %5.1f   (TRUE N %5.1f, TRUE S 0.0) | %5.1f / %5.1f'
		  % (dist, b[0], b[4], b[8], b[12], true_n, rN, rS))
print()
print('S is the bin pointing AWAY from the only object in the world.')
print('A march that reports a horizon there is reading its own receiver square.')
