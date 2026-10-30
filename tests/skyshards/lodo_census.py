"""SKYSHARDS2 step 1: the .lodo far-object library census (no stretched or stray vertices in what the baker wrote).

python -I tests/skyshards/lodo_census.py <ws.lodo> [--write-red <out.lodo>]
Read by tests/spells/lodgen_native_decode.py (independent of src/lodofile.cpp), which already refuses an index past its
cluster's vertices, an unsorted table or a bad CRC. On top of it, per mesh:
  * vertex bounds: the dequantised vertices (aabbMin + q / 65535 * extent) against the mesh box, and the mesh box's
    half-diagonal against the boundRadius of every base that uses the mesh (the source model's own bound, written by the
    baker from the NIF): a box more than 2x (+64 u) the source's radius is a stray vertex;
  * triangles: degenerate (zero area) and stretched (an edge longer than the mesh box's diagonal: impossible in the box,
    so a count > 0 means the box itself is wrong) per level-0 cluster.
--write-red: a copy with mesh 0's extent z x 40 (CRCs re-sealed), so the box test must FAIL on it (the red).
"""
import math, os, struct, sys, zlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'spells'))
import lodgen_native_decode as D


def write_red(src, out):
    b = bytearray(open(src, 'rb').read())
    offMeshes = struct.unpack_from('<Q', b, 0x78)[0]
    ez = struct.unpack_from('<f', b, offMeshes + 20)[0]
    struct.pack_into('<f', b, offMeshes + 20, ez * 40.0)
    # re-seal: indexCrc32 over the tables in file order (read_lodo's own loop), then the header CRC
    h = struct.unpack_from('<IIIIIIHHI', b, 0x50)
    offs = struct.unpack_from('<QQQQQQQ', b, 0x70)
    offLods = struct.unpack_from('<Q', b, 0xC0)[0]
    ver = struct.unpack_from('<I', b, 4)[0]
    cvc, offCol = struct.unpack_from('<IQ', b, 0xD4) if ver >= 5 else (0, 0)
    offChart = struct.unpack_from('<Q', b, 0xE0)[0] if ver == 8 else 0
    tabs = [(offs[0], h[0] * 32), (offs[1], h[1] * 56), (offs[2], h[2] * 16), (offLods, h[2] * 48),
            (offs[3], h[3] * 16), (offs[4], h[2] * 48), (offs[5], h[4] * 16), (offs[6], h[8])]
    if cvc:
        tabs.append((offCol, cvc * 4))
    if offChart:
        tabs.append((offChart, h[2] * 16))
    crc = 0
    for off, size in tabs:
        crc = zlib.crc32(bytes(b[off:off + size]), crc)
    struct.pack_into('<I', b, 0xA8, crc & 0xFFFFFFFF)
    struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])) & 0xFFFFFFFF)
    open(out, 'wb').write(bytes(b))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if '--write-red' in sys.argv:
        write_red(args[0], args[1])
        print('red written: %s (mesh 0 extent z x 40, CRCs re-sealed)' % args[1])
        return 0
    try:
        L = D.read_lodo(args[0])
    except D.Refusal as e:
        print('LODO CENSUS: decoder refused: %s\nLODO CENSUS FAIL' % e)
        return 1
    h = L['header']
    radius = {}
    for bse in L['bases']:
        for k in range(4):
            r = bse['rep%d' % k]
            if r != D.NO_MESH:
                radius[r] = max(radius.get(r, 0.0), bse['boundRadius'])
    V = L['vertices']
    li = L['localIndices']
    box_bad, degen, stretched, tris = [], 0, 0, 0
    for mi, m in enumerate(L['meshes']):
        lo, ex = m['aabbMin'], m['aabbExtent']
        diag = math.sqrt(sum(e * e for e in ex))
        r = radius.get(mi)
        if r is not None and diag / 2.0 > 2.0 * r + 64.0:
            box_bad.append((mi, diag / 2.0, r, L['string_at'](m['modelStringOffset'])))
        for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
            if L['clusterLods'][c]['level'] != 0:
                continue
            cl = L['clusters'][c]
            P = []
            for v in range(cl['vertexBase'], cl['vertexBase'] + cl['vertexCount']):
                q = V[v]
                P.append((lo[0] + q['px'] / 65535.0 * ex[0], lo[1] + q['py'] / 65535.0 * ex[1], lo[2] + q['pz'] / 65535.0 * ex[2]))
            for t in range(cl['triangleCount']):
                a, b2, c2 = (P[li[c * 48 + 3 * t + k]] for k in range(3))
                tris += 1
                e1 = [b2[i] - a[i] for i in range(3)]; e2 = [c2[i] - a[i] for i in range(3)]
                cr = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
                if cr[0] == 0 and cr[1] == 0 and cr[2] == 0:
                    degen += 1
                if max(math.dist(a, b2), math.dist(b2, c2), math.dist(c2, a)) > diag + 1e-3:
                    stretched += 1
    print('lodo v%d: %d meshes, %d clusters, %d vertices, %d level-0 triangles; meshes with a source radius %d'
          % (h['version'], len(L['meshes']), len(L['clusters']), len(V), tris, len(radius)))
    print('mesh box half-diagonal > 2 x source radius + 64: %d' % len(box_bad))
    for mi, hd, r, name in sorted(box_bad, key=lambda t: -t[1] / max(t[2], 1))[:12]:
        print('  mesh %5d half-diag %8.0f radius %8.0f  %s' % (mi, hd, r, name))
    print('degenerate triangles %d (counted, not failed: the source models carry some); stretched past the box %d'
          % (degen, stretched))
    ok = not box_bad and stretched == 0
    print('LODO CENSUS %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
