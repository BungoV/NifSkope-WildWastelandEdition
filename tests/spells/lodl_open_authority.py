#!/usr/bin/env python3
"""The RIGHT-HAND SIDE of tests/spells/lodl_open.sh -- a .lodl decoder that
shares no code with src/lodtfile.cpp.

WHY THIS EXISTS

`lodl_open.sh` asks whether a scene NifSkope MESHED from a .lodl carries the
numbers the FILE holds.  If the expected values came from our own reader, the
suite would only prove that two calls into the same function agree, which is
the check docs/MISTAKES.md keeps a section about.  So every expected value is
decoded here, from the format document (docs/LODGEN_BTD_FORMAT.md) and the
file's own bytes: the header at fixed offsets, the flat per-cell table, the
flat AO and overview planes, and the progressive zlib block pyramid.

The pyramid is the only part with any arithmetic in it, and it is the part
worth restating: a sample appears EXACTLY ONCE in the whole pyramid, at the
coarsest level whose stride divides both its coordinates.  Level `coarsest`
stores a full blockEdge^2 grid; every finer level stores only the three new
samples each parent sample gains, in the order right, below, below-right.

USAGE
    python lodl_open_authority.py <file.lodl> info
    python lodl_open_authority.py <file.lodl> height GX GY [GX GY ...]
    python lodl_open_authority.py <file.lodl> cell CX CY
    python lodl_open_authority.py <file.lodl> ao AX AY
    python lodl_open_authority.py <file.lodl> spread GX0 GY0 GX1 GY1 STEP
"""

import struct
import sys
import zlib


