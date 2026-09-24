# VT1 step 2: the candidate rule, modelled offline before any build.
#
# TODAY   the ring fill derives the inner unit from the caller's OWN rdim, so a
#         dim-2 tile protects a 2x2 inner unit and the dim-4 chunk protects a
#         4x4 one.  Every sample OUTSIDE the inner unit is plain later-wins, so
#         the same world sample is the south cell's copy in one grid and the
#         north cell's in the other.
# RULE    the inner unit is a WORLD-ANCHORED closed box; the tile baker passes
#         the box of the chunk it will be assembled into (floorTo(cellX0, 2d)),
#         the chunk baker passes its own (unchanged).
#
# Gate: over the chunk's whole ring grid, the tile grids must agree with the
# chunk grid on every overlapping sample, and the OLD rule must NOT (the
# refuter).
import struct, sys, numpy as np

LAND = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/land.bin'
RC = 1

class Land:
    def __init__(self, path):
        b = open(path, 'rb').read()
        self.minX, self.minY, self.cw, self.ch = struct.unpack_from('<iiii', b, 0)
        n = self.cw * self.ch
        off = 16
        self.have = np.frombuffer(b, np.uint8, n, off).reshape(self.ch, self.cw)
        off += n
        self.h = np.frombuffer(b, '<i2', n * 33 * 33, off).reshape(self.ch, self.cw, 33, 33).astype(np.float64) * 8.0
    def get(self, cx, cy):
        ix, iy = cx - self.minX, cy - self.minY
        if ix < 0 or iy < 0 or ix >= self.cw or iy >= self.ch:
            return None
        if not self.have[iy, ix]:
            return None
        return self.h[iy, ix]

def floor_to(v, m):
    r = v % m
    return v - r

def fill_ring(L, cellX0, cellY0, dim, inner=None, empty=0.0):
    """inner = (loX, hiX, loY, hiY) in THIS grid's coordinates, or None to derive."""
    rdim = dim + 2 * RC
    rx0, ry0 = cellX0 - RC, cellY0 - RC
    hn = rdim * 32 + 1
    haveInner = rdim > 2 * RC
    if inner is None:
        lo = 32 * RC; hi = hn - 1 - lo
        loX, hiX, loY, hiY = lo, hi, lo, hi
    else:
        loX, hiX, loY, hiY = inner
        haveInner = True
    g = np.full((hn, hn), empty, np.float64)
    for cy in range(rdim):
        for cx in range(rdim):
            land = L.get(rx0 + cx, ry0 + cy)
            if land is None:
                continue
            # a cell is an INNER cell when its own 33x33 block sits wholly inside
            # the inner box; with a derived box this is exactly the old margin test
            ringCell = haveInner and not (cx * 32 >= loX and cx * 32 + 32 <= hiX
                                          and cy * 32 >= loY and cy * 32 + 32 <= hiY)
            for row in range(33):
                gr = cy * 32 + row
                rowInside = (loY <= gr <= hiY)
                for col in range(33):
                    gc = cx * 32 + col
                    if ringCell and rowInside and loX <= gc <= hiX:
                        continue
                    g[gr, gc] = land[row, col]
    return g, hn, rx0, ry0

def parent_inner_box(cellX0, cellY0, d, rx0, ry0, hn):
    """the dim-2d chunk's closed grid box, in the tile grid's coordinates"""
    px0 = floor_to(cellX0, 2 * d); py0 = floor_to(cellY0, 2 * d)
    loX = (px0 - rx0) * 32; hiX = (px0 + 2 * d - rx0) * 32
    loY = (py0 - ry0) * 32; hiY = (py0 + 2 * d - ry0) * 32
    # clip to the grid; a box edge off the grid simply never bites
    return (max(0, loX), min(hn - 1, hiX), max(0, loY), min(hn - 1, hiY))

def compare(L, cx0, cy0, D, use_fix):
    gC, hnC, rxC, ryC = fill_ring(L, cx0, cy0, D)
    d = D // 2
    total = 0; worst = 0.0
    for by in range(2):
        for bx in range(2):
            tx, ty = cx0 + bx * d, cy0 + by * d
            if use_fix:
                rdim = d + 2 * RC; hnT = rdim * 32 + 1
                box = parent_inner_box(tx, ty, d, tx - RC, ty - RC, hnT)
                gT, hnT, rxT, ryT = fill_ring(L, tx, ty, d, inner=box)
            else:
                gT, hnT, rxT, ryT = fill_ring(L, tx, ty, d)
            oCx, oCy, oTx, oTy = rxC * 32, ryC * 32, rxT * 32, ryT * 32
            lox = max(oCx, oTx); hix = min(oCx + hnC, oTx + hnT)
            loy = max(oCy, oTy); hiy = min(oCy + hnC, oTy + hnT)
            sc = gC[loy - oCy:hiy - oCy, lox - oCx:hix - oCx]
            st = gT[loy - oTy:hiy - oTy, lox - oTx:hix - oTx]
            dif = np.abs(sc - st)
            total += int((dif > 0).sum())
            worst = max(worst, float(dif.max()) if dif.size else 0.0)
    return total, worst

def main():
    L = Land(LAND)
    chunks = [(-24, 24), (-20, 24), (-24, 28), (-20, 28)]
    if len(sys.argv) > 1:
        chunks = [(int(sys.argv[i]), int(sys.argv[i + 1])) for i in range(1, len(sys.argv), 2)]
    print('%-22s %14s %14s' % ('chunk dim 4', 'OLD rule', 'WORLD-BOX rule'))
    badOld = 0; badNew = 0
    for (cx, cy) in chunks:
        o, ow = compare(L, cx, cy, 4, False)
        n, nw = compare(L, cx, cy, 4, True)
        badOld += o; badNew += n
        print('Commonwealth.4.%d.%-8d %6d (%.0f u)  %6d (%.0f u)' % (cx, cy, o, ow, n, nw))
    print()
    print('FLOOR  the world-box rule must give 0 differing samples: %s'
          % ('ok' if badNew == 0 else 'FAIL %d' % badNew))
    print('REFUTER the old rule must give MORE than 0 on this set: %s (%d)'
          % ('ok' if badOld > 0 else 'FAIL - the bar cannot fail', badOld))

if __name__ == '__main__':
    main()
