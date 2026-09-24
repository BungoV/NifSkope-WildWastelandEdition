#!/usr/bin/env python
"""Where does Bethesda's landscape disagree across a shared cell edge, and
which BAKE-UNIT boundary does that edge sit on?

Lane CLAMP2's pre-build measurement. The ruling "the cell owns it" makes the
resolution of a shared VHGT row RELATIVE TO THE BAKE UNIT: a unit keeps its own
boundary row and never lets the ring overwrite it. A dim-4 chunk sheet is
assembled from FOUR dim-2 tiles (lodgen.cpp assembleChunkRow), so an edge that
is INTERIOR to the chunk but is a TILE boundary is resolved one way by the
direct chunk bake (later cell wins, north/east) and the other way by the lower
tile (its own, southern/western cell wins). Where Bethesda disagrees on such an
edge, V9a's assembled-vs-direct byte identity CANNOT hold.

This script counts those edges BEFORE the code is built, so the gate's verdict
is predicted rather than explained afterwards.

Reads --dump-land's format: int32 minX,minY,cellsX,cellsY; per cell one uint8
presence; per cell 33*33 int16 heights (units of 8), row 0 south, col 0 west.

Usage: python ownership_risk.py <land.bin> [x0 y0 x1 y1]
"""
import struct, sys


def main(path, x0=None, y0=None, x1=None, y1=None):
    b = open(path, 'rb').read()
    mnx, mny, cw, ch = struct.unpack_from('<iiii', b, 0)
    po = 16
    go = po + cw * ch
    print('grid %dx%d from (%d,%d)' % (cw, ch, mnx, mny))

    def cell(x, y):
        if not (0 <= x - mnx < cw and 0 <= y - mny < ch):
            return None
        i = (y - mny) * cw + (x - mnx)
        if b[po + i] == 0:
            return None
        o = go + i * 33 * 33 * 2
        return struct.unpack_from('<%dh' % (33 * 33), b, o)

    def north_seam(x, y):
        a, c = cell(x, y), cell(x, y + 1)
        if a is None or c is None:
            return None
        return max(abs(a[32 * 33 + k] - c[k]) for k in range(33))

    def east_seam(x, y):
        a, c = cell(x, y), cell(x + 1, y)
        if a is None or c is None:
            return None
        return max(abs(a[r * 33 + 32] - c[r * 33]) for r in range(33))

    if x0 is None:
        x0, y0, x1, y1 = mnx, mny, mnx + cw - 1, mny + ch - 1
    print('region x %d..%d  y %d..%d' % (x0, x1, y0, y1))

    # classify every shared edge by the coarsest bake unit it bounds.
    #   chunk4  : y+1 divisible by 4  -> a dim-4 chunk boundary (outer for both
    #             the direct bake and the dim-2 tile: consistent by the ruling)
    #   tile2   : y+1 even, not div by 4 -> INTERIOR to the dim-4 chunk but a
    #             dim-2 TILE boundary. THE RISK CLASS.
    #   inner   : y+1 odd -> interior to a dim-2 tile: later cell wins on both
    #             paths, unchanged by this lane.
    def cls(hi):
        if hi % 4 == 0:
            return 'chunk4'
        if hi % 2 == 0:
            return 'tile2'
        return 'inner'

    tot = {}
    bad = {}
    worst = {}
    for k in ('chunk4', 'tile2', 'inner'):
        tot[k] = bad[k] = worst[k] = 0
    risk = []
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            n = north_seam(x, y)
            if n is not None:
                k = cls(y + 1)
                tot[k] += 1
                if n:
                    bad[k] += 1
                    worst[k] = max(worst[k], n)
                    if k == 'tile2':
                        risk.append(('N', x, y, y + 1, n))
            e = east_seam(x, y)
            if e is not None:
                k = cls(x + 1)
                tot[k] += 1
                if e:
                    bad[k] += 1
                    worst[k] = max(worst[k], e)
                    if k == 'tile2':
                        risk.append(('E', x, y, x + 1, e))

    print()
    print('%-8s %10s %10s %8s %9s' %
          ('class', 'edges', 'disagreeing', 'pct', 'max |d|'))
    for k in ('chunk4', 'tile2', 'inner'):
        pct = (100.0 * bad[k] / tot[k]) if tot[k] else 0.0
        print('%-8s %10d %10d %7.3f%% %9d' % (k, tot[k], bad[k], pct, worst[k]))
    print()
    print('tile2 = interior to a dim-4 chunk, boundary of a dim-2 tile.')
    print('A NON-ZERO tile2 count inside the V9a fixture chunks means the')
    print('assembled sheet cannot be byte-identical to a direct bake.')
    if risk:
        print()
        print('the first 40 risk edges (dir, cell, shared index, max |d| in')
        print('VHGT units of 8):')
        for r in risk[:40]:
            print('   %s (%d,%d) shared %d  max %d' % r)
    return 0


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) >= 5:
        sys.exit(main(a[0], int(a[1]), int(a[2]), int(a[3]), int(a[4])))
    sys.exit(main(a[0]))
