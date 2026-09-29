#!/usr/bin/env python3
"""WATER1 task 3 gate on REAL data: the flat-only writer's vanilla .lodl against the sloped-water
writer's .lodl of the same load order. Reads bytes only (shares no code with src/lodtfile.cpp).

  real_cmp.py <old.lodl> <new.lodl> [--floor]

lodl_cmp.py assumed the vanilla bake has no sloped placed water, so the two files would differ only
by the 8-byte header growth. The real bake found 6 sloped placed water meshes, so the file changes
where they are. This gate asks the question that is left: is every change NEAR a sloped mesh?

Pre-registered (written before the first run):
  * S = the cells whose surface-plane tile is not uniform 0 (the sloped water's own cells);
  * WATR: every old WATR form is still in the new table, in the same order (new ones appended);
  * bodies: every old body is in the new table with the same class, WATR form and height, matched
    by those plus its bbox, except bodies whose bbox touches S within 1 cell (they may grow);
  * body-ID plane (ids compared through the old->new body match), flow plane (2 bytes a sample,
    measured from the container; first written here as 1 and refused by the reader), shore plane and the
    per-cell table: every cell that differs is within 2 cells of S (Chebyshev). 2 cells is the
    shore plane's reach (u8 steps of 32 units = 8160 units, under 2 cells of 4096);
  * outside that, byte for byte on the decoded samples.
--floor changes one body-ID sample in the old file's decoded plane, in a cell far from S: FAIL.
"""
import struct
import sys
import zlib

import numpy as np


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def u64(b, o):
    return struct.unpack_from('<Q', b, o)[0]


class F:
    def __init__(self, path):
        self.b = open(path, 'rb').read()
        b = self.b
        if b[:4] != b'LODT' or u32(b, 4) != 3:
            raise SystemExit('REFUSED: %s is not a version-3 .lodl' % path)
        self.minX, self.minY, self.maxX, self.maxY = struct.unpack_from('<4i', b, 8)
        self.cx = self.maxX - self.minX + 1
        self.cy = self.maxY - self.minY + 1
        self.nW = u32(b, 0x34)
        self.watr = list(struct.unpack_from('<%dI' % self.nW, b, u64(b, 0x50)))
        oB, n, rec = u64(b, 0xA0), u32(b, 0xA8), u32(b, 0xAC)
        self.bodies = []
        for i in range(n):
            r = b[oB + i * rec:oB + (i + 1) * rec]
            bid, cls, fl = struct.unpack_from('<HBB', r, 0)
            h, wf, area = struct.unpack_from('<fII', r, 4)
            x0, y0, x1, y1 = struct.unpack_from('<4h', r, 16)
            if bid != i + 1:
                raise SystemExit('body record %d has id %d' % (i, bid))
            self.bodies.append(dict(id=bid, cls=cls, fl=fl, h=h, wf=wf, area=area, box=(x0, y0, x1, y1), raw=r))
        oC = u64(b, 0x68)
        self.cells = [b[oC + s * 16:oC + s * 16 + 16] for s in range(self.cx * self.cy)]
        self.sect = u32(b, 0x44)
        self.surf = u64(b, 0xF8) if (self.sect & (1 << 9)) else 0

    def plane(self, off, bps_want):
        """decoded tiles: list over cells (row-major from west,south) of bytes, or ('u', value)."""
        b = self.b
        tX, tY, edge, bps = struct.unpack_from('<4I', b, off)
        dirOff = u64(b, off + 16)
        if bps != bps_want or tX != self.cx or tY != self.cy:
            raise SystemExit('plane at %d: %dx%d bps %d' % (off, tX, tY, bps))
        out = []
        for t in range(tX * tY):
            o, c, u = struct.unpack_from('<QII', b, dirOff + t * 16)
            if c == 0:
                out.append(np.full((edge, edge), u, dtype='<u%d' % bps if bps != 4 else '<u4'))
            else:
                raw = zlib.decompress(b[o:o + c])
                out.append(np.frombuffer(raw, dtype='<u%d' % bps).reshape(edge, edge))
        return out, edge


