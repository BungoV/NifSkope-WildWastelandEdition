#!/usr/bin/env python
"""ROW A: rewrite lodgen_terrain_vt.sh's V9c with a BC3 decoder and re-pinned bars.

Written as a FILE and applied with Python rather than typed into a heredoc:
the block carries backslashes and a shell heredoc in MSYS2/Git-Bash collapses
them (the trap `ww-shell-heredoc` warns about, and mistake 1 of this lane).

The replacement keeps V9c as exactly ONE check, so the spell's count does not
move because of this row.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_terrain_vt.sh'

START = b"\t# V9c: WHICH normal is right, measured on the DIRECT sheets alone.\n"
END = b'\t\t|| { fails=$((fails + 1)); echo "  FAIL V9c the direct sheets are continuous ACROSS a chunk seam (the block above)"; }\n'

NEW = b'''\t# V9c: WHICH normal is right, measured on the DIRECT sheets alone.
\t#
\t# REWRITTEN 2026-09-16 (lane GENSMALL1, director ROW A). Two things were
\t# wrong with the version this replaces, and both were wrong in the same
\t# direction -- it printed numbers that meant nothing and then judged them.
\t#
\t#   1. THE DECODE. The `_msn` sheets are DXT5 (BC3): 512 x 512, 10 mips,
\t#      349,680 bytes, `DXT5` in the header at byte 84. The old block read
\t#      them with a DXT1 reader, which walks 8-byte blocks through a
\t#      16-byte-block payload, so from the second block on it was decoding
\t#      alpha bytes as colour endpoints. Every reading it printed was noise,
\t#      and the interior control it printed (13.243 / 12.182) is the
\t#      signature of that noise rather than of any terrain.
\t#   2. THE OPERAND. These four chunks now COPY vanilla's normal file whole --
\t#      measured 2026-09-16, all four `_msn` sheets are byte-identical to
\t#      `<data>/Textures/Terrain/Commonwealth/`'s own. The bars were pinned in
\t#      2026-09-09 on sheets this fork generated, and a bar pinned on one
\t#      operand and applied to another is an accommodation, not a check.
\t#
\t# THE BARS, re-pinned on what is actually on disk (2026-09-16, BC3 decode):
\t#
\t#   reading            as baked   E shifted 16 rows   bar
\t#   E/W seam ratio       0.990          1.330         1.15
\t#   N/S seam ratio       1.311          1.602         1.45
\t#   interior control  28.804/25.488   (DXT1 reader: 13.243/12.182)  20..36
\t#   edge step / control  0.733/0.980                  1.20
\t#
\t# Each bar is the GEOMETRIC MIDPOINT of the true reading and the broken one,
\t# so it discriminates rather than accommodates: sqrt(0.990 * 1.330) = 1.147
\t# and sqrt(1.311 * 1.602) = 1.449. The interior band excludes the old DXT1
\t# reader's own numbers by a factor of 1.5, so this check would have gone red
\t# on the reader it replaces -- which is the floor under the whole rewrite.
\t#
\t# The control is taken WELL INSIDE the sheet (x = 100, 200, 300, 400), not
\t# one texel in: a clamped bake's own edge column is inside the defect, and
\t# using it once made the clamped bake look better (lane VTFIX, mistake 3).
\t#
\t# AND THE REFUTER RUNS IN THE SAME CHECK. After the real sheets are measured
\t# the east sheet is shifted 16 texel rows and the north sheet 16 texel
\t# columns, and the same bars are applied again: at least one must break. A
\t# green that cannot go red is not a measurement, and this is the one place
\t# that can be proved without a second bake.
\t"$PY" - "$W/novt/tex" <<'PYEOF'
import struct, sys

def c565(c):
    return (((c >> 11) & 31) * 255 + 15) // 31, (((c >> 5) & 63) * 255 + 31) // 63, ((c & 31) * 255 + 15) // 31

def bc3(path):
    """Decode mip 0 of a DXT5/BC3 DDS. Re-typed from the format on purpose: a
    check that decoded through the writer's own code could not fail on the
    writer. BC3 is 8 bytes of interpolated alpha and then a DXT1-shaped colour
    block that ALWAYS uses the four-colour rule -- the c0 <= c1 punch-through
    of DXT1 does not exist here, and reading it as DXT1 is the defect this
    block replaces."""
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<II', b, 12)
    if b[84:88] != b'DXT5':
        print('       FAIL %s is %r, not DXT5: the decoder below would read noise'
              % (path.rsplit('/', 1)[-1], b[84:88].decode('latin-1')))
        sys.exit(1)
    off = 128
    want = off + ((w + 3) // 4) * ((h + 3) // 4) * 16
    if len(b) < want:
        print('       FAIL %s is %d bytes, under the %d mip 0 alone needs at 16 bytes a block'
              % (path.rsplit('/', 1)[-1], len(b), want))
        sys.exit(1)
    px = [(0, 0, 0)] * (w * h)
    p = off
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            c0, c1 = struct.unpack_from('<HH', b, p + 8)
            idx = struct.unpack_from('<I', b, p + 12)[0]
            p += 16
            p0, p1 = c565(c0), c565(c1)
            pal = [p0, p1, tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                   tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x < w and y < h:
                        px[y * w + x] = pal[(idx >> (2 * (j * 4 + i))) & 3]
    return px, w, h

RATIO_EW = 1.15     # baked 0.990, E shifted 16 rows 1.330 (2026-09-16)
RATIO_NS = 1.45     # baked 1.311, N shifted 16 columns 1.602
EDGE_MAX = 1.20     # a sheet's own last-two-columns step over the control: 0.733 / 0.980
CTL_LO, CTL_HI = 20.0, 36.0   # interior control: 28.804 E/W, 25.488 N/S under a BC3 decode
SHIFT = 16

d = sys.argv[1]
W = bc3(d + '/Commonwealth.4.-24.24_msn.DDS')
E = bc3(d + '/Commonwealth.4.-20.24_msn.DDS')
S = W
N = bc3(d + '/Commonwealth.4.-24.28_msn.DDS')

def mad(a, b):
    return sum(max(abs(p[k] - q[k]) for k in range(3)) for p, q in zip(a, b)) / float(len(a))

def col(t, x):
    px, w, h = t
    return [px[y * w + x] for y in range(h)]

def row(t, y):
    px, w, h = t
    return [px[y * w + x] for x in range(w)]

def shift_rows(t, k):
    px, w, h = t
    return ([px[((y + k) % h) * w + x] for y in range(h) for x in range(w)], w, h)

def shift_cols(t, k):
    px, w, h = t
    return ([px[y * w + ((x + k) % w)] for y in range(h) for x in range(w)], w, h)

w = W[1]
ctl_x = sum(mad(col(t, x), col(t, x + 1)) for t in (W, E) for x in (100, 200, 300, 400)) / 8.0
ctl_y = sum(mad(row(t, y), row(t, y + 1)) for t in (S, N) for y in (100, 200, 300, 400)) / 8.0
edge_x = (mad(col(W, w - 2), col(W, w - 1)) + mad(col(E, 0), col(E, 1))) / 2.0
# row 0 is NORTH, so the south chunk's row 0 meets the north chunk's last row
edge_y = (mad(row(S, 0), row(S, 1)) + mad(row(N, w - 2), row(N, w - 1))) / 2.0

def seams(east, north):
    return mad(col(W, w - 1), col(east, 0)), mad(row(S, 0), row(north, w - 1))

seam_x, seam_y = seams(E, N)
fails = 0
print('       E/W seam %7.3f interior %7.3f ratio %5.3f (bar %.2f)'
      % (seam_x, ctl_x, seam_x / ctl_x if ctl_x else 0.0, RATIO_EW))
print('       N/S seam %7.3f interior %7.3f ratio %5.3f (bar %.2f)'
      % (seam_y, ctl_y, seam_y / ctl_y if ctl_y else 0.0, RATIO_NS))
print('       edge step E/W %7.3f N/S %7.3f, over the control %5.3f / %5.3f (bar %.2f)'
      % (edge_x, edge_y, edge_x / ctl_x if ctl_x else 0.0,
         edge_y / ctl_y if ctl_y else 0.0, EDGE_MAX))
# the floor first: a decode that returned a constant reads 0 everywhere and
# would sail through every ratio below as 0/0, and the DXT1 reader this block
# replaces reads 13.243 / 12.182, which this band excludes by half
if not (CTL_LO <= ctl_x <= CTL_HI) or not (CTL_LO <= ctl_y <= CTL_HI):
    print('       FAIL the interior control is %.3f / %.3f, outside %.1f..%.1f: these'
          % (ctl_x, ctl_y, CTL_LO, CTL_HI))
    print('            sheets are not the terrain the bars were measured on, or they')
    print('            were not decoded as BC3')
    fails += 1
if ctl_x and seam_x / ctl_x > RATIO_EW:
    print('       FAIL E/W seam is %.3fx the interior step, over %.2f (the edge clamp)'
          % (seam_x / ctl_x, RATIO_EW))
    fails += 1
if ctl_y and seam_y / ctl_y > RATIO_NS:
    print('       FAIL N/S seam is %.3fx the interior step, over %.2f' % (seam_y / ctl_y, RATIO_NS))
    fails += 1
if ctl_x and ctl_y and max(edge_x / ctl_x, edge_y / ctl_y) > EDGE_MAX:
    print('       FAIL a sheet own edge step is %.3fx its interior step, over %.2f: its'
          % (max(edge_x / ctl_x, edge_y / ctl_y), EDGE_MAX))
    print('            last column is anomalous against its own neighbours, the clamp')
    fails += 1
# THE REFUTER, in the same check: shift the neighbours and the bars must break
bx, by = seams(shift_rows(E, SHIFT), shift_cols(N, SHIFT))
rx = bx / ctl_x if ctl_x else 0.0
ry = by / ctl_y if ctl_y else 0.0
broke = (rx > RATIO_EW) + (ry > RATIO_NS)
print('       REFUTER neighbours shifted %d texels: E/W %5.3f (bar %.2f), N/S %5.3f (bar %.2f) -- %d of 2 break'
      % (SHIFT, rx, RATIO_EW, ry, RATIO_NS, broke))
if broke < 2:
    print('       FAIL the shifted sheets pass the same bars, so the bars are')
    print('            accommodating the data instead of measuring it')
    fails += 1
sys.exit(1 if fails else 0)
PYEOF
\tRC=$?; checks=$((checks + 1))
\t[ $RC -eq 0 ] \\
\t\t&& echo "  ok   V9c the direct sheets are continuous ACROSS a chunk seam under a BC3 decode, against an interior control, and shifted neighbours break the same bars (the block above)" \\
\t\t|| { fails=$((fails + 1)); echo "  FAIL V9c the direct sheets are continuous ACROSS a chunk seam under a BC3 decode (the block above)"; }
'''

d = open(P, 'rb').read()
i = d.find(START)
j = d.find(END)
if i < 0 or j < 0:
    sys.exit('anchor not found: start %d end %d' % (i, j))
j += len(END)
before = len(d)
out = d[:i] + NEW + d[j:]
open(P, 'wb').write(out)
print('lodgen_terrain_vt.sh  %d -> %d bytes  CR %d -> %d  LF %d -> %d'
      % (before, len(out), d.count(b'\r'), out.count(b'\r'),
         d.count(b'\n'), out.count(b'\n')))
