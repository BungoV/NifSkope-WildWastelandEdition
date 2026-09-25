"""C1: is the wall across the band LAND's own? The installed .lodl heights (cache h.npy, 32 samples a cell) vs EXTENT1's
LAND dumps (his MO2 order and vanilla Fallout4.esm; 33x33 VHGT grid per cell in 8-unit quanta; loader copied from
EXTENT1 census.py) at every sample of the cells across the west wall (x -78..-72 at y -5) and the north wall
(y 72..78 at x -50). Prints the cell-mean heights so the step itself is LAND's number."""
import struct, numpy as np
E = r'E:/Projects/NifskopeWWE-extent1/scratchpad/extent1_20260925/'
def load_dump(p):
    b = open(p, 'rb').read()
    mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
    pres = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
    g = np.frombuffer(b, '<i2', cw * ch * 33 * 33, 16 + cw * ch).reshape(ch, cw, 33, 33)
    return mnx, mny, pres, g
h = np.load('h.npy')                         # row 0 north; sample (r,c) of cell (cx,cy) = h[(95-cy)*32 + 31-r, (cx+96)*32 + c]
cells = [(x, -5) for x in range(-78, -71)] + [(-50, y) for y in range(78, 71, -1)]
for name in ('land_mo2.bin', 'land_vanilla.bin'):
    mnx, mny, pres, g = load_dump(E + name)
    mis = n = 0; rows = []
    for (cx, cy) in cells:
        L = g[cy - mny, cx - mnx].astype(np.float64) * 8          # row 0 = south, as the .lodl
        got = np.array([[h[(95 - cy) * 32 + 31 - r, (cx + 96) * 32 + c] for c in range(32)] for r in range(32)])
        d = np.abs(got - L[:32, :32]); mis += int((d > 0.5).sum()); n += d.size
        rows.append('(%d,%d) LAND mean %8.0f min %7.0f max %7.0f | .lodl mean %8.0f' % (cx, cy, L.mean(), L.min(), L.max(), got.mean()))
    print('%s: %d samples over %d wall cells vs the .lodl, %d mismatched (> 0.5 unit)' % (name, n, len(cells), mis))
    if name == 'land_mo2.bin':
        for r in rows: print('   ' + r)
