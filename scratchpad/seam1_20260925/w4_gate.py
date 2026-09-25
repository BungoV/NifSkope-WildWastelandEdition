"""W4 gate, pre-registered 2026-09-25 before the build (bungo's W4 ruling; docs/LODGEN_NATIVE_LODO_LODI.md 3.7).
usage: w4_gate.py <old dir> <new dir>     (dirs written by w4_bakes.sh: fx/, boston/Native/, sanc/Native/)
  G1 no colour -> the same bytes but the version word.
     G1a the fixture: new .lodo == old .lodo except byte 0x04 (4 -> 5); new .lodi == old .lodi.
     G1b each region: the new .lodo with its colour STRIPPED (mesh bits 8/16 cleared, 0xD4..0xDF zeroed,
         the blob cut, fileBytes and both CRCs recomputed) == the old .lodo except byte 0x04. A region
         whose new file carries no colour must match with NO strip, and its .lodi byte for byte; one
         that carries colour may differ in its .lodi only at lodoIdentity (0x20..0x27) and headerCrc32
         (0x0C..0x0F), since lodoIdentity is derived from the .lodo headerCrc32.
  G2 the colour is where the game draws it, and only there (Boston):
     G2a Amphitheater_LOD_0, TreeMapleblasted01_LOD_1, TreeMapleblasted02_LOD_1 are VERTEX_COLOUR meshes;
     G2b Amphitheater and TreeMapleblasted01 carry RGB other than white;
     G2c every mesh of the file: flagged VERTEX_COLOUR <=> its source NIF has a shape with a colour
         channel AND SLSF2 Vertex_Colors; VERTEX_ALPHA <=> such a shape also has SLSF1 Vertex_Alpha;
     G2d per flagged mesh: the set of decoded RGBA rows == the set of its source VC shapes' RGBA
         (plus opaque white, which a non-VC shape of the same mesh writes).
  G3 the refuter: `w4_gate.py <old> <old>` must print G2 RED (the current code writes no colour).
Reads with the INDEPENDENT decoder tests/spells/lodgen_native_decode.py, source NIFs in place from the BA2s."""
import sys, os, struct, zlib, collections
R = 'E:/Projects/NifskopeWWE-seam1'
sys.path.insert(0, R + '/tests/spells')
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
import lodgen_native_decode as dec
import ba2lib, nifwind

OLD, NEW = sys.argv[1], sys.argv[2]
BS = chr(92)
verdict = collections.OrderedDict()


def rd(p):
    return open(p, 'rb').read()


def diffs(a, b):
    n = min(len(a), len(b))
    d = [i for i in range(n) if a[i] != b[i]]
    return d, len(a) - len(b)


def strip(b):
    """the v5 file as a writer that knew no colour would have written it (version word left alone)"""
    b = bytearray(b)
    if struct.unpack_from('<I', b, 4)[0] != 5:
        return bytes(b)
    cnt, off = struct.unpack_from('<IQ', b, 0xD4)
    meshCount = struct.unpack_from('<I', b, 0x54)[0]
    offMeshes = struct.unpack_from('<Q', b, 0x78)[0]
    for i in range(meshCount):
        o = offMeshes + i * 56 + 46       # the mesh row u16 flags: 10 floats, clusterFirst u32, clusterCount u16
        f = struct.unpack_from('<H', b, o)[0]
        struct.pack_into('<H', b, o, f & ~(8 | 16))
    b[0xD4:0xE0] = bytes(12)
    if cnt:
        offStrings, stringBytes = struct.unpack_from('<Q', b, 0xA0)[0], struct.unpack_from('<I', b, 0x6C)[0]
        del b[offStrings + stringBytes:]
        struct.pack_into('<Q', b, 0xB0, len(b))
    # indexCrc32 over the eight v4 tables, in order
    offs = struct.unpack_from('<QQQQQQQ', b, 0x70)
    h = dict(zip(('bases', 'meshes', 'clusters', 'materials', 'li', 'verts', 'strings'), offs))
    baseCount, meshCount, clusterCount, materialCount, vertexCount = struct.unpack_from('<IIIII', b, 0x50)
    stringBytes = struct.unpack_from('<I', b, 0x6C)[0]
    offLods = struct.unpack_from('<Q', b, 0xC0)[0]
    crc = 0
    for off, size in ((h['bases'], baseCount * 32), (h['meshes'], meshCount * 56), (h['clusters'], clusterCount * 16),
                      (offLods, clusterCount * 48), (h['materials'], materialCount * 16), (h['li'], clusterCount * 48),
                      (h['verts'], vertexCount * 16), (h['strings'], stringBytes)):
        crc = zlib.crc32(bytes(b[off:off + size]), crc)
    struct.pack_into('<I', b, 0xA8, crc & 0xFFFFFFFF)
    struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])) & 0xFFFFFFFF)
    return bytes(b)


