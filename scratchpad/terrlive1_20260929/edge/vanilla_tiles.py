"""TERRLIVE1: bytes a hybrid pyramid spends on tiles that hold NO painted quadrant (law 2 = vanilla's diffuse
untouched there, plus our normal/mask/height). usage: python vanilla_tiles.py <dir with VT.*.lodt>"""
import sys, os, glob
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtread
d = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cells.npz'))
realq, minX, minY = d['realq'], int(d['minX']), int(d['minY'])
tot = van = 0
for p in sorted(glob.glob(os.path.join(sys.argv[1], '*.VT.*.lodt'))):
    v = vtread.Vt(p); b = vb = n = nv = 0
    for ty in range(v.tilesY):
        for tx in range(v.tilesX):
            i = ty * v.tilesX + tx
            if not (v.tFlags[i] & 1): continue
            cx0 = v.west + tx * v.levelDim; cy1 = v.north - ty * v.levelDim; cy0 = cy1 - v.levelDim + 1
            q = realq[max(0, 2 * (cy0 - minY) - 1):2 * (cy1 + 1 - minY) + 1, max(0, 2 * (cx0 - minX) - 1):2 * (cx0 + v.levelDim - minX) + 1]
            s = int(v.tStored[i]); b += s; n += 1
            if not q.any(): vb += s; nv += 1
    print(f'{os.path.basename(p)}: {n} tiles {b} bytes; tiles with no painted quadrant (incl. a quadrant ring) {nv} = {vb} bytes ({100*vb/max(b,1):.1f}%)')
    tot += b; van += vb
print(f'total {tot} bytes; on all-vanilla tiles {van} bytes ({100*van/max(tot,1):.1f}%)')
