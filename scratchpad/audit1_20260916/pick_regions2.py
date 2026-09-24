# AUDIT1: score 12x12-cell regions by ACTUAL SUBMERGED TEXELS from the coarse overview,
# so the water fixture is chosen on wet area, not on the has-water flag (which is set worldwide).
import struct

P = r'E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodl'
b = open(P, 'rb').read()
minX, minY, maxX, maxY = struct.unpack_from('<4i', b, 8)
quantum = struct.unpack_from('<f', b, 0x2C)[0]
ovSamp = struct.unpack_from('<I', b, 0x40)[0]
cellTableOff = struct.unpack_from('<Q', b, 0x68)[0]
ovOff = struct.unpack_from('<Q', b, 0x70)[0]
cellsX = maxX - minX + 1
cellsY = maxY - minY + 1
W = cellsX * ovSamp
print('quantum', quantum, 'ov samples/cell', ovSamp, 'overview grid', W, 'x', cellsY * ovSamp)

cell = {}
for j in range(cellsY):
    for i in range(cellsX):
        o = cellTableOff + (j * cellsX + i) * 16
        mn, mx, wh = struct.unpack_from('<3f', b, o)
        wt, fl = struct.unpack_from('<2H', b, o + 12)
        cell[(i, j)] = (mn, mx, wh, wt, fl)

def cell_wet(i, j):
    """submerged overview texels in cell index (i,j) and the distinct water height"""
    mn, mx, wh, wt, fl = cell[(i, j)]
    if not (fl & 1):
        return 0, None
    n = 0
    for r in range(ovSamp):
        gy = j * ovSamp + r
        base = ovOff + (gy * W + i * ovSamp) * 2
        for c in range(ovSamp):
            h = (struct.unpack_from('<H', b, base + c * 2)[0] - 32767) * quantum
            if h < wh:
                n += 1
    return n, round(wh, 3)

wetcache = {}
rows = []
for cy in range(minY, maxY - 11, 4):
    for cx in range(minX, maxX - 11, 4):
        tot = 0
        hs = {}
        ok = True
        for j in range(cy, cy + 12):
            for i in range(cx, cx + 12):
                k = (i - minX, j - minY)
                if k not in cell:
                    ok = False
                    break
                if k not in wetcache:
                    wetcache[k] = cell_wet(*k)
                n, h = wetcache[k]
                tot += n
                if n:
                    hs[h] = hs.get(h, 0) + n
            if not ok:
                break
        if ok:
            rows.append((tot, len(hs), cx, cy))
tot_texels = 12 * 12 * ovSamp * ovSamp
rows.sort(reverse=True)
print('\ntexels per region', tot_texels)
print('-- most submerged texels (texels, distinct water heights, cellX, cellY) --')
for r in rows[:10]:
    print(r, '%.1f%%' % (100.0 * r[0] / tot_texels))
print('-- most distinct water heights with >=5%% wet --')
for r in sorted([r for r in rows if r[0] > tot_texels * 0.05], key=lambda r: -r[1])[:10]:
    print(r, '%.1f%%' % (100.0 * r[0] / tot_texels))
for nm, (cx, cy) in {'sanctuary': (-20, 24), 'urban': (0, -12)}.items():
    for r in rows:
        if r[2] == cx and r[3] == cy:
            print(nm, r, '%.1f%% wet' % (100.0 * r[0] / tot_texels))
