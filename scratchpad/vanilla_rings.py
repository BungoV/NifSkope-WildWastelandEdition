"""Vanilla Commonwealth object-LOD triangle counts per ring, measured offline."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from btocount import parse

V = 'E:/Tools/Fallout 4/DataUnpacked/Data/meshes/terrain/commonwealth/objects'
names = os.listdir(V)
byDim = {}
for n in names:
    if not n.lower().endswith('.bto'):
        continue
    parts = n.split('.')
    # Commonwealth.<dim>.<x>.<y>.BTO
    try:
        dim = int(parts[1]); x = int(parts[2]); y = int(parts[3])
    except (ValueError, IndexError):
        continue
    byDim.setdefault(dim, []).append((x, y, n))

print('%-5s %6s %10s %10s %10s %10s %10s' % ('dim', 'files', 'tris', 'verts', 'tri/chunk', 'tri/cell', 'shapes'))
for dim in sorted(byDim):
    tot_t = tot_v = tot_s = 0
    for x, y, n in byDim[dim]:
        sh = parse(os.path.join(V, n))
        tot_t += sum(s[4] for s in sh)
        tot_v += sum(s[3] for s in sh)
        tot_s += len(sh)
    f = len(byDim[dim])
    print('%-5d %6d %10d %10d %10.1f %10.2f %10.2f'
          % (dim, f, tot_t, tot_v, tot_t / f, tot_t / (f * dim * dim), tot_s / f))

print()
print('Sanctuary cell (-20,24), the chunk that covers it at each ring:')
for dim in sorted(byDim):
    cx = (-20 // dim) * dim
    cy = (24 // dim) * dim
    n = 'Commonwealth.%d.%d.%d.BTO' % (dim, cx, cy)
    p = os.path.join(V, n)
    if not os.path.exists(p):
        print('  dim %-3d chunk (%d,%d): NO FILE' % (dim, cx, cy))
        continue
    sh = parse(p)
    print('  dim %-3d chunk (%4d,%4d): shapes %d verts %d tris %d'
          % (dim, cx, cy, len(sh), sum(s[3] for s in sh), sum(s[4] for s in sh)))
