#!/usr/bin/env python3
"""WATER1 continuation: measure the three open items of the 12:19 landing
offline, from a .lodl's own bytes (no NifSkope launch).

    python openitems.py <file.lodl> poke   [x0 y0 x1 y1 n]   (default Boston -6 -10 3 -3, 8 a cell)
    python openitems.py <file.lodl> v2     [x0 y0 x1 y1]
    python openitems.py <file.lodl> sea

poke : wet texels where the viewer's coarse terrain mesh is above the water,
       with the viewer's own mesh rule (point samples every spc/n, two
       triangles SW-SE-NE / SW-NE-NW); CONTROL = the viewer's logged counts
       (A_bodyid.log: 8415 wet, 0 full-rate above, 212 mesh above). Then two
       candidate viewer fixes, simulated on the same grid.
v2   : the cells the v2 path draws (water height above the cell's lowest
       ground), CONTROL = V2_default.log "22 cells drawn". In a v3 file bit 0
       is cleared on dry cells, so the flag is not used here.
sea  : body 1's cells, split by the land flag and by where they sit.

The body-ID plane container is decoded here from docs/LODGEN_BTD_FORMAT.md,
not from src/lodtfile.cpp; heights come through tests/spells' independent
reader.
"""
import os
import struct
import sys
import zlib

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
from lodl_open_authority import Lodt  # noqa: E402


class Water:
    def __init__(self, d):
        f = d.f
        f.seek(0)
        h = f.read(0x100)
        self.d = d
        (self.bodyOff,) = struct.unpack_from('<Q', h, 0xA0)
        self.bodyCount, self.bodyBytes = struct.unpack_from('<II', h, 0xA8)
        (self.bodyS,) = struct.unpack_from('<I', h, 0xBC)
        (self.planeOff,) = struct.unpack_from('<Q', h, 0xC0)
        f.seek(self.bodyOff)
        raw = f.read(self.bodyCount * self.bodyBytes)
        self.bodyH = {}
        self.bodyCls = {}
        for i in range(self.bodyCount):
            r = raw[i * self.bodyBytes:(i + 1) * self.bodyBytes]
            bid, cls = struct.unpack_from('<HB', r, 0)
            if bid != i + 1:
                raise SystemExit('body record %d has id %d' % (i, bid))
            self.bodyCls[bid] = cls
            self.bodyH[bid] = struct.unpack_from('<f', r, 4)[0]
        f.seek(self.planeOff)
        self.tX, self.tY, self.edge, self.bps = struct.unpack('<4I', f.read(16))
        self.dirOff, self.dataOff = struct.unpack('<QQ', f.read(16))
        if self.edge != self.bodyS or self.bps != 2:
            raise SystemExit('body plane container: edge %d bps %d' % (self.edge, self.bps))
        f.seek(self.dirOff)
        self.dir = np.frombuffer(f.read(self.tX * self.tY * 16), dtype=[('o', '<u8'), ('c', '<u4'), ('u', '<u4')])

    def tile(self, cx, cy):
        """uint16 [edge, edge], row 0 south; or an int for a uniform tile."""
        t = (cy - self.d.minY) * self.tX + (cx - self.d.minX)
        e = self.dir[t]
        if e['c'] == 0:
            return int(e['u'])
        self.d.f.seek(int(e['o']))
        raw = zlib.decompress(self.d.f.read(int(e['c'])))
        return np.frombuffer(raw, dtype='<u2').reshape(self.edge, self.edge)

    def ids(self, x0, y0, x1, y1):
        e = self.edge
        out = np.zeros(((y1 - y0 + 1) * e, (x1 - x0 + 1) * e), dtype=np.uint16)
        for cy in range(y0, y1 + 1):
            for cx in range(x0, x1 + 1):
                t = self.tile(cx, cy)
                out[(cy - y0) * e:(cy - y0 + 1) * e, (cx - x0) * e:(cx - x0 + 1) * e] = t
        return out


