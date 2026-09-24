#!/usr/bin/env python3
"""HORIZON4 -- is a shadow GROUP "one thing"?

bungo, 2026-09-19 03:5x: *"Identity was a far better idea, but it relied on each
lod object being one thing, so a house, a single thing, a tower, a single
thing."*  The runtime far-shadow route of rows M1-M3 excludes self-shadowing by
the `.lodi` v7 GROUP id, so every group that is NOT one thing is a place where
that route refuses a shadow it should cast.  This measures how many there are.

Three ways a group fails to be one thing, each measured separately because they
have different repairs:

  F1  the group is SEVERAL SEPARATE SOLIDS.  Re-run the grouping rule's own
      connected-component walk at tolerance ZERO over the placements' world
      boxes: if the group falls apart into more than one component, the 16-unit
      slack (or the SCOL clause, which does not look at geometry at all) fused
      things that do not touch.
  F2  the group is A TERRACE, not a building: one component, but its world
      footprint is far wider than a building.  Reported against the group's own
      largest member and against a cell (4,096 u), and the number is a
      DISTRIBUTION, not a threshold verdict -- Bethesda's row houses share walls
      and no rule over geometry can split them (s4.9 of the format doc says so
      already).
  F3  ONE PLACEMENT that is several things.  A SCOL whose drawn mesh is several
      lumps scattered over its own bound: the level-0 triangles are welded into
      connected components and the components' boxes compared with the mesh box.

Output: `groups.json` + `groups.log`.  Read-only against the bake.
"""
import json
import numpy as np
import sys
import time

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as ND                          # noqa: E402

BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
		'/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')
LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon4_20260919'

log = open(LANE + '/groups.log', 'w')


def p(*a):
	s = ' '.join(str(x) for x in a)
	print(s)
	log.write(s + '\n')
	log.flush()


def world_boxes(L, T):
	"""Each placement's DRAWN mesh AABB, eight corners placed, re-bounded.
	The format doc s4.9 says this is the box the grouping rule itself uses."""
	lo = np.full((len(T['instances']), 3), np.nan)
	hi = np.full((len(T['instances']), 3), np.nan)
	mdl = []
	for i, r in enumerate(T['instances']):
		bse = L['bases'][r['baseId']]
		mdl.append(L['string_at'](bse['modelStringOffset']))
		mid = bse['rep0']
		if mid == 0xFFFF or mid >= len(L['meshes']):
			continue
		m = L['meshes'][mid]
		a = np.array(m['aabbMin'], dtype=np.float64)
		e = np.array(m['aabbExtent'], dtype=np.float64)
		c = np.array([[a[0] + e[0] * ((k >> 0) & 1),
						a[1] + e[1] * ((k >> 1) & 1),
						a[2] + e[2] * ((k >> 2) & 1)] for k in range(8)])
		M = np.array(r['m'], dtype=np.float64).reshape(3, 3)
		w = (M @ c.T).T * r['scaleF'] + np.array([r['x'], r['y'], r['z']])
		lo[i] = w.min(0)
		hi[i] = w.max(0)
	return lo, hi, mdl


def components(idx, lo, hi, tol):
	"""Connected components over boxes that overlap or touch within `tol`."""
	k = len(idx)
	parent = list(range(k))

	def find(a):
		while parent[a] != a:
			parent[a] = parent[parent[a]]
			a = parent[a]
		return a

	for i in range(k):
		if not np.isfinite(lo[idx[i]][0]):
			continue
		for j in range(i + 1, k):
			if not np.isfinite(lo[idx[j]][0]):
				continue
			if np.all(lo[idx[i]] - tol <= hi[idx[j]]) and np.all(lo[idx[j]] - tol <= hi[idx[i]]):
				a, b = find(i), find(j)
				if a != b:
					parent[a] = b
	seen = {}
	for i in range(k):
		r = find(i)
		seen.setdefault(r, []).append(idx[i])
	return list(seen.values())


