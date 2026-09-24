#!/usr/bin/env python
"""How far from a chunk border did the ring move a texel?

The re-baseline number for lane CLAMP: for every sheet of every fixture chunk,
the count of texels whose RGB moved between the CLAMPED bake (before/) and the
RINGED one (after/), and the largest distance from the chunk's outer boundary at
which any of them sits.

PRE-REGISTERED BANDS, read off the code before the bakes existed
(scratchpad/lane_clamp_report.md section 0), at dim 4 = 32 world units a texel:

    colour   <=  4 texels   the normal's one-step central difference, 128 units
    _msn     <=  4 texels   the same
    _data    <= 64 texels   the AO march, `dist <= 2048.0f`, 2048/32

THE CONTROL is the other side of each band: beyond it the count must be ZERO.
A decoder that returned constants would read 0 differing everywhere and pass
every band, so the FLOOR is that at least one sheet of each role must move.

Usage:  python edgeband.py <beforeTexDir> <afterTexDir>
"""
import os, struct, sys, hashlib

CHUNKS = ['Commonwealth.4.-24.24', 'Commonwealth.4.-20.24',
          'Commonwealth.4.-24.28', 'Commonwealth.4.-20.28']
ROLES = [('', 'colour', 4), ('_msn', 'msn', 4), ('_data', 'data', 64)]


def c565(c):
    return ((((c >> 11) & 31) * 255 + 15) // 31,
            (((c >> 5) & 63) * 255 + 31) // 63,
            ((c & 31) * 255 + 15) // 31)


def decode(path):
    """mip 0 RGB of a DXT1 or DXT5 DDS, re-typed from the format."""
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<II', b, 12)
    fcc = b[84:88]
    off = 148 if fcc == b'DX10' else 128
    bs = 8 if fcc == b'DXT1' else 16
    cofs = 0 if bs == 8 else 8          # BC3 puts its alpha block first
    px = [(0, 0, 0)] * (w * h)
    p = off
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            q = p + cofs
            c0, c1 = struct.unpack_from('<HH', b, q)
            idx = struct.unpack_from('<I', b, q + 4)[0]
            p += bs
            p0, p1 = c565(c0), c565(c1)
            if c0 > c1 or bs == 16:
                pal = [p0, p1,
                       tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            else:
                pal = [p0, p1, tuple((p0[k] + p1[k]) // 2 for k in range(3)), (0, 0, 0)]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x < w and y < h:
                        px[y * w + x] = pal[(idx >> (2 * (j * 4 + i))) & 3]
    return px, w, h


def sha(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()


def main(dbefore, dafter):
    fails = 0
    moved = {}
    print('%-24s %-6s %9s %8s %6s %8s %8s' %
          ('chunk', 'sheet', 'differing', 'pct', 'maxd', 'beyond', 'band'))
    for role, name, band in ROLES:
        moved[name] = 0
        for stem in CHUNKS:
            fa = os.path.join(dbefore, stem + role + '.DDS')
            fb = os.path.join(dafter, stem + role + '.DDS')
            if not (os.path.exists(fa) and os.path.exists(fb)):
                print('MISSING %s or %s' % (fa, fb)); return 2
            if sha(fa) == sha(fb):
                print('%-24s %-6s %9d %8s %6s %8d %8d' % (stem, name, 0, '-', '-', 0, band))
                continue
            A, w, h = decode(fa)
            B, w2, h2 = decode(fb)
            if (w, h) != (w2, h2):
                print('%-24s %-6s SIZE %dx%d vs %dx%d' % (stem, name, w, h, w2, h2))
                fails += 1
                continue
            n = beyond = maxd = 0
            for y in range(h):
                for x in range(w):
                    if A[y * w + x] == B[y * w + x]:
                        continue
                    n += 1
                    d = min(x, w - 1 - x, y, h - 1 - y)
                    if d > maxd:
                        maxd = d
                    if d >= band:
                        beyond += 1
            moved[name] += n
            print('%-24s %-6s %9d %7.4f%% %6d %8d %8d' %
                  (stem, name, n, 100.0 * n / (w * h), maxd, beyond, band))
            if beyond:
                print('       FAIL %s %s: %d texels moved at or beyond %d from the border'
                      % (stem, name, beyond, band))
                fails += 1
    print()
    for role, name, band in ROLES:
        print('  %-6s total moved over the four chunks: %d' % (name, moved[name]))
    # the FLOOR: the colour and the msn MUST move, or the ring is inert and every
    # band above passed on nothing
    for name in ('colour', 'msn'):
        if moved[name] == 0:
            print('       FAIL the %s sheets did not move at all: the ring is inert' % name)
            fails += 1
    print()
    print('  the new baseline, sha256 of every after sheet:')
    for stem in CHUNKS:
        for role, name, band in ROLES:
            f = os.path.join(dafter, stem + role + '.DDS')
            print('    %-32s %s %d bytes' % (os.path.basename(f), sha(f)[:16],
                                             os.path.getsize(f)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))
