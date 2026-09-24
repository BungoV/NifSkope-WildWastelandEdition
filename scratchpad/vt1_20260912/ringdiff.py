# VT1 step 1: do the CHUNK path's ring height grid and the TILE path's ring
# height grids disagree, and where?
#
# Reproduces lodgenTerrainFillRing (src/lodgen.cpp:5724) offline, from the
# already-dumped Commonwealth VHGT (scratchpad/build4_20260910/land.bin,
# --dump-land format documented at src/nifcli.cpp:3054).
#
# The chunk baker's inner unit is the dim-D chunk; the tile baker's is the
# dim-D/2 tile.  "The cell owns it" therefore resolves a cell line that is
# INTERNAL to the chunk differently on the two paths.
import struct, sys, numpy as np

LAND = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/land.bin'
RC = 1          # LODGEN_TERRAIN_RING_CELLS

class Land:
    def __init__(self, path):
        b = open(path, 'rb').read()
        self.minX, self.minY, self.cw, self.ch = struct.unpack_from('<iiii', b, 0)
        n = self.cw * self.ch
        off = 16
        self.have = np.frombuffer(b, np.uint8, n, off).reshape(self.ch, self.cw)
        off += n
        self.h = np.frombuffer(b, '<i2', n * 33 * 33, off).reshape(self.ch, self.cw, 33, 33).astype(np.float64) * 8.0
        print('land.bin: cells (%d,%d) %dx%d, %d present'
              % (self.minX, self.minY, self.cw, self.ch, int(self.have.sum())))
    def get(self, cx, cy):
        ix, iy = cx - self.minX, cy - self.minY
        if ix < 0 or iy < 0 or ix >= self.cw or iy >= self.ch:
            return None
        if not self.have[iy, ix]:
            return None
        return self.h[iy, ix]            # [row][col], row 0 south, col 0 west

def fill_ring(L, cellX0, cellY0, dim, empty=0.0):
    """lodgenTerrainFillRing, verbatim in its ordering and its ring rule."""
    rdim = dim + 2 * RC
    rx0, ry0 = cellX0 - RC, cellY0 - RC
    hn = rdim * 32 + 1
    haveInner = rdim > 2 * RC
    innerLo = 32 * RC
    innerHi = hn - 1 - innerLo
    g = np.full((hn, hn), empty, np.float64)
    for cy in range(rdim):
        for cx in range(rdim):
            land = L.get(rx0 + cx, ry0 + cy)
            if land is None:
                continue
            ringCell = haveInner and (cx < RC or cx >= rdim - RC
                                      or cy < RC or cy >= rdim - RC)
            for row in range(33):
                gr = cy * 32 + row
                rowInside = (innerLo <= gr <= innerHi)
                for col in range(33):
                    gc = cx * 32 + col
                    if ringCell and rowInside and innerLo <= gc <= innerHi:
                        continue
                    g[gr, gc] = land[row, col]
    return g, hn, rx0, ry0

def main():
    cx0, cy0, D = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    L = Land(LAND)
    gC, hnC, rxC, ryC = fill_ring(L, cx0, cy0, D)
    print('chunk ring: cells (%d,%d) rdim %d hn %d' % (rxC, ryC, D + 2 * RC, hnC))
    d = D // 2
    tiles = [(cx0 + bx * d, cy0 + by * d) for by in range(2) for bx in range(2)]
    for (tx, ty) in tiles:
        gT, hnT, rxT, ryT = fill_ring(L, tx, ty, d)
        # overlap in GRID coordinates: both grids are anchored to world cell
        # lines, chunk grid sample (gr,gc) is world cell-line (rxC*32+gc, ryC*32+gr)
        offC_x = rxC * 32
        offC_y = ryC * 32
        offT_x = rxT * 32
        offT_y = ryT * 32
        lo_x = max(offC_x, offT_x); hi_x = min(offC_x + hnC, offT_x + hnT)
        lo_y = max(offC_y, offT_y); hi_y = min(offC_y + hnC, offT_y + hnT)
        sub_c = gC[lo_y - offC_y:hi_y - offC_y, lo_x - offC_x:hi_x - offC_x]
        sub_t = gT[lo_y - offT_y:hi_y - offT_y, lo_x - offT_x:hi_x - offT_x]
        dif = np.abs(sub_c - sub_t)
        nz = np.argwhere(dif > 0)
        print('tile (%d,%d) dim %d: overlap %dx%d, differing samples %d, max |dh| %.1f u'
              % (tx, ty, d, sub_c.shape[1], sub_c.shape[0], len(nz), dif.max() if dif.size else 0.0))
        if len(nz):
            rows = sorted(set(int(r) + lo_y for r, c in nz))
            cols = sorted(set(int(c) + lo_x for r, c in nz))
            print('    grid rows (world cell-line*32) %s..%s  -> world Y %d..%d'
                  % (rows[0], rows[-1], rows[0] * 128, rows[-1] * 128))
            print('    grid cols %s..%s  -> world X %d..%d'
                  % (cols[0], cols[-1], cols[0] * 128, cols[-1] * 128))
            print('    distinct rows %s' % rows[:12])
            print('    distinct cols %s' % cols[:12])

main()
