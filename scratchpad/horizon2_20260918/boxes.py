#!/usr/bin/env python3
"""World-space placement boxes for the THIRD WITNESS, straight from the shipped
.lodi/.lodo through tests/spells/lodgen_native_decode.py -- an independent
decoder written from the byte tables, sharing no code with the bake's
LodgenObjectHeightField, no lattice, no mip, no 128 u square.

A box is the world AABB of the mesh's own local AABB (.lodo mesh row
aabbMin/aabbExtent) carried through the instance's rotation, scale and
position. That is an OVER-estimate of the mesh it encloses, never an
under-estimate, so a skyline the third witness reports from boxes is an UPPER
bound on the truth.  Exported: boxes_for(), an (N,5) float64 array of
(x0, y0, x1, y1, top)."""
import os
import sys
import numpy as np

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT + '/tests/spells')
import lodgen_native_decode as ND

NAT = ROOT + '/scratchpad/horizon1_20260918/v8/nat/FO4CSLOD/Commonwealth/Commonwealth'


def load(lodo=NAT + '.lodo', lodi=NAT + '.lodi'):
	return ND.read_lodo(lodo), ND.read_lodi(lodi)


def boxes_for(clip=None, verbose=True):
	L, T = load()
	bases = L['bases']
	meshes = L['meshes']
	out = []
	nomesh = 0
	for r in T['instances']:
		b = bases[r['baseId']] if r['baseId'] < len(bases) else None
		if b is None:
			continue
		ms = [meshes[b['rep%d' % q]] for q in range(4)
			  if b['rep%d' % q] != 0xFFFF and b['rep%d' % q] < len(meshes)]
		if not ms:
			nomesh += 1
			continue
		# union of every mesh that carries this base's model path
		lo = np.array([min(m['aabbMin'][i] for m in ms) for i in range(3)])
		hi = np.array([max(m['aabbMin'][i] + m['aabbExtent'][i] for m in ms) for i in range(3)])
		m3 = r['m']
		s = r['scaleF']
		cx = np.array([[[lo[0], hi[0]][(k >> 0) & 1], [lo[1], hi[1]][(k >> 1) & 1],
						[lo[2], hi[2]][(k >> 2) & 1]] for k in range(8)])
		M = np.array(m3, dtype=np.float64).reshape(3, 3)
		w = (M @ cx.T).T * s + np.array([r['x'], r['y'], r['z']])
		out.append((w[:, 0].min(), w[:, 1].min(), w[:, 0].max(), w[:, 1].max(), w[:, 2].max()))
	a = np.array(out, dtype=np.float64)
	if clip is not None:
		x0, y0, x1, y1 = clip
		keep = (a[:, 2] >= x0) & (a[:, 0] <= x1) & (a[:, 3] >= y0) & (a[:, 1] <= y1)
		a = a[keep]
	if verbose:
		print('boxes: %d instances, %d with no mesh row, %d boxes kept'
			  % (len(T['instances']), nomesh, len(a)))
		if len(a):
			print('  box world x %.0f..%.0f  y %.0f..%.0f  top %.0f..%.0f'
				  % (a[:, 0].min(), a[:, 2].max(), a[:, 1].min(), a[:, 3].max(),
					 a[:, 4].min(), a[:, 4].max()))
	return a


if __name__ == '__main__':
	a = boxes_for()
	np.save(ROOT + '/scratchpad/horizon2_20260918/boxes.npy', a)
	w = np.maximum(a[:, 2] - a[:, 0], a[:, 3] - a[:, 1])
	for q in (50, 90, 99, 100):
		print('  box footprint p%-3d %8.1f u' % (q, np.percentile(w, q)))
