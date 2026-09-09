import struct, sys

def c565(c):
    return (((c >> 11) & 31) * 255 + 15) // 31, (((c >> 5) & 63) * 255 + 31) // 63, ((c & 31) * 255 + 15) // 31

def bc1(path):
    """Decode mip 0 of a DXT1 DDS. Re-typed from the format on purpose: a check
    that decoded through the writer's own code could not fail on the writer."""
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<II', b, 12)
    off = 148 if b[84:88] == b'DX10' else 128
    px = [(0, 0, 0)] * (w * h)
    p = off
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            c0, c1 = struct.unpack_from('<HH', b, p)
            idx = struct.unpack_from('<I', b, p + 4)[0]
            p += 8
            p0, p1 = c565(c0), c565(c1)
            if c0 > c1:
                pal = [p0, p1, tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            else:
                pal = [p0, p1, tuple((p0[k] + p1[k]) // 2 for k in range(3)), (0, 0, 0)]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x < w and y < h:
                        px[y * w + x] = pal[(idx >> (2 * (j * 4 + i))) & 3]
    return px, w, h

BAND = 4          # one heightfield sample at 32 units per texel
MAXD = 24         # measured 17 on the worst of the four chunks, 2026-09-09
FRAC = 0.0005     # measured 0.016% and 0.032%; 0 on the other two chunks
SEAM = 8          # a dominantBase rescope would sit ON the interior tile seams

da, db, stems = sys.argv[1], sys.argv[2], sys.argv[3:]
fails, total = 0, 0
for stem in stems:
    A, w, h = bc1(da + '/' + stem + '.DDS')
    B, w2, h2 = bc1(db + '/' + stem + '.DDS')
    if (w, h) != (w2, h2):
        print('       %s: SIZE %dx%d vs %dx%d' % (stem, w, h, w2, h2)); fails += 1; continue
    n = far = mx = 0
    seam = 10 ** 9
    for y in range(h):
        for x in range(w):
            d = max(abs(A[y * w + x][k] - B[y * w + x][k]) for k in range(3))
            if not d:
                continue
            n += 1
            if d > mx:
                mx = d
            if min(x, w - 1 - x, y, h - 1 - y) >= BAND:
                far += 1
            seam = min(seam, min(abs(x - w // 2), abs(y - h // 2)))
    total += n
    print('       %-24s differing %5d/%d (%.4f%%) max %3d  outside the %d-texel boundary band %d  nearest interior tile seam %s'
          % (stem, n, w * h, 100.0 * n / (w * h), mx, BAND, far, seam if n else '-'))
    if far:
        print('       FAIL %s: %d texels differ AWAY from the chunk boundary' % (stem, far)); fails += 1
    if mx > MAXD:
        print('       FAIL %s: max channel difference %d > %d' % (stem, mx, MAXD)); fails += 1
    if n > FRAC * w * h:
        print('       FAIL %s: %d differing texels > %.4f%% of the sheet' % (stem, n, 100.0 * FRAC)); fails += 1
    if n and seam < SEAM:
        print('       FAIL %s: a differing texel sits %d texels from an interior tile seam' % (stem, seam)); fails += 1
# the floor: a check that cannot fail is not a check. Cover is ON, the fixture
# is painted, and the boundary normals DO differ, so at least one chunk must
# show the difference -- otherwise the tint, the cover pass or this decode is
# silently inert and every bar above passed on nothing.
if total == 0:
    print('       FAIL the difference the bars bound is absent entirely; nothing was tested'); fails += 1
print('       %d differing texels over %d chunks, %d bar(s) failed' % (total, len(stems), fails))
sys.exit(1 if fails else 0)
