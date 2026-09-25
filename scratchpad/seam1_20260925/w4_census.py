"""W4a: vertex colour census of the LOD models placed in downtown Boston (cells -5,-10..2,-3).
Bases from the bake's own chunk manifests (rows inside the cell box), base -> MNAM slot meshes from the
.lodo base/mesh tables (docs/LODGEN_NATIVE_LODO_LODI.md 3.2), NIFs read in place from the game BA2s.
Prints one verdict block; per-model TSV to w4_models.tsv."""
import struct, sys, os, glob, collections
import numpy as np
T = r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools'
sys.path.insert(0, T)
import ba2lib, nifwind
D = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth'
X0, Y0, X1, Y1 = -5, -10, 2, -3
lodo = open(D + '/Commonwealth.lodo', 'rb').read()
assert lodo[:4] == b'LODO'
baseCount, meshCount = struct.unpack_from('<II', lodo, 0x50)
offBase, offMesh = struct.unpack_from('<QQ', lodo, 0x70)
offStr = struct.unpack_from('<Q', lodo, 0xA0)[0]
def cstr(o):
    e = lodo.index(b'\0', offStr + o); return lodo[offStr + o:e].decode('utf-8')
meshPath = [cstr(struct.unpack_from('<I', lodo, offMesh + 56 * m + 48)[0]) for m in range(meshCount)]
bases = {}
for i in range(baseCount):
    o = offBase + 32 * i
    fid, so = struct.unpack_from('<II', lodo, o)
    rep = struct.unpack_from('<4H', lodo, o + 8)
    bases[fid] = (cstr(so), rep)
# placements in the box, from the manifests
cnt = collections.Counter()
for f in glob.glob(D + '/Commonwealth.4.*.BTO.manifest.txt'):
    for ln in open(f):
        if ln.startswith('#'): continue
        p = ln.split()
        if len(p) != 11: continue
        x, y = float(p[3]), float(p[4])
        cx, cy = int(np.floor(x / 4096)), int(np.floor(y / 4096))
        if X0 <= cx <= X1 and Y0 <= cy <= Y1:
            cnt[(int(p[1], 16), p[8])] += 1
print('placements in box (dim-4 manifests):', sum(cnt.values()), 'distinct bases', len({k[0] for k in cnt}))
arcs = [ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Meshes.ba2'),
        ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - MeshesExtra.ba2')]
def getnif(path):
    n = path.lower().replace('/', chr(92))
    if not n.startswith('meshes' + chr(92)): n = 'meshes' + chr(92) + n
    for a in arcs:
        try: d = ba2lib.get(a, n)
        except KeyError: d = None
        if d: return d
    return None
rows = []; missing = 0; byClass = collections.Counter(); placedBy = collections.Counter()
seen = {}
for (fid, cls), c in cnt.items():
    if fid not in bases: missing += c; continue
    mp, rep = bases[fid]
    m = rep[0]
    if m == 0xFFFF: continue
    path = meshPath[m]
    placedBy[path] += c
    if path in seen: continue
    data = getnif(path)
    if data is None: seen[path] = None; continue
    n = nifwind.Nif(data)
    shapes = []
    for k, (t, o, sz) in enumerate(n.blocks):
        if t in nifwind.SHAPES:
            sh = n.shape(k)
            shapes.append(sh)
    seen[path] = (cls, shapes)
# verdict
tot = collections.Counter()
out = open('w4_models.tsv', 'w')
out.write('model\tclass\tplaced\tshapes\tverts\twithColour\tRmin/max/sd\tGmin/max/sd\tBmin/max/sd\tAmin/max/sd\tdistinctRGB\twhiteRGBshare\n')
for path, v in sorted(seen.items(), key=lambda kv: -placedBy[kv[0]]):
    if v is None:
        tot['nifMissing'] += 1; continue
    cls, shapes = v
    cols = [s['cols'] for s in shapes if s['cols'] is not None]
    nv = sum(s['nv'] for s in shapes)
    tot['models'] += 1; tot['placed'] += placedBy[path]
    if not cols:
        out.write('%s\t%s\t%d\t%d\t%d\t0\t-\t-\t-\t-\t-\t-\n' % (path, cls, placedBy[path], len(shapes), nv)); tot['noColour'] += 1; continue
    C = np.concatenate(cols)
    st = ['%d/%d/%.1f' % (C[:, j].min(), C[:, j].max(), C[:, j].std()) for j in range(4)]
    rgb = C[:, 0] * 65536 + C[:, 1] * 256 + C[:, 2]
    dist = len(np.unique(rgb)); white = float(np.mean(rgb == 0xFFFFFF))
    varies = C[:, :3].std(axis=0).max() > 1.0 or white < 0.99
    tot['colour'] += 1; tot['rgbVaries'] += int(varies); tot['placedVaries'] += placedBy[path] * int(varies)
    tot['aVaries'] += int(C[:, 3].std() > 1.0)
    out.write('%s\t%s\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%d\t%.3f\n' % (path, cls, placedBy[path], len(shapes), nv, len(C), *st, dist, white))
print('base not in .lodo:', missing, 'placements')
print(dict(tot))
