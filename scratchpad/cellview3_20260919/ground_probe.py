#!/usr/bin/env python3
"""CELLVIEW3 item 2 -- what is actually under the 133 bare quads.

Independent of the viewer: the LAND record is decoded here with the SAME field
layout src/esmdata.cpp:306-336 declares (BTXT formid+quadrant, ATXT
formid+quadrant, VTXT entries of u16 posn / u16 / float opacity, posn/17 and
posn%17 into a 17x17 grid), and the quadrant map from src/cellground.cpp:26-35.

It answers three questions the repair depends on:
  1. do the bare quads' quadrants carry a BTXT at all?
  2. what is the STRONGEST layer opacity at a bare quad's own corner?
  3. how many quads would stop being bare if the 0.5 floor were dropped only
     where the quadrant has no BTXT?

Usage: python ground_probe.py <Fallout4.esm> <cx> <cy> [<cx> <cy> ...]
"""
import os
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tests', 'spells'))
from cell_census import Esm, Rec, fields, decompress   # noqa: E402

QUAD_GRID = 17
LAND_GRID = 33


def quadrant_of(row, col):
    top = 1 if row >= QUAD_GRID - 1 else 0
    right = 1 if col >= QUAD_GRID - 1 else 0
    q = top * 2 + right
    lrow = min(max(row - top * (QUAD_GRID - 1), 0), QUAD_GRID - 1)
    lcol = min(max(col - right * (QUAD_GRID - 1), 0), QUAD_GRID - 1)
    return q, lrow, lcol


def lands_of_world(esm, world_edid, want):
    """(cx,cy) -> {'base':[4], 'layers':[[...]]} for the wanted cells."""
    buf = esm.buf
    target = world_edid.encode('cp1252')
    wform = [None]

    def find(rec, path):
        if rec is not None and rec.type == b'WRLD':
            for t, p in fields(buf, rec):
                if t == b'EDID' and p.split(b'\0')[0] == target:
                    wform[0] = rec.form
    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), find)

    cellxy = {}
    lands = {}

    def it_of(rec):
        if rec.flags & 0x00040000:
            data = decompress(buf, rec)
            return fields(data, Rec(rec.type, len(data), 0, rec.form, 0))
        return fields(buf, rec)

    def cb(rec, path):
        under = False
        parent = None
        for label, gtype, goff in path:
            if gtype == 1 and struct.unpack_from('<I', label, 0)[0] == wform[0]:
                under = True
            if gtype in (8, 9, 10):
                parent = struct.unpack_from('<I', label, 0)[0]
        if not under or rec is None:
            return
        if rec.type == b'CELL':
            for t, p in it_of(rec):
                if t == b'XCLC' and len(p) >= 8:
                    cellxy[rec.form] = struct.unpack_from('<ii', p, 0)
        elif rec.type == b'LAND' and parent is not None:
            base = [0, 0, 0, 0]
            layers = [[], [], [], []]
            pending = -1
            for t, p in it_of(rec):
                if t == b'BTXT' and len(p) >= 8:
                    ltex, q = struct.unpack_from('<IB', p, 0)
                    if 0 <= q < 4:
                        base[q] = ltex
                elif t == b'ATXT' and len(p) >= 8:
                    ltex, q = struct.unpack_from('<IB', p, 0)
                    if 0 <= q < 4:
                        layers[q].append([ltex, [[0.0] * 17 for _ in range(17)]])
                        pending = q
                    else:
                        pending = -1
                elif t == b'VTXT' and pending >= 0 and layers[pending]:
                    grid = layers[pending][-1][1]
                    for e in range(len(p) // 8):
                        posn, _u, op = struct.unpack_from('<HHf', p, 8 * e)
                        if posn <= 288:
                            grid[posn // 17][posn % 17] = op
                    pending = -1
            lands[parent] = (base, layers)

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), cb)
    out = {}
    for f, xy in cellxy.items():
        if xy in want and f in lands:
            out[xy] = lands[f]
    return out


def main():
    esm = Esm(sys.argv[1])
    a = sys.argv[2:]
    want = set()
    while a:
        want.add((int(a[0]), int(a[1])))
        a = a[2:]
    lands = lands_of_world(esm, 'Commonwealth', want)
    for xy in sorted(want):
        if xy not in lands:
            print('cell %d,%d has NO LAND record' % xy)
            continue
        base, layers = lands[xy]
        print('cell %d,%d' % xy)
        for q in range(4):
            print('  quadrant %d: BTXT %s, %d ATXT layers %s'
                  % (q, ('%08X' % base[q]) if base[q] else 'NONE -- no base texture',
                     len(layers[q]), [('%08X' % l[0]) for l in layers[q]]))
        bare = 0
        bare_by_q = defaultdict(int)
        strongest = defaultdict(int)     # bucketed max opacity at a bare corner
        curable = 0
        maxops = []
        for row in range(LAND_GRID - 1):
            for col in range(LAND_GRID - 1):
                q, lr, lc = quadrant_of(row, col)
                ltex = base[q]
                best = 0.5
                for l in layers[q]:
                    if l[1][lr][lc] >= best:
                        best = l[1][lr][lc]
                        ltex = l[0]
                if ltex:
                    continue
                bare += 1
                bare_by_q[q] += 1
                mx = 0.0
                for l in layers[q]:
                    mx = max(mx, l[1][lr][lc])
                maxops.append(mx)
                strongest['%.1f' % (int(mx * 10) / 10.0)] += 1
                if mx > 0.0:
                    curable += 1
        print('  BARE quads: %d of 1024   by quadrant %s'
              % (bare, dict(bare_by_q)))
        print('  of those, the strongest layer opacity at the quad corner:')
        for k in sorted(strongest, key=float):
            print('     %s..  %d quads' % (k, strongest[k]))
        print('  would a floor of >0 (instead of >=0.5) cure them? %d of %d'
              % (curable, bare))
        if maxops:
            print('  max layer opacity over the bare quads: min %.3f mean %.3f max %.3f'
                  % (min(maxops), sum(maxops) / len(maxops), max(maxops)))


if __name__ == '__main__':
    main()
