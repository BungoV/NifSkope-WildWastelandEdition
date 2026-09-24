#!/usr/bin/env python3
"""CELLVIEW4 item 2 -- A MEASUREMENT of the still-bare quads, over the corpus.

CELLVIEW3 left 24 of Sanctuary -20,7's 1024 quads with no texture at all.  The
brief asks what those quads CARRY in the record, and what the game draws there.

This walks every LAND in the Commonwealth and, for each one, replays
src/cellground.cpp's exact rule at each 128-unit quad's own SW corner:

    ltex = BTXT[quadrant]
    best = 0.5 if BTXT else 0.0          # CELL_GROUND_LAYER_MIN
    for each ATXT layer of that quadrant:
        o = layer.opacity[row][col]
        if layer.ltex and o >= best and o > 0: ltex, best = layer.ltex, o
    bare  <=>  ltex == 0

and then says WHY each bare quad is bare, in the record's own terms:

    no-btxt-no-layers   the quadrant has no BTXT and no ATXT at all
    no-btxt-zero-here   it has ATXT layers, but every one reads 0.0 at THIS
                        corner (paint exists in the quadrant, not on this quad)
    no-btxt-null-form   its only layers name LTEX form 0
    btxt-null-form      it HAS a BTXT whose LTEX form is 0

The last two matter because a null form is a quadrant the record explicitly
leaves to the engine, not an omission.

Usage: python bare_quads.py <Fallout4.esm> [max cells]
"""
import collections
import struct
import sys

import numpy as np

import splat_sim as ss


def four_corner_bare(base, layers):
    """The same 32x32 quads under the BLEND's rule instead of the mosaic's.

    The mosaic asks ONE corner of each quad (its SW one) which texture wins.
    A splat quad carries a weight at each of its FOUR corners and interpolates
    between them, so it is blank only when the quadrant has no BTXT AND every
    layer reads 0.0 at ALL FOUR corners.  This counts what survives -- it is
    the number the proposed rule has to cover, and it is why the rule is small.
    """
    n = 0
    still = set()
    for row in range(32):
        for col in range(32):
            # THE QUADRANT IS THE QUAD'S, NOT THE CORNER'S.  A quad in
            # quadrant 0 has corners on grid lines 16, and quadrant_of() hands
            # those to quadrant 2/3 because the middle row and column are
            # SHARED.  Their weights still have to be read out of quadrant 0's
            # own 17x17 grids, where they are the last row and column.
            q, _lr, _lc = ss.quadrant_of(row, col)
            painted = bool(base[q])
            for dr, dc in ((0, 0), (0, 1), (1, 0), (1, 1)):
                if painted:
                    break
                lrow = min(max((row + dr) - (16 if q >= 2 else 0), 0), 16)
                lcol = min(max((col + dc) - (16 if (q & 1) else 0), 0), 16)
                for flt, _li, g in layers[q]:
                    if flt and float(g[lrow][lcol]) > 0.0:
                        painted = True
                        break
            if not painted:
                n += 1
                still.add((row, col))
    return n, still


def classify(base, layers):
    """-> (bare count, Counter of reasons, set of (row,col) bare corners)."""
    reasons = collections.Counter()
    bare = set()
    for row in range(32):
        for col in range(32):
            # the quad's SW corner on the 33x33 land grid, then its quadrant
            q, lrow, lcol = ss.quadrant_of(row, col)
            lt = base[q]
            best = ss.LAYER_MIN if lt else 0.0
            for flt, _li, g in layers[q]:
                o = float(g[lrow][lcol])
                if flt and o >= best and o > 0.0:
                    lt, best = flt, o
            if lt:
                continue
            bare.add((row, col))
            if base[q]:
                reasons['btxt-null-form'] += 1
            elif not layers[q]:
                reasons['no-btxt-no-layers'] += 1
            elif not any(flt for flt, _li, _g in layers[q]):
                reasons['no-btxt-null-form'] += 1
            else:
                reasons['no-btxt-zero-here'] += 1
    return len(bare), reasons, bare


def main():
    esmp = sys.argv[1]
    cap = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    esm = ss.Esm(esmp)
    lands, ltex, txst = ss.scan(esm, 'Commonwealth', None)
    print('LAND records in the Commonwealth: %d' % len(lands))

    total = collections.Counter()
    cellsWithBare = 0
    quadsTotal = 0
    worst = []
    quadrantNoBtxt = quadrantNoBtxtNoLayer = quadrantTotal = 0
    for i, (xy, (base, layers, _h)) in enumerate(sorted(lands.items())):
        if cap and i >= cap:
            break
        n, reasons, _b = classify(base, layers)
        quadsTotal += 1024
        for q in range(4):
            quadrantTotal += 1
            if not base[q]:
                quadrantNoBtxt += 1
                if not layers[q]:
                    quadrantNoBtxtNoLayer += 1
        total.update(reasons)
        # the same cells under the four-corner rule, but only where the record
        # actually paints something -- the Commonwealth's 95,* filler cells
        # carry no BTXT and no ATXT at all and are bare under every rule.
        if any(base) or any(layers[q] for q in range(4)):
            fc, _s = four_corner_bare(base, layers)
            total['FOUR-CORNER still bare (painted cells only)'] += fc
            total['(painted cells counted)'] += 1
            total['SW-CORNER bare (painted cells only)'] += n
        if n:
            cellsWithBare += 1
            worst.append((n, xy))
    print('cells measured: %d  (%d quads)' % (min(len(lands), cap or len(lands)),
                                              quadsTotal))
    print('cells with at least one bare quad: %d' % cellsWithBare)
    print('bare quads by reason:')
    for k, v in total.most_common():
        print('   %-20s %d' % (k, v))
    print('quadrants with NO BTXT: %d of %d  (of those, no ATXT either: %d)'
          % (quadrantNoBtxt, quadrantTotal, quadrantNoBtxtNoLayer))
    worst.sort(reverse=True)
    print('worst cells:')
    for n, xy in worst[:12]:
        print('   %5d bare   cell %d,%d' % (n, xy[0], xy[1]))

    if (-20, 7) in lands:
        base, layers, _h = lands[(-20, 7)]
        n, reasons, b = classify(base, layers)
        print('\n=== Sanctuary -20,7 ===')
        print('bare quads: %d   reasons: %s' % (n, dict(reasons)))
        fc, still = four_corner_bare(base, layers)
        print('still bare under the FOUR-CORNER (splat) rule: %d' % fc)
        print('   they are: %s' % sorted(still))
        rows = sorted({r for r, _c in b})
        cols = sorted({c for _r, c in b})
        print('rows %s  cols %s' % (rows, cols))
        for q in range(4):
            print('  quadrant %d: BTXT %s, %d layer(s), nonnull %d'
                  % (q, ('%08X' % base[q]) if base[q] else 'NONE',
                     len(layers[q]),
                     sum(1 for f, _l, _g in layers[q] if f)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
