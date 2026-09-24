#!/usr/bin/env python
"""The mask sheet's B channel (the far terrain's AO texel), read
out of a `.lodt` container and averaged over rectangles given IN WORLD UNITS.

ONE READER PER FORMAT: the container header and the BC1 block decoder are
imported from `tests/spells/lodgen_vt_check.py` (`Lodv`, `decode_bc1`), never
re-implemented here.

    usage: lodgen_slab_mask.py <finest.lodt> [name=x0,y0,x1,y1 ...]
           lodgen_slab_mask.py <finest.lodt> --lattice <OBJH dump> [rects...]

    It is `tests/spells/lodgen_slab.sh`'s reader (lane SLAB1, 2026-09-18)
    and prints one line per rectangle: `n <count> mean <mean> min <a> max <b>`.

The mask sheet is role 5 (RMAOS): R roughness, G metallic, **B the sky AO**,
A ground cover (docs/LODGEN_TERRAIN_VT.md, the mask-sheet paragraph). At
`compression 0` and no cover the payload is BC1, so B carries 5 bits and the
values it can take are 0, 8, 16, ... 255 -- a mean is fine, a single texel is
coarse, and that is stated beside every number this prints.

GEOREF (the mapping this script asserts and CHECKS against the object lattice):
tile index `ty * tilesX + tx`, **row 0 is NORTH** (lodgen_vt_check cmd_georef
V20). Tile (tx,ty) covers cells x = west + tx*levelDim, y = north - ty*levelDim.
A stored texel (i,j) is content texel (i-border, j-border); its centre is
    world x = cellX*4096 + (i - border + 0.5) * u
    world y = (cellY+1)*4096 - (j - border + 0.5) * u
with u = levelDim * 4096 / content units a texel.
"""
import os
import struct
import sys
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
_spec = importlib.util.spec_from_file_location(
    'vtc', os.path.join(REPO, 'tests', 'spells', 'lodgen_vt_check.py'))
vtc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(vtc)


def mask_b(v, index):
    """The B channel of one tile's MASK sheet (role 5), mip 0, as rows of ints.
    `stored` x `stored`, border included."""
    e = v.table[index]
    p = v.payload(index)
    if p is None:
        return None
    ms = None
    for s in range(v.sheetCount):
        if v.sheets[s]['role'] == 5:
            ms = s
    if ms is None:
        raise SystemExit('no role-5 mask sheet in %s' % v.path)
    fmt = v.sheets[ms]['dxgiCover'] if (bool(e['flags'] & 2)
                                        and v.sheets[ms]['dxgiCover'] != v.sheets[ms]['dxgi']) \
        else v.sheets[ms]['dxgi']
    o = v.sheetOffset(bool(e['flags'] & 2), ms, 0)
    if fmt in (77, 78):        # BC3: the 8-byte BC4 alpha block, then a BC1 block
        raise SystemExit('BC3 mask (dxgi %d) -- this bake carries cover; not handled' % fmt)
    rgb = vtc.decode_bc1(p, o, v.stored, v.stored)
    return [[px[2] for px in row] for row in rgb]


