#!/usr/bin/env python
"""Do adjacent FO4 cells agree on the VHGT row they SHARE?

The discriminator for lane CLAMP's north-edge band miss. lodgenTerrainFillRing's
documented contract is that a later cell overwrites the sample it shares with an
earlier one (south->north, west->east). If Bethesda's own data disagrees across a
shared cell edge, the ring necessarily changes the chunk's OWN boundary grid row
where the clamp did not -- which is a property of the master, not of our code.

Reads --dump-land's format: int32 minX,minY,cellsX,cellsY; per cell one uint8
presence; per cell 33*33 int16 heights (units of 8), row 0 south, col 0 west.
"""
import struct, sys


def main(path):
    b = open(path, 'rb').read()
    mnx, mny, cw, ch = struct.unpack_from('<iiii', b, 0)
    po = 16
    go = po + cw * ch
    print('grid %dx%d from (%d,%d)' % (cw, ch, mnx, mny))

    def cell(x, y):
        i = (y - mny) * cw + (x - mnx)
        if not (0 <= x - mnx < cw and 0 <= y - mny < ch):
            return None
        if b[po + i] == 0:
            return None
        o = go + i * 33 * 33 * 2
        return struct.unpack_from('<%dh' % (33 * 33), b, o)

    def north_seam(x, y):
        """cell (x,y) row 32 vs cell (x,y+1) row 0 -- the shared vertex row."""
        a, c = cell(x, y), cell(x, y + 1)
        if a is None or c is None:
            return None
        d = [abs(a[32 * 33 + k] - c[0 * 33 + k]) for k in range(33)]
        return max(d), sum(1 for v in d if v)

    def east_seam(x, y):
        a, c = cell(x, y), cell(x + 1, y)
        if a is None or c is None:
            return None
        d = [abs(a[r * 33 + 32] - c[r * 33 + 0]) for r in range(33)]
        return max(d), sum(1 for v in d if v)

    print()
    print('NORTH seams, x = -24..-17, for the y rows the four fixture chunks touch')
    print('%6s | %s' % ('y', '  '.join('%5d' % x for x in range(-24, -16))))
    for y in (23, 27, 31, 32):
        row = []
        for x in range(-24, -16):
            r = north_seam(x, y)
            row.append('  -  ' if r is None else '%5d' % r[0])
        print('%6d | %s   <- shared with y=%d' % (y, '  '.join(row), y + 1))
    print()
    print('EAST seams, y = 24..31, at the chunk boundary columns x = -21 and -17')
    print('%6s | %s' % ('x', '  '.join('%5d' % y for y in range(24, 32))))
    for x in (-21, -17):
        row = []
        for y in range(24, 32):
            r = east_seam(x, y)
            row.append('  -  ' if r is None else '%5d' % r[0])
        print('%6d | %s' % (x, '  '.join(row)))
    print()
    print('(values are |difference| in VHGT units of 8; 0 = the two cells agree')
    print(' exactly on the row they share, so the ring cannot move that row)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
