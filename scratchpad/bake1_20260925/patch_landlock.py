"""BAKE1: the vertex-AO loop reads LAND through ONE shared EsmWorld from every worker; libfo76utils'
ESMFile::uncompressRecord mutates the reader (zlibBuf.back() resize, emplace_back, the record's flags
and fileData) on a record's FIRST decompression, so two workers reaching two never-read LAND records at
once corrupt each other and throw "invalid compressed record size" from a pool thread -> terminate
(the whole-Commonwealth bake, 05:47:14, rc 127). Serialize the land() call; the heights copy and every
ray stay parallel. Byte-identical by construction: each land() answers the same bytes, only its timing
changes."""
import sys
p = 'E:/Projects/NifskopeWWE-bake1/src/nativeemit.cpp'
b = open(p, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')

T = chr(9)
def L(n, t): return T * n + t
NL = chr(10)
a1 = NL.join([L(2, "std::vector<int> chunkNoLand( chunkKeys.size(), 0 );"),
              L(2, "lodgenParallelFor( int( chunkKeys.size() ), [&]( int ci ) {")])
n1 = NL.join([L(2, "std::vector<int> chunkNoLand( chunkKeys.size(), 0 );"),
              L(2, "/* ONE reader, many workers (lane BAKE1, 2026-09-25): `s.world` is shared, and"),
              L(2, " * libfo76utils decompresses a record IN the reader on its first read"),
              L(2, " * (`ESMFile::uncompressRecord`: `zlibBuf.back()` resized, a new buffer"),
              L(2, " * emplaced, the record's flags and data pointer rewritten). Two workers"),
              L(2, " * reaching two never-read LAND records at once corrupt each other; on the"),
              L(2, " * whole Commonwealth a pool thread threw and the bake died in terminate()."),
              L(2, " * The read is serialized; the heights copy and the rays stay parallel, and"),
              L(2, " * the bytes cannot move. */"),
              L(2, "QMutex landLock;"),
              L(2, "lodgenParallelFor( int( chunkKeys.size() ), [&]( int ci ) {")])
a2 = NL.join([L(5, "for ( int lx = 0; lx < cells; lx++ )"),
              L(6, "if ( s.world->land( cx - SKIRT + lx, cy - SKIRT + ly, land ) ) {"),
              L(7, "landCells++;")])
n2 = NL.join([L(5, "for ( int lx = 0; lx < cells; lx++ ) {"),
              L(6, "bool got;"),
              L(6, "{"),
              L(7, "QMutexLocker lock( &landLock );"),
              L(7, "got = s.world->land( cx - SKIRT + lx, cy - SKIRT + ly, land );"),
              L(6, "}"),
              L(6, "if ( got ) {"),
              L(7, "landCells++;")])
a3 = NL.join([L(9, "scene.hgt[size_t( ly * 32 + row ) * size_t( scene.hn ) + size_t( lx * 32 + col )] = land.heights[row][col] * inv;"),
              L(6, "}"),
              L(3, "}")])
n3 = NL.join([L(9, "scene.hgt[size_t( ly * 32 + row ) * size_t( scene.hn ) + size_t( lx * 32 + col )] = land.heights[row][col] * inv;"),
              L(6, "}"),
              L(5, "}"),
              L(3, "}")])
for a, n in ((a1, n1), (a2, n2), (a3, n3)):
    c = s.count(a)
    assert c == 1, (c, a[:60])
    s = s.replace(a, n)
out = s.encode('utf-8')
assert out.count(b'\r') == cr, (out.count(b'\r'), cr)
open(p, 'wb').write(out)
print('patched; CR', cr, 'lines', out.count(b'\n'))
