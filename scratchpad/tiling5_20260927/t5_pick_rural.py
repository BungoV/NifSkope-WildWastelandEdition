"""TILING5 -- pick the wide rural hills camera by measurement, not by eye.

Reads the Commonwealth .lodl's per-cell table (lo, hi, water height) and scores
every chunk-aligned 12x12-cell window (the Boston box's size) by the SD of the
cell mid-heights, keeping only windows that are >= 95 % dry and that do not
overlap the Boston box (-8,-12..3,-1).  Prints the top five; the first is the
rural camera.

    usage: python t5_pick_rural.py <Commonwealth.lodl>
"""
import sys
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-tiling5/tests/spells')
import lodl_open_authority as A


def main():
    L = A.Lodt(sys.argv[1])
    mid = np.full((L.cellsY, L.cellsX), np.nan)
    dry = np.zeros((L.cellsY, L.cellsX), bool)
    for cy in range(L.minY, L.maxY + 1):
        for cx in range(L.minX, L.maxX + 1):
            lo, hi, wh, wt, fl = L.cell(cx, cy)
            mid[cy - L.minY, cx - L.minX] = 0.5 * (lo + hi)
            dry[cy - L.minY, cx - L.minX] = lo > wh
    if len(sys.argv) > 2:
        # the named windows' stats: x0,y0 [x0,y0 ...]
        for w in sys.argv[2:]:
            x0, y0 = (int(v) for v in w.split(','))
            sy, sx = y0 - L.minY, x0 - L.minX
            m = mid[sy:sy + 12, sx:sx + 12]
            d = dry[sy:sy + 12, sx:sx + 12]
            print("window %d,%d..%d,%d  mid-height SD %.0f  dry %.2f" % (x0, y0, x0 + 11, y0 + 11, float(np.nanstd(m)), float(d.mean())))
        return
    res = []
    # inside the playable map (+-40 cells): the border ranges beyond it are
    # the scenery wall, one texture, not "rural hills"
    for y0 in range(-40, 40 - 11, 4):
        for x0 in range(-40, 40 - 11, 4):
            if not (x0 + 11 < -8 or x0 > 3 or y0 + 11 < -12 or y0 > -1):
                continue
            sy, sx = y0 - L.minY, x0 - L.minX
            if sy < 0 or sx < 0 or sy + 12 > L.cellsY or sx + 12 > L.cellsX:
                continue
            m = mid[sy:sy + 12, sx:sx + 12]
            d = dry[sy:sy + 12, sx:sx + 12]
            if d.mean() < 0.95:
                continue
            res.append((float(np.nanstd(m)), x0, y0, float(d.mean())))
    res.sort(reverse=True)
    for sd, x0, y0, df in res[:5]:
        print("window %d,%d..%d,%d  mid-height SD %.0f  dry %.2f" % (x0, y0, x0 + 11, y0 + 11, sd, df))


if __name__ == "__main__":
    main()
