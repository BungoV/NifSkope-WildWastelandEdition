"""W2: cells with .lodi placements minus cells with terrain. Terrain = (a) .lodl geometry (every cell of its extent),
(b) PAINTED terrain = a LAND quadrant with a BTXT or an ATXT layer (Fallout4.esm; the 6 DLC LAND cells lie inside).
Placement cell = floor(world xy / 4096) from the chunk index + the u16 position (chunk box 16384 u, row 0 = north)."""
import struct, mmap, pickle, numpy as np, collections
P = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.lodi'
f = open(P, 'rb'); b = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
assert b[:4] == b'LODI'
cw, cs, ce, cn = struct.unpack_from('<4h', b, 0x48)
nchunk, ninst = struct.unpack_from('<II', b, 0x54)
oCh, oCr, oIn = struct.unpack_from('<3Q', b, 0x68)
w = ce - cw + 1; n = cn - cs + 1; assert nchunk == w * n
ch = np.frombuffer(b, np.uint32, nchunk * 8, oCh).reshape(-1, 8)
ins = np.frombuffer(b, np.uint16, ninst * 12, oIn).reshape(-1, 12)
cells = collections.Counter(); tot = 0
for k in range(nchunk):
    first, cnt = int(ch[k, 0]), int(ch[k, 1])
    if not cnt: continue
    r, c = divmod(k, w); chx = cw + c; chy = cn - r          # north-up rows
    x0 = chx * 16384; y0 = chy * 16384
    px = ins[first:first + cnt, 0].astype(np.float64) * 16384 / 65535 + x0
    py = ins[first:first + cnt, 1].astype(np.float64) * 16384 / 65535 + y0
    for cx, cy in zip(np.floor(px / 4096).astype(int), np.floor(py / 4096).astype(int)): cells[(cx, cy)] += 1
    tot += cnt
assert tot == ninst, (tot, ninst)
# refuter: every placement's cell must lie inside its own chunk
bad = sum(1 for (cx, cy) in cells if False)
d = pickle.load(open('fo4esm_cw.pkl', 'rb')); lands = d['lands']
painted = {k for k, L in lands.items() if any(L['base']) or any(len(q) for q in L['layers'])}
P_ = set(cells)
print('placements %d in %d cells; .lodl terrain covers all 192x192 cells (extent -96..95, all 36864 cell records flag 3)' % (ninst, len(P_)))
print('placement cells minus .lodl cells: 0 by extent (min/max placement cell x %d..%d y %d..%d)' % (
    min(k[0] for k in P_), max(k[0] for k in P_), min(k[1] for k in P_), max(k[1] for k in P_)))
nop = P_ - painted
print('placement cells minus PAINTED cells: %d cells, %d placements (%.1f%% of all)' % (
    len(nop), sum(cells[k] for k in nop), 100.0 * sum(cells[k] for k in nop) / ninst))
inf = [k for k in nop if -64 <= k[0] <= 31 and -48 <= k[1] <= 47]
print('   of which inside the overview frame: %d cells, %d placements' % (len(inf), sum(cells[k] for k in inf)))
print('   top cells:', sorted(((cells[k], k) for k in nop), reverse=True)[:10])
print('painted cells %d; painted bounds x %d..%d y %d..%d' % (len(painted), min(k[0] for k in painted), max(k[0] for k in painted),
      min(k[1] for k in painted), max(k[1] for k in painted)))
pickle.dump({'cells': dict(cells), 'painted': painted}, open('w2_cells.pkl', 'wb'))
