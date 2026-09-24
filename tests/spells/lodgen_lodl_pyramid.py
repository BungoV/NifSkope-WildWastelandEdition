#!/usr/bin/env python
"""The PYRAMID and WATER invariants of a `.lodl`, measured on the file's own
bytes -- lane AUDIT1, 2026-09-17.

WHY A NEW READER.  `lodl_open_authority.py` decodes a `.lodl` (and this file
imports its `Lodt` class rather than writing a second decoder), but it answers
QUESTIONS -- one height, one cell, a spread -- for `lodl_open.sh` to compare
against a meshed scene.  Nothing in tests/spells/ asked the questions the audit
owes:

  A  THE PYRAMID IS A DROP, NEVER AN AVERAGE.  The format CANNOT average, and
     this is the measurement that says so rather than the claim: every sample
     of the level-0 grid maps to EXACTLY ONE stored word.  A block at the
     coarsest level stores blockEdge^2 words; every finer block stores three
     children a parent and NOT the parent, so the sample a coarse level shows
     IS the word the fine level shows -- there is no second copy that could
     hold a mean.  The check is a BIJECTION between grid samples and slots.

     THE FLOOR: the same walk with the three children of a parent collapsed
     onto one slot must NOT be a bijection.  (A merely wrong `levels` is not a
     floor: measured 2026-09-17, the walk stays injective one level too deep.)

  B  EVERY SAMPLE IS INSIDE ITS OWN CELL'S RANGE.  The flat per-cell section
     stores each cell's min and max height; the pyramid stores the heights.
     Two passes write them, so agreement is a real cross-check of both -- and
     of the quantiser between them.  Slack is one height quantum.

     THE FLOOR: the same heights against a range shrunk to a tenth of its span
     must NOT all fit.

  C  THE WATER FIELDS, by the rule docs/LODGEN_BTD_FORMAT.md actually states.
     A cell without water writes height 0 and type 0xFFFF; no stored height is
     one of the three no-water sentinels; every interned type is inside the
     WATR table.  The audit brief's "water table inside the cell's height
     range" is NOT asserted, and must not be: the stored height is RESOLVED
     (the cell's `XCLW`, else the worldspace `DNAM` default), and whether a
     plane is DRAWN is a second question the file does not answer.  Measured
     on the whole Commonwealth: 20,340 cells sit under their own plane and
     14,586 sit over it.  Both counts are printed instead of judged.

  D  THE SHORE PLANE is reported when the section is present (LODL_SECT_SHORE,
     1 << 6) and reported ABSENT when it is not.  This file does not claim
     monotonicity it has not measured.

    usage: lodgen_lodl_pyramid.py <file.lodl> [--stride N] [--budget N]

`--stride` subsamples inside a cell for B (default 4 of the 32 samples an
edge); `--budget` caps the number of samples A walks (default 300,000), and A
says whether it walked the whole grid or a centred window of it.

Prints `ok` / `FAIL` lines; exits 1 on any failure.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodl_open_authority as LO            # noqa: E402  the ONE .lodl decoder

SENTINELS = (0xFF7FFFFF, 0x7F7FFFFF, 0x4F7FFFC9)


class Checker(object):
    def __init__(self):
        self.fails = 0
        self.n = 0

    def check(self, what, ok, note=''):
        self.n += 1
        if not ok:
            self.fails += 1
        print('  %s %s%s' % ('ok  ' if ok else 'FAIL', what,
                             ('  [%s]' % note) if note else ''))


def slot_of(t, gx, gy, broken=False):
    """(block index, word slot) for a grid sample, by the file's OWN rule --
    the same walk `Lodt.plane_word` does.

    `broken=True` is THE FLOOR: it sends all three children of a parent to the
    parent's first child slot, a mapping that cannot be a bijection."""
    coarsest = t.levels - 1
    level = 0
    while level < coarsest and gx % (1 << (level + 1)) == 0 and gy % (1 << (level + 1)) == 0:
        level += 1
    lx, ly = gx >> level, gy >> level
    bi, bj = lx // t.blockEdge, ly // t.blockEdge
    wx, wy = lx % t.blockEdge, ly % t.blockEdge
    first = 0
    for j in range(coarsest, level, -1):
        first += t.blocks_x(j) * t.blocks_y(j)
    idx = first + bj * t.blocks_x(level) + bi
    if level == coarsest:
        k = wy * t.blockEdge + wx
    else:
        half = t.blockEdge // 2
        px, py = wx // 2, wy // 2
        if (wx & 1) and not (wy & 1):
            sub = 0
        elif not (wx & 1) and (wy & 1):
            sub = 1
        else:
            sub = 2
        k = (py * half + px) * 3 + (0 if broken else sub)
    return idx, k


def bijection(t, win, broken=False):
    """True when every sample in the window (x0, y0, x1, y1 in SAMPLE
    coordinates -- the grid is cellsX * spc wide) lands on its own slot."""
    x0, y0, x1, y1 = win
    seen = {}
    for gy in range(y0, y1):
        for gx in range(x0, x1):
            key = slot_of(t, gx, gy, broken)
            if key in seen:
                return False, len(seen), ('sample (%d,%d) shares block %d slot %d with (%d,%d)'
                                          % (gx, gy, key[0], key[1], seen[key][0], seen[key][1]))
            seen[key] = (gx, gy)
    return True, len(seen), ''


