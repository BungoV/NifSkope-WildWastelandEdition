"""G4: the ground-cover channel of a VT level (the mask sheet's A, role 5), measured over named cells.
Container read by tests/spells/lodgen_vt_check.py's Lodv (one reader per format), the file mapped with mmap
so a 12 GB level is not read into memory. Georef = lodgen_slab_mask.py's (row 0 north; tile (tx,ty) has its
north-west cell at x = west + tx*d, y = north - ty*d).
usage: g4_cover.py <Commonwealth.VT.<d>.lodt> cx,cy [cx,cy ...]
Prints the mask format, then per cell: texel count, min, max, mean, distinct values, share of texels > 0.
A cell whose cover is ABSENT (a BC1 mask, no alpha) or UNIFORM (one value) is the red answer."""
import sys, os, mmap, struct, importlib.util

REPO = 'E:/Projects/NifskopeWWE-bake2'
spec = importlib.util.spec_from_file_location('vtc', REPO + '/tests/spells/lodgen_vt_check.py')
vtc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vtc)


class _Mapped(object):
    def __init__(self, path, mode):
        self.f = open(path, mode)
    def __enter__(self):
        return self
    def __exit__(self, *a):
        return False
    def read(self):
        return mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)


vtc.open = _Mapped   # Lodv.__init__ does `with open(path,'rb') as f: self.b = f.read()`


def bc3_alpha(p, o, w, h):
    """The alpha of a BC3 surface, rows of ints (BC4-style 8-byte block before each BC1 block)."""
    rows = [[0] * w for _ in range(h)]
    bw = w // 4
    for by in range(h // 4):
        for bx in range(bw):
            b = o + (by * bw + bx) * 16
            a0, a1 = p[b], p[b + 1]
            bits = int.from_bytes(bytes(p[b + 2:b + 8]), 'little')
            if a0 > a1:
                pal = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
            else:
                pal = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
            for k in range(16):
                rows[by * 4 + k // 4][bx * 4 + k % 4] = pal[(bits >> (3 * k)) & 7]
    return rows


def main(argv):
    path = argv[0]
    v = vtc.Lodv(path)
    ms = [i for i in range(v.sheetCount) if v.sheets[i]['role'] == 5]
    if not ms:
        print('RED: no role-5 mask sheet in', path); return 1
    ms = ms[0]
    sd = v.sheets[ms]
    print('%s: levelDim %d tiles %dx%d (%d entries) content %d border %d stored %d coverNorm %.3f tint %.3f'
          % (os.path.basename(path), v.levelDim, v.tilesX, v.tilesY, v.tileCount, v.content, v.border, v.stored,
             v.coverNorm, v.tintStrength))
    print('  mask sheet %d: dxgi %d, dxgiCover %d (77/78 = BC3, the cover in A; 71/72 = BC1, no alpha)'
          % (ms, sd['dxgi'], sd['dxgiCover']))
    u = v.levelDim * 4096.0 / v.content
    red = 0
    for spec in argv[1:]:
        cx, cy = (int(t) for t in spec.split(','))
        tx = (cx - v.west) // v.levelDim
        # the tile's north-west cell y is north - ty*d, and it spans d cells southward
        ty = (v.north - cy) // v.levelDim
        i = ty * v.tilesX + tx
        e = v.table[i]
        if not (e['flags'] & 1):
            print('  cell %d,%d: tile %d,%d not present -> RED' % (cx, cy, tx, ty)); red += 1; continue
        cover = bool(e['flags'] & 2)
        fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
        if fmt not in (77, 78):
            print('  cell %d,%d: tile %d,%d flags 0x%x, mask format dxgi %d carries NO cover channel -> RED (absent)'
                  % (cx, cy, tx, ty, e['flags'], fmt)); red += 1; continue
        p = v.payload(i)
        o = v.sheetOffset(cover, ms, 0)
        rows = bc3_alpha(p, o, v.stored, v.stored)
        cellX0 = v.west + tx * v.levelDim
        cellYn = v.north - ty * v.levelDim
        vals = []
        for j in range(v.border, v.border + v.content):
            wy = (cellYn + 1) * 4096.0 - (j - v.border + 0.5) * u
            if not (cy * 4096.0 <= wy < (cy + 1) * 4096.0):
                continue
            for k in range(v.border, v.border + v.content):
                wx = cellX0 * 4096.0 + (k - v.border + 0.5) * u
                if cx * 4096.0 <= wx < (cx + 1) * 4096.0:
                    vals.append(rows[j][k])
        n = len(vals)
        mean = sum(vals) / float(n)
        sd2 = (sum((x - mean) ** 2 for x in vals) / n) ** 0.5
        distinct = len(set(vals))
        verdict = 'ok (present, non-uniform)' if distinct > 1 else 'RED (uniform)'
        if distinct <= 1:
            red += 1
        print('  cell %d,%d: tile %d,%d flags 0x%x, %d texels, min %d max %d mean %.2f sd %.2f distinct %d, >0 %.3f -> %s'
              % (cx, cy, tx, ty, e['flags'], n, min(vals), max(vals), mean, sd2, distinct,
                 sum(1 for x in vals if x > 0) / float(n), verdict))
    print('G4 %s: %d red cell(s) of %d' % ('PASS' if red == 0 else 'RED', red, len(argv) - 1))
    return 0 if red == 0 else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
