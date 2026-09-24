#!/usr/bin/env python3
"""LODLEVELS pictures, probe 1: what is in esm.pkl, and does the NIF reader
open a .BTO?  Verdict lines only."""
import os
import pickle
import sys

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919'
MESH = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/'
sys.path.insert(0, LANE)

D = pickle.load(open(LANE + '/esm.pkl', 'rb'))
print('keys:', sorted(D.keys()))
bases, refs = D['bases'], D['refs']
print('bases %d  refs %d  cwcount %d' % (len(bases), len(refs), len(D['cwcount'])))
k = next(iter(bases))
print('sample base %08X: %r' % (k, bases[k]))
print('sample ref: %r' % (refs[0],))

# how many bases have each slot
import collections
have = [0, 0, 0, 0]
for b in bases.values():
	for i in range(4):
		if b['slots'][i]:
			have[i] += 1
print('bases with slot k non-empty:', have)

# ref counts per slot over the whole pickle
rhave = [0, 0, 0, 0]
miss = 0
for r in refs:
	b = bases.get(r[1])
	if b is None:
		miss += 1
		continue
	for i in range(4):
		if b['slots'][i]:
			rhave[i] += 1
print('refs whose base has slot k:', rhave, ' refs with no base in pickle:', miss)

# types present
print('base types:', collections.Counter(b['type'] for b in bases.values()).most_common(12))

# --- BTO control opens?
from pics_nifread import Nif
p = MESH + 'Terrain/Commonwealth/Objects/Commonwealth.4.4.-12.BTO'
print('BTO exists', os.path.getsize(p))
try:
	n = Nif(p)
	tt = sum(s['numTris'] for s in n.shapes.values())
	vv = sum(s['numVerts'] for s in n.shapes.values())
	print('BTO ok: %d blocks %d nodes %d shapes  tris %d verts %d' % (n.numBlocks, len(n.nodes), len(n.shapes), tt, vv))
	print('problems:', n.problems[:3])
except Exception as e:
	print('BTO FAILED:', type(e).__name__, e)

# --- a sample LOD mesh opens?
for b in bases.values():
	if b['slots'][0]:
		q = MESH + b['slots'][0].replace('\\', '/')
		print('sample slot0 path %r exists %s' % (b['slots'][0], os.path.exists(q)))
		if os.path.exists(q):
			m = Nif(q)
			print('  %d shapes %d tris' % (len(m.shapes), sum(s['numTris'] for s in m.shapes.values())))
		break