def mesh_lumps(L, mid):
	"""Level-0 triangles of a mesh, welded and split into components.
	Returns a list of (triangleCount, lo, hi) in MESH units."""
	m = L['meshes'][mid]
	cs = L['clusters'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
	ls = L['clusterLods'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
	if not cs:
		return []
	vb = min(c['vertexBase'] for c in cs)
	ve = max(c['vertexBase'] + c['vertexCount'] for c in cs)
	a = np.array(m['aabbMin'], dtype=np.float64)
	e = np.array(m['aabbExtent'], dtype=np.float64)
	q = np.array([(x['px'], x['py'], x['pz']) for x in L['vertices'][vb:ve]], dtype=np.float64)
	pos = a + q / 65535.0 * e
	tl = []
	for ci, (c, l) in enumerate(zip(cs, ls)):
		if l['level'] != 0:
			continue
		gi = m['clusterFirst'] + ci
		li = L['localIndices'][gi * 48:(gi + 1) * 48]
		off = c['vertexBase'] - vb
		for t in range(c['triangleCount']):
			tl.append((li[t * 3] + off, li[t * 3 + 1] + off, li[t * 3 + 2] + off))
	if not tl:
		return []
	tri = np.array(tl, dtype=np.int64)
	# weld at 0.01 mesh units, the same tolerance HORIZON3's face rule used
	key = np.round(pos * 100.0).astype(np.int64)
	_, inv = np.unique(key, axis=0, return_inverse=True)
	w = inv[tri]
	n = int(inv.max()) + 1
	parent = np.arange(n)

	def find(x):
		while parent[x] != x:
			parent[x] = parent[parent[x]]
			x = parent[x]
		return x

	for t in w:
		r = find(t[0])
		for o in t[1:]:
			s = find(o)
			if s != r:
				parent[s] = r
	root = np.array([find(i) for i in range(n)])
	troot = root[w[:, 0]]
	out = []
	for r in np.unique(troot):
		sel = tri[troot == r]
		v = pos[np.unique(sel)]
		out.append((int((troot == r).sum()), v.min(0), v.max(0)))
	return out


def main():
	t0 = time.time()
	L = ND.read_lodo(BAKE + '.lodo')
	T = ND.read_lodi(BAKE + '.lodi')
	g = np.array(T['group'], dtype=np.int64)
	# group ids are dense PER CHUNK -- make them globally unique for the walk
	ch = np.zeros(len(g), dtype=np.int64)
	for ci, c in enumerate(T['chunks']):
		ch[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
	gid = ch * 100000 + g
	lo, hi, mdl = world_boxes(L, T)
	p('placements %d  groups %d (header groupCount %d)'
	  % (len(g), len(np.unique(gid)), T['header']['groupCount']))

	members = {}
	for i, k in enumerate(gid):
		members.setdefault(int(k), []).append(i)
	sizes = np.array([len(v) for v in members.values()])
	p('group sizes: 1 -> %d, 2..9 -> %d, 10..49 -> %d, 50+ -> %d, largest %d'
	  % (int((sizes == 1).sum()), int(((sizes >= 2) & (sizes < 10)).sum()),
		 int(((sizes >= 10) & (sizes < 50)).sum()), int((sizes >= 50).sum()), int(sizes.max())))

	# ---- F1: several separate solids
	f1 = []
	for k, v in members.items():
		if len(v) < 2:
			continue
		if len(v) > 400:
			continue
		comp = components(v, lo, hi, 0.0)
		if len(comp) > 1:
			# how far apart are the two furthest components?
			cl = [np.min(lo[c], axis=0) for c in comp]
			chh = [np.max(hi[c], axis=0) for c in comp]
			gap = 0.0
			for i in range(len(comp)):
				for j in range(i + 1, len(comp)):
					d = np.maximum(cl[i] - chh[j], cl[j] - chh[i])
					gap = max(gap, float(np.max(d)))
			f1.append((k, len(v), len(comp), gap,
					   [mdl[c[0]] for c in comp[:3]]))
	f1.sort(key=lambda r: -r[3])
	p('')
	p('F1  groups that are SEVERAL SEPARATE SOLIDS at tolerance 0: %d of %d multi-placement groups'
	  % (len(f1), int((sizes >= 2).sum())))
	for r in f1[:8]:
		p('    group %-8d %3d placements -> %2d components, widest gap %8.0f u   %s'
		  % (r[0], r[1], r[2], r[3], r[4][0].split('\\')[-1]))

	# ---- F2: terraces
	ext = []
	for k, v in members.items():
		bl = np.nanmin(lo[v], axis=0)
		bh = np.nanmax(hi[v], axis=0)
		if not np.isfinite(bl[0]):
			continue
		d = bh[:2] - bl[:2]
		big = float(max(np.nanmax(hi[v][:, :2] - lo[v][:, :2], axis=0)))
		ext.append((k, len(v), float(max(d)), big, mdl[v[0]]))
	ext.sort(key=lambda r: -r[2])
	span = np.array([r[2] for r in ext])
	p('')
	p('F2  group XY footprint (longest side): p50 %.0f u  p90 %.0f  p99 %.0f  max %.0f'
	  % (np.percentile(span, 50), np.percentile(span, 90), np.percentile(span, 99), span.max()))
	p('    groups wider than 2,048 u: %d   wider than a CELL (4,096 u): %d   wider than 8,192 u: %d'
	  % (int((span > 2048).sum()), int((span > 4096).sum()), int((span > 8192).sum())))
	for r in ext[:8]:
		p('    group %-8d %3d placements  footprint %8.0f u (largest single member %6.0f u)  %s'
		  % (r[0], r[1], r[2], r[3], r[4].split('\\')[-1]))

	# ---- F3: one placement that is several things
	seen = {}
	f3 = []
	for i, r in enumerate(T['instances']):
		bse = L['bases'][r['baseId']]
		mid = bse['rep0']
		if mid == 0xFFFF or mid >= len(L['meshes']):
			continue
		if mid in seen:
			lumps = seen[mid]
		else:
			lumps = mesh_lumps(L, mid)
			seen[mid] = lumps
		if len(lumps) < 2:
			continue
		m = L['meshes'][mid]
		e = np.array(m['aabbExtent'], dtype=np.float64)
		# the biggest gap between two lumps along any axis, in world units
		gap = 0.0
		for a in range(len(lumps)):
			for b in range(a + 1, len(lumps)):
				d = np.maximum(lumps[a][1] - lumps[b][2], lumps[b][1] - lumps[a][2])
				gap = max(gap, float(np.max(d)))
		gap *= r['scaleF']
		if gap > 256.0:
			f3.append((i, len(lumps), gap, float(max(e)) * r['scaleF'],
					   int(gid[i]), len(members[int(gid[i])]), mdl[i]))
	f3.sort(key=lambda r: -r[2])
	p('')
	p('F3  placements whose OWN drawn mesh is >= 2 welded lumps separated by > 256 u: %d of %d'
	  % (len(f3), len(T['instances'])))
	for r in f3[:8]:
		p('    placement %-5d %2d lumps, widest gap %7.0f u (mesh box %7.0f u), group %d holds %d   %s'
		  % (r[0], r[1], r[2], r[3], r[4], r[5], r[6].split('\\')[-1]))

	json.dump(dict(placements=len(g), groups=int(len(members)),
				   f1=[[int(a), int(b), int(c), float(d), e] for a, b, c, d, e in f1],
				   f2_span_p50=float(np.percentile(span, 50)),
				   f2_span_p90=float(np.percentile(span, 90)),
				   f2_span_max=float(span.max()),
				   f2_over2048=int((span > 2048).sum()),
				   f2_over4096=int((span > 4096).sum()),
				   f2_top=[[int(a), int(b), float(c), float(d), e] for a, b, c, d, e in ext[:25]],
				   f3=[[int(a), int(b), float(c), float(d), int(e), int(f), gg] for a, b, c, d, e, f, gg in f3[:60]]),
			  open(LANE + '/groups.json', 'w'), indent=1)
	p('')
	p('%.1fs' % (time.time() - t0))


if __name__ == '__main__':
	main()
