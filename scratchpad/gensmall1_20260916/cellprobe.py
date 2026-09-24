#!/usr/bin/env python
"""GENSMALL1 probe: how far apart are the WRITER's cell (float position) and the
DECODER's cell (dequantised position) on a real .lodi?  Prints the table, not
the count (ww-spec-gate-audit)."""
import struct, sys
CHUNK = 16384.0
CELL = 4096.0
p = sys.argv[1]
b = open(p, 'rb').read()
def le(f, o):
    return struct.unpack_from('<' + f, b, o)
cw, cs, ce, cn = le('hhhh', 0x48)
presentChunks, = le('I', 0x5C)
instanceCount, = le('I', 0x58)
offChunks, offCellRanges, offInstances, offCold, fileBytes = le('QQQQQ', 0x68)
w = ce - cw + 1
rows = []
present = 0
for ci in range((cn - cs + 1) * w):
    first, cnt, zMin, zExt, maxR, cro, crc, rsv = le('IIfffIII', offChunks + ci * 32)
    if cnt == 0:
        continue
    cx = cw + ci % w
    cy = cn - ci // w
    # index -> cell row from the RANGE table
    cellOf = {}
    for k in range(16):
        f, n = le('II', offCellRanges + (cro + k) * 8)
        for i in range(f, f + n):
            cellOf[i] = k
    for i in range(first, first + cnt):
        px, py, pz = le('HHH', offInstances + i * 24)
        fx = px / 65535.0 * CHUNK
        fy = py / 65535.0 * CHUNK
        lx = min(3, max(0, int(fx // CELL)))
        ly = min(3, max(0, int(fy // CELL)))
        derived = (3 - ly) * 4 + lx
        stored = cellOf[i]
        if derived != stored:
            # distance to the nearest cell line on each axis
            dx = min(abs(fx - k * CELL) for k in range(5))
            dy = min(abs(fy - k * CELL) for k in range(5))
            ref, part, ident = le('IhH', offCold + i * 8)
            rows.append((i, ci, cx, cy, derived, stored, fx / CELL, fy / CELL, dx, dy, ref))
    present += 1
step = CHUNK / 65535.0
print('file %s   instances %d   presentChunks %d   quant step %.7f u (half %.7f)'
      % (p, instanceCount, present, step, step / 2))
print('instances whose DERIVED cell differs from the cell RANGE it is stored in: %d' % len(rows))
print('%6s %6s %5s %5s %4s %4s %12s %12s %10s %10s %10s'
      % ('inst', 'chunk', 'cx', 'cy', 'der', 'rng', 'fx(cells)', 'fy(cells)', 'dx(u)', 'dy(u)', 'ref'))
for r in rows:
    print('%6d %6d %5d %5d %4d %4d %12.6f %12.6f %10.6f %10.6f   %08x'
          % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10]))
if rows:
    print('worst distance from a cell line among the disagreements: dx %.6f u, dy %.6f u'
          % (max(min(r[8], 1e9) for r in rows), max(r[9] for r in rows)))
    # the axis that actually moved
    for r in rows:
        dlx, dly = r[4] % 4, r[5] % 4
        rlx, rly = 3 - r[4] // 4, 3 - r[5] // 4
        print('  inst %d: derived (lx=%d, ly=%d) vs range (lx=%d, ly=%d); |dx|=%.6f |dy|=%.6f'
              % (r[0], dlx, rlx, r[5] % 4, 3 - r[5] // 4, r[8], r[9]))