def window(t, budget):
    """the whole sample grid when it fits in `budget`, else a centred square of
    it, snapped to cell edges"""
    w, h = t.cellsX * t.spc, t.cellsY * t.spc
    if w * h <= budget:
        return (0, 0, w, h), True
    side = max(t.spc, int(budget ** 0.5) // t.spc * t.spc)
    x0 = ((w - side) // 2) // t.spc * t.spc
    y0 = ((h - side) // 2) // t.spc * t.spc
    return (x0, y0, x0 + side, y0 + side), False


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(__doc__)
    path = args[0]
    stride = 4
    budget = 300000
    for i, a in enumerate(args):
        if a == '--stride':
            stride = int(args[i + 1])
        elif a == '--budget':
            budget = int(args[i + 1])
    t = LO.Lodt(path)
    ck = Checker()
    print('%s: version %d, cells %dx%d (%d..%d, %d..%d), spc %d, levels %d, blockEdge %d, '
          'blocks %d, planes %d, sect 0x%X, quantum %g'
          % (os.path.basename(path), t.version, t.cellsX, t.cellsY, t.minX, t.maxX,
             t.minY, t.maxY, t.spc, t.levels, t.blockEdge, t.nBlocks, t.planes,
             t.sect, t.quantum))

    # ---- A: the pyramid is a drop, never an average ------------------------
    win, whole = window(t, budget)
    npts = (win[2] - win[0]) * (win[3] - win[1])
    ok, used, why = bijection(t, win)
    ck.check('A %s %d sample(s) of the %dx%d grid land on %d distinct stored slots -- one '
             'word a sample, so a coarse level IS the fine sample and cannot hold a mean'
             % ('the whole grid,' if whole else 'a centred window of', npts,
                t.cellsX * t.spc, t.cellsY * t.spc, used),
             ok and used == npts, why)
    floorwin = (win[0], win[1], win[0] + 2 * t.spc, win[1] + 2 * t.spc)
    bad, _, _ = bijection(t, floorwin, True)
    ck.check('A-FLOOR the same walk with the three children of a parent collapsed onto one '
             'slot is NOT a bijection', not bad)

    # ---- B: every sample inside its own cell's stored range ----------------
    inside = outside = noland = tight = 0
    worst = 0.0
    q = t.quantum
    cx0, cy0 = t.minX + win[0] // t.spc, t.minY + win[1] // t.spc
    cx1, cy1 = t.minX + win[2] // t.spc, t.minY + win[3] // t.spc
    for cy in range(cy0, cy1):
        for cx in range(cx0, cx1):
            lo, hi, wh, wt, fl = t.cell(cx, cy)
            if not (fl & 2):          # bit 1 = has land; a landless cell has no range
                noland += 1
                continue
            bx, by = (cx - t.minX) * t.spc, (cy - t.minY) * t.spc
            mid, span = (lo + hi) / 2.0, (hi - lo) / 20.0
            for sy in range(0, t.spc, stride):
                for sx in range(0, t.spc, stride):
                    h = t.height(bx + sx, by + sy)
                    if lo - q <= h <= hi + q:
                        inside += 1
                    else:
                        outside += 1
                        worst = max(worst, min(abs(h - lo), abs(h - hi)))
                    if not (mid - span - q <= h <= mid + span + q):
                        tight += 1
    ck.check('B every sampled height is inside the stored range of its own cell '
             '(%d inside, %d outside, %d landless cell(s) skipped, stride %d, worst %.3f u)'
             % (inside, outside, noland, stride, worst), outside == 0 and inside > 0)
    ck.check('B-FLOOR the same heights against a range shrunk to a tenth of its span do NOT '
             'all fit (%d outside)' % tight, tight > 0)

    # ---- C: the water fields, by the doc's own rule ------------------------
    defW, defT = 0.0, 0
    if t.version >= 2:
        t.f.seek(0x98)
        defW, defT = struct.unpack('<fI', t.f.read(8))
    wet = dry = drybad = sent = badtype = above = below = 0
    for cy in range(t.minY, t.maxY + 1):
        for cx in range(t.minX, t.maxX + 1):
            lo, hi, wh, wt, fl = t.cell(cx, cy)
            if not (fl & 1):
                dry += 1
                if wh != 0.0 or wt != 0xFFFF:
                    drybad += 1
                continue
            wet += 1
            if struct.unpack('<I', struct.pack('<f', wh))[0] in SENTINELS:
                sent += 1
            if wt != 0xFFFF and wt >= t.nWatr:
                badtype += 1
            if wh > hi:
                above += 1
            elif wh < lo:
                below += 1
    ck.check('C1 every cell WITHOUT water writes height 0 and type 0xFFFF '
             '(%d dry cell(s), %d broke it)' % (dry, drybad), drybad == 0)
    ck.check('C2 no stored water height is one of the three no-water sentinels '
             '(%d water cell(s), %d sentinel)' % (wet, sent), sent == 0)
    ck.check('C3 every interned water type is inside the WATR table '
             '(%d type(s) interned, %d row(s) past it)' % (t.nWatr, badtype), badtype == 0)
    print('    for the record: worldspace default water %.1f, type %08X; %d cell(s) sit '
          'under their own plane and %d over it -- both legal, which is why the "table '
          'inside the cell range" rule is not asserted' % (defW, defT, below, above))
    if wet == 0:
        print('    NOTE: not one cell carries the water flag, so C checked nothing. That '
              'is a fact about the region, not a pass.')

    # ---- D: the shore plane -------------------------------------------------
    print('  shore plane (LODL_SECT_SHORE, 1 << 6): %s'
          % ('present' if (t.sect & 64) else 'absent, sect 0x%X' % t.sect))
    print('%d checks, %d failures' % (ck.n, ck.fails))
    return 1 if ck.fails else 0


if __name__ == '__main__':
    sys.exit(main())