class Sheet(object):
    def __init__(self, path):
        self.v = vtc.Lodv(path)
        v = self.v
        self.u = v.levelDim * 4096.0 / v.content
        self.tiles = {}
        for ty in range(v.tilesY):
            for tx in range(v.tilesX):
                i = ty * v.tilesX + tx
                if v.table[i]['flags'] & 1:
                    self.tiles[(tx, ty)] = mask_b(v, i)

    def samples(self, x0, y0, x1, y1):
        """Every CONTENT texel whose centre lies in the world rectangle."""
        v, u = self.v, self.u
        out = []
        for (tx, ty), rows in self.tiles.items():
            cellX = v.west + tx * v.levelDim
            cellY = v.north - ty * v.levelDim
            for j in range(v.border, v.border + v.content):
                wy = (cellY + 1) * 4096.0 - (j - v.border + 0.5) * u
                if not (y0 <= wy <= y1):
                    continue
                row = rows[j]
                for i in range(v.border, v.border + v.content):
                    wx = cellX * 4096.0 + (i - v.border + 0.5) * u
                    if x0 <= wx <= x1:
                        out.append(row[i])
        return out

    def all_content(self):
        v = self.v
        out = []
        for rows in self.tiles.values():
            for j in range(v.border, v.border + v.content):
                out.extend(rows[j][v.border:v.border + v.content])
        return out

    def square_means(self, cell=128.0):
        """Mean B per world-aligned `cell`-unit square -- the grid the object
        height field itself is indexed on, so the two can be compared."""
        v, u = self.v, self.u
        acc = {}
        for (tx, ty), rows in self.tiles.items():
            cellX = v.west + tx * v.levelDim
            cellY = v.north - ty * v.levelDim
            for j in range(v.border, v.border + v.content):
                wy = (cellY + 1) * 4096.0 - (j - v.border + 0.5) * u
                gy = int((wy // cell))
                row = rows[j]
                for i in range(v.border, v.border + v.content):
                    wx = cellX * 4096.0 + (i - v.border + 0.5) * u
                    gx = int((wx // cell))
                    a = acc.setdefault((gx, gy), [0, 0])
                    a[0] += row[i]
                    a[1] += 1
        return dict((k, a[0] / float(a[1])) for k, a in acc.items())


def read_objh(path):
    """The `--dump-object-ao` lattice. `OBJH`, int32 gx0 gy0 gw gh, float cell,
    then gw*gh floats of max Z. A v2 dump (lane SLAB1) appends the SAME
    gw*gh floats of MIN Z after them; older dumps stop at the max plane."""
    b = open(path, 'rb').read()
    assert b[0:4] == b'OBJH', b[0:4]
    gx0, gy0, gw, gh = struct.unpack_from('<4i', b, 4)
    cell = struct.unpack_from('<f', b, 20)[0]
    n = gw * gh
    mx = struct.unpack_from('<%df' % n, b, 24)
    mn = None
    if len(b) >= 24 + n * 8:
        mn = struct.unpack_from('<%df' % n, b, 24 + n * 4)
    return dict(gx0=gx0, gy0=gy0, gw=gw, gh=gh, cell=cell, maxz=mx, minz=mn,
                bytes=len(b))


def main(argv):
    if not argv:
        raise SystemExit(__doc__)
    path = argv[0]
    rest = argv[1:]
    lattice = None
    if rest and rest[0] == '--lattice':
        lattice = read_objh(rest[1])
        rest = rest[2:]
    sh = Sheet(path)
    v = sh.v
    print('sheet %s' % path)
    print('  levelDim %d tiles %dx%d present %d content %d border %d stored %d '
          'cells x %d..%d y %d..%d  -> %.1f world units a texel'
          % (v.levelDim, v.tilesX, v.tilesY, len(sh.tiles), v.content, v.border,
             v.stored, v.west, v.east, v.south, v.north, sh.u))
    ms = [i for i in range(v.sheetCount) if v.sheets[i]['role'] == 5][0]
    print('  mask sheet index %d role 5 dxgi %d (BC1: B carries 5 bits, so a '
          'single texel is one of 32 steps)' % (ms, v.sheets[ms]['dxgi']))
    whole = sh.all_content()
    print('  (b) WHOLE CHUNK, every content texel: n %d mean %.3f min %d max %d'
          % (len(whole), sum(whole) / float(len(whole)), min(whole), max(whole)))
    for spec in rest:
        name, _, r = spec.partition('=')
        x0, y0, x1, y1 = [float(t) for t in r.split(',')]
        s = sh.samples(x0, y0, x1, y1)
        if not s:
            print('  %-14s rect x %.0f..%.0f y %.0f..%.0f -- NO TEXEL, refused'
                  % (name, x0, x1, y0, y1))
            continue
        print('  %-14s rect x %.0f..%.0f y %.0f..%.0f (%.0f x %.0f units): '
              'n %d mean %.3f min %d max %d'
              % (name, x0, x1, y0, y1, x1 - x0, y1 - y0, len(s),
                 sum(s) / float(len(s)), min(s), max(s)))
    if lattice is not None:
        # THE GEOREF CONTROL. The lattice is in world units straight from the
        # ESM walk, so it is an INDEPENDENT ruler for the mapping above: if the
        # mapping is right, squares with a high object top are the dark ones,
        # and shifting the mapping by one cell must destroy that.
        sq = sh.square_means(lattice['cell'])
        L = lattice
        def corr(shift_cells):
            xs, ys = [], []
            for (gx, gy), m in sq.items():
                lx = gx - L['gx0'] + shift_cells * 32
                ly = gy - L['gy0']
                if 0 <= lx < L['gw'] and 0 <= ly < L['gh']:
                    t = L['maxz'][ly * L['gw'] + lx]
                    if t > -1.0e29:
                        xs.append(t); ys.append(m)
            if len(xs) < 50:
                return (len(xs), float('nan'))
            n = float(len(xs))
            mx, my = sum(xs) / n, sum(ys) / n
            sxy = sum((a - mx) * (b - my) for a, b in zip(xs, ys))
            sxx = sum((a - mx) ** 2 for a in xs)
            syy = sum((b - my) ** 2 for b in ys)
            return (len(xs), sxy / ((sxx * syy) ** 0.5) if sxx and syy else float('nan'))
        n0, r0 = corr(0)
        n1, r1 = corr(1)
        print('  GEOREF CONTROL: object top vs mask B over %d occupied squares, '
              'r = %+.4f; shifted one CELL east, %d squares, r = %+.4f'
              % (n0, r0, n1, r1))
        print('  lattice %s: origin %d,%d grid %dx%d cell %.1f minZ plane %s'
              % (os.path.basename(argv[rest and 0 or 0] if False else 'OBJH'),
                 L['gx0'], L['gy0'], L['gw'], L['gh'], L['cell'],
                 'PRESENT' if L['minz'] is not None else 'absent (v1 dump)'))


if __name__ == '__main__':
    main(sys.argv[1:])