def pair(d):
    lo = [f for f in os.listdir(d) if f.endswith('.lodo')]
    li = [f for f in os.listdir(d) if f.endswith('.lodi')]
    assert len(lo) == 1 and len(li) == 1, (d, lo, li)
    return d + '/' + lo[0], d + '/' + li[0]


# ------------------------------------------------------------------ G1 re-pin, attributed (2026-09-25)
# ATTRIBUTED: two selfAO bytes of the library .lodo (vertex rows 270124 and 271177, byte 15, both on the water-tower
# meshes) read 228 in the pre-v5 bake and 201 from commit 62e53a3b on: one more of the 8 fixed hemisphere rays hits
# (1 - 0.85*1/8 = 0.894 -> 228; 1 - 0.85*2/8 = 0.788 -> 201). NOT a law change: the AO block of lodofile.cpp is the
# same text at a6e5e8de and 62e53a3b, and lodgenao.h is untouched. 62e53a3b grew lodoAppendMesh (the colour stream)
# around the inlined ambientOcclusion, and -O3 -march=haswell contracts a*b+c into FMA per its own codegen; a ray at a
# grazing tie flips. Measured (coordinator order (A), bisect/): exe a6e5e8de 228, exe 62e53a3b 201; the same two
# trees with lodofile.cpp under #pragma GCC optimize("fp-contract=off") bake BYTE-IDENTICAL W4 files (w4_bytes.py
# fpc_a6e5e8de fpc_HEAD: 0 diffs, both regions). The pin: exactly these two bytes, exactly 228 -> 201, may differ;
# they are put back before the compare, so every other byte (and both CRCs, recomputed by strip) still has to match.
# A listed byte holding any third value REFUSES the file. The old gate is kept as w4_gate_prepin.py.
ATTRIBUTED = [(270124, 15, 228, 201), (271177, 15, 228, 201)]


def attributed(A, B):
    if B[4] != 5 or A[4] != 4:
        return B, []
    offV = struct.unpack_from('<Q', A, 0x98)[0]      # offVertices (header 0x70 + 5*8)
    oB = struct.unpack_from('<Q', B, 0x98)[0]
    b = bytearray(B); out = []
    for row, byte, was, now in ATTRIBUTED:
        oa, ob = offV + row * 16 + byte, oB + row * 16 + byte
        if A[oa] != was:
            return B, 'REFUSED'
        if b[ob] == now:
            b[ob] = was; out.append('row %d %d->%d' % (row, now, was))
        elif b[ob] != was:
            return B, 'REFUSED'
    return bytes(b), out


# ------------------------------------------------------------------ G1
def g1(tag, dold, dnew, fixture):
    (ao, ai), (bo, bi) = pair(dold), pair(dnew)
    A, B = rd(ao), rd(bo)
    LB = dec.read_lodo(bo)                  # the new file must decode
    colour = LB['header']['colourVertexCount'] if B[4] == 5 else 0
    applied = []
    if not fixture:
        B, applied = attributed(A, B)
    S = B if (fixture or not colour) else strip(B)
    d, dl = diffs(A, S)
    okLodo = dl == 0 and d == ([4] if B[4] != A[4] else [])
    di, dli = diffs(rd(ai), rd(bi))
    allowed = set() if not colour else set(range(0x0C, 0x10)) | set(range(0x20, 0x28))
    okLodi = dli == 0 and set(di) <= allowed
    print('G1 %-7s colourRows %6d | .lodo %s: %d bytes differ %s, length %+d -> %s | .lodi %d differ %s -> %s' % (
        tag, colour, 'stripped' if S is not B else 'as written', len(d), ['0x%X' % x for x in d[:6]], dl,
        'ok' if okLodo else 'FAIL', len(di), ['0x%X' % x for x in di[:8]], 'ok' if okLodi else 'FAIL'))
    print('   %-7s attributed AO bytes put back before the compare (62e53a3b codegen, see ATTRIBUTED): %s' % (
        tag, applied if applied != 'REFUSED' else 'REFUSED -- a listed byte holds neither value'))
    return okLodo and okLodi and (not fixture or colour == 0) and applied != 'REFUSED'


