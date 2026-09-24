#!/usr/bin/env python
"""HORIZON3 step 1(a) + 1(b) -- what tier 2 (long-edge subdivision) and tier 3
(a per-face horizon sheet) would COST on the 33,123-placement urban region.

Reads only files already on disk, through tests/spells/lodgen_native_decode.py
(the byte-table decoder, no code shared with the C++ writers).

WHY THE SCALE MATTERS, and it is the one design decision this script forces.
The geometry lives ONCE in the .lodo library and is instanced; the per-vertex
shading streams (AO v6, sky v7, horizon v8) live PER PLACEMENT in the .lodi.
A world-unit edge threshold is therefore ambiguous: the same library edge is a
different number of world units under every placement's own scale.

  RULE A (the one tier 2 implements): one subdivided library mesh per mesh,
    cut at the mesh-space threshold  t = T / max(scale over that mesh's drawn
    placements).  Every placement of that mesh then satisfies the world bound,
    and the smaller-scaled copies simply carry more vertices than they need.
  RULE B (reported as the floor, NOT implementable without per-placement
    geometry): each placement cut at its own scale.  The gap between A and B
    is exactly the price of sharing one library mesh.

CONTROL (root MISTAKES 2026-09-18 08:0x): the walk prints instances_walked and
slot0_walked, which must equal the .lodi header's instanceCount and
slotInstances[0]; and drawn_triangles / edge percentiles must reproduce
HORIZON1 s1a on the same pair (250,320 tris; p50 239.7, p90 512.0, p99 1,158.7,
max 5,476.1; 71,465 / 15,022 / 1,085 edges over 512 / 1,024 / 2,048).

usage: measure_tiers.py <ws>.lodo <ws>.lodi --json out.json [--faces faces.npz]
"""
import argparse
import json
import math
import os
import sys
from collections import defaultdict

import numpy as np

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, os.path.join(ROOT, 'tests', 'spells'))
import lodgen_native_decode as D  # noqa: E402

LOCAL_INDEX_BYTES = 48
LOCAL_INDEX_NONE = 0xFF

EDGE_BARS = [256.0, 384.0, 512.0, 768.0, 1024.0]
FACE_BARS = [256.0 ** 2, 512.0 ** 2, 1024.0 ** 2]
TEXELS = [64.0, 128.0]
HORIZON_BINS = 16            # bytes a vertex in the v8 stream
LODO_VERTEX_BYTES = 16       # NATIVE 3.1
LODI_PER_VERTEX_BYTES = 1 + 1 + HORIZON_BINS   # ao + sky + horizon
CLUSTER_BYTES = 16 + LOCAL_INDEX_BYTES         # entry + local-index blob
CLUSTER_VERT_CAP = 48
CLUSTER_TRI_CAP = 16

WELD = 100.0                 # weld key = round(pos * WELD): 0.01 mesh units
COPLANAR_DEG = 1.0           # a face's triangles' normals within this
COPLANAR_DIST = 0.5          # and every vertex within this of the seed plane


def dequant(v, mn, ext):
	return mn + (v / 65535.0) * ext


