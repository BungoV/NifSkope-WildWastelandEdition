"""TINT1 gates on the whole-map object library, pre-registered 2026-09-25 before the re-bake finished.
usage: gate.py <old Commonwealth dir> <new Commonwealth dir>
  OLD = the installed BAKE1 library (v4, exe 27a7bb29, VT on); NEW = this lane's re-bake (main d5764fbe, no VT).
  G1  (W4 G1 on the whole library) strip(new .lodo) == old .lodo except byte 0x04. Every other differing byte is
      ATTRIBUTED: table, row, field, mesh name, old -> new value. The .lodi: identical except lodoIdentity
      (0x20..0x27) and headerCrc32 (0x0C..0x0F) when the new .lodo carries colour; every other byte attributed.
  G2  (W4 G2 on the whole library) every mesh: flagged VERTEX_COLOUR <=> its source NIF (resolved through his MO2
      stack like census.py) has a shape with a colour channel AND SLSF2 Vertex_Colors; VERTEX_ALPHA <=> such a
      shape has SLSF1 Vertex_Alpha; per flagged mesh the decoded RGBA row set == the source VC rows (+ opaque
      white from a non-VC shape of the same mesh); Amphitheater_LOD_0 and TreeMapleblasted01_LOD_1 non-white.
  G3  refuter: gate.py <old> <old> must be RED on G2 (a v4 file carries no colour)."""
import sys, os, struct, zlib, collections
import numpy as np
BS = chr(92)
sys.path.insert(0, r'E:/Projects/NifskopeWWE-tint1/tests/spells')
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/seam1_20260925')
import lodgen_native_decode as dec
import ba2lib, nifwind
OLD, NEW = sys.argv[1], sys.argv[2]
W = 'Commonwealth'
verdict = collections.OrderedDict()
rd = lambda p: open(p, 'rb').read()


def strip(b):   # SEAM1 w4_gate.strip, verbatim logic
    b = bytearray(b)
    if struct.unpack_from('<I', b, 4)[0] != 5:
        return bytes(b)
    cnt, off = struct.unpack_from('<IQ', b, 0xD4)
    meshCount = struct.unpack_from('<I', b, 0x54)[0]
    offMeshes = struct.unpack_from('<Q', b, 0x78)[0]
    for i in range(meshCount):
        o = offMeshes + i * 56 + 46
        f = struct.unpack_from('<H', b, o)[0]
        struct.pack_into('<H', b, o, f & ~(8 | 16))
    b[0xD4:0xE0] = bytes(12)
    if cnt:
        offStrings, stringBytes = struct.unpack_from('<Q', b, 0xA0)[0], struct.unpack_from('<I', b, 0x6C)[0]
        del b[offStrings + stringBytes:]
        struct.pack_into('<Q', b, 0xB0, len(b))
    offs = struct.unpack_from('<QQQQQQQ', b, 0x70)
    h = dict(zip(('bases', 'meshes', 'clusters', 'materials', 'li', 'verts', 'strings'), offs))
    baseCount, meshCount, clusterCount, materialCount, vertexCount = struct.unpack_from('<IIIII', b, 0x50)
    stringBytes = struct.unpack_from('<I', b, 0x6C)[0]
    offLods = struct.unpack_from('<Q', b, 0xC0)[0]
    crc = 0
    for o2, size in ((h['bases'], baseCount * 32), (h['meshes'], meshCount * 56), (h['clusters'], clusterCount * 16),
                     (offLods, clusterCount * 48), (h['materials'], materialCount * 16), (h['li'], clusterCount * 48),
                     (h['verts'], vertexCount * 16), (h['strings'], stringBytes)):
        crc = zlib.crc32(bytes(b[o2:o2 + size]), crc)
    struct.pack_into('<I', b, 0xA8, crc & 0xFFFFFFFF)
    struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])) & 0xFFFFFFFF)
    return bytes(b)


