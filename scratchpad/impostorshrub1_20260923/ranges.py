"""IMPOSTORSHRUB1: what are the ranges of a BSMeshLODTriShape?  Reads the exe's
own `dump --all` of one block and prints, per range (LOD0 | LOD1 | LOD2 slot, in
triangle-list order), the triangle count, the distinct vertex set, its box and
its projected area proxy (sum of triangle areas), plus the vertex overlap
between ranges. Two ranges that are ALTERNATIVE detail levels of one plant
share a box and not their vertices; two that are PARTS of one plant do not
share a box.

  python ranges.py NIF BLOCK"""
import re, subprocess, sys
import numpy as np
NS = 'E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_impostorshrub1.exe'
nif, blk = sys.argv[1], sys.argv[2]
t = subprocess.run([NS, '-no-gui', 'dump', nif, '-b', blk, '--all', '-n', '1000000'],
                   capture_output=True, text=True).stdout
lod = [int(re.search(r'LOD%d Size  <uint>  = (\d+)' % k, t).group(1)) for k in range(3)]
V = np.array([[float(a) for a in m] for m in re.findall(
    r'Vertex  <HalfVector3>  = X (\S+) Y (\S+) Z (\S+)', t)] or [[float(a) for a in m] for m in re.findall(
    r'Vertex  <Vector3>  = X (\S+) Y (\S+) Z (\S+)', t)])
T = np.array([[int(a) for a in m] for m in re.findall(r'Triangles  <Triangle>  = (\d+) (\d+) (\d+)', t)])
print('LOD sizes', lod, 'triangles', len(T), 'vertices', len(V))
o, sets = 0, []
for k in range(3):
    tr = T[o:o + lod[k]]; o += lod[k]
    if not len(tr):
        sets.append(set()); print('  range %d: empty' % k); continue
    vs = set(tr.ravel().tolist()); sets.append(vs)
    P = V[sorted(vs)]
    a = V[tr[:, 1]] - V[tr[:, 0]]; b = V[tr[:, 2]] - V[tr[:, 0]]
    area = 0.5 * np.linalg.norm(np.cross(a, b), axis=1).sum()
    print('  range %d: %4d tris %4d verts  box min %s max %s  area %.0f' % (
        k, len(tr), len(vs), np.round(P.min(0), 1), np.round(P.max(0), 1), area))
for i in range(3):
    for j in range(i + 1, 3):
        if sets[i] and sets[j]:
            print('  shared vertices range %d/%d: %d' % (i, j, len(sets[i] & sets[j])))
