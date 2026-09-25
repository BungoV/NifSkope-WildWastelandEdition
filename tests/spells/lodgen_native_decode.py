#!/usr/bin/env python
"""Independent decoder for the FO4CS-native far field: a `.lodo` + `.lodi`
pair read back with NO code shared with the C++ writers (src/lodofile.cpp,
src/lodifile.cpp), from docs/LODGEN_NATIVE_LODO_LODI.md's byte tables alone.

Three jobs, picked by the arguments:

  lodgen_native_decode.py <ws>.lodo <ws>.lodi
      validate both files by every refusal the contract names, print the
      census (`lodo.* / lodi.*` lines) and exit 1 on any refusal;

  ... --expect <Synthetic.expect.txt>
      the KNOWN-ANSWER control: check the pair against answers written down
      BEFORE the run by lodNativeFixtureWrite (src/lodifile.cpp), and against
      nothing the writer produced;

  ... --esm <Fallout4.esm> --worldspace 3C [--chunk X Y DIM]...
      cross-check instance count, base ids and positions against an
      independent walk of the plugin (tools/lod_emission_probe.py's record
      walker, which links none of our C++): every (ref, part) in the named
      stock chunks must be in the table with the same base form, the same
      position to 0.125 u in X/Y, and a rotation within 0.02 deg of the
      ESM's DATA euler composed as lodgen composes it (negated angles,
      Rx.Ry.Rz), times the tree yaw recomputed from the position hash.

  ... --manifest <chunk>.BTO.manifest.txt ...
      the second leg: rows of a stock manifest (index base type x y z scale
      class radius ref part) against the table.

Exit 0 = every check passed; 1 = a refusal or a mismatch, named.
"""
import argparse
import math
import os
import struct
import sys
import zlib

LODO_MAGIC = b'LODO'
LODI_MAGIC = b'LODI'
NO_MESH = 0xFFFF
NO_CARD = 0xFFFF
NO_LAYER = 0xFFFF
CHUNK_UNITS = 16384.0
CELL_UNITS = 4096.0
SQRT_HALF = math.sqrt(0.5)

#: One step of the instance position quantiser in X/Y (NATIVE 4.1: 3 x u16 over
#: the 16,384-unit chunk box).  `lodoQuantU16` rounds to nearest, so a stored
#: position is at most HALF a step from the float the writer sorted on.
POS_QUANT_STEP = CHUNK_UNITS / 65535.0
#: Half a step, plus two ulp of 16,384 for the float32 round trip the C++ side
#: does and this decoder recomputes in double.  THE CELL AMBIGUITY BAND: a
#: position this close to a cell line could have been on either side of it
#: before quantisation, so the writer's cell and ours may differ by one.
CELL_QUANT_TOL = POS_QUANT_STEP / 2.0 + CHUNK_UNITS * 2.0 ** -23


class Refusal(Exception):
    pass


#: `LodoClusterLod::parentError` at a root -- FLT_MAX, so no tolerance is above it
ROOT_ERROR = struct.unpack('<f', struct.pack('<I', 0x7F7FFFFF))[0]


#: the --esm argument, so the pair checks can reproduce the load-order hash
a_esm_for_load_order = [None]


def crc32(b, seed=0):
    return zlib.crc32(b, seed) & 0xFFFFFFFF


def fnv1a64(b, h=0xCBF29CE484222325):
    for c in b:
        h ^= c
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def le(fmt, b, off):
    return struct.unpack_from('<' + fmt, b, off)


