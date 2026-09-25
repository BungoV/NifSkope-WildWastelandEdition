"""W4 self-test of the gate's instruments before any v5 file exists: build a v5 file BY HAND from the shipped v4
library (one colour blob for the first N contiguous meshes, the writer's layout: blob last, 4096-aligned, in
indexCrc32), then (1) the independent decoder must read it and return the rows put in, (2) w4_gate.strip() must
give back the v4 bytes except 0x04, and (3) three mutations must be refused by name."""
import sys, struct, zlib, random
R = 'E:/Projects/NifskopeWWE-seam1'
sys.path.insert(0, R + '/tests/spells')
import lodgen_native_decode as dec
V4 = 'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.lodo'
A = open(V4, 'rb').read()
L = dec.read_lodo(V4); h = L['header']
end = h['offStrings'] + h['stringBytes']
print('v4: fileBytes %d, strings end %d (the file ends at the strings: %s)' % (len(A), end, len(A) == end))
rng = random.Random(7)
pick, rows = [], []
for i, m in enumerate(L['meshes']):
    cs = L['clusters'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
    lo = min(c['vertexBase'] for c in cs); hi = max(c['vertexBase'] + c['vertexCount'] for c in cs)
    if hi - lo == sum(c['vertexCount'] for c in cs) and len(pick) < 5 and i % 97 == 3:
        pick.append((i, lo, hi))
put = {}
for i, lo, hi in pick:
    for v in range(lo, hi):
        put[v] = tuple(rng.randrange(256) for _ in range(4)); rows.append(bytes(put[v]))


def build(pick, rows, va=()):
    b = bytearray(A)
    struct.pack_into('<I', b, 4, 5)
    for i, lo, hi in pick:
        o = h['offMeshes'] + i * 56 + 46
        struct.pack_into('<H', b, o, struct.unpack_from('<H', b, o)[0] | 8 | (16 if i in va else 0))
    off = (len(b) + 4095) // 4096 * 4096
    b += bytes(off - len(b)) + b''.join(rows)
    struct.pack_into('<IQ', b, 0xD4, len(rows), off)
    struct.pack_into('<Q', b, 0xB0, len(b))
    crc = 0
    for o, n in ((h['offBases'], h['baseCount'] * 32), (h['offMeshes'], h['meshCount'] * 56),
                 (h['offClusters'], h['clusterCount'] * 16), (h['offClusterLods'], h['clusterCount'] * 48),
                 (h['offMaterials'], h['materialCount'] * 16), (h['offLocalIndices'], h['clusterCount'] * 48),
                 (h['offVertices'], h['vertexCount'] * 16), (h['offStrings'], h['stringBytes']), (off, len(rows) * 4)):
        crc = zlib.crc32(bytes(b[o:o + n]), crc)
    struct.pack_into('<I', b, 0xA8, crc)
    struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])))
    return bytes(b)


T = 'C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/'
B = build(pick, rows)
open(T + 'w4_synth.lodo', 'wb').write(B)
L5 = dec.read_lodo(T + 'w4_synth.lodo')
got = {v: c for v, c in enumerate(L5['colours']) if c is not None}
print('(1) decoder: v%d, colourRows %d, meshes coloured %d, rows back == rows put: %s' % (
    L5['header']['version'], L5['header']['colourVertexCount'], len(pick), got == put))
sys.argv = ['', '.', '.']
src = open(R + '/scratchpad/seam1_20260925/w4_gate.py').read()
strip = {'struct': struct, 'zlib': zlib}
exec(src[src.index('def diffs'):src.index('def pair')], strip)
S = strip['strip'](B)
d = [i for i in range(min(len(A), len(S))) if A[i] != S[i]]
print('(2) strip(v5) vs v4: %d differ %s, length %+d -> %s' % (len(d), d[:4], len(S) - len(A),
      'ok' if d == [4] and len(S) == len(A) else 'FAIL'))
def recrc(bb):
    """both CRCs recomputed, so a mutation is refused by the rule under test, never by a stale CRC"""
    bb = bytearray(bb)
    tabs = [(h['offBases'], h['baseCount'] * 32), (h['offMeshes'], h['meshCount'] * 56),
            (h['offClusters'], h['clusterCount'] * 16), (h['offClusterLods'], h['clusterCount'] * 48),
            (h['offMaterials'], h['materialCount'] * 16), (h['offLocalIndices'], h['clusterCount'] * 48),
            (h['offVertices'], h['vertexCount'] * 16), (h['offStrings'], h['stringBytes'])]
    if struct.unpack_from('<I', bb, 4)[0] == 5 and struct.unpack_from('<I', bb, 0xD4)[0]:
        tabs.append((struct.unpack_from('<Q', bb, 0xD8)[0], struct.unpack_from('<I', bb, 0xD4)[0] * 4))
    crc = 0
    for oo, n in tabs:
        crc = zlib.crc32(bytes(bb[oo:oo + n]), crc)
    struct.pack_into('<I', bb, 0xA8, crc)
    struct.pack_into('<I', bb, 0x0C, zlib.crc32(bytes(bb[0x10:0x100])))
    return bytes(bb)


def setbit(bb, mesh, bit):
    bb = bytearray(bb); o = h['offMeshes'] + mesh * 56 + 46
    struct.pack_into('<H', bb, o, struct.unpack_from('<H', bb, o)[0] | bit); return bytes(bb)


# the control: recrc() of an unmutated file must still be accepted, or every refusal below is suspect
muts = [('control, no mutation', recrc(B), False),
        ('alpha without colour', recrc(setbit(B, pick[0][0] + 1, 16)), True),
        ('count off by one', build(pick, rows[:-1]), True),
        ('colour bit on a v4', recrc(setbit(bytes(bytearray(S[:4]) + struct.pack('<I', 4) + S[8:]), pick[0][0], 8)), True)]
ok = 0
for name, bb, expectRefuse in muts:
    open(T + 'w4_mut.lodo', 'wb').write(bb)
    try:
        dec.read_lodo(T + 'w4_mut.lodo'); res, why = False, 'accepted'
    except dec.Refusal as e:
        res, why = True, 'refused: ' + str(e)[:90]
    ok += res == expectRefuse
    print('(3) %-22s %s -> %s' % (name, why, 'ok' if res == expectRefuse else 'FAIL'))
ok = ok == len(muts)
print('self-test %s' % ('GREEN' if got == put and d == [4] and len(S) == len(A) and ok else 'RED'))
