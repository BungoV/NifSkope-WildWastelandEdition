#!/usr/bin/env python3
"""LODLEVELS probe 2: the BUDGET.  How many placements survive the cull and how
many triangles they carry, per camera per level.  Verdict lines only."""
import sys
import time

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919'
sys.path.insert(0, LANE)

import pics_build as B                              # noqa: E402
from pics_cams import build as build_cams           # noqa: E402
from pics_scene import Terrain                      # noqa: E402

t0 = time.time()
ter = Terrain()
print('terrain %.1fs %s' % (time.time() - t0, ter.h.shape), flush=True)
cams = build_cams(ter)
D = B.load_pickle()
refs, bases = D['refs'], D['bases']

for cn in ('east', 'full'):
	cam = cams[cn]
	print('camera %s eye %s' % (cn, cam.eye.round(0)), flush=True)
	for k, L in enumerate(B.LEVELS):
		t = time.time()
		idx = B.visible_refs(refs, bases, k, cam)
		ob = B.build_level(refs, bases, k, idx, verbose=False)
		nkit = int(ob.kit.sum())
		print('  LOD%-2d slot %d: culled-in %6d  placed %6d  tris %8d  kit %6d  (%.1fs)'
			  % (L, k, len(idx), len(ob.refid), len(ob.tri), nkit, time.time() - t), flush=True)

print('missing slot meshes on disk: %d' % len(B.MISSING))
print('unreadable: %d %s' % (len(B.UNREADABLE), list(B.UNREADABLE.items())[:3]))
