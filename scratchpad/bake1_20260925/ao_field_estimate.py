"""How big is the vertex-AO scene of each chunk (src/nativeemit.cpp ~2306)? The scene holds EVERY instance of
EVERY ring whose origin is inside the chunk's field (the chunk plus one cell of skirt), with the triangles of
the mesh that instance draws. Read from the bake's own .lodj cache (placements: base form, position, slot) and
the .lodo library (base -> mesh per slot, mesh triangle counts).
usage: ao_field_estimate.py <dir with .lodj + .lodo>"""
import sys, os, glob, struct, collections
sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake1/tests/spells')
from lodgen_native_decode import read_lodo

d = sys.argv[1]
L = read_lodo(os.path.join(d, 'Commonwealth.lodo'))
print('lodo keys', sorted(k for k in L.keys())[:40])
tris = [sum(L['clusters'][c]['triangleCount'] for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCountL0']))
        for m in L['meshes']]
rep = {b['formId']: [b['rep0'], b['rep1'], b['rep2'], b['rep3']] for b in L['bases']}
# placements of every ring: (x, y, triangles)
pts = []
missing = 0
for f in glob.glob(os.path.join(d, '*.lodj')):
    for line in open(f):
        if not line.startswith('p' + chr(9)):
            continue
        fd = line.rstrip(chr(10)).split(chr(9))
        base = int(fd[1], 16)
        x = struct.unpack('<f', bytes.fromhex(fd[4])[::-1])[0]
        y = struct.unpack('<f', bytes.fromhex(fd[5])[::-1])[0]
        slot = int(fd[17])
        r = rep.get(base)
        if r is None or slot > 3 or r[slot] == 65535:
            missing += 1
            continue
        pts.append((x, y, tris[r[slot]]))
print('placements with a mesh', len(pts), 'without', missing, 'triangles total', sum(p[2] for p in pts))
cell = collections.Counter()
for x, y, t in pts:
    cell[(int(x // 4096), int(y // 4096))] += t
worst = []
for dim in (4, 8, 16, 32):
    best = (0, None)
    for cx in range(-96, 96, dim):
        for cy in range(-96, 96, dim):
            s = sum(cell.get((x, y), 0) for x in range(cx - 1, cx + dim + 1) for y in range(cy - 1, cy + dim + 1))
            if s > best[0]:
                best = (s, (cx, cy))
    print('dim %2d worst field %s: %d triangles, ~%.2f GB of scene (tri 36 B + bin refs, x2 vector growth)'
          % (dim, best[1], best[0], best[0] * 100 / 1e9))