def mesh_geometry(L, mid):
	"""Level-0 triangles of mesh `mid` in MESH units at scale 1.

	Returns (pos, tris, weld, ncluster) where `pos` is the .lodo's own vertex
	list (cluster-duplicated, which is what the per-vertex streams are sized
	by), `tris` indexes into it, and `weld[i]` is the welded id of vertex i."""
	mesh = L['meshes'][mid]
	lo, hi = 0xFFFFFFFF, 0
	for c in range(mesh['clusterFirst'], mesh['clusterFirst'] + mesh['clusterCount']):
		cl = L['clusters'][c]
		lo = min(lo, cl['vertexBase'])
		hi = max(hi, cl['vertexBase'] + cl['vertexCount'])
	if hi <= lo:
		return None
	pos = np.empty((hi - lo, 3), dtype=np.float64)
	for v in range(lo, hi):
		lv = L['vertices'][v]
		pos[v - lo] = (dequant(lv['px'], mesh['aabbMin'][0], mesh['aabbExtent'][0]),
					   dequant(lv['py'], mesh['aabbMin'][1], mesh['aabbExtent'][1]),
					   dequant(lv['pz'], mesh['aabbMin'][2], mesh['aabbExtent'][2]))
	tris = []
	tri_cluster = []
	used = np.zeros(hi - lo, dtype=bool)
	ncluster = 0
	for c in range(mesh['clusterFirst'], mesh['clusterFirst'] + mesh['clusterCount']):
		if c < len(L['clusterLods']) and L['clusterLods'][c]['level'] != 0:
			continue
		ncluster += 1
		cl = L['clusters'][c]
		li = L['localIndices'][c * LOCAL_INDEX_BYTES:(c + 1) * LOCAL_INDEX_BYTES]
		for t in range(cl['triangleCount']):
			a, b, cc = li[t * 3], li[t * 3 + 1], li[t * 3 + 2]
			if LOCAL_INDEX_NONE in (a, b, cc):
				continue
			idx = (cl['vertexBase'] + a - lo, cl['vertexBase'] + b - lo,
				   cl['vertexBase'] + cc - lo)
			tris.append(idx)
			tri_cluster.append(ncluster - 1)
			used[list(idx)] = True
	if not tris:
		return None
	# weld by quantised position: clusters duplicate the vertices on their own
	# seams, and an inserted vertex must be inserted ONCE for the shared edge.
	key = {}
	weld = np.full(len(pos), -1, dtype=np.int64)
	for i in range(len(pos)):
		if not used[i]:
			continue
		k = (int(round(pos[i, 0] * WELD)), int(round(pos[i, 1] * WELD)),
			 int(round(pos[i, 2] * WELD)))
		if k not in key:
			key[k] = len(key)
		weld[i] = key[k]
	return dict(pos=pos, tris=np.asarray(tris, dtype=np.int64), weld=weld,
				tri_cluster=np.asarray(tri_cluster, dtype=np.int64),
				ncluster=ncluster, drawn_vertices=int(used.sum()),
				stored_vertices=len(pos))


def subdiv_cost(G, t_mesh):
	"""Tier 2's cost on ONE mesh at a MESH-space edge threshold.

	Returns a dict.  `welded_new` is the number of genuinely new points (the
	number a T-junction refuter must find welded); `stored_new` is how many
	.lodo vertex rows that becomes once the clusters duplicate their seams;
	`tris_added` is exact for ANY triangulation of the refined boundary
	polygon, because a polygon with V boundary vertices and no interior vertex
	triangulates to V - 2 triangles, so a triangle whose three edges gain
	n0+n1+n2 points gains exactly n0+n1+n2 triangles."""
	pos, tris, weld = G['pos'], G['tris'], G['weld']
	if t_mesh <= 0:
		return dict(welded_new=0, stored_new=0, tris_added=0, clusters_added=0,
					edges_over=0)
	welded_ins = {}          # welded undirected edge -> inserted count
	edges_over = 0
	stored_edge = {}         # (cluster, stored undirected edge) -> count
	tris_added = 0
	per_cluster_v = defaultdict(int)
	per_cluster_t = defaultdict(int)
	for ti in range(len(tris)):
		a, b, c = tris[ti]
		cl = int(G['tri_cluster'][ti])
		per_cluster_t[cl] += 1
		n_here = 0
		for (u, v) in ((a, b), (b, c), (c, a)):
			L = float(np.linalg.norm(pos[u] - pos[v]))
			if L <= t_mesh:
				continue
			n = int(math.ceil(L / t_mesh)) - 1
			if n <= 0:
				continue
			wk = (min(int(weld[u]), int(weld[v])), max(int(weld[u]), int(weld[v])))
			if wk not in welded_ins:
				welded_ins[wk] = n
				edges_over += 1
			sk = (cl, min(int(u), int(v)), max(int(u), int(v)))
			if sk not in stored_edge:
				stored_edge[sk] = n
				per_cluster_v[cl] += n
			n_here += n
		tris_added += n_here
		per_cluster_t[cl] += n_here
	welded_new = sum(welded_ins.values())
	stored_new = sum(stored_edge.values())
	# the two .lodo caps: a cluster holds <= 48 vertices and <= 16 triangles,
	# and its local indices are u8, so a refined cluster must SPLIT.
	clusters_added = 0
	orig_v = defaultdict(set)
	orig_t = defaultdict(int)
	for ti in range(len(tris)):
		cl = int(G['tri_cluster'][ti])
		orig_t[cl] += 1
		for k in tris[ti]:
			orig_v[cl].add(int(k))
	for cl in orig_t:
		V = len(orig_v[cl]) + per_cluster_v.get(cl, 0)
		T = per_cluster_t.get(cl, orig_t[cl])
		need = max(int(math.ceil(V / float(CLUSTER_VERT_CAP))),
				   int(math.ceil(T / float(CLUSTER_TRI_CAP))), 1)
		clusters_added += need - 1
	return dict(welded_new=welded_new, stored_new=stored_new,
				tris_added=tris_added, clusters_added=clusters_added,
				edges_over=edges_over)


