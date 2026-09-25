# Placements per cell from the installed .lodi (decoder = SEAM1 w2_cells.py arithmetic, same as placements.py).
import struct, mmap, collections
import numpy as np
F = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/'
def placement_cells(path=F + 'Commonwealth.lodi'):
    f = open(path, 'rb'); b = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert b[:4] == b'LODI'
    cw, cs, ce, cn = struct.unpack_from('<4h', b, 0x48)
    nchunk, ninst = struct.unpack_from('<II', b, 0x54)
    oCh, oCr, oIn = struct.unpack_from('<3Q', b, 0x68)
    wch = ce - cw + 1
    ch = np.frombuffer(b, np.uint32, nchunk * 8, oCh).reshape(-1, 8)
    ins = np.frombuffer(b, np.uint16, ninst * 12, oIn).reshape(-1, 12)
    cells = collections.Counter(); tot = 0
    for k in range(nchunk):
        first, cnt = int(ch[k, 0]), int(ch[k, 1])
        if not cnt: continue
        r, c = divmod(k, wch); chx = cw + c; chy = cn - r
        px = ins[first:first + cnt, 0].astype(np.float64) * 16384 / 65535 + chx * 16384
        py = ins[first:first + cnt, 1].astype(np.float64) * 16384 / 65535 + chy * 16384
        for x, y in zip(np.floor(px / 4096).astype(int), np.floor(py / 4096).astype(int)): cells[(x, y)] += 1
        tot += cnt
    assert tot == ninst, (tot, ninst)
    return cells
