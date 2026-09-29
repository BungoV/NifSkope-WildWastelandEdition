"""MERGE1: find which whole-map VT.2 tiles hold the Boston render region, and prove it against the Boston bake.
Prints the region bake's grid, the whole map's grid, the tile offset, and whether the height sheets match."""
import sys
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake2/tests/spells')
import lodgen_vt_check as V

W = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/'
reg = V.Lodv(W + 'g/on/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt')
whole = V.Lodv(W + 'stage/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt')
for n, v in (('region', reg), ('whole', whole)):
    print(n, 'west', v.west if hasattr(v, 'west') else '?', 'east', v.east, 'south', v.south, 'north', v.north,
          'tilesX', v.tilesX, 'tiles', v.tileCount)


def height(v, tx, ty):
    idx = ty * v.tilesX + tx
    p = v.payload(idx)
    si = [i for i in range(v.sheetCount) if v.sheets[i]['role'] == 4][0]
    cover = bool(v.table[idx]['flags'] & 2)
    o = v.sheetOffset(cover, si, 0)
    return np.frombuffer(p, dtype='<u2', count=v.stored * v.stored, offset=o)


best = None
for dy in range(0, whole.tileCount // whole.tilesX):
    for dx in range(0, whole.tilesX):
        try:
            if np.array_equal(height(reg, 1, 1), height(whole, 1 + dx, 1 + dy)):
                best = (dx, dy)
                break
        except Exception:
            pass
    if best:
        break
print('tile (1,1) of the region bake = tile', None if best is None else (1 + best[0], 1 + best[1]), 'of the whole map')
if best:
    same = all(np.array_equal(height(reg, tx, ty), height(whole, tx + best[0], ty + best[1]))
               for tx in range(1, 6) for ty in range(1, 5))
    print('all 20 Boston tiles (tx 1..5, ty 1..4) have identical height sheets at that offset:', same)
    print('OFFSET', best[0], best[1])
