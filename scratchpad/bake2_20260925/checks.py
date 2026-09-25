"""BAKE2 file-level checks for one worldspace bake (no renderer involved).
  1. Placement cells with no terrain under them: every .lodi placement mapped to its cell (EXTENT1/SEAM1 decoder
     arithmetic), against the LAND cells of the worldspace (lodgen --dump-land, the plugins' own LAND records).
     Each such cell is named with its placement count.
  2. Flat-grey terrain chunks: the VT.16 colour mosaic (vtread, the per-tile codec reader), per dim-4 chunk over its
     LAND cells' texels only: mean chroma (max-min of RGB) and luminance SD. Flat grey = chroma < 6 AND lum SD < 2.
     The same instrument is run as a refuter on a VT whose grey is known (--control <VT.16 file> <x0 y0 x1 y1>).
usage: python checks.py <lod dir> <EDID> <land.bin> [--control <VT.16.lodt> x0 y0 x1 y1]
"""
import sys, struct, mmap, collections
import numpy as np
sys.path.insert(0, r'E:/Projects/NifskopeWWE-bake2/scratchpad/seam1_20260925')
import vtread

def land_cells(fn):
    b = open(fn, 'rb').read()
    mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
    f = b[16:16 + cw * ch]
    return {(mnx + i % cw, mny + i // cw) for i, v in enumerate(f) if v}

def placement_cells(path):
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
    return cells, ninst, (cw, cs, ce, cn)

def grey_chunks(vtpath, cells, x0, y0, x1, y1, label):
    v = vtread.Vt(vtpath)
    m, wW, nN = v.mosaic(x0, y0, x1, y1, 1)
    per = m.shape[1] // ((m.shape[1] // v.content) * v.levelDim) if False else None
    upc = v.content // v.levelDim          # texels per cell at this level
    rgb = m[..., :3].astype(np.float32)
    chroma = rgb.max(-1) - rgb.min(-1)
    lum = rgb @ np.array([0.299, 0.587, 0.114], np.float32)
    chunks = collections.defaultdict(list)
    for (cx, cy) in cells:
        if not (x0 <= cx <= x1 and y0 <= cy <= y1): continue
        c0 = (cx - wW) * upc; r0 = (nN - 1 - cy) * upc
        chunks[(cx // 4 * 4, cy // 4 * 4)].append((r0, c0))
    grey = []; n = 0
    for k, lst in sorted(chunks.items()):
        ch = np.concatenate([chroma[r:r + upc, c:c + upc].ravel() for r, c in lst])
        lu = np.concatenate([lum[r:r + upc, c:c + upc].ravel() for r, c in lst])
        n += 1
        if ch.mean() < 6 and lu.std() < 2:
            grey.append((k, round(float(ch.mean()), 2), round(float(lu.std()), 2), round(float(lu.mean()), 1), len(lst)))
    print('%s: flat-grey dim-4 chunks %d of %d LAND chunks (chroma < 6 and lum SD < 2, VT.16 %s, %d texels/cell)'
          % (label, len(grey), n, vtpath.replace('\\', '/').split('/')[-1], upc))
    for g in grey[:40]:
        print('   grey chunk %s: chroma %.2f lumSD %.2f lum %.1f over %d LAND cells' % g)
    allch = chroma.mean(); print('   whole mosaic: mean chroma %.2f, lum mean %.1f sd %.1f' % (allch, lum.mean(), lum.std()))
    return grey

def nonland_grey(vtpath, land, label):
    """Every VT cell with NO LAND under it (a worldspace whose VT box is wider than its LAND; never the case on the
    Commonwealth). Per cell: mean chroma; flat grey = chroma < 6. The vanilla fill blends such a cell from the
    generator's own colour toward vanilla by a smoothstep of its distance to painted LAND (lodgen.cpp, the fill's
    band loop), so the ring next to LAND keeps the generator's grey. Reported with each cell's distance to LAND."""
    v = vtread.Vt(vtpath)
    m, wW, nN = v.mosaic(v.west, v.south, v.east, v.north, 1)
    upc = v.content // v.levelDim
    rgb = m[..., :3].astype(np.float32)
    chroma = rgb.max(-1) - rgb.min(-1)
    lum = rgb @ np.array([0.299, 0.587, 0.114], np.float32)
    lx = np.array([c[0] for c in land]); ly = np.array([c[1] for c in land])
    grey = collections.Counter(); n = 0; worst = []
    for cy in range(v.south, v.north + 1):
        for cx in range(v.west, v.east + 1):
            if (cx, cy) in land: continue
            r0 = (nN - 1 - cy) * upc; c0 = (cx - wW) * upc
            if r0 < 0 or c0 < 0 or r0 + upc > chroma.shape[0] or c0 + upc > chroma.shape[1]: continue
            n += 1
            ch = float(chroma[r0:r0 + upc, c0:c0 + upc].mean())
            if ch < 6:
                d = int(np.max(np.stack([abs(lx - cx), abs(ly - cy)]), 0).min())
                grey[d] += 1
                worst.append((cx, cy, d, round(ch, 2), round(float(lum[r0:r0 + upc, c0:c0 + upc].mean()), 1)))
    print('%s: no-LAND VT cells %d; flat grey (chroma < 6) %d; by distance to LAND (cells) %s'
          % (label, n, sum(grey.values()), dict(sorted(grey.items()))))
    for w in worst[:12]:
        print('   grey no-LAND cell %d,%d dist %d chroma %.2f lum %.1f' % w)
    return worst


if __name__ == '__main__':
    D, E, LB = sys.argv[1:4]
    land = land_cells(LB)
    cells, ninst, grid = placement_cells(D + '/' + E + '.lodi')
    off = sorted((k, n) for k, n in cells.items() if k not in land)
    print('%s: %d placements in %d cells (lodi chunk grid %s); LAND cells %d' % (E, ninst, len(cells), grid, len(land)))
    print('%s: placement cells with NO LAND under them: %d cells, %d placements' % (E, len(off), sum(n for _, n in off)))
    for (x, y), n in off:
        print('   no-LAND cell %d,%d: %d placements' % (x, y, n))
    xs = [c[0] for c in land]; ys = [c[1] for c in land]
    import glob, re
    vts = sorted(glob.glob(D + '/' + E + '.VT.*.lodt'), key=lambda f: int(re.search(r'VT\.(\d+)\.', f).group(1)))
    vt = [f for f in vts if f.endswith('.VT.16.lodt')] or vts[-1:]   # VT.16 where the ladder reaches it, else its coarsest
    grey_chunks(vt[0], land, min(xs), min(ys), max(xs), max(ys), E)
    nonland_grey(vt[0], land, E)
    if '--control' in sys.argv:
        i = sys.argv.index('--control'); p = sys.argv[i + 1]; b = list(map(int, sys.argv[i + 2:i + 6]))
        allc = {(x, y) for x in range(b[0], b[2] + 1) for y in range(b[1], b[3] + 1)}
        grey_chunks(p, allc, *b, 'CONTROL')
