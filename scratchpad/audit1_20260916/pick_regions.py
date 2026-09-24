# AUDIT1: pick the WATER fixture region by measurement, off the shipped whole-Commonwealth .lodl.
# Per-cell table: float minZ, float maxZ, float waterH, u16 waterType, u16 flags (bit0 has water, bit1 has land)
import struct, sys

P = r'E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodl'
b = open(P, 'rb').read()
assert b[:4] == b'LODT', b[:4]
ver = struct.unpack_from('<I', b, 4)[0]
minX, minY, maxX, maxY = struct.unpack_from('<4i', b, 8)
spc, blockEdge, levels = struct.unpack_from('<3I', b, 0x18)
cellTableOff = struct.unpack_from('<Q', b, 0x68)[0]
watrOff, watrCount = struct.unpack_from('<Q', b, 0x50)[0], struct.unpack_from('<I', b, 0x34)[0]
cellsX = maxX - minX + 1
cellsY = maxY - minY + 1
print('ver', ver, 'cells', minX, minY, maxX, maxY, '=', cellsX, 'x', cellsY, 'spc', spc, 'levels', levels)

water = {}
for j in range(cellsY):
    for i in range(cellsX):
        o = cellTableOff + (j * cellsX + i) * 16
        mn, mx, wh = struct.unpack_from('<3f', b, o)
        wt, fl = struct.unpack_from('<2H', b, o + 12)
        water[(minX + i, minY + j)] = (mn, mx, wh, wt, fl)

# score every 12x12-cell (3x3 chunk) region aligned to chunk boundaries (cellX % 4 == 0)
best = []
for cy in range(minY, maxY - 11, 4):
    for cx in range(minX, maxX - 11, 4):
        n = wet = typed = 0
        hs = set()
        for j in range(cy, cy + 12):
            for i in range(cx, cx + 12):
                v = water.get((i, j))
                if not v:
                    continue
                n += 1
                if v[4] & 1:
                    wet += 1
                    hs.add(round(v[2], 3))
                    if v[3] != 0xFFFF:
                        typed += 1
        if n == 144:
            best.append((wet, typed, len(hs), cx, cy))
best.sort(reverse=True)
print('\n-- most water-bearing full 12x12 regions (wet cells, typed cells, distinct heights, cellX, cellY) --')
for r in best[:12]:
    print(r)
print('\n-- regions with MANY DISTINCT water heights (lakes+coast mix) --')
for r in sorted(best, key=lambda r: (-r[2], -r[0]))[:12]:
    print(r)
# named fixtures
for nm, (cx, cy) in {'sanctuary': (-20, 24), 'urban': (0, -12)}.items():
    n = wet = typed = 0
    hs = set()
    for j in range(cy, cy + 12):
        for i in range(cx, cx + 12):
            v = water.get((i, j))
            if not v:
                continue
            n += 1
            if v[4] & 1:
                wet += 1
                hs.add(round(v[2], 3))
                if v[3] != 0xFFFF:
                    typed += 1
    print(nm, (cx, cy), 'cells', n, 'wet', wet, 'typed', typed, 'heights', len(hs))
