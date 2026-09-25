# Placement cells vs terrain: the installed .lodi's placements binned to cells (decoder = SEAM1 w2_cells.py, same
# arithmetic), counted against (a) the .lodl extent, (b) a render frame, (c) PAINTED LAND (BTXT/ATXT, SEAM1's pickle
# of Fallout4.esm). usage: python placements.py [x0 y0 x1 y1 ...]  (frames, cells inclusive)
import sys, struct, mmap, pickle, collections
import numpy as np
sys.path.insert(0, r'E:/Projects/NifskopeWWE-extent1/tests/spells')
from lodl_open_authority import Lodt
F = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/'
f = open(F + 'Commonwealth.lodi', 'rb'); b = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
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
    r, c = divmod(k, w); chx = cw + c; chy = cn - r
    px = ins[first:first + cnt, 0].astype(np.float64) * 16384 / 65535 + chx * 16384
    py = ins[first:first + cnt, 1].astype(np.float64) * 16384 / 65535 + chy * 16384
    for x, y in zip(np.floor(px / 4096).astype(int), np.floor(py / 4096).astype(int)): cells[(x, y)] += 1
    tot += cnt
assert tot == ninst, (tot, ninst)
xs = [k[0] for k in cells]; ys = [k[1] for k in cells]
print('placements %d in %d cells; placement cell bounds x %d..%d y %d..%d' % (ninst, len(cells), min(xs), max(xs), min(ys), max(ys)))
L = Lodt(F + 'Commonwealth.lodl')
out = [(k, v) for k, v in cells.items() if not (L.minX <= k[0] <= L.maxX and L.minY <= k[1] <= L.maxY)]
print('.lodl extent [%d,%d]..[%d,%d]: placement cells outside it %d (%d placements)' % (L.minX, L.minY, L.maxX, L.maxY, len(out), sum(v for _, v in out)))
d = pickle.load(open(r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/fo4esm_cw.pkl', 'rb'))
painted = {k for k, Ld in d['lands'].items() if any(Ld['base']) or any(len(q) for q in Ld['layers'])}
unp = [(k, v) for k, v in cells.items() if k not in painted]
print('placement cells on UNPAINTED LAND (no BTXT/ATXT in Fallout4.esm): %d cells, %d placements' % (len(unp), sum(v for _, v in unp)))
a = sys.argv[1:]
for i in range(0, len(a) - 3, 4):
    x0, y0, x1, y1 = map(int, a[i:i + 4])
    o = [(k, v) for k, v in cells.items() if not (x0 <= k[0] <= x1 and y0 <= k[1] <= y1)]
    print('frame [%d,%d]..[%d,%d]: placement cells outside the terrain region %d (%d placements)' % (x0, y0, x1, y1, len(o), sum(v for _, v in o)))
