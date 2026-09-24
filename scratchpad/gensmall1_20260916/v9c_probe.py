"""V9c re-measurement (lane GENSMALL1, 2026-09-16, director ROW A).

The `_msn` sheets are DXT5 (BC3), 512x512, 10 mips -- and today they are
byte-identical to vanilla's own shipped normal files.  The check in
lodgen_terrain_vt.sh decoded them with a DXT1 reader, which walks 8-byte
blocks through a 16-byte-block payload, so every number it printed was noise.

This prints the seam/interior/edge readings under a real BC3 decode so the
bars can be re-pinned on measurement instead of on the old DXT1 noise.
"""
import struct
import sys


def c565(c):
    return ((((c >> 11) & 31) * 255 + 15) // 31,
            (((c >> 5) & 63) * 255 + 31) // 63,
            ((c & 31) * 255 + 15) // 31)


def bc3(path):
    """Decode mip 0 of a DXT5/BC3 DDS.  Re-typed from the format on purpose:
    a check that decoded through the writer's own code could not fail on the
    writer.  BC3 = 8 bytes of interpolated alpha then a DXT1 colour block that
    ALWAYS uses the four-colour rule (the c0 <= c1 punch-through does not
    exist in BC3)."""
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<II', b, 12)
    assert b[84:88] == b'DXT5', (path, b[84:88])
    off = 148 if b[84:88] == b'DX10' else 128
    px = [(0, 0, 0, 0)] * (w * h)
    p = off
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            a0, a1 = b[p], b[p + 1]
            abits = int.from_bytes(b[p + 2:p + 8], 'little')
            if a0 > a1:
                al = [a0, a1] + [((7 - k) * a0 + k * a1) // 7 for k in range(1, 7)]
            else:
                al = [a0, a1] + [((5 - k) * a0 + k * a1) // 5 for k in range(1, 5)] + [0, 255]
            c0, c1 = struct.unpack_from('<HH', b, p + 8)
            idx = struct.unpack_from('<I', b, p + 12)[0]
            p += 16
            p0, p1 = c565(c0), c565(c1)
            pal = [p0, p1,
                   tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                   tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x < w and y < h:
                        c = pal[(idx >> (2 * (j * 4 + i))) & 3]
                        a = al[(abits >> (3 * (j * 4 + i))) & 7]
                        px[y * w + x] = (c[0], c[1], c[2], a)
    return px, w, h


def mad(a, b):
    return sum(max(abs(p[k] - q[k]) for k in range(3)) for p, q in zip(a, b)) / float(len(a))


def col(t, x):
    px, w, h = t
    return [px[y * w + x] for y in range(h)]


def row(t, y):
    px, w, h = t
    return [px[y * w + x] for x in range(w)]


def readings(d, shift=None):
    W = bc3(d + '/Commonwealth.4.-24.24_msn.DDS')
    E = bc3(d + '/Commonwealth.4.-20.24_msn.DDS')
    N = bc3(d + '/Commonwealth.4.-24.28_msn.DDS')
    if shift:
        px, w, h = E
        E = ([px[((y + shift) % h) * w + x] for y in range(h) for x in range(w)], w, h)
    S = W
    w = W[1]
    seam_x = mad(col(W, w - 1), col(E, 0))
    ctl_x = sum(mad(col(t, x), col(t, x + 1)) for t in (W, E) for x in (100, 200, 300, 400)) / 8.0
    edge_x = (mad(col(W, w - 2), col(W, w - 1)) + mad(col(E, 0), col(E, 1))) / 2.0
    seam_y = mad(row(S, 0), row(N, w - 1))
    ctl_y = sum(mad(row(t, y), row(t, y + 1)) for t in (S, N) for y in (100, 200, 300, 400)) / 8.0
    edge_y = (mad(row(S, 0), row(S, 1)) + mad(row(N, w - 2), row(N, w - 1))) / 2.0
    return seam_x, ctl_x, edge_x, seam_y, ctl_y, edge_y


d = sys.argv[1]
for label, sh in (('as baked', None), ('E shifted 1 texel row', 1), ('E shifted 4 texel rows', 4)):
    sx, cx, ex, sy, cy, ey = readings(d, sh)
    print('%-24s E/W seam %7.3f interior %6.3f ratio %6.2f edge %6.3f' % (label, sx, cx, sx / cx if cx else 0, ex))
    print('%-24s N/S seam %7.3f interior %6.3f ratio %6.2f edge %6.3f' % ('', sy, cy, sy / cy if cy else 0, ey))