g1ok = [g1('fixture', OLD + '/fx', NEW + '/fx', True),
        g1('boston', OLD + '/boston/Native/FO4CSLOD/Commonwealth', NEW + '/boston/Native/FO4CSLOD/Commonwealth', False),
        g1('sanc', OLD + '/sanc/Native/FO4CSLOD/Commonwealth', NEW + '/sanc/Native/FO4CSLOD/Commonwealth', False)]
verdict['G1'] = all(g1ok)

# ------------------------------------------------------------------ G2
bo, _ = pair(NEW + '/boston/Native/FO4CSLOD/Commonwealth')
L = dec.read_lodo(bo)
h = L['header']
arcs = [ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Meshes.ba2'),
        ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - MeshesExtra.ba2')]


def src(path):
    n = path.lower().replace('/', BS)
    if not n.startswith('meshes' + BS):
        n = 'meshes' + BS + n
    for a in arcs:
        try:
            d = ba2lib.get(a, n)
        except KeyError:
            d = None
        if d:
            N = nifwind.Nif(d)
            out = []
            for k, (t, o, sz) in enumerate(N.blocks):
                if t in nifwind.SHAPES:
                    sh = N.shape(k)
                    f1, f2, _ = N.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(N.blocks) else (0, 0, '')
                    out.append((sh, (f1 or 0), (f2 or 0)))
            return out
    return None


rows = collections.defaultdict(set)
for ci, c in enumerate(L['clusters']):
    for v in range(c['vertexBase'], c['vertexBase'] + c['vertexCount']):
        if L['colours'][v] is not None:
            rows[c['meshId']].add(L['colours'][v])
names = {i: L['string_at'](m['modelStringOffset']) for i, m in enumerate(L['meshes'])}
want = {'amphitheater_lod_0.nif': True, 'treemapleblasted01_lod_1.nif': True, 'treemapleblasted02_lod_1.nif': False}
g2a, g2b, g2c, g2d = [], [], [], []
nMissing = 0
for i, m in enumerate(L['meshes']):
    base = names[i].split(BS)[-1].split('/')[-1].lower()
    flagged, va = bool(m['flags'] & 8), bool(m['flags'] & 16)
    if base in want:
        g2a.append((base, flagged))
        if want[base]:
            g2b.append((base, any(r[:3] != (255, 255, 255) for r in rows[i])))
    S = src(names[i])
    if S is None:
        nMissing += 1
        continue
    vc = [(sh, f1) for sh, f1, f2 in S if sh['cols'] is not None and f2 & 0x20]
    expectFlag, expectVa = bool(vc), any(f1 & 8 for sh, f1 in vc)
    if flagged != expectFlag or va != expectVa:
        g2c.append((names[i], flagged, expectFlag, va, expectVa))
    if flagged:
        srcSet = {tuple(c4) for sh, f1 in vc for c4 in sh['cols']}
        extra = rows[i] - srcSet - {(255, 255, 255, 255)}
        if extra or not srcSet <= rows[i]:
            g2d.append((names[i], len(rows[i]), len(srcSet), len(extra), len(srcSet - rows[i])))
nFlag = sum(1 for m in L['meshes'] if m['flags'] & 8)
print('G2 boston v%d: %d meshes, %d VERTEX_COLOUR, %d VERTEX_ALPHA, colourRows %d, source NIFs missing %d' % (
    h['version'], h['meshCount'], nFlag, sum(1 for m in L['meshes'] if m['flags'] & 16), h['colourVertexCount'], nMissing))
print('G2a named meshes flagged:', g2a)
print('G2b non-white RGB:', g2b)
print('G2c flag <=> source (channel AND Vertex_Colors; Vertex_Alpha): %d disagree %s' % (len(g2c), g2c[:5]))
print('G2d decoded rows == source rows: %d disagree %s' % (len(g2d), g2d[:5]))
verdict['G2'] = (len(g2a) == 3 and all(f for _, f in g2a) and len(g2b) == 2 and all(f for _, f in g2b)
                 and not g2c and not g2d and nMissing == 0)
for k, v in verdict.items():
    print('%s %s' % (k, 'GREEN' if v else 'RED'))