def faces_of(G):
	"""A FACE = a maximal run of level-0 triangles of one mesh that are
	edge-connected (through WELDED edges, so cluster seams do not cut a face)
	and coplanar: every triangle normal within COPLANAR_DEG of the seed's, and
	every vertex within COPLANAR_DIST of the seed plane.

	Returns a list of dicts with the face's area at scale 1, its normal, and
	the 2D extent of its own bounding rectangle in the plane (u,v), which is
	what a per-face texture rectangle would have to cover."""
	pos, tris, weld = G['pos'], G['tris'], G['weld']
	n = len(tris)
	nrm = np.zeros((n, 3))
	area = np.zeros(n)
	for i in range(n):
		a, b, c = pos[tris[i]]
		cr = np.cross(b - a, c - a)
		L = np.linalg.norm(cr)
		area[i] = 0.5 * L
		nrm[i] = cr / L if L > 1e-12 else (0.0, 0.0, 1.0)
	# adjacency through welded edges
	edge = defaultdict(list)
	for i in range(n):
		w = [int(weld[k]) for k in tris[i]]
		for (u, v) in ((w[0], w[1]), (w[1], w[2]), (w[2], w[0])):
			edge[(min(u, v), max(u, v))].append(i)
	adj = defaultdict(set)
	for k, lst in edge.items():
		for i in lst:
			for j in lst:
				if i != j:
					adj[i].add(j)
	cosbar = math.cos(math.radians(COPLANAR_DEG))
	seen = np.zeros(n, dtype=bool)
	out = []
	for s in range(n):
		if seen[s] or area[s] <= 0:
			continue
		seed_n = nrm[s]
		seed_p = pos[tris[s][0]]
		stack = [s]
		seen[s] = True
		members = []
		while stack:
			i = stack.pop()
			members.append(i)
			for j in adj[i]:
				if seen[j]:
					continue
				if float(np.dot(nrm[j], seed_n)) < cosbar:
					continue
				d = np.abs((pos[tris[j]] - seed_p) @ seed_n)
				if float(d.max()) > COPLANAR_DIST:
					continue
				seen[j] = True
				stack.append(j)
		idx = np.unique(tris[members].ravel())
		P = pos[idx]
		# a 2D frame in the plane
		t1 = np.array([1.0, 0.0, 0.0])
		if abs(float(np.dot(t1, seed_n))) > 0.9:
			t1 = np.array([0.0, 1.0, 0.0])
		t1 = t1 - seed_n * float(np.dot(t1, seed_n))
		t1 /= np.linalg.norm(t1)
		t2 = np.cross(seed_n, t1)
		u = P @ t1
		v = P @ t2
		out.append(dict(tris=len(members), area=float(area[members].sum()),
						nx=float(seed_n[0]), ny=float(seed_n[1]), nz=float(seed_n[2]),
						du=float(u.max() - u.min()), dv=float(v.max() - v.min()),
						members=[int(x) for x in members]))
	return out