def main():
    old, new = F(sys.argv[1]), F(sys.argv[2])
    floor = '--floor' in sys.argv
    fails = []
    if (old.minX, old.minY, old.maxX, old.maxY) != (new.minX, new.minY, new.maxX, new.maxY):
        raise SystemExit('REFUSED: grids differ')
    W = old.cx

    def xy(s):
        return (s % W + old.minX, s // W + old.minY)

    # ---- S: the sloped water's cells
    if not new.surf:
        raise SystemExit('REFUSED: the new file has no surface plane')
    surf, _ = new.plane(new.surf, 4)
    S = [xy(s) for s, t in enumerate(surf) if t.any()]
    print('S (cells with a non-uniform surface tile): %d: %s' % (len(S), ' '.join('%d,%d' % c for c in S)))

    def dist(c):
        return min(max(abs(c[0] - s[0]), abs(c[1] - s[1])) for s in S) if S else 999

    # ---- WATR
    if new.watr[:old.nW] != old.watr:
        fails.append('WATR: the old forms are not the new table\'s prefix')
    added = ['%08x' % w for w in new.watr[old.nW:]]
    print('WATR: old %d, new %d, appended: %s' % (old.nW, new.nW, ' '.join(added) or 'none'))

    # ---- bodies: match old -> new by (cls, wf, h, box); then relaxed (cls, wf, h) near S
    key = lambda d: (d['cls'], d['wf'], d['h'], d['box'])
    newByKey = {}
    for d in new.bodies:
        newByKey.setdefault(key(d), []).append(d)
    used = set()
    idmap = {0: 0}
    exact = relaxed = 0
    for d in old.bodies:
        cands = [c for c in newByKey.get(key(d), []) if c['id'] not in used]
        if cands:
            c = cands[0]
            exact += 1
        else:
            x0, y0, x1, y1 = d['box']
            nearS = any(x0 - 1 <= s[0] <= x1 + 1 and y0 - 1 <= s[1] <= y1 + 1 for s in S)
            cands = [c for c in new.bodies if c['id'] not in used and c['cls'] == d['cls'] and c['wf'] == d['wf']
                     and c['h'] == d['h']]
            if not cands or not nearS:
                fails.append('body %d (cls %d, h %.1f, box %s) has no match in the new file%s'
                             % (d['id'], d['cls'], d['h'], d['box'], '' if nearS else ' and is not near S'))
                continue
            c = min(cands, key=lambda c: sum(abs(a - b) for a, b in zip(c['box'], d['box'])))
            relaxed += 1
            print('  body %d -> %d changed near S: area %d -> %d, box %s -> %s'
                  % (d['id'], c['id'], d['area'], c['area'], d['box'], c['box']))
        used.add(c['id'])
        idmap[d['id']] = c['id']
    newOnly = [d for d in new.bodies if d['id'] not in used]
    if '--amend-s' in sys.argv:
        # AMENDMENT written after the first run (not pre-registered): a sloped mesh whose only wet
        # texel is its own lowest surface has surface offset 0 there, so its tile is uniform 0 and
        # not in S. Add the cells of every body the flat file did not have.
        for d in newOnly:
            x0, y0, x1, y1 = d['box']
            for cx in range(x0, x1 + 1):
                for cy in range(y0, y1 + 1):
                    if (cx, cy) not in S:
                        S.append((cx, cy))
        print('S amended with the new bodies\' cells: %d: %s' % (len(S), ' '.join('%d,%d' % c for c in S)))
    renum = sum(1 for a, b in idmap.items() if a != b)
    print('bodies: old %d, new %d; %d matched exactly, %d changed near S, %d renumbered; new bodies: %s'
          % (len(old.bodies), len(new.bodies), exact, relaxed, renum,
             '; '.join('id %d cls %d h %.1f wf %08x area %d box %s' % (d['id'], d['cls'], d['h'], d['wf'],
                                                                         d['area'], d['box']) for d in newOnly)))

    # ---- planes + cell table
    lut = np.zeros(65536, dtype=np.uint16)
    for a, b2 in idmap.items():
        lut[a] = b2
    lut[[i for i in range(65536) if i not in idmap]] = 0xFFFF   # an unmatched old id never equals a new id
    report = {}
    far = None
    for name, off, bps in (('body-ID', 0xC0, 2), ('flow', 0xD0, 2), ('shore', 0xE0, 1)):
        po, _ = old.plane(u64(old.b, off), bps)
        pn, _ = new.plane(u64(new.b, off), bps)
        diff = []
        for s in range(len(po)):
            a = po[s]
            if name == 'body-ID':
                a = lut[a]
                if floor and far is None and dist(xy(s)) > 10 and po[s].any():
                    a = a.copy()
                    a[0, 0] ^= 1
                    far = xy(s)
            if not np.array_equal(a, pn[s]):
                diff.append(xy(s))
        report[name] = diff
    report['cell table'] = [xy(s) for s in range(len(old.cells)) if old.cells[s] != new.cells[s]]
    for name, diff in report.items():
        ds = [dist(c) for c in diff]
        hist = {}
        for d in ds:
            hist[min(d, 3)] = hist.get(min(d, 3), 0) + 1
        bad = [c for c, d in zip(diff, ds) if d > 2]
        print('%s: %d cell(s) differ; by distance from S (0,1,2,3+): %s%s'
              % (name, len(diff), ' '.join(str(hist.get(k, 0)) for k in range(4)),
                 ('; FAR: ' + ' '.join('%d,%d' % c for c in bad[:10])) if bad else ''))
        if bad:
            fails.append('%s: %d cell(s) differ more than 2 cells from the sloped water' % (name, len(bad)))
    for c in report['cell table']:
        s = (c[1] - old.minY) * W + (c[0] - old.minX)
        a, b2 = old.cells[s], new.cells[s]
        print('  cell %d,%d: old %s | new %s' % (c[0], c[1], struct.unpack('<3f2H', a), struct.unpack('<3f2H', b2)))
    if floor:
        print('FLOOR: one body-ID sample flipped in the old file at cell %s' % (far,))
    for f in fails[:10]:
        print('  FAIL', f)
    print('FAIL' if fails else 'PASS')
    sys.exit(1 if fails else 0)


main()