class Lodt:
    def __init__(self, path):
        self.f = open(path, 'rb')
        h = self.f.read(0x98)
        # The magic still spells LODT: the 2026-09-09 rename moved the
        # extension, deliberately not a byte of the file.
        if len(h) < 0x98 or h[0:4] != b'LODT':
            raise SystemExit('not a .lodl: %s' % path)
        (self.version,) = struct.unpack_from('<I', h, 0x04)
        (self.minX, self.minY, self.maxX, self.maxY) = struct.unpack_from('<4i', h, 0x08)
        (self.spc, self.blockEdge, self.levels) = struct.unpack_from('<3I', h, 0x18)
        (self.minH, self.maxH, self.quantum) = struct.unpack_from('<3f', h, 0x24)
        (self.nLtex, self.nWatr, self.nGcvr, self.aoS, self.ovS,
         self.sect) = struct.unpack_from('<6I', h, 0x30)
        (self.oLtex, self.oWatr, self.oGcvr, self.oQuad, self.oCell,
         self.oOver, self.oAo, self.oDir, self.oData,
         self.total) = struct.unpack_from('<10Q', h, 0x48)
        self.cellsX = self.maxX - self.minX + 1
        self.cellsY = self.maxY - self.minY + 1
        self.nBlocks = (self.oData - self.oDir) // 16
        # how many uint16 planes ride in a block payload
        self.planes = 2 + (1 if (self.sect & 1) else 0) + (1 if (self.sect & 2) else 0)
        self._cache = {}

    # -- the pyramid ------------------------------------------------------
    def blocks_x(self, level):
        return (self.cellsX + (1 << level) - 1) >> level

    def blocks_y(self, level):
        return (self.cellsY + (1 << level) - 1) >> level

    def _block(self, idx):
        raw = self._cache.get(idx)
        if raw is not None:
            return raw
        self.f.seek(self.oDir + idx * 16)
        off, csz, usz = struct.unpack('<QII', self.f.read(16))
        self.f.seek(off)
        raw = zlib.decompress(self.f.read(csz))
        if len(raw) != usz:
            raise SystemExit('block %d inflated to %d, header says %d' % (idx, len(raw), usz))
        if len(self._cache) > 512:
            self._cache.clear()
        self._cache[idx] = raw
        return raw

    def plane_word(self, gx, gy, plane):
        coarsest = self.levels - 1
        level = 0
        while level < coarsest and gx % (1 << (level + 1)) == 0 and gy % (1 << (level + 1)) == 0:
            level += 1
        lx, ly = gx >> level, gy >> level
        bi, bj = lx // self.blockEdge, ly // self.blockEdge
        wx, wy = lx % self.blockEdge, ly % self.blockEdge
        first = 0
        for j in range(coarsest, level, -1):
            first += self.blocks_x(j) * self.blocks_y(j)
        idx = first + bj * self.blocks_x(level) + bi
        if idx < 0 or idx >= self.nBlocks:
            raise SystemExit('block index %d out of %d' % (idx, self.nBlocks))
        raw = self._block(idx)
        if level == coarsest:
            n = self.blockEdge * self.blockEdge
            k = wy * self.blockEdge + wx
        else:
            half = self.blockEdge // 2
            n = half * half * 3
            px, py = wx // 2, wy // 2
            if (wx & 1) and not (wy & 1):
                sub = 0            # right of the parent
            elif not (wx & 1) and (wy & 1):
                sub = 1            # below it
            else:
                sub = 2            # below-right
            k = (py * half + px) * 3 + sub
        at = (plane * n + k) * 2
        return struct.unpack_from('<H', raw, at)[0]

    def height(self, gx, gy):
        return (self.plane_word(gx, gy, 0) - 32767) * self.quantum

    # -- the flat sections ------------------------------------------------
    def cell(self, cx, cy):
        s = (cy - self.minY) * self.cellsX + (cx - self.minX)
        self.f.seek(self.oCell + s * 16)
        lo, hi, wh, wt, fl = struct.unpack('<3f2H', self.f.read(16))
        return lo, hi, wh, wt, fl

    def ao(self, ax, ay):
        if not (self.sect & 4) or self.aoS <= 0:
            return 255
        aw = self.cellsX * self.aoS
        self.f.seek(self.oAo + ay * aw + ax)
        return self.f.read(1)[0]

    def overview(self, ox, oy):
        ow = self.cellsX * self.ovS
        self.f.seek(self.oOver + (oy * ow + ox) * 2)
        return (struct.unpack('<H', self.f.read(2))[0] - 32767) * self.quantum

    def plane_keys(self):
        """The plane list `lodl --info` must print, derived from the header
        alone -- so the C++ availability rule is checked, not trusted."""
        keys = ['height']
        if (self.sect & 4) and self.aoS > 0:
            keys.append('ao')
        if self.nLtex > 0:
            keys.append('blend')
        if self.sect & 1:
            keys.append('colour')
        if self.sect & 2:
            keys.append('groundcover')
        if self.sect & 8:
            keys += ['waterheight', 'watertype']
        keys += ['cellflags', 'cellrange']
        if self.ovS > 0:
            keys.append('overview')
        return keys


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    d = Lodt(sys.argv[1])
    what = sys.argv[2]
    a = sys.argv[3:]
    if what == 'info':
        print(d.minX, d.minY, d.maxX, d.maxY)
        print('%.3f %.3f %.3f' % (d.minH, d.maxH, d.quantum))
        print(d.spc, d.blockEdge, d.levels, d.nBlocks)
        print(d.nLtex, d.nWatr, d.nGcvr, d.aoS, d.ovS, d.sect)
        print(' '.join(d.plane_keys()))
    elif what == 'height':
        for i in range(0, len(a), 2):
            print('%.3f' % d.height(int(a[i]), int(a[i + 1])))
    elif what == 'cell':
        lo, hi, wh, wt, fl = d.cell(int(a[0]), int(a[1]))
        print('%.3f %.3f %.3f %d %d' % (lo, hi, wh, wt, fl))
    elif what == 'ao':
        print(d.ao(int(a[0]), int(a[1])))
    elif what == 'overview':
        print('%.3f' % d.overview(int(a[0]), int(a[1])))
    elif what == 'spread':
        gx0, gy0, gx1, gy1, step = (int(v) for v in a[:5])
        vals = [d.height(x, y)
                for y in range(gy0, gy1 + 1, step)
                for x in range(gx0, gx1 + 1, step)]
        print('%.3f' % (max(vals) - min(vals)))
    elif what == 'tiles':
        # shapes and vertices a cell rectangle meshes to, from the OUTPUT
        # format's rules (a BSTriShape counts vertices in a u16, so a tile side
        # stops at 254) rather than from our estimator
        x0, y0, x1, y1, lod = (int(v) for v in a[:5])
        n = max(1, d.spc >> lod)
        k = max(1, 254 // n)
        cx, cy = x1 - x0 + 1, y1 - y0 + 1
        tx, ty = (cx + k - 1) // k, (cy + k - 1) // k
        verts = 0
        for j in range(ty):
            hc = min(k, cy - j * k)
            for i in range(tx):
                wc = min(k, cx - i * k)
                verts += (wc * n + 1) * (hc * n + 1)
        print(tx * ty, verts)
    elif what == 'waterrange':
        x0, y0, x1, y1 = (int(v) for v in a[:4])
        vals = [d.cell(cx, cy)[2]
                for cy in range(y0, y1 + 1) for cx in range(x0, x1 + 1)
                if d.cell(cx, cy)[4] & 1]
        print('%.1f %.1f' % (min(vals), max(vals)) if vals else 'none none')
    elif what == 'aorange':
        # min, max and mean of the AO texels the SCENE samples, over the same
        # grid the mesher walks: cells x0..x1 by y0..y1 at detail level `lod`.
        x0, y0, x1, y1, lod = (int(v) for v in a[:5])
        n = max(1, d.spc >> lod)
        step = max(1, d.spc // n)
        lastX = ((d.cellsX * d.spc - 1) // step) * step
        lastY = ((d.cellsY * d.spc - 1) // step) * step
        baseX, baseY = (x0 - d.minX) * d.spc, (y0 - d.minY) * d.spc
        gw, gh = (x1 - x0 + 1) * n + 1, (y1 - y0 + 1) * n + 1
        lo, hi, total = 255, 0, 0
        for j in range(gh):
            ay = min(baseY + j * step, lastY) * d.aoS // d.spc
            for i in range(gw):
                ax = min(baseX + i * step, lastX) * d.aoS // d.spc
                v = d.ao(ax, ay)
                lo = min(lo, v)
                hi = max(hi, v)
                total += v
        print(lo, hi, '%.1f' % (total / float(gw * gh)))
    elif what == 'watercells':
        # how many cells in an inclusive rectangle carry water / land
        x0, y0, x1, y1 = (int(v) for v in a[:4])
        water = land = 0
        for cy in range(y0, y1 + 1):
            for cx in range(x0, x1 + 1):
                fl = d.cell(cx, cy)[4]
                water += 1 if (fl & 1) else 0
                land += 1 if (fl & 2) else 0
        print(water, land)
    else:
        raise SystemExit('unknown query %r' % what)


if __name__ == '__main__':
    main()