def regions(h, kind):
    if kind == 'lodo':
        t = [('bases', h['offBases'], h['baseCount'] * 32, 32), ('meshes', h['offMeshes'], h['meshCount'] * 56, 56),
             ('clusters', h['offClusters'], h['clusterCount'] * 16, 16),
             ('clusterLods', h['offClusterLods'], h['clusterCount'] * 48, 48),
             ('materials', h['offMaterials'], h['materialCount'] * 16, 16),
             ('localIndices', h['offLocalIndices'], h['clusterCount'] * 48, 48),
             ('vertices', h['offVertices'], h['vertexCount'] * 16, 16), ('strings', h['offStrings'], h['stringBytes'], 1)]
    else:
        t = [('chunks', h['offChunks'], h['chunkCount'] * 32, 32), ('cellRanges', h['offCellRanges'], h['presentChunks'] * 16 * 8, 8),
             ('instances', h['offInstances'], h['instanceCount'] * 24, 24), ('cold', h['offCold'], h['instanceCount'] * 8, 8),
             ('occluders', h['offOccluders'], h['occluderCount'] * 40, 40),
             ('occluderRanges', h['offOccluderRanges'], h['presentChunks'] * 16 * 8, 8),
             ('placementAo', h['offPlacementAo'], h['placementAoCount'], 1), ('vertexAo', h['offVertexAo'], h['vertexAoBytes'], 1),
             ('group', h['offGroup'], h['groupCount'] * 2, 2), ('vertexSky', h['offVertexSky'], h['vertexSkyBytes'], 1)]
    return [('header', 0, 256, 256)] + t


def where(off, regs):
    for name, o, sz, stride in regs:
        if o <= off < o + sz:
            return name, (off - o) // stride, (off - o) % stride
    return 'pad', 0, off


def attribute(A, S, kind, hA, meshOfVertex=None, names=None):
    d = np.nonzero(np.frombuffer(A, np.uint8) != np.frombuffer(S, np.uint8))[0] if len(A) == len(S) else None
    if d is None:
        return None
    regs = regions(hA, kind)
    groups = collections.Counter(); ex = collections.defaultdict(list)
    for off in d.tolist():
        name, row, byte = where(off, regs)
        key = (name, byte) if name not in ('header',) else (name, byte)
        groups[key] += 1
        if len(ex[key]) < 3:
            extra = ''
            if name == 'vertices' and meshOfVertex is not None:
                extra = names[meshOfVertex[row]].split(BS)[-1]
            ex[key].append((row, A[off], S[off], extra))
    return d, groups, ex


# ---------------------------------------------------------------- G1 .lodo
ao, bo = OLD + '/' + W + '.lodo', NEW + '/' + W + '.lodo'
A, B = rd(ao), rd(bo)
LA, LB = dec.read_lodo(ao), dec.read_lodo(bo)
colour = LB['header'].get('colourVertexCount', 0)
S = strip(B)
names = [LA['string_at'](m['modelStringOffset']) for m in LA['meshes']]
mov = np.zeros(LA['header']['vertexCount'], np.int64)
for c in LA['clusters']:
    mov[c['vertexBase']:c['vertexBase'] + c['vertexCount']] = c['meshId']
print('lodo old v%d %d B, new v%d %d B, colourRows %d, stripped %d B' % (A[4], len(A), B[4], len(B), colour, len(S)))
r = attribute(A, S, 'lodo', LA['header'], mov, names)
if r is None:
    print('G1 lodo: LENGTH differs after strip (%+d); header words:' % (len(S) - len(A)))
    for k in ('baseCount', 'meshCount', 'clusterCount', 'materialCount', 'vertexCount', 'stringBytes', 'cardCount'):
        print('   %-14s old %s new %s' % (k, LA['header'][k], LB['header'][k]))
    g1lodo = False
else:
    d, groups, ex = r
    rest = {k: v for k, v in groups.items() if k != ('header', 4)}
    print('G1 lodo: %d bytes differ; version word %s; others %d in %d (table, byte) groups' % (
        len(d), 'differs' if ('header', 4) in groups else 'same', sum(rest.values()), len(rest)))
    for k, v in sorted(rest.items(), key=lambda kv: -kv[1])[:25]:
        print('   %-12s byte %-4s x%-6d e.g. %s' % (k[0], k[1], v, ex[k]))
    g1lodo = not rest
verdict['G1 lodo (0x04 only)'] = g1lodo

# ---------------------------------------------------------------- G1 .lodi
ai, bi = OLD + '/' + W + '.lodi', NEW + '/' + W + '.lodi'
AI, BI = rd(ai), rd(bi)
TA = dec.read_lodi(ai); TB = dec.read_lodi(bi)
allowed = set(range(0x0C, 0x10)) | set(range(0x20, 0x28)) if colour else set()
print('lodi old %d B new %d B, instances old %d new %d' % (len(AI), len(BI), TA['header']['instanceCount'], TB['header']['instanceCount']))
r = attribute(AI, BI, 'lodi', TA['header'])
if r is None:
    print('G1 lodi: LENGTH differs (%+d)' % (len(BI) - len(AI)))
    for k in ('instanceCount', 'chunkCount', 'presentChunks', 'occluderCount', 'groupCount', 'vertexAoBytes'):
        print('   %-14s old %s new %s' % (k, TA['header'][k], TB['header'][k]))
    g1lodi = False
