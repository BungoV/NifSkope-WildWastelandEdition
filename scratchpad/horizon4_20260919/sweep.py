#!/usr/bin/env python3
"""HORIZON4 -- the ceiling sweep.

One pass over 64 azimuths (5.625 deg apart).  Bin centres for A = 16 are every
4th of them, for A = 32 every 2nd, for A = 64 all of them, so ONE sweep feeds
every bin-count row; and the max over the 5 fine azimuths inside a 22.5-degree
bin is the SECTOR maximum that row T4 needs, for free.

Receiver sets, all cast with the LEFT panel's own grids and bias (h4core):

  vert      -- the 53,396 .lodo vertices at their own position (no offset)
  vertP     -- the same, pushed +16 u along the vertex normal
  vertM     -- the same, pushed -16 u along the vertex normal
  edge      -- the midpoints of every drawn edge longer than 256 world units
  ter64     -- terrain texel centres at 64 u over chunk 4.4.-12
  ter32     -- terrain texel centres at 32 u (the shipped sheet's own size)

Output: `ceiling.npz`, one uint8 (N, 64) block a set, already through
`lodgenHorizonQuantise`, plus the edge table.  Log `sweep.log`.
"""
import numpy as np
import os
import sys
import time

import h4core as H

OUT = H.LANE + '/ceiling.npz'
A64 = 64
NORMAL_PUSH = 16.0
EDGE_MIN = 256.0


def build_receivers(ter, ob):
	sets = {}
	sets['vert'] = ob.v.astype(np.float64)
	n = ob.n.astype(np.float64)
	sets['vertP'] = sets['vert'] + n * NORMAL_PUSH
	sets['vertM'] = sets['vert'] - n * NORMAL_PUSH
	# unique drawn edges over EDGE_MIN world units
	t = ob.tri.astype(np.int64)
	e = np.concatenate([t[:, [0, 1]], t[:, [1, 2]], t[:, [2, 0]]], axis=0)
	e = np.sort(e, axis=1)
	e = np.unique(e, axis=0)
	L = np.linalg.norm(ob.v[e[:, 0]].astype(np.float64) - ob.v[e[:, 1]].astype(np.float64), axis=1)
	keep = L > EDGE_MIN
	e = e[keep]
	L = L[keep]
	sets['edge'] = 0.5 * (ob.v[e[:, 0]].astype(np.float64) + ob.v[e[:, 1]].astype(np.float64))
	for upt, nm in ((64.0, 'ter64'), (32.0, 'ter32')):
		X, Y, nx, ny, ox, oy = H.texel_grid(upt)
		Z = ter.atf(X, Y)
		sets[nm] = np.stack([X.ravel(), Y.ravel(), Z.ravel()], axis=1)
	return sets, e, L


def main():
	log = open(H.LANE + '/sweep.log', 'w')

	def p(*a):
		s = ' '.join(str(x) for x in a)
		print(s)
		log.write(s + '\n')
		log.flush()

	T0 = time.time()
	ter, ob, sh = H.load_scene()
	sets, edges, elen = build_receivers(ter, ob)
	for k, v in sets.items():
		p('receiver set %-6s %8d' % (k, len(v)))
	p('edges over %.0f u: %d of %d unique drawn edges' % (EDGE_MIN, len(edges), len(edges)))
	out = {k: np.zeros((len(v), A64), dtype=np.uint8) for k, v in sets.items()}
	for j in range(A64):
		az = j * (360.0 / A64)
		t0 = time.time()
		sg = H.ShearGrid(ter, ob, az)
		tb = time.time() - t0
		for k, v in sets.items():
			idx = sg.index(v)
			out[k][:, j] = H.quantise(sg.horizon(idx))
		p('az %6.3f  build %4.1fs  total %5.1fs  elapsed %5.1f min'
		  % (az, tb, time.time() - t0, (time.time() - T0) / 60.0))
		del sg
	np.savez_compressed(OUT, edges=edges, elen=elen,
						**{('h_' + k): v for k, v in out.items()})
	p('wrote %s  %.1f MB  TOTAL %.1f min'
	  % (OUT, os.path.getsize(OUT) / 1e6, (time.time() - T0) / 60.0))
	log.close()


if __name__ == '__main__':
	main()
