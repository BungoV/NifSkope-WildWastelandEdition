#!/usr/bin/env python3
"""lodgen_cardlink ID: a plain bake (no --impostors) of the new exe against the rung's.

usage: lodgen_cardlink_id.py <new bake dir> <rung bake dir>     exit 0 = identical under the attribution

Every file must be byte-identical, EXCEPT the two moves below, each named by the commit that made it.
Nothing else is forgiven, and each forgiveness is exact (a value that is neither the old nor the new
one is refused).

 1. 62e53a3b (lane SEAM1, 2026-09-25) ".lodo v5: optional per-vertex colour stream, only where the
    game draws it". A v5 .lodo against a v4 rung is compared after taking the stream out: version word
    left at 5, header 0xD4..0xDF zeroed, mesh-row flag bits 8|16 cleared, the tail after the string
    table cut, fileBytes, indexCrc32 and headerCrc32 recomputed. The .lodi records its .lodo's identity,
    so it may then differ at its header CRC (0x0C..0x0F) and the .lodo identity (0x20..0x27) only.
 2. 62e53a3b, SAME commit, codegen: two selfAO bytes on the water-tower meshes read 201 (two ray hits)
    where the rung reads 228 (one). The AO law did not change: built with lodofile.cpp under
    fp-contract=off, a6e5e8de and 62e53a3b bake byte-identical files, so the flip is FMA contraction at
    a grazing ray tie. Measured in scratchpad/seam1_20260925 (bisect/fpc.sh, w4_gate.py ATTRIBUTED),
    and the same two bytes on this spell's Sanctuary bake (cl_id.py).
"""
import os, struct, sys, zlib

ATTRIBUTED = [(270124, 15, 228, 201), (271177, 15, 228, 201)]   # (vertex row, byte, rung, new)


def strip(b):
    b = bytearray(b)
    cnt = struct.unpack_from('<I', b, 0xD4)[0]
    meshCount = struct.unpack_from('<I', b, 0x54)[0]
    offMeshes = struct.unpack_from('<Q', b, 0x78)[0]
    for i in range(meshCount):
        o = offMeshes + i * 56 + 46
        struct.pack_into('<H', b, o, struct.unpack_from('<H', b, o)[0] & ~(8 | 16))
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
    for off, size in ((h['bases'], baseCount * 32), (h['meshes'], meshCount * 56), (h['clusters'], clusterCount * 16),
                      (offLods, clusterCount * 48), (h['materials'], materialCount * 16), (h['li'], clusterCount * 48),
                      (h['verts'], vertexCount * 16), (h['strings'], stringBytes)):
        crc = zlib.crc32(bytes(b[off:off + size]), crc)
    struct.pack_into('<I', b, 0xA8, crc & 0xFFFFFFFF)
    struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])) & 0xFFFFFFFF)
    return bytes(b)


def lodo(A, B):
    """(verdict, note): B the new v5 file, A the rung's v4 file"""
    if A[4] != 4 or B[4] != 5:
        return False, 'versions %d -> %d, not the attributed 4 -> 5' % (A[4], B[4])
    oA, oB = struct.unpack_from('<Q', A, 0x98)[0], struct.unpack_from('<Q', B, 0x98)[0]
    b = bytearray(B); put = []
    for row, byte, was, now in ATTRIBUTED:
        a, n = oA + row * 16 + byte, oB + row * 16 + byte
        if a >= len(A) or n >= len(b) or A[a] != was or b[n] not in (was, now):
            return False, 'attributed AO byte row %d holds neither %d nor %d' % (row, was, now)
        if b[n] == now:
            b[n] = was; put.append(row)
    S = strip(b)
    d = [i for i in range(min(len(A), len(S))) if A[i] != S[i]]
    ok = len(A) == len(S) and d == [4]
    return ok, 'stripped (62e53a3b), AO rows put back %s: %d bytes differ %s, length %+d' % (
        put, len(d), ['0x%X' % x for x in d[:6]], len(S) - len(A))


def main(new, rung):
    files = sorted(os.path.relpath(os.path.join(r, f), new).replace(os.sep, '/')
                   for r, _, fs in os.walk(new) for f in fs if not f.endswith(('.lodb', '.log')))
    nr = sum(1 for r, _, fs in os.walk(rung) for f in fs if not f.endswith(('.lodb', '.log')))
    lodoOk = {}; bad = 0
    for f in files:
        if f.endswith('.lodi'):
            continue
        A, B = open(os.path.join(rung, f), 'rb').read(), open(os.path.join(new, f), 'rb').read()
        if A == B:
            continue
        ok, note = lodo(A, B) if f.endswith('.lodo') else (False, 'not a .lodo')
        lodoOk[f[:-5]] = ok
        print('    %s %s: %s' % ('attributed' if ok else 'differs', f, note)); bad += not ok
    for f in files:
        if not f.endswith('.lodi'):
            continue
        A, B = open(os.path.join(rung, f), 'rb').read(), open(os.path.join(new, f), 'rb').read()
        d = [i for i in range(min(len(A), len(B))) if A[i] != B[i]]
        if not d and len(A) == len(B):
            continue
        ok = lodoOk.get(f[:-5], False) and len(A) == len(B) and set(d) <= set(range(0x0C, 0x10)) | set(range(0x20, 0x28))
        print('    %s %s: %d bytes differ %s (its .lodo %s)' % ('attributed' if ok else 'differs', f, len(d),
              ['0x%X' % x for x in d[:12]], 'attributed' if lodoOk.get(f[:-5]) else 'unchanged or refused'))
        bad += not ok
    print('ID %d of %d files differ past the 62e53a3b attribution (%d on the rung)' % (bad, len(files), nr))
    return 0 if files and bad == 0 and len(files) == nr else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))