else:
    d, groups, ex = r
    rest = {k: v for k, v in groups.items() if not (k[0] == 'header' and k[1] in allowed)}
    print('G1 lodi: %d bytes differ, %d outside lodoIdentity/headerCrc32 in %d groups' % (len(d), sum(rest.values()), len(rest)))
    for k, v in sorted(rest.items(), key=lambda kv: -kv[1])[:25]:
        print('   %-12s byte %-4s x%-6d e.g. %s' % (k[0], k[1], v, ex[k]))
    g1lodi = not rest
verdict['G1 lodi'] = g1lodi

# ---------------------------------------------------------------- G2 against the source, whole library
RES = [l.strip() for l in open(r'E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt') if l.strip()]
arcs = []
for rr in reversed(RES):
    try:
        fs = sorted(f for f in os.listdir(rr) if f.lower().endswith('.ba2'))
    except OSError:
        continue
    for f in fs:
        a = ba2lib.load(rr + '/' + f)
        if a is not None:
            arcs.append(a)


def src(path):
    n = path.lower().replace('/', BS)
    if not n.startswith('meshes' + BS):
        n = 'meshes' + BS + n
    data = None
    for rr in reversed(RES):
        p = rr + '/' + n.replace(BS, '/')
        if os.path.isfile(p):
            data = rd(p); break
    if data is None:
        for a in arcs:
            if n in a['recs']:
                data = ba2lib.get(a, n); break
    if data is None:
        return None
    N = nifwind.Nif(data); out = []
    for k, (t, o, sz) in enumerate(N.blocks):
        if t in nifwind.SHAPES:
            sh = N.shape(k)
            f1, f2, _ = N.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(N.blocks) else (0, 0, '')
            out.append((sh, f1 or 0, f2 or 0))
    return out


L = LB
rows = collections.defaultdict(set)
cols = L.get('colours')
if cols is not None:
    for c in L['clusters']:
        for v in range(c['vertexBase'], c['vertexBase'] + c['vertexCount']):
            if cols[v] is not None:
                rows[c['meshId']].add(cols[v])
nm = {i: L['string_at'](m['modelStringOffset']) for i, m in enumerate(L['meshes'])}
want = {'amphitheater_lod_0.nif', 'treemapleblasted01_lod_1.nif'}
g2b, g2c, g2d = {}, [], []
nMissing = nFlag = nVa = 0
for i, m in enumerate(L['meshes']):
    base = nm[i].split(BS)[-1].split('/')[-1].lower()
    flagged, va = bool(m['flags'] & 8), bool(m['flags'] & 16)
    nFlag += flagged; nVa += va
    if base in want:
        g2b[base] = flagged and any(x[:3] != (255, 255, 255) for x in rows[i])
    Sx = src(nm[i])
    if Sx is None:
        nMissing += 1; continue
    vc = [(sh, f1) for sh, f1, f2 in Sx if sh['cols'] is not None and f2 & 0x20]
    if flagged != bool(vc) or va != any(f1 & 8 for sh, f1 in vc):
        g2c.append((nm[i], flagged, bool(vc), va))
    if flagged:
        srcSet = {tuple(int(x) for x in c4) for sh, f1 in vc for c4 in sh['cols']}
        extra = rows[i] - srcSet - {(255, 255, 255, 255)}
        if extra or not srcSet <= rows[i]:
            g2d.append((nm[i], len(rows[i]), len(srcSet), len(extra), len(srcSet - rows[i])))
print('G2 new v%d: %d meshes, %d VERTEX_COLOUR, %d VERTEX_ALPHA, colourRows %d, source NIFs missing %d' % (
    L['header']['version'], L['header']['meshCount'], nFlag, nVa, L['header'].get('colourVertexCount', 0), nMissing))
print('G2 named meshes non-white:', g2b)
print('G2 flag <=> source: %d disagree %s' % (len(g2c), g2c[:5]))
print('G2 decoded rows == source rows: %d disagree %s' % (len(g2d), g2d[:5]))
verdict['G2'] = len(g2b) == 2 and all(g2b.values()) and not g2c and not g2d and nMissing == 0
for k, v in verdict.items():
    print('%s %s' % (k, 'GREEN' if v else 'RED'))