# ----------------------------------------------------------------- .lodo
def read_lodo(path):
    b = open(path, 'rb').read()
    if len(b) < 256:
        raise Refusal('%s: shorter than the 256-byte header' % path)
    if b[:4] != LODO_MAGIC:
        raise Refusal('%s: magic %r is not LODO' % (path, b[:4]))
    h = {}
    h['version'], h['flags'], h['headerCrc32'] = le('III', b, 4)
    if h['version'] == 1:
        raise Refusal('version 1: the v1 vertex blob is in source order and carries no loadOrderHash')
    if h['version'] == 2:
        raise Refusal('version 2: a v2 library has no cluster ladder table (header 0xC0 was reserved), '
                      'so every cluster would read error 0 and be drawn at full detail at every distance')
    if h['version'] == 3:
        raise Refusal('version 3: a v3 base row spends crossPx16[0..1] on two screen-size steps in '
                      '1/16 px, and this reader takes those same four bytes as the base FULL-DETAIL '
                      'TRIANGLE COUNT. Re-bake; this reader knows versions 4 and 5')
    # v5 (lane SEAM1, W4) = v4 + an optional per-vertex RGBA8 colour stream; a v4 file reads as v5 without one
    if h['version'] not in (4, 5):
        raise Refusal('version %d' % h['version'])
    if crc32(b[0x10:0x100]) != h['headerCrc32']:
        raise Refusal('headerCrc32 mismatch')
    if not h['flags'] & 1:
        raise Refusal('flags bit0 clear')
    if h['flags'] & ~15:
        raise Refusal('reserved flag bits set')
    (h['pluginCorpusHash'], h['objectCorpusHash'], h['modelCorpusHash'],
     h['cardCorpusHash']) = le('QQQQ', b, 0x10)
    edid = b[0x30:0x50]
    if b'\0' not in edid:
        raise Refusal('worldspace editor ID not NUL-terminated within 32 bytes')
    h['worldspace'] = edid.split(b'\0')[0].decode('utf-8')
    (h['baseCount'], h['meshCount'], h['clusterCount'], h['materialCount'],
     h['vertexCount'], h['maxClustersPerMesh'], h['clusterMaxTris'],
     h['vertexStride'], h['stringBytes']) = le('IIIIIIHHI', b, 0x50)
    if h['clusterMaxTris'] != 16:
        raise Refusal('clusterMaxTris %d' % h['clusterMaxTris'])
    if h['vertexStride'] != 16:
        raise Refusal('vertexStride %d' % h['vertexStride'])
    (h['offBases'], h['offMeshes'], h['offClusters'], h['offMaterials'],
     h['offLocalIndices'], h['offVertices'], h['offStrings']) = le('QQQQQQQ', b, 0x70)
    h['indexCrc32'], reservedAC = le('II', b, 0xA8)
    if reservedAC:
        raise Refusal('reserved word at 0xAC is not zero')
    h['fileBytes'] = le('Q', b, 0xB0)[0]
    h['loadOrderHash'] = le('Q', b, 0xB8)[0]
    # v3: the ladder's room, 0xC0..0xCD; the pad now starts at 0xCE
    h['offClusterLods'] = le('Q', b, 0xC0)[0]
    h['clusterLodStride'] = le('I', b, 0xC8)[0]
    h['levelMax'] = b[0xCC]
    h['ladderGroup'] = b[0xCD]
    if h['clusterLodStride'] != 48:
        raise Refusal('clusterLodStride %d; the v3 row is 48 bytes' % h['clusterLodStride'])
    if h['levelMax'] > 15:
        raise Refusal('levelMax %d past the format cap' % h['levelMax'])
    if bool(h['flags'] & 8) != bool(h['ladderGroup']):
        raise Refusal('the LADDER flag and ladderGroup disagree (%d / %d)' % (h['flags'] & 8, h['ladderGroup']))
    if not h['flags'] & 8 and h['levelMax']:
        raise Refusal('the LADDER flag is clear but levelMax is %d' % h['levelMax'])
    # v4: the card count lives at 0xD0; the pad is 0xCE..0xCF and 0xD4..0xFF
    # v5: colourVertexCount u32 at 0xD4, offColours u64 at 0xD8; the pad is 0xCE..0xCF and 0xE0..0xFF
    h['cardCount'] = le('I', b, 0xD0)[0]
    h['colourVertexCount'], h['offColours'] = (le('IQ', b, 0xD4) if h['version'] == 5 else (0, 0))
    padFrom = 0xE0 if h['version'] == 5 else 0xD4
    if any(b[0xCE:0xD0]) or any(b[padFrom:0x100]):
        raise Refusal('reserved bytes 0xCE..0xCF / 0x%02X..0xFF not zero' % padFrom)
    if bool(h['colourVertexCount']) != bool(h['offColours']):
        raise Refusal('colourVertexCount %d and offColours %d: both or neither'
                      % (h['colourVertexCount'], h['offColours']))
    if h['colourVertexCount'] > h['vertexCount']:
        raise Refusal('colourVertexCount %d past the %d vertices' % (h['colourVertexCount'], h['vertexCount']))
    if h['cardCount'] > h['baseCount']:
        raise Refusal('cardCount %d is more than the %d bases' % (h['cardCount'], h['baseCount']))
    if h['fileBytes'] != len(b):
        raise Refusal('fileBytes %d but file is %d' % (h['fileBytes'], len(b)))
    tabs = [('bases', h['offBases'], h['baseCount'] * 32),
            ('meshes', h['offMeshes'], h['meshCount'] * 56),
            ('clusters', h['offClusters'], h['clusterCount'] * 16),
            ('clusterLods', h['offClusterLods'], h['clusterCount'] * 48),
            ('materials', h['offMaterials'], h['materialCount'] * 16),
            ('localIndices', h['offLocalIndices'], h['clusterCount'] * 48),
            ('vertices', h['offVertices'], h['vertexCount'] * 16),
            ('strings', h['offStrings'], h['stringBytes'])]
    if h['colourVertexCount']:
        # v5: the colour blob is written LAST, and is in indexCrc32 only when present
        tabs.append(('colours', h['offColours'], h['colourVertexCount'] * 4))
    prev = 256
    crc = 0
    for name, off, size in tabs:
        if off % 4096:
            raise Refusal('%s offset %d not 4096-aligned' % (name, off))
        if off < prev:
            raise Refusal('%s out of table order' % name)
        if off + size > len(b):
            raise Refusal('%s runs past the file' % name)
        if any(b[prev:off]):
            raise Refusal('pad before %s not zero' % name)
        crc = crc32(b[off:off + size], crc)
        prev = off + size
    if crc != h['indexCrc32']:
        raise Refusal('indexCrc32 mismatch')
    strings = b[h['offStrings']:h['offStrings'] + h['stringBytes']]
    if not strings or strings[0] != 0 or strings[-1] != 0:
        raise Refusal('string blob malformed')

    def string_at(off):
        if off >= len(strings):
            raise Refusal('string offset %d past stringBytes' % off)
        return strings[off:strings.index(b'\0', off)].decode('utf-8')

    L = {'header': h, 'strings': strings, 'string_at': string_at}
    # v4: the row is still 32 bytes, but crossPx16[0..1] is now one u32
    # fullTriangles -- the base's FULL-DETAIL triangle count
    L['bases'] = [dict(zip(('formId', 'modelStringOffset', 'rep0', 'rep1', 'rep2', 'rep3', 'cardLayer',
                            'flags', 'boundRadius', 'fullTriangles', 'cross2', 'cross3'),
                           le('IIHHHHHHfIHH', b, h['offBases'] + i * 32))) for i in range(h['baseCount'])]
    # v3: the mesh row's v2 reserved word became clusterCountL0 + levelCount + a byte
    L['meshes'] = [dict(zip(('aabbMin', 'aabbExtent', 'uvMin', 'uvExtent', 'clusterFirst', 'clusterCount',
                             'flags', 'modelStringOffset', 'clusterCountL0', 'levelCount', 'reserved'),
                            (lambda t: (t[0:3], t[3:6], t[6:8], t[8:10]) + t[10:])(
                                le('ffffffffffIHHIHBB', b, h['offMeshes'] + i * 56)))) for i in range(h['meshCount'])]
    L['clusters'] = [dict(zip(('vertexBase', 'vertexCount', 'triangleCount', 'materialId', 'bc0', 'bc1', 'bc2',
                               'boundRadius', 'meshId', 'flags'),
                              le('IBBHBBBBHH', b, h['offClusters'] + i * 16))) for i in range(h['clusterCount'])]
    L['clusterLods'] = [dict(zip(('cx', 'cy', 'cz', 'radius', 'geometricError', 'parentError',
                                  'parentFirst', 'parentCount', 'level', 'reserved0',
                                  'coneAxis0', 'coneAxis1', 'coneCos', 'sourceTriangles', 'reserved1'),
                                 le('ffffffIHBBHHfII', b, h['offClusterLods'] + i * 48)))
                        for i in range(h['clusterCount'])]
    L['materials'] = [dict(zip(('arrayClass', 'arraySet', 'layer', 'family', 'alphaThreshold', 'flags', 'reserved',
                                'emissiveScale', 'lodmStringOffset'),
                               le('BBHBBBBfI', b, h['offMaterials'] + i * 16))) for i in range(h['materialCount'])]
    L['localIndices'] = b[h['offLocalIndices']:h['offLocalIndices'] + h['clusterCount'] * 48]
    L['vertices'] = [dict(zip(('px', 'py', 'pz', 'u', 'v', 'n0', 'n1', 'n2', 'tangent', 'sway', 'selfAO'),
                              le('HHHHHBBBBBB', b, h['offVertices'] + i * 16))) for i in range(h['vertexCount'])]

    # v5: scatter the colour rows back per vertex (None = no colour), the flagged meshes in mesh order,
    # each over its clusters' ONE contiguous vertex range
    L['colours'] = [None] * h['vertexCount']
    row = 0
    for i, m in enumerate(L['meshes']):
        if m['flags'] & ~(1 | 2 | 4 | (8 | 16 if h['version'] == 5 else 0)):
            raise Refusal('mesh %d flags 0x%x: unknown bits for version %d' % (i, m['flags'], h['version']))
        if m['flags'] & 16 and not m['flags'] & 8:
            raise Refusal('mesh %d: VERTEX_ALPHA without VERTEX_COLOUR' % i)
        if not m['flags'] & 8:
            continue
        cs = L['clusters'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
        lo = min((c['vertexBase'] for c in cs), default=0)
        hi = max((c['vertexBase'] + c['vertexCount'] for c in cs), default=0)
        n = sum(c['vertexCount'] for c in cs)
        if n == 0 or hi - lo != n:
            raise Refusal('mesh %d flagged VERTEX_COLOUR but its vertices are not one contiguous range' % i)
        if row + n > h['colourVertexCount']:
            raise Refusal('colour rows run past colourVertexCount at mesh %d' % i)
        o = h['offColours'] + row * 4
        for v in range(lo, hi):
            L['colours'][v] = tuple(b[o + 4 * (v - lo):o + 4 * (v - lo) + 4])
        row += n
    if row != h['colourVertexCount']:
        raise Refusal('colourVertexCount %d but the flagged meshes hold %d' % (h['colourVertexCount'], row))

    # row rules
    for i, m in enumerate(L['meshes']):
        if m['reserved']:
            raise Refusal('mesh %d reserved not zero' % i)
        if m['clusterFirst'] + m['clusterCount'] > h['clusterCount']:
            raise Refusal('mesh %d cluster range past table' % i)
        if m['levelCount'] == 0:
            raise Refusal('mesh %d levelCount is 0' % i)
        rows = L['clusterLods'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
        nL0 = sum(1 for r in rows if r['level'] == 0)
        lvMax = max([r['level'] for r in rows], default=0)
        if nL0 != m['clusterCountL0']:
            raise Refusal('mesh %d clusterCountL0 %d but %d rows are level 0' % (i, m['clusterCountL0'], nL0))
        if lvMax + 1 != m['levelCount']:
            raise Refusal('mesh %d levelCount %d but its deepest level is %d' % (i, m['levelCount'], lvMax))
        if i and string_at(L['meshes'][i - 1]['modelStringOffset']).lower().replace('/', '\\') >= \
                string_at(m['modelStringOffset']).lower().replace('/', '\\'):
            raise Refusal('mesh table not sorted by path at %d' % i)
    if L['meshes'] and max(m['clusterCount'] for m in L['meshes']) != h['maxClustersPerMesh']:
        raise Refusal('maxClustersPerMesh wrong')
    if L['clusterLods'] and max(r['level'] for r in L['clusterLods']) != h['levelMax']:
        raise Refusal('levelMax %d but the ladder table reaches %d'
                      % (h['levelMax'], max(r['level'] for r in L['clusterLods'])))
    for i, c in enumerate(L['clusters']):
        if not 1 <= c['vertexCount'] <= 48 or not 1 <= c['triangleCount'] <= 16:
            raise Refusal('cluster %d counts out of range' % i)
        if c['vertexBase'] + c['vertexCount'] > h['vertexCount']:
            raise Refusal('cluster %d vertices past blob' % i)
        if c['meshId'] >= h['meshCount'] or c['materialId'] >= h['materialCount']:
            raise Refusal('cluster %d ids out of range' % i)
        if c['flags'] & ~7:
            raise Refusal('cluster %d reserved flags' % i)
        want = 0 if c['triangleCount'] <= 4 else 1 if c['triangleCount'] <= 8 else 2
        if c['flags'] & 3 != want:
            raise Refusal('cluster %d size class %d for %d tris' % (i, c['flags'] & 3, c['triangleCount']))
        li = L['localIndices'][i * 48:(i + 1) * 48]
        for k in range(48):
            if k < c['triangleCount'] * 3:
                if li[k] >= c['vertexCount']:
                    raise Refusal('cluster %d local index %d >= %d' % (i, li[k], c['vertexCount']))
            elif li[k] != 0xFF:
                raise Refusal('cluster %d slot %d past triangleCount not 0xFF' % (i, k))
        m = L['meshes'][c['meshId']]
        if not m['clusterFirst'] <= i < m['clusterFirst'] + m['clusterCount']:
            raise Refusal('cluster %d outside its mesh range' % i)
        cl = L['clusterLods'][i]
        if i and (L['clusters'][i - 1]['meshId'], L['clusters'][i - 1]['materialId'],
                  L['clusterLods'][i - 1]['level']) > (c['meshId'], c['materialId'], cl['level']):
            raise Refusal('cluster table not sorted by (meshId, materialId, level) at %d' % i)
        # ---- v3: the ladder row parallel to this cluster ----
        if cl['reserved0'] or cl['reserved1']:
            raise Refusal('cluster %d ladder row reserved field not zero' % i)
        if not cl['radius'] > 0:
            raise Refusal('cluster %d sphere radius %r' % (i, cl['radius']))
        if cl['level'] > h['levelMax']:
            raise Refusal('cluster %d level %d past levelMax' % (i, cl['level']))
        if cl['level'] == 0 and cl['geometricError'] != 0.0:
            raise Refusal('cluster %d is level 0 with error %r' % (i, cl['geometricError']))
        if cl['level'] == 0 and cl['sourceTriangles'] != c['triangleCount']:
            raise Refusal('cluster %d is level 0 but sourceTriangles %d != triangleCount %d'
                          % (i, cl['sourceTriangles'], c['triangleCount']))
        if cl['level'] > 0 and not cl['geometricError'] > 0:
            raise Refusal('cluster %d is level %d with error %r' % (i, cl['level'], cl['geometricError']))
        isroot = cl['parentFirst'] == 0xFFFFFFFF
        if isroot != (cl['parentCount'] == 0) or isroot != (cl['parentError'] == ROOT_ERROR):
            raise Refusal('cluster %d disagrees with itself about being a root' % i)
        if not cl['geometricError'] <= cl['parentError']:
            raise Refusal('cluster %d: geometricError %r > parentError %r -- the ladder is not monotone'
                          % (i, cl['geometricError'], cl['parentError']))
        if not isroot:
            if cl['parentFirst'] + cl['parentCount'] > h['clusterCount']:
                raise Refusal('cluster %d parent range past the table' % i)
            pc = L['clusters'][cl['parentFirst']]
            pl = L['clusterLods'][cl['parentFirst']]
            if pc['meshId'] != c['meshId'] or pc['materialId'] != c['materialId']:
                raise Refusal('cluster %d parent crosses a mesh or a material' % i)
            if pl['level'] != cl['level'] + 1:
                raise Refusal('cluster %d parent is at level %d, not %d' % (i, pl['level'], cl['level'] + 1))
            if pl['geometricError'] != cl['parentError']:
                raise Refusal('cluster %d parentError does not equal its parent error' % i)
        if c['flags'] & 4:
            if cl['coneAxis0'] or cl['coneAxis1'] or cl['coneCos'] != -1.0:
                raise Refusal('cluster %d is CONE_OPEN but carries a cone' % i)
        elif not 0.0 < cl['coneCos'] <= 1.0:
            raise Refusal('cluster %d cone cosine %r outside (0, 1]' % (i, cl['coneCos']))
    for i, m in enumerate(L['materials']):
        if m['reserved']:
            raise Refusal('material %d reserved not zero' % i)
        if m['layer'] != NO_LAYER and m['layer'] >= 2048:
            raise Refusal('material %d layer %d >= 2048' % (i, m['layer']))
        if m['family'] > 1:
            raise Refusal('material %d family %d' % (i, m['family']))
        key = lambda x: (x['family'], x['arrayClass'], x['arraySet'], x['layer'],
                         string_at(x['lodmStringOffset']).lower().replace('/', '\\'))
        if i and key(L['materials'][i - 1]) > key(m):
            raise Refusal('material table not sorted at %d' % i)
    for i, bse in enumerate(L['bases']):
        if i and L['bases'][i - 1]['formId'] >= bse['formId']:
            raise Refusal('base table not sorted by formId at %d' % i)
        reps = [bse['rep%d' % k] for k in range(4)]
        for r in reps:
            if r != NO_MESH and r >= h['meshCount']:
                raise Refusal('base %08x rep past meshCount' % bse['formId'])
        if all(r == NO_MESH for r in reps) and bse['cardLayer'] == NO_CARD:
            raise Refusal('base %08x has no mesh and no card' % bse['formId'])
        if not bse['boundRadius'] > 0:
            raise Refusal('base %08x boundRadius 0' % bse['formId'])
        if bse['flags'] & ~7:
            raise Refusal('base %08x reserved flags' % bse['formId'])
        string_at(bse['modelStringOffset'])
    return L


def load_order_hash(plugin_list):
    """EsmWorld::loadOrderHash, from the same comma-separated list: for each
    plugin IN ORDER, the lower-cased base file name's UTF-8 bytes then its byte
    size as a little-endian u64, folded through FNV-1a 64. Nothing else."""
    h = 0xCBF29CE484222325
    for one in plugin_list.split(','):
        one = one.strip()
        if not one:
            continue
        h = fnv1a64(os.path.basename(one).lower().encode('utf-8'), h)
        h = fnv1a64(struct.pack('<Q', os.path.getsize(one)), h)
    return h


def draw_key_ranks(L):
    """The (primary mesh, that mesh's first material) rank of every base, the
    number the instance record's drawKey must carry (src/nativeemit.cpp)."""
    pairs = []
    for bse in L['bases']:
        mid = NO_MESH
        for k in range(4):
            if mid == NO_MESH:
                mid = bse['rep%d' % k]
        mat = 0xFFFF
        if mid != NO_MESH:
            m = L['meshes'][mid]
            if m['clusterCount']:
                mat = L['clusters'][m['clusterFirst']]['materialId']
        pairs.append((mid, mat))
    order = sorted(set(pairs))
    rank = dict((p, i) for i, p in enumerate(order))
    return [rank[p] for p in pairs]


def print_step(token):
    """The manifest prints coordinates with six significant digits, so its own
    print step depends on the magnitude: 143360 comes back as `143360` and the
    fraction is GONE. Budget half a step of it, or the bar measures the
    manifest's printf and not the writer (lane NATIVE1a; lane BUILD6 saw the
    0.1 case and this is the 1.0 one)."""
    s = token[1:] if token.startswith('-') else token
    if 'e' in s or 'E' in s:
        return 0.0
    if '.' in s:
        return 10.0 ** -len(s.split('.')[1])
    return 10.0 ** (len(s) - 6) if len(s) > 6 else 1.0


def lodo_identity(h):
    return fnv1a64(struct.pack('<IQQ', h['headerCrc32'], h['modelCorpusHash'], h['objectCorpusHash']))


# ----------------------------------------------------------------- .lodi
def unpack_rotation(r0, r1, r2):
    bits = r0 | (r1 << 16) | (r2 << 32)
    big = bits & 3
    q = [0.0] * 4
    shift = 2
    ss = 0.0
    for i in range(4):
        if i == big:
            continue
        u = (bits >> shift) & 0x7FFF
        q[i] = (u / 32767.0 * 2.0 - 1.0) * SQRT_HALF
        ss += q[i] * q[i]
        shift += 15
    q[big] = math.sqrt(max(0.0, 1.0 - ss))
    w, x, y, z = q
    return [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
            2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
            2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]


def mat_angle_deg(a, b):
    """Angle between two rotations: acos((trace(A^T B) - 1) / 2)."""
    t = sum(a[r * 3 + c] * b[r * 3 + c] for r in range(3) for c in range(3))
    return math.degrees(math.acos(max(-1.0, min(1.0, (t - 1.0) / 2.0))))


def read_lodi(path):
    b = open(path, 'rb').read()
    if len(b) < 256:
        raise Refusal('%s: shorter than the 256-byte header' % path)
    if b[:4] != LODI_MAGIC:
        raise Refusal('%s: magic %r is not LODI' % (path, b[:4]))
    h = {}
    h['version'], h['flags'], h['headerCrc32'] = le('III', b, 4)
    if h['version'] == 1:
        raise Refusal("version 1: the v1 record's 0x16 word is reserved where v2 carries drawKey, "
                      "and the v1 cold record's is reserved where v2 carries the stock identity")
    if h['version'] == 2:
        raise Refusal('version 2: a v2 instance table has no occluder tables (header 0x98 and 0xA0 were '
                      'reserved), so every cell would read as occluding nothing')
    if h['version'] not in (3, 4, 5, 6, 7, 8, 9, 10):
        raise Refusal('version %d; this reader knows 3, 4, 5, 6, 7, 8, 9 and 10' % h['version'])
    # v8 = v7 + the per-vertex HORIZON stream, in the room v7's header left reserved.
    # RETIRED 2026-09-19 (lane HORIZONOUT): no exe writes one, this reader still reads one.
    v8 = h['version'] == 8
    # v9 = v7 + instance flag bit 6, the workshop-scrappable bit. It is a superset of v7
    # and NOT of v8: a v9 file carries no horizon stream and its 0x11C..0x1FF are reserved.
    # v10 = v9 + instance flag bit 7, SCALE_WIDE: scale = 8 + u16 / 8192 (lane BAKE2, 2026-09-25).
    v10 = h['version'] == 10
    v9 = h['version'] == 9 or v10
    # v7 = v6 + the group table and/or the per-vertex sky stream, in a 512-byte header BLOCK
    v7 = h['version'] == 7 or v8 or v9
    v6 = h['version'] == 6 or v7    # v6 = v5 + the per-instance vertex-AO stream
    v5 = h['version'] == 5 or v6
    hdr = 512 if v7 else 256
    if len(b) < hdr:
        raise Refusal('%s: shorter than the %d-byte version-%d header' % (path, hdr, h['version']))
    if crc32(b[0x10:hdr]) != h['headerCrc32']:
        raise Refusal('headerCrc32 mismatch')
    if not h['flags'] & 1:
        raise Refusal('ROW_ORDER_NORTH_UP clear')
    if h['flags'] & ~7:
        raise Refusal('reserved flag bits set')
    h['pluginCorpusHash'], h['objectCorpusHash'], h['lodoIdentity'] = le('QQQ', b, 0x10)
    if h['lodoIdentity'] == 0 and not h['flags'] & 4:
        raise Refusal('lodoIdentity 0 without NOLIB')
    if h['lodoIdentity'] != 0 and h['flags'] & 4:
        raise Refusal('NOLIB with a non-zero identity')
    edid = b[0x28:0x48]
    if b'\0' not in edid:
        raise Refusal('worldspace editor ID not NUL-terminated')
    h['worldspace'] = edid.split(b'\0')[0].decode('utf-8')
    (h['chunkWest'], h['chunkSouth'], h['chunkEast'], h['chunkNorth'], h['chunkCells'], h['instanceStride'],
     h['chunkCount'], h['instanceCount'], h['presentChunks'], h['maxInstancesPerChunk'],
     h['indexCrc32']) = le('hhhhHHIIIII', b, 0x48)
    if h['chunkCells'] != 4 or h['instanceStride'] != 24:
        raise Refusal('chunkCells %d / instanceStride %d' % (h['chunkCells'], h['instanceStride']))
    h['offChunks'], h['offCellRanges'], h['offInstances'], h['offCold'], h['fileBytes'] = le('QQQQQ', b, 0x68)
    h['loadOrderHash'] = le('Q', b, 0x90)[0]
    # v3: the occluders' room, 0x98..0xAF; the pad now starts at 0xB0
    h['offOccluders'], h['offOccluderRanges'] = le('QQ', b, 0x98)
    h['occluderCount'] = le('I', b, 0xA8)[0]
    h['occluderStride'], h['maxOccludersPerCell'] = le('HH', b, 0xAC)
    if h['occluderStride'] != 40:
        raise Refusal('occluderStride %d; the v3 row is 40 bytes' % h['occluderStride'])
    if h['maxOccludersPerCell'] == 0:
        raise Refusal('maxOccludersPerCell is 0')
    # v4 the aggregate words 0xB0..0xD3; v5 the four slot totals 0xD4..0xE3 and
    # the placement-AO words 0xE4..0xF0.  The pad starts where the version says.
    h['aggregateCount'] = h['coveredCount'] = 0
    h['offAggregates'] = h['offCovered'] = 0
    if h['version'] >= 4:
        h['offAggregates'], h['offCovered'] = le('QQ', b, 0xB0)
        h['aggregateCount'], h['coveredCount'] = le('II', b, 0xC0)
        h['aggregateStride'], h['aggregateViews'] = le('HH', b, 0xC8)
        if h['aggregateCount'] and h['aggregateStride'] != 48:
            raise Refusal('aggregateStride %d; the v4 row is 48 bytes' % h['aggregateStride'])
    h['slotInstances'] = [0, 0, 0, 0]
    h['offPlacementAo'] = h['placementAoCount'] = h['placementAoStride'] = 0
    if v5:
        h['slotInstances'] = list(le('IIII', b, 0xD4))
        if sum(h['slotInstances']) != h['instanceCount']:
            raise Refusal('the four MNAM-slot instance totals sum to %d, not the %d instances'
                          % (sum(h['slotInstances']), h['instanceCount']))
        h['offPlacementAo'] = le('Q', b, 0xE4)[0]
        h['placementAoCount'] = le('I', b, 0xEC)[0]
        h['placementAoStride'] = b[0xF0]
        if h['placementAoStride'] != 1:
            raise Refusal('placementAoStride %d; the v5 blob is one byte an instance'
                          % h['placementAoStride'])
        if h['offPlacementAo'] == 0 or h['placementAoCount'] == 0:
            raise Refusal('version 5 with no placement-AO blob: version 5 IS the blob, and a file '
                          'without one is written at version 3 or 4')
        if h['placementAoCount'] != h['instanceCount']:
            raise Refusal('placementAoCount %d but %d instances'
                          % (h['placementAoCount'], h['instanceCount']))
    elif any(b[0xE4:0xF1]):
        raise Refusal('version %d carries placement-AO header words; the blob is a version 5 payload '
                      'and this file does not say so' % h['version'])
    # v6: the vertex-AO stream words 0xF4 (offset, u64) and 0xFC (bytes, u32); the pad is 0xF1..0xF3
    h['offVertexAo'] = h['vertexAoBytes'] = 0
    if v6:
        h['offVertexAo'] = le('Q', b, 0xF4)[0]
        h['vertexAoBytes'] = le('I', b, 0xFC)[0]
        if h['offVertexAo'] == 0 or h['vertexAoBytes'] < 4 * (h['instanceCount'] + 1):
            raise Refusal('version 6 with no vertex-AO stream: version 6 IS the stream, and a file '
                          'without one is a version 5 file')
    elif any(b[0xF4:0x100]):
        raise Refusal('version %d carries vertex-AO header words; the stream is a version 6 payload '
                      'and this file does not say so' % h['version'])
    if v6 and any(b[0xF1:0xF4]):
        raise Refusal('reserved bytes 0xF1..0xF3 not zero')
    padFrom = 0x100 if v6 else (0xF1 if v5 else (0xD4 if h['version'] == 4 else 0xB0))
    if any(b[padFrom:0x100]):
        raise Refusal('reserved bytes 0x%02X..0xFF not zero' % padFrom)
    # v7: the group table (0x100/0x108/0x10C) and the vertex-sky stream (0x110/0x118)
    h['offGroup'] = h['groupCount'] = h['groupStride'] = 0
    h['offVertexSky'] = h['vertexSkyBytes'] = 0
    if v7:
        h['offGroup'] = le('Q', b, 0x100)[0]
        h['groupCount'] = le('I', b, 0x108)[0]
        h['groupStride'] = le('H', b, 0x10C)[0]
        h['offVertexSky'] = le('Q', b, 0x110)[0]
        h['vertexSkyBytes'] = le('I', b, 0x118)[0]
        if h['offGroup'] == 0 and h['offVertexSky'] == 0:
            raise Refusal('version 7 carrying neither a group table (header 0x100) nor a vertex-sky '
                          'stream (header 0x110); version 7 IS one of the two')
        if h['offGroup']:
            if h['groupStride'] != 2:
                raise Refusal('groupStride %d; this reader knows 2' % h['groupStride'])
            if h['groupCount'] == 0 and h['instanceCount']:
                raise Refusal('a group table is present but groupCount is 0')
        elif h['groupCount'] or h['groupStride']:
            raise Refusal('no group table but groupCount %d / groupStride %d say otherwise'
                          % (h['groupCount'], h['groupStride']))
        if h['offVertexSky']:
            if h['vertexSkyBytes'] < 4 * (h['instanceCount'] + 1):
                raise Refusal('vertex-sky stream of %d bytes cannot hold its own %d offset words'
                              % (h['vertexSkyBytes'], h['instanceCount'] + 1))
        elif h['vertexSkyBytes']:
            raise Refusal('no vertex-sky stream but vertexSkyBytes is %d' % h['vertexSkyBytes'])
        # v8: the vertex-horizon stream at 0x11C/0x124, its stride at 0x128, and the
        # two numbers the cast was run with at 0x12A/0x12C. Version 8 IS the stream:
        # a bake without one is written at version 7, so an absent stream is a
        # refusal and not a quiet zero.
        h['offVertexHorizon'] = h['vertexHorizonBytes'] = 0
        h['horizonAzimuths'] = h['horizonSteps'] = 0
        h['horizonReach'] = 0.0
        if v7 and v8:
            h['offVertexHorizon'] = le('Q', b, 0x11C)[0]
            h['vertexHorizonBytes'] = le('I', b, 0x124)[0]
            h['horizonAzimuths'] = le('H', b, 0x128)[0]
            h['horizonSteps'] = le('H', b, 0x12A)[0]
            h['horizonReach'] = le('f', b, 0x12C)[0]
            if h['offVertexHorizon'] == 0:
                raise Refusal('version 8 with no vertex-horizon stream (header 0x11C is 0); '
                              'the stream is what version 8 IS')
            if h['vertexHorizonBytes'] < 4 * (h['instanceCount'] + 1):
                raise Refusal('vertex-horizon stream of %d bytes cannot hold its own %d offset words'
                              % (h['vertexHorizonBytes'], h['instanceCount'] + 1))
            # the stride is the bin count, and the sheet packs bins four to an RGBA
            # sheet, so a count that is not a multiple of four leaves a dead channel
            if h['horizonAzimuths'] < 4 or h['horizonAzimuths'] > 64 or h['horizonAzimuths'] % 4:
                raise Refusal('horizonAzimuths %d; the stride is 4..64 in steps of 4'
                              % h['horizonAzimuths'])
            if not (h['horizonReach'] > 0.0):
                raise Refusal('horizonReach %g; a march that walks nowhere measures nothing'
                              % h['horizonReach'])
            if any(b[0x130:0x200]):
                raise Refusal('reserved bytes 0x130..0x1FF not zero')
        elif v7:
            if any(b[0x11C:0x200]):
                raise Refusal('reserved bytes 0x11C..0x1FF not zero (version-8 words in a version-7 file)')
    elif any(b[0x100:0x120]) and len(b) >= 0x120:
        raise Refusal('version %d carrying version-7 header words; versions 3 to 6 have a 256-byte '
                      'header and end at 0x100' % h['version'])
    if h['fileBytes'] != len(b):
        raise Refusal('fileBytes %d but file is %d' % (h['fileBytes'], len(b)))
    w = h['chunkEast'] - h['chunkWest'] + 1
    n = h['chunkNorth'] - h['chunkSouth'] + 1
    if h['instanceCount'] and h['chunkCount'] != w * n:
        raise Refusal('chunkCount %d but extent implies %d' % (h['chunkCount'], w * n))
    if h['chunkCount'] > 65536:
        raise Refusal('chunkCount past cap')
    tabs = [('chunks', h['offChunks'], h['chunkCount'] * 32),
            ('cellRanges', h['offCellRanges'], h['presentChunks'] * 16 * 8),
            ('instances', h['offInstances'], h['instanceCount'] * 24),
            ('cold', h['offCold'], h['instanceCount'] * 8),
            ('occluders', h['offOccluders'], h['occluderCount'] * 40),
            ('occluderRanges', h['offOccluderRanges'], h['presentChunks'] * 16 * 8)]
    iAgg = iPao = -1
    if h['version'] == 4 or (v5 and h['aggregateCount']):
        iAgg = len(tabs)
        tabs.append(('aggregates', h['offAggregates'], h['aggregateCount'] * 48))
        tabs.append(('covered', h['offCovered'], h['coveredCount'] * 4))
    if v5:
        iPao = len(tabs)
        tabs.append(('placementAo', h['offPlacementAo'], h['placementAoCount']))
    iVao = -1
    if v6:
        iVao = len(tabs)
        tabs.append(('vertexAo', h['offVertexAo'], h['vertexAoBytes']))
    iGrp = iVsky = -1
    if v7 and h['offGroup']:
        iGrp = len(tabs)
        tabs.append(('group', h['offGroup'], h['instanceCount'] * 2))
    if v7 and h['offVertexSky']:
        iVsky = len(tabs)
        tabs.append(('vertexSky', h['offVertexSky'], h['vertexSkyBytes']))
    iVhor = -1
    if v8 and h['offVertexHorizon']:
        iVhor = len(tabs)
        tabs.append(('vertexHorizon', h['offVertexHorizon'], h['vertexHorizonBytes']))
    prev = hdr
    for name, off, size in tabs:
        if off % 4096:
            raise Refusal('%s offset not 4096-aligned' % name)
        if off < prev:
            raise Refusal('%s out of table order' % name)
        if off + size > len(b):
            raise Refusal('%s runs past the file' % name)
        if any(b[prev:off]):
            raise Refusal('pad before %s not zero' % name)
        prev = off + size
    # v3: indexCrc32 covers the chunk table, the cell ranges, the occluder table
    # and the occluder ranges, in that order.
    # The RANGES are kept as well as the bytes, because a script that edits a
    # `.lodi` has to rebuild this blob from the file's own definition rather
    # than from a second copy of the rule (`lodgen_scrappable_flip.py`).
    ranges = [(h['offChunks'], tabs[0][2]),
              (h['offCellRanges'], tabs[1][2]),
              (h['offOccluders'], tabs[4][2]),
              (h['offOccluderRanges'], tabs[5][2])]
    if iAgg >= 0:
        for name, off, size in tabs[iAgg:iAgg + 2]:
            ranges.append((off, size))
    if iPao >= 0:                      # v5: the AO blob joins after the aggregates
        ranges.append((tabs[iPao][1], tabs[iPao][2]))
    if iVao >= 0:                      # v6: the vertex-AO stream joins after it
        ranges.append((tabs[iVao][1], tabs[iVao][2]))
    if iGrp >= 0:                      # v7: the group table then the sky stream join LAST
        ranges.append((tabs[iGrp][1], tabs[iGrp][2]))
    if iVsky >= 0:
        ranges.append((tabs[iVsky][1], tabs[iVsky][2]))
    if iVhor >= 0:                     # v8: the horizon stream joins after the sky one
        ranges.append((tabs[iVhor][1], tabs[iVhor][2]))
    idx = b''.join(b[off:off + size] for off, size in ranges)
    if crc32(idx) != h['indexCrc32']:
        raise Refusal('indexCrc32 mismatch')
    T = {'header': h, 'indexRanges': ranges, 'headerBytes': hdr}
    T['placementAo'] = (list(b[h['offPlacementAo']:h['offPlacementAo'] + h['placementAoCount']])
                        if v5 else [])
    # v6: u32 first[instanceCount + 1] in instance order, then the bytes; instance i owns
    # first[i]..first[i+1], one byte a vertex of its drawn mesh (bases[baseId].rep[mnamSlot])
    T['vertexAoFirst'], T['vertexAo'] = [], []
    if v6:
        n1 = h['instanceCount'] + 1
        o = h['offVertexAo']
        T['vertexAoFirst'] = list(le('%dI' % n1, b, o))
        T['vertexAo'] = list(b[o + 4 * n1:o + h['vertexAoBytes']])
        f = T['vertexAoFirst']
        if f[0] != 0 or any(f[i] > f[i + 1] for i in range(n1 - 1)) or f[-1] != len(T['vertexAo']):
            raise Refusal('vertex-AO offsets are not a monotone run from 0 to the byte count')
    # v7: one u16 a placement, in instance order, dense per CHUNK from 0
    T['group'] = []
    if iGrp >= 0:
        T['group'] = list(le('%dH' % h['instanceCount'], b, h['offGroup'])) if h['instanceCount'] else []
    # v7: s4.8's layout exactly, for sky
    T['vertexSkyFirst'], T['vertexSky'] = [], []
    if iVsky >= 0:
        n1 = h['instanceCount'] + 1
        o = h['offVertexSky']
        T['vertexSkyFirst'] = list(le('%dI' % n1, b, o))
        T['vertexSky'] = list(b[o + 4 * n1:o + h['vertexSkyBytes']])
        f = T['vertexSkyFirst']
        if f[0] != 0 or any(f[i] > f[i + 1] for i in range(n1 - 1)) or f[-1] != len(T['vertexSky']):
            raise Refusal('vertex-sky offsets are not a monotone run from 0 to the byte count')
    # v8: the same layout, A bytes a vertex instead of one
    T['vertexHorizonFirst'], T['vertexHorizon'] = [], []
    if iVhor >= 0:
        n1 = h['instanceCount'] + 1
        o = h['offVertexHorizon']
        T['vertexHorizonFirst'] = list(le('%dI' % n1, b, o))
        T['vertexHorizon'] = list(b[o + 4 * n1:o + h['vertexHorizonBytes']])
        f = T['vertexHorizonFirst']
        if f[0] != 0 or any(f[i] > f[i + 1] for i in range(n1 - 1)) or f[-1] != len(T['vertexHorizon']):
            raise Refusal('vertex-horizon offsets are not a monotone run from 0 to the byte count')
    T['chunks'] = [dict(zip(('instanceFirst', 'instanceCount', 'zMin', 'zExtent', 'maxBoundRadius', 'cellRangeOffset',
                             'crc32', 'reserved'), le('IIfffIII', b, h['offChunks'] + i * 32))) for i in range(h['chunkCount'])]
    T['cellRanges'] = [le('II', b, h['offCellRanges'] + i * 8) for i in range(h['presentChunks'] * 16)]
    T['instances'] = [dict(zip(('px', 'py', 'pz', 'r0', 'r1', 'r2', 'scale', 'baseId', 'ao', 'sky', 'ground', 'seed',
                                'flags', 'drawKey'), le('HHHHHHHHBBBBHH', b, h['offInstances'] + i * 24)))
                      for i in range(h['instanceCount'])]
    T['cold'] = [dict(zip(('refFormId', 'scolPart', 'identity'), le('IhH', b, h['offCold'] + i * 8)))
                 for i in range(h['instanceCount'])]
    T['occluders'] = [dict(zip(('x', 'y', 'z', 'hx', 'hy', 'hz', 'r0', 'r1', 'r2', 'flags',
                                'instanceIndex', 'meshId', 'reserved'),
                               le('ffffffHHHHIHH', b, h['offOccluders'] + i * 40)))
                      for i in range(h['occluderCount'])]
    T['occluderRanges'] = [le('II', b, h['offOccluderRanges'] + i * 8) for i in range(h['presentChunks'] * 16)]
    #: instances whose cell RANGE and whose re-derived cell differ inside the
    #: quantiser's ambiguity band (see the per-instance loop below).  Written
    #: here so it is 0 on a file with none, not absent.
    T['cellAmbiguous'] = []
    groupSum = [0]      # v7: the chunks' group counts, summed, checked against the header word
    occCursor = 0
    present = covered = maxInst = 0
    nextFirst = 0
    for ci, c in enumerate(T['chunks']):
        if c['reserved']:
            raise Refusal('chunk %d reserved' % ci)
        if c['instanceCount'] == 0:
            if any(c.values()):
                raise Refusal('absent chunk %d not all-zero' % ci)
            continue
        if c['instanceFirst'] != nextFirst:
            raise Refusal('chunk %d instanceFirst %d != %d' % (ci, c['instanceFirst'], nextFirst))
        if c['cellRangeOffset'] != present * 16:
            raise Refusal('chunk %d cellRangeOffset' % ci)
        if T['group']:
            # v7 THE DENSE RULE: a chunk holding C groups uses exactly {0..C-1}
            ids = T['group'][c['instanceFirst']:c['instanceFirst'] + c['instanceCount']]
            hi = max(ids)
            if hi >= c['instanceCount']:
                raise Refusal("group id %d in chunk %d is past its %d placements; ids are dense per "
                              "chunk from 0" % (hi, ci, c['instanceCount']))
            missing = set(range(hi + 1)) - set(ids)
            if missing:
                raise Refusal('chunk %d uses group ids up to %d but never uses %d; ids are dense per '
                              'chunk from 0' % (ci, hi, min(missing)))
            groupSum[0] += hi + 1
        s = c['instanceFirst']
        e = s + c['instanceCount']
        rec = b[h['offInstances'] + s * 24:h['offInstances'] + e * 24]
        cold = b[h['offCold'] + s * 8:h['offCold'] + e * 8]
        if crc32(cold, crc32(rec)) != c['crc32']:
            raise Refusal('chunk %d crc32 mismatch' % ci)
        total = 0
        cursor = s
        cellOfIndex = {}
        for k in range(16):
            f, cnt = T['cellRanges'][c['cellRangeOffset'] + k]
            if cnt == 0:
                continue
            if f != cursor:
                raise Refusal('chunk %d cell %d range start' % (ci, k))
            for i in range(f, f + cnt):
                cellOfIndex[i] = k
            cursor += cnt
            total += cnt
        if total != c['instanceCount']:
            raise Refusal('chunk %d cell ranges do not partition it' % ci)
        # v3: the occluder ranges walk the box table in the same cell order, and
        # every box names an instance OF ITS OWN CELL
        for k in range(16):
            of, ocnt = T['occluderRanges'][c['cellRangeOffset'] + k]
            cf, ccnt = T['cellRanges'][c['cellRangeOffset'] + k]
            if ocnt == 0:
                if of:
                    raise Refusal('chunk %d cell %d: empty occluder range not all-zero' % (ci, k))
                continue
            if ocnt > h['maxOccludersPerCell']:
                raise Refusal('chunk %d cell %d: %d occluders past the cap %d'
                              % (ci, k, ocnt, h['maxOccludersPerCell']))
            if of != occCursor:
                raise Refusal('chunk %d cell %d: occluders start at %d, expected %d' % (ci, k, of, occCursor))
            for bi in range(of, of + ocnt):
                o = T['occluders'][bi]
                if o['reserved']:
                    raise Refusal('occluder %d reserved not zero' % bi)
                if o['flags'] & ~1 or not o['flags'] & 1:
                    raise Refusal('occluder %d flags %d' % (bi, o['flags']))
                if min(o['hx'], o['hy'], o['hz']) <= 0:
                    raise Refusal('occluder %d has a non-positive half extent' % bi)
                if o['instanceIndex'] >= h['instanceCount']:
                    raise Refusal('occluder %d instanceIndex past the table' % bi)
                if not cf <= o['instanceIndex'] < cf + ccnt:
                    raise Refusal('occluder %d is listed in chunk %d cell %d but names instance %d, which is '
                                  'not one of that cell instances' % (bi, ci, k, o['instanceIndex']))
            occCursor += ocnt
        cx = h['chunkWest'] + ci % w
        cy = h['chunkNorth'] - ci // w
        c['cx'], c['cy'] = cx, cy
        prevkey = None
        for i in range(s, e):
            r = T['instances'][i]
            known = 0xFF if v10 else (0x7F if v9 else 0x3F)
            if r['flags'] & ~known:
                raise Refusal('instance %d reserved flags 0x%04x (version %d knows 0x%02x)'
                              % (i, r['flags'], h['version'], known))
            if r['scale'] == 0 and not r['flags'] & 0x80:
                raise Refusal('instance %d (ref %08x): scale is 0, so base.boundRadius x scale is 0'
                              % (i, T['cold'][i]['refFormId']))
            r['x'] = cx * CHUNK_UNITS + r['px'] / 65535.0 * CHUNK_UNITS
            r['y'] = cy * CHUNK_UNITS + r['py'] / 65535.0 * CHUNK_UNITS
            r['z'] = c['zMin'] + r['pz'] / 65535.0 * c['zExtent']
            r['chunk'] = ci
            fx = r['x'] - cx * CHUNK_UNITS
            fy = r['y'] - cy * CHUNK_UNITS
            lx = min(3, max(0, int(fx // CELL_UNITS)))
            ly = min(3, max(0, int(fy // CELL_UNITS)))
            derived = (3 - ly) * 4 + lx
            # THE WRITER'S CELL RULE, and why we do not simply re-derive it.
            # `lodifile.cpp:312` sorts on `lodiCellOf(set.instances[i].pos[...])`
            # -- the FLOAT position, before the u16 quantiser touches it.  We
            # only ever see the quantised one, and it can have crossed a cell
            # line by up to half a step.  So the cell we USE is the one the
            # file itself states (the cell RANGE this index falls in, already
            # checked above to partition the chunk in order), and the derived
            # one is kept as a CHECK on it: the two may differ only by one on
            # ONE axis, and only within the ambiguity band of that cell line.
            stored = cellOfIndex[i]
            r['cell'] = stored
            r['cellDerived'] = derived
            if derived != stored:
                slx, sly = stored % 4, 3 - stored // 4
                dax, day = abs(slx - lx), abs(sly - ly)
                line = None
                if dax == 1 and day == 0:
                    line = abs(fx - max(slx, lx) * CELL_UNITS)
                elif day == 1 and dax == 0:
                    line = abs(fy - max(sly, ly) * CELL_UNITS)
                if line is None or line > CELL_QUANT_TOL:
                    raise Refusal(
                        'instance %d (ref %08x) is stored in chunk %d cell %d but its position '
                        '(%.4f, %.4f in the chunk box) derives cell %d; %s'
                        % (i, T['cold'][i]['refFormId'], ci, stored, fx, fy, derived,
                           'the two cells are not neighbours on one axis'
                           if line is None else
                           'it is %.6f u from the cell line, past the %.6f u the quantiser '
                           'could have moved it' % (line, CELL_QUANT_TOL)))
                T['cellAmbiguous'].append((i, ci, stored, derived, line))
            r['m'] = unpack_rotation(r['r0'], r['r1'], r['r2'])
            r['scaleF'] = r['scale'] / 8192.0 + (8.0 if r['flags'] & 0x80 else 0.0)
            key = (r['cell'], r['drawKey'], T['cold'][i]['refFormId'], T['cold'][i]['scolPart'])
            if prevkey is not None and prevkey > key:
                raise Refusal('instance %d out of (cell, drawKey, ref, part) order' % i)
            prevkey = key
        present += 1
        covered += c['instanceCount']
        nextFirst = e
        maxInst = max(maxInst, c['instanceCount'])
    if present != h['presentChunks'] or covered != h['instanceCount'] or maxInst != h['maxInstancesPerChunk']:
        raise Refusal('presentChunks / instanceCount / maxInstancesPerChunk disagree with the table')
    if occCursor != h['occluderCount']:
        raise Refusal('the cell ranges cover %d occluders but the header says %d'
                      % (occCursor, h['occluderCount']))
    if T['group'] and groupSum[0] != h['groupCount']:
        raise Refusal("groupCount %d but the chunks' group counts sum to %d"
                      % (h['groupCount'], groupSum[0]))
    # v7: the two streams stand for one vertex population
    # v8: the horizon stream is the SAME vertex population, A bytes each. This is
    # the one thing the format can get wrong without announcing it -- a slice
    # that is not exactly A x the AO slice makes every later vertex read another
    # vertex's bins, and nothing downstream would notice.
    if T['vertexHorizonFirst'] and T['vertexAoFirst']:
        A = h['horizonAzimuths']
        for i in range(h['instanceCount']):
            hn = T['vertexHorizonFirst'][i + 1] - T['vertexHorizonFirst'][i]
            an = T['vertexAoFirst'][i + 1] - T['vertexAoFirst'][i]
            if hn and hn != an * A:
                raise Refusal('instance %d has %d horizon bytes but %d AO bytes x %d azimuths = %d; '
                              'the two streams are one vertex population' % (i, hn, an, A, an * A))
    if T['vertexSkyFirst'] and T['vertexAoFirst']:
        for i in range(h['instanceCount']):
            sn = T['vertexSkyFirst'][i + 1] - T['vertexSkyFirst'][i]
            an = T['vertexAoFirst'][i + 1] - T['vertexAoFirst'][i]
            if sn and sn != an:
                raise Refusal('instance %d has %d sky bytes but %d AO bytes; the two streams are one '
                              'vertex population' % (i, sn, an))
    return T


# ----------------------------------------------------------------- checks
class Checker:
    def __init__(self):
        self.ok = 0
        self.fails = []

    def check(self, what, cond, detail=''):
        if cond:
            self.ok += 1
            print('  ok   %s' % what)
        else:
            self.fails.append(what)
            print('  FAIL %s %s' % (what, detail))


def eval_int(s):
    return sum(int(t) for t in s.split('+'))


def check_expect(L, T, expect_path, ck):
    exp = {}
    for line in open(expect_path, encoding='utf-8'):
        t = line.split(None, 2)
        if len(t) == 3 and t[0] == 'expect':
            exp[t[1]] = t[2].strip()
    h, ih = L['header'], T['header']
    ck.check('lodo.meshCount', h['meshCount'] == eval_int(exp['lodo.meshCount']), h['meshCount'])
    l0 = [i for i, r in enumerate(L['clusterLods']) if r['level'] == 0]
    ck.check('lodo.level0.clusterCount', len(l0) == eval_int(exp['lodo.level0.clusterCount']), len(l0))
    ck.check('lodo.baseCount', h['baseCount'] == eval_int(exp['lodo.baseCount']), h['baseCount'])
    ck.check('lodo.materialCount', h['materialCount'] == eval_int(exp['lodo.materialCount']), h['materialCount'])
    l0v = sum(L['clusters'][i]['vertexCount'] for i in l0)
    ck.check('lodo.level0.vertices', l0v == eval_int(exp['lodo.level0.vertices']), l0v)
    l0t = sum(L['clusters'][i]['triangleCount'] for i in l0)
    ck.check('lodo.level0.triangles', l0t == eval_int(exp['lodo.level0.triangles']), l0t)
    ck.check('lodo.levelMaxAtLeast', h['levelMax'] >= int(exp['lodo.levelMaxAtLeast']), h['levelMax'])
    roots = sum(r['sourceTriangles'] for r in L['clusterLods'] if r['parentFirst'] == 0xFFFFFFFF)
    ck.check('lodo.rootSourceTriangles', roots == eval_int(exp['lodo.rootSourceTriangles']), roots)
    ck.check('lodo.mesh0.path', L['string_at'](L['meshes'][0]['modelStringOffset']) == exp['lodo.mesh0.path'])
    aabb = [float(x) for x in exp['lodo.mesh0.aabb'].split(',')]
    got = list(L['meshes'][0]['aabbMin']) + list(L['meshes'][0]['aabbExtent'])
    ck.check('lodo.mesh0.aabb', all(abs(a - b) < 1e-4 for a, b in zip(aabb, got)), got)
    uvr = [float(x) for x in exp['lodo.mesh0.uvrect'].split(',')]
    got = list(L['meshes'][0]['uvMin']) + list(L['meshes'][0]['uvExtent'])
    ck.check('lodo.mesh0.uvrect', all(abs(a - b) < 1e-5 for a, b in zip(uvr, got)), got)
    ck.check('lodo.base0.formId', '%08x' % L['bases'][0]['formId'] == exp['lodo.base0.formId'])
    reps = ','.join(str(L['bases'][1]['rep%d' % k]) for k in range(4))
    ck.check('lodo.base1.rep', reps == exp['lodo.base1.rep'], reps)
    def l0_of(mesh):
        m = L['meshes'][mesh]
        return [i for i in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount'])
                if L['clusterLods'][i]['level'] == 0]
    m1 = l0_of(1)
    ck.check('lodo.mesh1.l0cluster0.triangleCount',
             L['clusters'][m1[0]]['triangleCount'] == int(exp['lodo.mesh1.l0cluster0.triangleCount']),
             L['clusters'][m1[0]]['triangleCount'])
    ck.check('lodo.mesh1.l0cluster1.triangleCount',
             L['clusters'][m1[1]]['triangleCount'] == int(exp['lodo.mesh1.l0cluster1.triangleCount']),
             L['clusters'][m1[1]]['triangleCount'])
    ck.check('lodo.mesh1.l0cluster1.sizeClass',
             L['clusters'][m1[1]]['flags'] & 3 == int(exp['lodo.mesh1.l0cluster1.sizeClass']))
    m0 = l0_of(0)
    ck.check('lodo.mesh0.l0cluster0.coneOpen',
             bool(L['clusters'][m0[0]]['flags'] & 4) == bool(int(exp['lodo.mesh0.l0cluster0.coneOpen'])),
             L['clusters'][m0[0]]['flags'])
    ck.check('lodo.mesh1.l0cluster0.coneOpen',
             bool(L['clusters'][m1[0]]['flags'] & 4) == bool(int(exp['lodo.mesh1.l0cluster0.coneOpen'])),
             L['clusters'][m1[0]]['flags'])
    for key, ci in (('lodo.mesh0.l0cluster0.sphere', m0[0]), ('lodo.mesh1.l0cluster0.sphere', m1[0])):
        want = [float(x) for x in exp[key].split(',')]
        r = L['clusterLods'][ci]
        got = [r['cx'], r['cy'], r['cz'], r['radius']]
        # the stored radius is widened by the writer's own epsilon, never shrunk
        ck.check(key, all(abs(a - b) < 0.01 for a, b in zip(want[:3], got[:3]))
                 and want[3] <= got[3] <= want[3] * 1.001 + 0.01, got)
    v = L['vertices']
    ck.check('lodo.vertex0.pos', '%d,%d,%d' % (v[0]['px'], v[0]['py'], v[0]['pz']) == exp['lodo.vertex0.pos'])
    ck.check('lodo.vertex6.pos', '%d,%d,%d' % (v[6]['px'], v[6]['py'], v[6]['pz']) == exp['lodo.vertex6.pos'])
    ck.check('lodo.vertex4.sway', v[4]['sway'] == int(exp['lodo.vertex4.sway']), v[4]['sway'])
    # the normal round trip on the cube's first vertex: (-50,-50,-100)/|.| within 0.06 deg
    n = v[0]['n0'] | (v[0]['n1'] << 8) | (v[0]['n2'] << 16)
    u = (n & 0xFFF) / 4095.0 * 2 - 1
    w = (n >> 12) / 4095.0 * 2 - 1
    z = 1 - abs(u) - abs(w)
    if z < 0:
        u, w = (1 - abs(w)) * (1 if u >= 0 else -1), (1 - abs(u)) * (1 if w >= 0 else -1)
    l = math.sqrt(u * u + w * w + z * z)
    dec = (u / l, w / l, z / l)
    src = (-50.0, -50.0, -100.0)
    sl = math.sqrt(sum(c * c for c in src))
    dot = sum(a * b / sl for a, b in zip(dec, src))
    ck.check('lodo.vertex0.normalRoundTrip', math.degrees(math.acos(min(1.0, dot))) < 0.06, dot)

    ck.check('lodi.instanceCount', ih['instanceCount'] == int(exp['lodi.instanceCount']))
    ext = '%d,%d,%d,%d' % (ih['chunkWest'], ih['chunkSouth'], ih['chunkEast'], ih['chunkNorth'])
    ck.check('lodi.chunkExtent', ext == exp['lodi.chunkExtent'], ext)
    ck.check('lodi.chunkCount', ih['chunkCount'] == int(exp['lodi.chunkCount']))
    ck.check('lodi.presentChunks', ih['presentChunks'] == int(exp['lodi.presentChunks']))
    ck.check('lodi.maxInstancesPerChunk', ih['maxInstancesPerChunk'] == int(exp['lodi.maxInstancesPerChunk']))
    refs = ','.join('%08x' % c['refFormId'] for c in T['cold'])
    ck.check('lodi.order.refs', refs == exp['lodi.order.refs'], refs)
    chunks = ','.join(str(r['chunk']) for r in T['instances'])
    ck.check('lodi.order.chunks', chunks == exp['lodi.order.chunks'], chunks)
    by = {'%08x' % c['refFormId']: r for c, r in zip(T['cold'], T['instances'])}
    tree, stat, part = by['00020001'], by['00020002'], by['00020003']
    tol = [float(x) for x in exp['lodi.tree.posTol'].split(',')]
    for name, r in (('tree', tree), ('stat', stat), ('part', part)):
        want = [float(x) for x in exp['lodi.%s.pos' % name].split(',')]
        got = [r['x'], r['y'], r['z']]
        ck.check('lodi.%s.pos' % name, all(abs(a - b) <= t + 1e-3 for a, b, t in zip(want, got, tol)), got)
    ck.check('lodi.tree.scale', abs(tree['scaleF'] - float(exp['lodi.tree.scale'])) <= 1 / 16384.0, tree['scaleF'])
    ck.check('lodi.part.scale', abs(part['scaleF'] - float(exp['lodi.part.scale'])) <= 1 / 16384.0, part['scaleF'])
    ck.check('lodi.tree.baseId', tree['baseId'] == int(exp['lodi.tree.baseId']))
    ck.check('lodi.part.baseId', part['baseId'] == int(exp['lodi.part.baseId']))
    ck.check('lodi.tree.flags', tree['flags'] == int(exp['lodi.tree.flags']), tree['flags'])
    ck.check('lodi.tree.seed', tree['seed'] == int(exp['lodi.tree.seed']), tree['seed'])
    asg = '%d,%d,%d' % (tree['ao'], tree['sky'], tree['ground'])
    ck.check('lodi.tree.aoSkyGround', asg == exp['lodi.tree.aoSkyGround'], asg)
    ck.check('lodi.tree.cell', tree['cell'] == int(exp['lodi.tree.cell']), tree['cell'])
    ck.check('lodi.stat.cell', stat['cell'] == int(exp['lodi.stat.cell']), stat['cell'])
    ck.check('lodi.part.scolPart', by_cold(T, '00020003')['scolPart'] == int(exp['lodi.part.scolPart']))
    rtol = float(exp['lodi.rotTolDeg'])
    for name, r in (('tree', tree), ('part', part)):
        want = [float(exp['lodi.%s.rot%d' % (name, i)]) for i in range(9)]
        ang = mat_angle_deg(want, r['m'])
        ck.check('lodi.%s.rotation' % name, ang <= rtol, '%.4f deg' % ang)
    ck.check('lodi.identityMatchesLodo', (ih['lodoIdentity'] == lodo_identity(h)) == bool(int(exp['lodi.identityMatchesLodo'])))
    ck.check('lodi.chunk4.maxBoundRadius', abs(T['chunks'][4]['maxBoundRadius'] - float(exp['lodi.chunk4.maxBoundRadius'])) < 1e-3)
    # v2
    ck.check('lodo.version', h['version'] == int(exp['lodo.version']), h['version'])
    ck.check('lodi.version', ih['version'] == int(exp['lodi.version']), ih['version'])
    ck.check('lodo.flags', h['flags'] == int(exp['lodo.flags']), h['flags'])
    want = int(exp['loadOrderHash'], 16)
    ck.check('loadOrderHash in both headers',
             h['loadOrderHash'] == want and ih['loadOrderHash'] == want,
             '%016x / %016x' % (h['loadOrderHash'], ih['loadOrderHash']))
    keys = ','.join(str(r['drawKey']) for r in T['instances'])
    ck.check('lodi.order.drawKeys', keys == exp['lodi.order.drawKeys'], keys)
    ids = ','.join(str(c['identity']) for c in T['cold'])
    ck.check('lodi.order.identities', ids == exp['lodi.order.identities'], ids)
    cells = ','.join(str(r['cell']) for r in T['instances'])
    ck.check('lodi.order.cells', cells == exp['lodi.order.cells'], cells)
    ck.check('lodi.chunk4.instanceCount',
             T['chunks'][4]['instanceCount'] == int(exp['lodi.chunk4.instanceCount']),
             T['chunks'][4]['instanceCount'])
    # v3
    ck.check('lodi.occluderCount', ih['occluderCount'] == int(exp['lodi.occluderCount']), ih['occluderCount'])
    ck.check('lodi.maxOccludersPerCell',
             ih['maxOccludersPerCell'] == int(exp['lodi.maxOccludersPerCell']), ih['maxOccludersPerCell'])
    if T['occluders']:
        o = T['occluders'][0]
        want = [float(x) for x in exp['lodi.occluder0.centre'].split(',')]
        got = [o['x'], o['y'], o['z']]
        ck.check('lodi.occluder0.centre', all(abs(a - b) < 0.01 for a, b in zip(want, got)), got)
        want = [float(x) for x in exp['lodi.occluder0.half'].split(',')]
        got = [o['hx'], o['hy'], o['hz']]
        ck.check('lodi.occluder0.half', all(abs(a - b) < 0.01 for a, b in zip(want, got)), got)
        ck.check('lodi.occluder0.instanceIndex',
                 o['instanceIndex'] == int(exp['lodi.occluder0.instanceIndex']), o['instanceIndex'])
        ck.check('lodi.occluder0.cell',
                 T['instances'][o['instanceIndex']]['cell'] == int(exp['lodi.occluder0.cell']),
                 T['instances'][o['instanceIndex']]['cell'])
        ck.check('lodi.occluder0.meshId', o['meshId'] == int(exp['lodi.occluder0.meshId']), o['meshId'])


def by_cold(T, ref):
    for c in T['cold']:
        if '%08x' % c['refFormId'] == ref:
            return c
    raise Refusal('ref %s not in the cold table' % ref)


# ----------------------------------------------------------------- ESM leg
def euler_neg_matrix(rx, ry, rz):
    """Matrix::fromEuler(-x, -y, -z) as lodgen composes a REFR's DATA."""
    x, y, z = -rx, -ry, -rz
    sx, cx, sy, cy, sz, cz = math.sin(x), math.cos(x), math.sin(y), math.cos(y), math.sin(z), math.cos(z)
    return [cy * cz, -cy * sz, sy,
            sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy,
            sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy]


def mat_mul(a, b):
    return [sum(a[r * 3 + k] * b[k * 3 + c] for k in range(3)) for r in range(3) for c in range(3)]


def tree_hash(x, y):
    """lodgen.cpp: (quint32(qRound(x)) * 2654435761U) ^ (quint32(qRound(y)) * 40503U)."""
    def qround(v):
        return int(math.floor(v + 0.5)) if v >= 0 else -int(math.floor(-v + 0.5))
    return ((qround(x) & 0xFFFFFFFF) * 2654435761 ^ (qround(y) & 0xFFFFFFFF) * 40503) & 0xFFFFFFFF


def check_esm(L, T, esm, ws, chunks, ck):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
    from lod_emission_probe import walk, subrecords, recordData
    buf = open(esm, 'rb').read()
    cellWorld, cellXY = {}, {}
    refs = []
    for typ, form, flags, doff, dsize, stack in walk(buf):
        if typ == b'CELL':
            w = None
            for label, gtype in reversed(stack):
                if gtype == 1:
                    w = struct.unpack_from('<I', label, 0)[0]
                    break
            cellWorld[form] = w
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'XCLC' and len(payload) >= 8:
                    cellXY[form] = struct.unpack_from('<ii', payload, 0)
        elif typ == b'REFR':
            c = None
            for label, gtype in reversed(stack):
                if gtype in (6, 8, 9, 10):
                    c = struct.unpack_from('<I', label, 0)[0]
                    if gtype == 6:
                        break
            if c is None or cellWorld.get(c) != ws:
                continue
            if flags & 0x820:           # deleted 0x20, initially disabled 0x800
                continue
            base = pos = None
            scale = 1.0
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'NAME':
                    base = struct.unpack_from('<I', payload, 0)[0]
                elif st == b'DATA' and len(payload) >= 24:
                    pos = struct.unpack_from('<6f', payload, 0)
                elif st == b'XSCL':
                    scale = struct.unpack_from('<f', payload, 0)[0]
            if base and pos:
                refs.append((form, base, pos, scale, cellXY.get(c)))
    table = {}
    for c, r in zip(T['cold'], T['instances']):
        table[(c['refFormId'], c['scolPart'])] = r
    baseForm = [b['formId'] for b in L['bases']]
    baseTree = [bool(b['flags'] & 1) for b in L['bases']]
    for (x0, y0, dim) in chunks:
        inChunk = [r for r in refs if r[4] and x0 <= r[4][0] < x0 + dim and y0 <= r[4][1] < y0 + dim]
        present = missing = wrongBase = wrongPos = wrongRot = 0
        worstPos = worstRot = 0.0
        for form, base, data, scale, _ in inChunk:
            key = (form, -1)
            if key not in table:
                missing += 1     # a plain ref with no LOD base, or one the writer dropped: counted, named below
                continue
            r = table[key]
            present += 1
            if baseForm[r['baseId']] != base:
                wrongBase += 1
            d = max(abs(r['x'] - data[0]), abs(r['y'] - data[1]))
            worstPos = max(worstPos, d)
            if d > 0.125 + 1e-3:
                wrongPos += 1
            m = euler_neg_matrix(data[3], data[4], data[5])
            if baseTree[r['baseId']]:
                th = tree_hash(data[0], data[1])
                yaw = (th % 360) * 0.01745329
                m = mat_mul(m, euler_neg_matrix(0.0, 0.0, -yaw))
            ang = mat_angle_deg(m, r['m'])
            worstRot = max(worstRot, ang)
            if ang > 0.02:
                wrongRot += 1
        print('  chunk (%d,%d) dim %d: %d plain refs in the ESM, %d in the table, %d not in the table' % (x0, y0, dim, len(inChunk), present, missing))
        ck.check('chunk (%d,%d): every present ref has its base form' % (x0, y0), wrongBase == 0, wrongBase)
        ck.check('chunk (%d,%d): X/Y within 0.125 u (worst %.4f)' % (x0, y0, worstPos), wrongPos == 0, wrongPos)
        ck.check('chunk (%d,%d): rotation within 0.02 deg incl. tree yaw (worst %.4f)' % (x0, y0, worstRot), wrongRot == 0, wrongRot)
        ck.check('chunk (%d,%d): at least one ref present' % (x0, y0), present > 0)


def check_manifest(L, T, path, ck, identity=True):
    table = {}
    for c, r in zip(T['cold'], T['instances']):
        table[(c['refFormId'], c['scolPart'])] = (r, c)
    baseForm = [b['formId'] for b in L['bases']]
    rows = missing = wrongBase = wrongPos = wrongScale = wrongId = 0
    worst = worstRaw = 0.0
    ids = {}
    for line in open(path, encoding='utf-8'):
        t = line.split()
        if not t or not t[0].isdigit() or len(t) < 11:
            continue
        rows += 1
        key = (int(t[9], 16), int(t[10]))
        if key not in table:
            missing += 1
            continue
        r, c = table[key]
        if baseForm[r['baseId']] != int(t[1], 16):
            wrongBase += 1
        # the bar is the quantisation bound PLUS half the manifest's own print step
        dx = abs(r['x'] - float(t[3])) - 0.5 * print_step(t[3])
        dy = abs(r['y'] - float(t[4])) - 0.5 * print_step(t[4])
        d = max(dx, dy)
        worst = max(worst, d)
        worstRaw = max(worstRaw, abs(r['x'] - float(t[3])), abs(r['y'] - float(t[4])))
        if d > 0.125 + 1e-3:
            wrongPos += 1
        if abs(r['scaleF'] - float(t[6])) > 1 / 16384.0 + 1e-6:
            wrongScale += 1
        if int(t[0]) != c['identity']:
            wrongId += 1
        ids.setdefault(c['identity'], 0)
        ids[c['identity']] += 1
    print('  manifest %s: %d rows, %d not in the table' % (os.path.basename(path), rows, missing))
    ck.check('manifest: every row is in the table', missing == 0, missing)
    ck.check('manifest: base forms match', wrongBase == 0, wrongBase)
    ck.check('manifest: X/Y within 0.125 u + half a print step (worst %.4f, raw %.4f)' % (worst, worstRaw),
             wrongPos == 0, wrongPos)
    ck.check('manifest: scale within 1/16384', wrongScale == 0, wrongScale)
    if identity:
        ck.check('manifest: the stock identity index is in the .lodi cold record', wrongId == 0, wrongId)
        dupes = sum(1 for v in ids.values() if v > 1)
        ck.check('manifest: identity unique over this chunk (%d distinct)' % len(ids), dupes == 0, dupes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('lodi')
    ap.add_argument('--expect')
    ap.add_argument('--esm')
    ap.add_argument('--worldspace', default='3C')
    ap.add_argument('--chunk', nargs=3, type=int, action='append', default=[])
    ap.add_argument('--manifest', action='append', default=[])
    a = ap.parse_args()
    a_esm_for_load_order[0] = a.esm
    ck = Checker()
    try:
        L = read_lodo(a.lodo)
        T = read_lodi(a.lodi)
    except Refusal as e:
        print('REFUSED: %s' % e)
        print('RESULT FAIL')
        return 1
    h, ih = L['header'], T['header']
    for k in ('worldspace', 'baseCount', 'meshCount', 'clusterCount', 'materialCount', 'vertexCount', 'stringBytes', 'fileBytes'):
        print('lodo.%s %s' % (k, h[k]))
    print('lodo.triangles %d' % sum(c['triangleCount'] for c in L['clusters']))
    for k in ('worldspace', 'chunkCount', 'presentChunks', 'instanceCount', 'maxInstancesPerChunk',
              'occluderCount', 'fileBytes'):
        print('lodi.%s %s' % (k, ih[k]))
    # The cell-line ambiguity, printed whether or not there is any, with the
    # worst distance from a line so a reader can see the band it sat inside.
    amb = T['cellAmbiguous']
    print('lodi.cellQuantAmbiguous %d' % len(amb))
    print('lodi.cellQuantWorstU %.6f (band %.6f)'
          % (max([x[4] for x in amb]) if amb else 0.0, CELL_QUANT_TOL))
    for i, ci, stored, derived, line in amb[:8]:
        print('lodi.cellQuant instance %d chunk %d stored-cell %d derived-cell %d %.6f u from the line'
              % (i, ci, stored, derived, line))
    ck.check('pair: worldspace names agree', h['worldspace'] == ih['worldspace'])
    ck.check('pair: corpus hashes agree', h['pluginCorpusHash'] == ih['pluginCorpusHash'] and h['objectCorpusHash'] == ih['objectCorpusHash'])
    ck.check('pair: lodoIdentity names this .lodo (or NOLIB)', bool(ih['flags'] & 4) or ih['lodoIdentity'] == lodo_identity(h))
    ck.check('every baseId < baseCount', all(r['baseId'] < h['baseCount'] for r in T['instances']))
    ck.check('pair: loadOrderHash agrees', h['loadOrderHash'] == ih['loadOrderHash'],
             '%016x / %016x' % (h['loadOrderHash'], ih['loadOrderHash']))
    ranks = draw_key_ranks(L)
    bad = sum(1 for r in T['instances'] if r['drawKey'] != ranks[r['baseId']])
    ck.check('every drawKey is its base\'s (mesh, material) rank', bad == 0, bad)
    if a_esm_for_load_order[0]:
        want = load_order_hash(a_esm_for_load_order[0])
        ck.check('loadOrderHash recomputed from the plugin list', h['loadOrderHash'] == want,
                 '%016x vs %016x' % (h['loadOrderHash'], want))
    if a.expect:
        check_expect(L, T, a.expect, ck)
    if a.esm:
        check_esm(L, T, a.esm, int(a.worldspace, 16), [tuple(c) for c in a.chunk], ck)
    for m in a.manifest:
        check_manifest(L, T, m, ck)
    print('%d checks, %d failures' % (ck.ok + len(ck.fails), len(ck.fails)))
    print('RESULT %s' % ('PASS' if not ck.fails else 'FAIL'))
    return 0 if not ck.fails else 1


if __name__ == '__main__':
    sys.exit(main())