def heights(d, gx0, gy0, w, h, step):
    lastX = ((d.cellsX * d.spc - 1) // step) * step
    lastY = ((d.cellsY * d.spc - 1) // step) * step
    z = np.zeros((h, w), dtype=np.float64)
    for j in range(h):
        gy = min(gy0 + j * step, lastY)
        for i in range(w):
            z[j, i] = d.height(min(gx0 + i * step, lastX), gy)
    return z


def mesh_at(Z, u, v):
    """The viewer's meshHeight: u, v in mesh-grid units (arrays)."""
    gH, gW = Z.shape
    i = np.clip(np.floor(u).astype(int), 0, gW - 2)
    j = np.clip(np.floor(v).astype(int), 0, gH - 2)
    fx = np.clip(u - i, 0, 1)
    fy = np.clip(v - j, 0, 1)
    z00, z10, z01, z11 = Z[j, i], Z[j, i + 1], Z[j + 1, i], Z[j + 1, i + 1]
    a = z00 + fx * (z10 - z00) + fy * (z11 - z10)
    b = z00 + fy * (z01 - z00) + fx * (z11 - z01)
    return np.where(fx >= fy, a, b)


def poke(d, w, x0, y0, x1, y1, n):
    spc, bs = d.spc, w.bodyS
    step = spc // n
    ids = w.ids(x0, y0, x1, y1)
    H, W = ids.shape
    fullStep = max(1, spc // bs)
    gx0, gy0 = (x0 - d.minX) * spc, (y0 - d.minY) * spc
    G = heights(d, gx0, gy0, W, H, fullStep)          # full-rate ground at every texel
    Z = heights(d, gx0, gy0, (x1 - x0 + 1) * n + 1, (y1 - y0 + 1) * n + 1, step)   # the view mesh
    wh = np.zeros_like(G)
    wet = ids > 0
    for b in np.unique(ids[wet]):
        wh[ids == b] = w.bodyH[int(b)]
    vv, uu = np.mgrid[0:H, 0:W]
    U = uu * (n / bs)
    V = vv * (n / bs)                                   # texel centre in mesh units
    M = mesh_at(Z, U, V)
    aboveFull = int(np.sum(wet & (G >= wh)))
    aboveMesh = int(np.sum(wet & (M > wh)))
    print('poke: region [%d,%d]..[%d,%d], mesh %d a cell, body plane %d a cell' % (x0, y0, x1, y1, n, bs))
    print('  CONTROL wet %d (viewer 8415), full-rate ground at/above water %d (viewer 0), '
          'mesh above water %d (viewer 212)' % (int(wet.sum()), aboveFull, aboveMesh))
    pk = wet & (M > wh)
    if pk.any():
        by = sorted(((int(b), int(np.sum(pk & (ids == b)))) for b in np.unique(ids[pk])), key=lambda t: -t[1])
        print('  poke-through by body: %s' % ', '.join('body %d (%s, %.0f u) %d' % (b, 'sea river lake'.split()[w.bodyCls[b]],
                                                                                   w.bodyH[b], c) for b, c in by))
        print('  largest mesh-over-water %.1f u, median %.1f u' % (float((M - wh)[pk].max()), float(np.median((M - wh)[pk]))))
    # Candidate fixes (viewer only). Each mesh vertex owns the texels of the
    # (up to) four mesh squares round it; a texel is "under" a vertex when it
    # lies within one mesh spacing of it.
    r = bs // n                                          # texels per mesh spacing
    gH, gW = Z.shape
    capA = np.full(Z.shape, np.inf)                      # A: min of (water - 1) over wet texels round the vertex
    capB = np.full(Z.shape, np.inf)                      # B: A, only round a texel that pokes
    for j in range(gH):
        v0, v1 = max(0, (j - 1) * r), min(H, (j + 1) * r + 1)
        for i in range(gW):
            u0, u1 = max(0, (i - 1) * r), min(W, (i + 1) * r + 1)
            wsub = wet[v0:v1, u0:u1]
            if not wsub.any():
                continue
            cap = float((wh[v0:v1, u0:u1][wsub]).min()) - 1.0
            capA[j, i] = cap
            if pk[v0:v1, u0:u1].any():
                capB[j, i] = cap
    # C: each poking texel lowers the three corners of ITS triangle by its own
    # excess (+1 u); a corner takes the largest excess asking. A uniform shift of
    # all three corners moves the interpolated height by exactly that shift.
    dropC = np.zeros(Z.shape)
    pv, pu = np.nonzero(pk)
    for v, u in zip(pv, pu):
        uf, vf = u * (n / bs), v * (n / bs)
        i = min(max(int(np.floor(uf)), 0), gW - 2)
        j = min(max(int(np.floor(vf)), 0), gH - 2)
        fx, fy = uf - i, vf - j
        tri = ((j, i), (j, i + 1), (j + 1, i + 1)) if fx >= fy else ((j, i), (j + 1, i + 1), (j + 1, i))
        e = float(M[v, u] - wh[v, u]) + 1.0
        for (a, b) in tri:
            dropC[a, b] = max(dropC[a, b], e)
    for name, cap in (('A (every vertex next to water)', capA), ('B (only vertices next to a poke)', capB),
                      ('C (a poke lowers its own triangle by its excess)', Z - dropC)):
        Z2 = np.minimum(Z, cap)
        M2 = mesh_at(Z2, U, V)
        moved = Z2 < Z
        dry = ~wet
        drop = M - M2
        print('  fix %s: vertices lowered %d of %d (max %.1f u, median %.1f u); poke-through after %d; '
              'dry texels whose drawn ground dropped %d of %d (max %.1f u)'
              % (name, int(moved.sum()), Z.size, float((Z - Z2)[moved].max()) if moved.any() else 0.0,
                 float(np.median((Z - Z2)[moved])) if moved.any() else 0.0,
                 int(np.sum(wet & (M2 > wh))), int(np.sum(dry & (drop > 1e-3))), int(dry.sum()),
                 float(drop[dry].max()) if dry.any() else 0.0))


def v2(d, x0, y0, x1, y1):
    drawn = []
    for cy in range(y0, y1 + 1):
        for cx in range(x0, x1 + 1):
            lo, hi, wh, wt, fl = d.cell(cx, cy)
            if wh > lo:
                drawn.append((cx, cy, lo, hi, wh, fl))
    print('v2: region [%d,%d]..[%d,%d]: %d cells with water above their lowest ground (CONTROL: V2 log 22)'
          % (x0, y0, x1, y1, len(drawn)))
    for cx, cy, lo, hi, wh, fl in drawn:
        edge = []
        if cx == x0: edge.append('W')
        if cx == x1: edge.append('E')
        if cy == y0: edge.append('S')
        if cy == y1: edge.append('N')
        print('  cell %3d,%3d lo %8.1f hi %8.1f water %6.1f flags %d%s'
              % (cx, cy, lo, hi, wh, fl, ('  region edge ' + ''.join(edge)) if edge else ''))


def sea(d, w):
    x0, y0, x1, y1 = d.minX, d.minY, d.maxX, d.maxY
    tot = land = noland = submerged = 0
    rows = {}
    ext = {'land': [10**9, 10**9, -10**9, -10**9]}
    for cy in range(y0, y1 + 1):
        for cx in range(x0, x1 + 1):
            lo, hi, wh, wt, fl = d.cell(cx, cy)
            if fl & 2:
                e = ext['land']
                e[0], e[1], e[2], e[3] = min(e[0], cx), min(e[1], cy), max(e[2], cx), max(e[3], cy)
    lx0, ly0, lx1, ly1 = ext['land']
    where = {}
    groundNoLand = []
    for cy in range(y0, y1 + 1):
        for cx in range(x0, x1 + 1):
            t = w.tile(cx, cy)
            has1 = (t == 1) if isinstance(t, int) else bool((t == 1).any())
            if not has1:
                continue
            lo, hi, wh, wt, fl = d.cell(cx, cy)
            tot += 1
            if fl & 2:
                land += 1
            else:
                noland += 1
                groundNoLand.append((lo, hi))
            if hi < w.bodyH[1]:
                submerged += 1
            k = ('inside' if lx0 <= cx <= lx1 and ly0 <= cy <= ly1 else 'outside') + ' the land cells\' box'
            where[k] = where.get(k, 0) + 1
    print('sea: body 1 (%s, %.1f u) covers %d cells: %d with a land flag, %d without; %d wholly under it (hi < water)'
          % ('sea river lake'.split()[w.bodyCls[1]], w.bodyH[1], tot, land, noland, submerged))
    print('  land-flag cells span [%d,%d]..[%d,%d]; file span [%d,%d]..[%d,%d]' % (lx0, ly0, lx1, ly1, x0, y0, x1, y1))
    print('  body-1 cells by place: %s' % ', '.join('%s %d' % kv for kv in sorted(where.items())))
    if groundNoLand:
        los = sorted(set(round(a, 1) for a, b in groundNoLand))
        print('  ground in body-1 cells without a land flag: lo values %s%s'
              % (los[:6], ' ...' if len(los) > 6 else ''))


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    d = Lodt(sys.argv[1])
    if d.version != 3:
        raise SystemExit('version %d: this needs a v3 file' % d.version)
    what, a = sys.argv[2], [int(v) for v in sys.argv[3:]]
    if what == 'poke':
        x0, y0, x1, y1, n = a if a else (-6, -10, 3, -3, 8)
        poke(d, Water(d), x0, y0, x1, y1, n)
    elif what == 'v2':
        x0, y0, x1, y1 = a if a else (-6, -10, 3, -3)
        v2(d, x0, y0, x1, y1)
    elif what == 'sea':
        sea(d, Water(d))
    else:
        raise SystemExit('unknown %r' % what)


if __name__ == '__main__':
    main()
