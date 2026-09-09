import struct, sys, zlib
f = open(sys.argv[1], 'rb').read()
minX, minY, maxX, maxY = struct.unpack_from('<iiii', f, 8)
spc, be, levels = struct.unpack_from('<III', f, 0x18)
quant = struct.unpack_from('<f', f, 0x2C)[0]
off = struct.unpack_from('<QQQQQQQQQQ', f, 0x48)
oCell, oDir, oData = off[4], off[7], off[8]
cellsX, cellsY = maxX - minX + 1, maxY - minY + 1
gw, gh = cellsX * spc, cellsY * spc
H = [[0] * gw for _ in range(gh)]
idx = 0
for L in range(levels - 1, -1, -1):
    s = 1 << L
    bx, by = (cellsX + s - 1) // s, (cellsY + s - 1) // s
    for j in range(by):
        for i in range(bx):
            o, csz, usz = struct.unpack_from('<QII', f, oDir + idx * 16)
            idx += 1
            raw = zlib.decompress(f[o:o + csz])
            n = be * be if L == levels - 1 else be * be * 3 // 4
            v = struct.unpack_from('<%dH' % n, raw, 0)
            ox, oy = i * be, j * be
            k = 0
            if L == levels - 1:
                for y in range(be):
                    for x in range(be):
                        gy, gx = (oy + y) * s, (ox + x) * s
                        if gy < gh and gx < gw:
                            H[gy][gx] = v[k]
                        k += 1
            else:
                for y in range(0, be, 2):
                    for x in range(0, be, 2):
                        for (dy, dx) in ((0, 1), (1, 0), (1, 1)):
                            gy, gx = (oy + y + dy) * s, (ox + x + dx) * s
                            if gy < gh and gx < gw:
                                H[gy][gx] = v[k]
                            k += 1
d = open(sys.argv[2], 'rb').read()
h, w = struct.unpack_from('<II', d, 12)
po = 148 if d[84:88] == b'DX10' else 128
D = struct.unpack_from('<%dH' % (w * h), d, po)
fails = []


def check(name, cond):
    print("  %s %s" % ("ok  " if cond else "FAIL", name))
    if not cond:
        fails.append(name)


check("the .lodt grid and the heightmap are the same size (%dx%d)" % (w, h),
      (w, h) == (gw, gh))
n = 0
first = None
if (w, h) == (gw, gh):
    for y in range(gh):
        row = D[(gh - 1 - y) * gw:(gh - 1 - y) * gw + gw]   # the DDS is north-up
        for x in range(gw):
            if row[x] != H[y][x]:
                n += 1
                if first is None:
                    first = (minX + x // spc, minY + y // spc, x % spc, y % spc,
                             H[y][x], row[x])
landless = sum(1 for i in range(cellsX * cellsY)
               if not (struct.unpack_from('<H', f, oCell + i * 16 + 14)[0] & 2))
print("  %d of %d cells carry no LAND record" % (landless, cellsX * cellsY))
if first:
    print("  first difference: cell (%d,%d) col %d row %d  lodt %d  heightmap %d"
          % first)
check("the worldspace HAS landless cells, so this can fail", landless > 0)
check("every texel agrees with the shadow heightmap (%d differ)" % n, n == 0)
print("RESULT %s" % ("PASS" if not fails else "FAIL"))
sys.exit(1 if fails else 0)