def pct(v, p):
	if not len(v):
		return 0.0
	return float(np.percentile(np.asarray(v), p))


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('lodo')
	ap.add_argument('lodi')
	ap.add_argument('--json', required=True)
	ap.add_argument('--faces')
	ap.add_argument('--topfaces', type=int, default=20)
	a = ap.parse_args()

	L = D.read_lodo(a.lodo)
	T = D.read_lodi(a.lodi)
	hi = T['header']
	inst = T['instances']

	# ---- the drawn population, exactly HORIZON1 s1a's rule (base rep0)
	per_mesh_scales = defaultdict(list)
	placements = []
	for ii, r in enumerate(inst):
		b = L['bases'][r['baseId']] if r['baseId'] < len(L['bases']) else None
		if not b:
			continue
		mid = b['rep0']
		if mid == 0xFFFF or mid >= len(L['meshes']):
			continue
		s = r['scale'] / 8192.0
		per_mesh_scales[mid].append(s)
		placements.append((ii, mid, s))
	n_slot0 = len(placements)

	G = {}
	for mid in per_mesh_scales:
		g = mesh_geometry(L, mid)
		if g is not None:
			G[mid] = g

	# per-mesh edge lengths at scale 1, computed ONCE
	MESH_E = {}
	for mid, g in G.items():
		pos, tris = g['pos'], g['tris']
		MESH_E[mid] = np.linalg.norm(pos[tris[:, [0, 1, 2]]] - pos[tris[:, [1, 2, 0]]],
									 axis=2)
	# placements grouped by (mesh, scale): the whole region is a few thousand
	groups = defaultdict(int)
	for (ii, mid, s) in placements:
		if mid in G:
			groups[(mid, s)] += 1

	# ---- the HORIZON1 control: reproduce s1a's edge table
	all_edges = []
	tri_over = {512: 0, 1024: 0, 2048: 0}
	edge_over = {512: 0, 1024: 0, 2048: 0}
	total_tris = 0
	for (mid, s), cnt in groups.items():
		e = MESH_E[mid] * s
		total_tris += len(e) * cnt
		mx = e.max(axis=1)
		for bar in (512, 1024, 2048):
			tri_over[bar] += int((mx > bar).sum()) * cnt
			edge_over[bar] += int((e > bar).sum()) * cnt
		all_edges.append(np.repeat(e.ravel()[None, :], cnt, axis=0).ravel()
						 if cnt > 1 else e.ravel())
	all_edges = np.concatenate(all_edges) if all_edges else np.zeros(0)
	control = dict(instances_header=hi.get('instanceCount'),
				   instances_walked=len(inst), slot0_walked=n_slot0,
				   drawn_triangles=total_tris, drawn_edges=int(all_edges.size),
				   edge_p50=pct(all_edges, 50), edge_p90=pct(all_edges, 90),
				   edge_p99=pct(all_edges, 99),
				   edge_max=float(all_edges.max()) if all_edges.size else 0.0,
				   edges_over_512=edge_over[512], edges_over_1024=edge_over[1024],
				   edges_over_2048=edge_over[2048],
				   tris_over_512=tri_over[512], tris_over_1024=tri_over[1024],
				   tris_over_2048=tri_over[2048])
	print('CONTROL instances %s/%d slot0 %d  tris %d edges %d'
		  % (control['instances_header'], control['instances_walked'], n_slot0,
			 total_tris, all_edges.size))
	print('CONTROL edge p50 %.1f p90 %.1f p99 %.1f max %.1f  over512 %d over1024 %d over2048 %d'
		  % (control['edge_p50'], control['edge_p90'], control['edge_p99'],
			 control['edge_max'], edge_over[512], edge_over[1024], edge_over[2048]))

	smax = {mid: max(v) for mid, v in per_mesh_scales.items()}

	# ---------------------------------------------------------------- 1(a)
	rows_a = []
	for Tbar in EDGE_BARS:
		# RULE A: one library mesh, cut at T / smax
		lib_stored = 0
		lib_welded = 0
		lib_tris = 0
		lib_clusters = 0
		per_mesh = {}
		for mid, g in G.items():
			c = subdiv_cost(g, Tbar / smax[mid])
			per_mesh[mid] = c
			lib_stored += c['stored_new']
			lib_welded += c['welded_new']
			lib_tris += c['tris_added']
			lib_clusters += c['clusters_added']
		# per placement: the drawn-vertex streams grow by the STORED count
		pl_verts = 0
		pl_tris = 0
		b_verts = 0
		for (mid, s), cnt in groups.items():
			c = per_mesh[mid]
			pl_verts += c['stored_new'] * cnt
			pl_tris += c['tris_added'] * cnt
			# RULE B floor: each placement cut at its OWN scale
			b_verts += subdiv_cost(G[mid], Tbar / s)['stored_new'] * cnt
		lodo_bytes = lib_stored * LODO_VERTEX_BYTES + lib_clusters * CLUSTER_BYTES
		lodi_bytes = pl_verts * LODI_PER_VERTEX_BYTES
		rows_a.append(dict(threshold=Tbar,
						   edges_over=int((all_edges > Tbar).sum()),
						   tris_with_edge_over=int(sum(
							   1 for _ in [] )) if False else None,
						   lib_welded_vertices=lib_welded,
						   lib_stored_vertices=lib_stored,
						   lib_triangles_added=lib_tris,
						   lib_clusters_added=lib_clusters,
						   placement_vertices_added=pl_verts,
						   placement_triangles_added=pl_tris,
						   ruleB_placement_vertices=b_verts,
						   lodo_bytes=lodo_bytes, lodi_bytes=lodi_bytes,
						   total_bytes=lodo_bytes + lodi_bytes))
		print('1a T=%-6.0f edges>T %8d  lib verts +%7d (welded %7d) tris +%7d clusters +%6d '
			  '| placement verts +%9d tris +%9d | ruleB verts %9d | .lodo %10d B .lodi %10d B'
			  % (Tbar, rows_a[-1]['edges_over'], lib_stored, lib_welded, lib_tris,
				 lib_clusters, pl_verts, pl_tris, b_verts, lodo_bytes, lodi_bytes))

	# triangles with an edge over T, per bar
	for row in rows_a:
		Tbar = row['threshold']
		n = 0
		for (mid, s), cnt in groups.items():
			n += int(((MESH_E[mid] * s).max(axis=1) > Tbar).sum()) * cnt
		row['tris_with_edge_over'] = n

	# ---------------------------------------------------------------- 1(b)
	face_cache = {mid: faces_of(g) for mid, g in G.items()}
	rows_b = []
	for Abar in FACE_BARS:
		n_faces = 0
		tex = {t: 0 for t in TEXELS}
		for (mid, s), cnt in groups.items():
			for f in face_cache.get(mid, []):
				A = f['area'] * s * s
				if A <= Abar:
					continue
				n_faces += cnt
				for t in TEXELS:
					tu = max(1, int(math.ceil(f['du'] * s / t)))
					tv = max(1, int(math.ceil(f['dv'] * s / t)))
					tex[t] += tu * tv * cnt
		rows_b.append(dict(area_threshold=Abar, side=math.sqrt(Abar),
						   faces_over=n_faces,
						   texels_64=tex[64.0], texels_128=tex[128.0],
						   bytes_64=tex[64.0] * HORIZON_BINS,
						   bytes_128=tex[128.0] * HORIZON_BINS))
		print('1b A=%-10.0f (%.0f u side) faces %7d  texels@64 %10d (%10d B)  texels@128 %10d (%10d B)'
			  % (Abar, math.sqrt(Abar), n_faces, tex[64.0], tex[64.0] * HORIZON_BINS,
				 tex[128.0], tex[128.0] * HORIZON_BINS))

	# the biggest faces in the world, for 1(c) -- one PLACEMENT each, and never
	# twenty copies of one model, so the interior-shadow count is not decided
	# by a single repeated mesh.
	first_of = {}
	for (ii, mid, s) in placements:
		first_of.setdefault((mid, s), ii)
	big = []
	for (mid, s), cnt in groups.items():
		for fi, f in enumerate(face_cache.get(mid, [])):
			big.append((f['area'] * s * s, first_of[(mid, s)], mid, fi, s, cnt))
	big.sort(key=lambda r: -r[0])
	top = []
	seen_model = defaultdict(int)
	for (A, ii, mid, fi, s, cnt) in big:
		if len(top) >= a.topfaces:
			break
		if seen_model[mid] >= 3:
			continue
		seen_model[mid] += 1
		f = face_cache[mid][fi]
		top.append(dict(area=A, instance=ii, mesh=mid, face=fi, scale=s,
						copies=cnt, tris=f['tris'], du=f['du'] * s, dv=f['dv'] * s,
						normal=[f['nx'], f['ny'], f['nz']],
						members=f['members'],
						model=L['string_at'](L['meshes'][mid]['modelStringOffset'])))
	print('--- top %d faces by world area ---' % len(top))
	for t in top:
		print('  %-52s A %12.0f u2  %4d tri  %7.0f x %7.0f u  n (%.2f %.2f %.2f) inst %d'
			  % (t['model'][-52:], t['area'], t['tris'], t['du'], t['dv'],
				 t['normal'][0], t['normal'][1], t['normal'][2], t['instance']))

	# face population summary
	allA = []
	wts = []
	single = 0
	for (mid, s), cnt in groups.items():
		for f in face_cache.get(mid, []):
			allA.append(f['area'] * s * s)
			wts.append(cnt)
			if f['tris'] == 1:
				single += cnt
	allA = np.repeat(np.asarray(allA), np.asarray(wts, dtype=np.int64)) \
		if allA else np.zeros(0)
	face_pop = dict(faces=int(allA.size), area_p50=pct(allA, 50),
					area_p90=pct(allA, 90), area_p99=pct(allA, 99),
					area_max=float(allA.max()) if allA.size else 0.0,
					single_tri_faces=single)
	print('faces %d  area p50 %.0f p90 %.0f p99 %.0f max %.0f  single-triangle %d'
		  % (face_pop['faces'], face_pop['area_p50'], face_pop['area_p90'],
			 face_pop['area_p99'], face_pop['area_max'], face_pop['single_tri_faces']))

	res = dict(lodo=os.path.basename(a.lodo), lodi=os.path.basename(a.lodi),
			   lodi_version=hi.get('version'), control=control,
			   meshes_walked=len(G), rule='A: t = T / max(scale) per mesh',
			   edge_rows=rows_a, face_rows=rows_b, face_population=face_pop,
			   top_faces=top,
			   constants=dict(horizon_bins=HORIZON_BINS,
							  lodo_vertex_bytes=LODO_VERTEX_BYTES,
							  lodi_per_vertex_bytes=LODI_PER_VERTEX_BYTES,
							  cluster_bytes=CLUSTER_BYTES,
							  weld=WELD, coplanar_deg=COPLANAR_DEG,
							  coplanar_dist=COPLANAR_DIST))
	with open(a.json, 'w') as f:
		json.dump(res, f, indent=1)
	if a.faces:
		np.savez_compressed(a.faces, top=json.dumps(top))
	return 0


if __name__ == '__main__':
	sys.exit(main())
