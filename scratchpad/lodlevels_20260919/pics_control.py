#!/usr/bin/env python3
"""LODLEVELS control: Bethesda's OWN level-4 bake of this chunk vs mine.

Commonwealth.4.4.-12.BTO is a NIF of exactly the geometry this lane
reconstructs for chunk x 4..7 / y -12..-9 at object LOD level 4.  Read it with
the SAME reader, carry its shapes up their own node chain, and put its triangle
count and world bounding box beside my reconstruction restricted to the same
cells.  Numbers are printed whatever they say."""
import json
import os
import sys

import numpy as np

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919'
MESH = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/'
sys.path.insert(0, LANE)

import pics_build as B                              # noqa: E402
from pics_nifread import Nif                        # noqa: E402

CELL = 4096.0
X0, Y0, X1, Y1 = 4 * CELL, -12 * CELL, 8 * CELL, -8 * CELL


def bto_stats(path):
	n = Nif(path)
	tot, V = 0, []
	for bi in sorted(n.shapes):
		sh = n.shapes[bi]
		if sh['numVerts'] == 0:
			continue
		v = np.array(sh['verts'], dtype=np.float64)
		R, t, s = B._chain(n, bi)
		V.append(t[None, :] + s * (v @ R.T))
		tot += sh['numTris']
	v = np.concatenate(V)
	return tot, len(v), v.min(0), v.max(0), len(n.shapes)


def main():
	out = {}
	D = B.load_pickle()
	refs, bases = D['refs'], D['bases']
	for L, k in ((4, 0), (8, 1), (16, 2), (32, 3)):
		if L == 4:
			p = MESH + 'Terrain/Commonwealth/Objects/Commonwealth.4.4.-12.BTO'
		elif L == 8:
			p = MESH + 'Terrain/Commonwealth/Objects/Commonwealth.8.4.-12.BTO'
		elif L == 16:
			p = MESH + 'Terrain/Commonwealth/Objects/Commonwealth.16.0.-16.BTO'
		else:
			p = MESH + 'Terrain/Commonwealth/Objects/Commonwealth.32.0.-32.BTO'
		if not os.path.exists(p):
			print('level %d: no BTO at %s' % (L, p))
			continue
		tt, nv, lo, hi, ns = bto_stats(p)
		print('BTO  %-44s tris %7d verts %7d shapes %2d  bbox x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f'
			  % (os.path.basename(p), tt, nv, ns, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]), flush=True)
		out['bto%d' % L] = dict(path=os.path.basename(p), tris=tt, verts=nv,
								lo=list(lo), hi=list(hi), shapes=ns)
		if L != 4:
			continue
		# --- my reconstruction, restricted to the SAME cells
		idx = [i for i, r in enumerate(refs)
			   if X0 <= r[2] < X1 and Y0 <= r[3] < Y1
			   and bases.get(r[1]) is not None and bases[r[1]]['slots'][k]]
		ob = B.build_level(refs, bases, k, np.array(idx, dtype=np.int64), verbose=False)
		lo2, hi2 = ob.v.min(0), ob.v.max(0)
		print('MINE level %d, cells 4..7 / -12..-9:        tris %7d verts %7d placements %6d  bbox x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f'
			  % (L, len(ob.tri), len(ob.v), len(ob.refid),
				 lo2[0], hi2[0], lo2[1], hi2[1], lo2[2], hi2[2]), flush=True)
		out['mine%d' % L] = dict(tris=int(len(ob.tri)), verts=int(len(ob.v)),
								 placements=int(len(ob.refid)),
								 lo=[float(q) for q in lo2], hi=[float(q) for q in hi2])
		r = len(ob.tri) / float(out['bto4']['tris'])
		print('ratio mine/BTO triangles = %.3f' % r, flush=True)
		out['ratio4'] = r
	json.dump(out, open(LANE + '/pics_control.json', 'w'), indent=1)
	print('wrote pics_control.json', os.path.getsize(LANE + '/pics_control.json'))


if __name__ == '__main__':
	main()
