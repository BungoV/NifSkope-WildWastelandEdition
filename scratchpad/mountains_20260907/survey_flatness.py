"""survey_flatness.py - of the 32,909 untextured cells, how many carry REAL
terrain and how many are flat filler?

This is what decides "all 32,909 cells or a ring around the playable area".
An untextured LAND whose VHGT is dead flat has nothing for a LOD generator to
show, so texturing it buys nothing; an untextured LAND with real relief is a
distant mountain the player can see.

VHGT decode is the one from src/esmdata.cpp:334 (LODGEN_ESM_LAYOUTS.md says
the deltas are SIGNED and that was VERIFIED against the game's own baked
mesh): float base, then 33x33 signed byte deltas, column 0 of each row
offsetting the PREVIOUS row's column 0, everything x8 game units.

Read-only.
"""

import collections
import struct

import fo4esm as E
import esp_lib_land as L

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'


def decode_vhgt(content):
    """-> (min height, max height) in game units."""
    base = struct.unpack_from('<f', content, 0)[0]
    d = struct.unpack_from('<1089b', content, 4)
    lo = hi = None
    row_start = base
    for row in range(33):
        row_start += d[row * 33]
        v = row_start
        for col in range(33):
            if col:
                v += d[row * 33 + col]
            h = v * 8.0
            if lo is None or h < lo:
                lo = h
            if hi is None or h > hi:
                hi = h
    return lo, hi


def main():
    buf = E.load(ESM)
    wc = None
    for node in E.top_level_groups(buf):
        if node.label != b'WRLD' or node.gtype != E.GT_TOP:
            continue
        got = []
        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node],
               lambda *a: None,
               lambda g, s: got.append(g) if (g.gtype == E.GT_WORLD_CHILDREN and
                                              struct.unpack('<I', g.label)[0]
                                              == E.COMMONWEALTH) else None)
        wc = got[0]
        break

    cur = {'xy': None}
    rows = []

    def on_rec(off, sig, dsize, flags, formid, tail, stack):
        if sig == b'CELL' and stack[-1].gtype == E.GT_EXTERIOR_SUBBLOCK:
            for t, c in E.subrecords(E.record_payload(buf, off, dsize, flags)):
                if t == b'XCLC' and len(c) >= 8:
                    cur['xy'] = struct.unpack_from('<ii', c, 0)
                    break
        elif sig == b'LAND':
            payload = E.record_payload(buf, off, dsize, flags)
            base, has_alpha = L.land_quadrant_state(payload)
            vhgt = None
            for t, c in E.subrecords(payload):
                if t == b'VHGT' and len(c) >= 4 + 1089:
                    vhgt = c
                    break
            if vhgt is None:
                return
            lo, hi = decode_vhgt(vhgt)
            rows.append((cur['xy'], dsize, bool(base or has_alpha), lo, hi))

    E.walk(buf, wc.offset + 24, wc.offset + wc.gsize, [wc], on_rec)

    untex = [r for r in rows if not r[2]]
    tex = [r for r in rows if r[2]]
    print('cells with a LAND and a VHGT: %d  (textured %d, untextured %d)'
          % (len(rows), len(tex), len(untex)))

    print()
    print('=== relief of the untextured cells (max - min height, game units) ===')
    buckets = [(0.0, 'dead flat (relief == 0)'),
               (1.0, 'under 1 unit'),
               (64.0, 'under 64 units (under half a metre-ish)'),
               (512.0, 'under 512'),
               (4096.0, 'under 4096'),
               (float('inf'), '4096 or more')]
    counts = collections.Counter()
    for xy, dsize, _t, lo, hi in untex:
        rel = hi - lo
        for lim, name in buckets:
            if rel <= lim:
                counts[name] += 1
                break
    for lim, name in buckets:
        print('  %-42s %6d' % (name, counts[name]))

    real = [r for r in untex if (r[4] - r[3]) > 64.0]
    print()
    print('untextured cells with more than 64 units of relief: %d' % len(real))
    if real:
        xs = [r[0][0] for r in real]
        ys = [r[0][1] for r in real]
        print('  their extent: x %d..%d  y %d..%d' % (min(xs), max(xs), min(ys), max(ys)))
        print('  their total on-disk LAND bytes in the master: %s'
              % '{:,}'.format(sum(r[1] for r in real)))

    print()
    print('=== how far out do textured cells go? (the playable ring) ===')
    if tex:
        xs = [r[0][0] for r in tex]
        ys = [r[0][1] for r in tex]
        print('  textured extent: x %d..%d  y %d..%d' % (min(xs), max(xs), min(ys), max(ys)))
        cheb = max(max(abs(x) for x in xs), max(abs(y) for y in ys))
        print('  furthest textured cell, Chebyshev radius from (0,0): %d' % cheb)

    print()
    print('=== cells inside successive rings (all cells / untextured only) ===')
    for r in (16, 24, 32, 40, 48, 56, 64, 80, 96):
        allc = sum(1 for row in rows if abs(row[0][0]) <= r and abs(row[0][1]) <= r)
        unt = sum(1 for row in untex if abs(row[0][0]) <= r and abs(row[0][1]) <= r)
        untb = sum(row[1] for row in untex
                   if abs(row[0][0]) <= r and abs(row[0][1]) <= r)
        rel = sum(1 for row in untex if abs(row[0][0]) <= r and abs(row[0][1]) <= r
                  and (row[4] - row[3]) > 64.0)
        print('  ring +-%-3d  cells %6d  untextured %6d  of those with relief %6d '
              '  master LAND bytes %s'
              % (r, allc, unt, rel, '{:,}'.format(untb)))


if __name__ == '__main__':
    main()
