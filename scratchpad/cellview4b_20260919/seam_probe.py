"""Lane CELLVIEW4B -- is the straight SEAM the director saw in the simulation
IN THE RECORD, or did the simulation/viewer invent it?

The director saw a straight line in `sim_m20_7_blended.png` where the TOP-RIGHT
quadrant meets its neighbours.  The design in `src/cellsplat.h` claims outright
that there is no seam to invent a rule for:

    "the 33x33 land vertex grid maps ONE-TO-ONE onto the four 17x17 quadrant
     grids, with the middle row and column SHARED ... neighbouring quads read
     the same stored number at the edge they share ... so there is no seam"

That claim is about ONE layer's opacity.  It says nothing about whether the two
quadrants are compositing THE SAME LIST OF TEXTURES, and that is the thing a
seam would come from.  This probe asks the record directly, three ways:

  A. what each quadrant's BTXT and ATXT set actually is (form, paint index);
  B. for every LTEX that appears in more than one quadrant, whether the two
     quadrants' stored opacities AGREE along the grid line they share;
  C. for the quads immediately either side of each centre line, the full
     ordered ingredient list, so a discontinuity can be read off as a
     difference in INGREDIENTS rather than in weights.

Nothing here renders.  It reads Fallout4.esm and prints numbers.
"""
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'cellview4_20260919'))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tools'))

from splat_sim import scan, diffuse_of  # noqa: E402
from cell_census import Esm  # noqa: E402

ESM = os.environ.get(
    'ESM', 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm')
CX, CY = -20, 7
QNAME = {0: 'BL (0)', 1: 'BR (1)', 2: 'TL (2)', 3: 'TR (3)'}


def local(q, row, col):
    """The (lrow, lcol) a 33x33 grid point has INSIDE quadrant q.

    The same arithmetic src/cellsplat.cpp weightAt() uses: the quadrant is the
    caller's, so the middle row/col is the LAST row/col of the low quadrants and
    the FIRST of the high ones.  Out-of-range is clamped exactly as the C++
    clamps, and the caller is told when it clamped.
    """
    lr = row - (16 if q >= 2 else 0)
    lc = col - (16 if (q & 1) else 0)
    return lr, lc, (0 <= lr <= 16 and 0 <= lc <= 16)


def main():
    esm = Esm(ESM)
    lands, ltex, txst = scan(esm, 'Commonwealth', {(CX, CY)})
    if (CX, CY) not in lands:
        print('REFUSED: no LAND for %d,%d' % (CX, CY))
        return 2
    base, layers, _h = lands[(CX, CY)]

    def tex(form):
        if not form:
            return '(none)'
        d = diffuse_of(form, ltex, txst)
        return os.path.basename(d) if d else '<%08X unresolved>' % form

    print('=== A. what each quadrant carries, cell %d,%d' % (CX, CY))
    for q in range(4):
        print('  quadrant %s  BTXT %s' % (QNAME[q], tex(base[q])))
        if not layers[q]:
            print('      (no ATXT layers)')
        for lt, idx, grid in sorted(layers[q], key=lambda L: L[1]):
            print('      ATXT index %2d  %-34s  opacity min %.3f max %.3f  '
                  'nonzero %d/289' % (idx, tex(lt), grid.min(), grid.max(),
                                      int((grid > 1 / 255.0).sum())))

    # ---------------------------------------------------------------- B
    print()
    print('=== B. do two quadrants agree about a SHARED LTEX on the grid line')
    print('    they share?  (the claim in cellsplat.h that makes the blend')
    print('    seamless is only about one layer; this is whether the layer is')
    print('    even the same layer on both sides.)')
    pairs = [(0, 1, 'col 16', 'vertical'), (2, 3, 'col 16', 'vertical'),
             (0, 2, 'row 16', 'horizontal'), (1, 3, 'row 16', 'horizontal')]
    for qa, qb, where, _kind in pairs:
        sa = {L[0] for L in layers[qa]} | ({base[qa]} if base[qa] else set())
        sb = {L[0] for L in layers[qb]} | ({base[qb]} if base[qb] else set())
        shared = sorted(sa & sb)
        onlya = sorted(sa - sb)
        onlyb = sorted(sb - sa)
        print('  %s | %s across %s' % (QNAME[qa], QNAME[qb], where))
        print('      shared textures %d, only in %s: %s, only in %s: %s'
              % (len(shared), QNAME[qa], [tex(f) for f in onlya] or 'none',
                 QNAME[qb], [tex(f) for f in onlyb] or 'none'))
        for f in shared:
            ga = next((L[2] for L in layers[qa] if L[0] == f), None)
            gb = next((L[2] for L in layers[qb] if L[0] == f), None)
            if ga is None or gb is None:
                continue
            if where == 'col 16':
                va, vb = ga[:, 16], gb[:, 0]
            else:
                va, vb = ga[16, :], gb[0, :]
            d = np.abs(va - vb)
            print('      %-34s shared-edge |a-b| max %.4f mean %.4f'
                  % (tex(f), d.max(), d.mean()))

    # ---------------------------------------------------------------- C
    print()
    print('=== C. the ingredient list either side of each centre line.')
    print('    A quad is named by its SW corner on the 33x33 grid.  The pair')
    print('    (15,c)/(16,c) and (r,15)/(r,16) are the two quads that touch')
    print('    the line from opposite quadrants.  A seam is a change of')
    print('    INGREDIENTS, not of weights.')

    def passes_for(row, col):
        """Exactly src/cellsplat.cpp passesFor(): quadrant from the quad's own
        SW corner, every corner read out of THAT quadrant, base first, then
        layers with any weight, ascending by ATXT paint index."""
        q = (1 if row >= 16 else 0) * 2 + (1 if col >= 16 else 0)
        out = []
        if base[q]:
            out.append(('base', base[q], -1, 1.0))
        rr = [row, row, row + 1, row + 1]
        cc = [col, col + 1, col + 1, col]
        cand = []
        for lt, idx, grid in layers[q]:
            if not lt:
                continue
            w = []
            for k in range(4):
                lr, lc, _ok = local(q, rr[k], cc[k])
                w.append(grid[min(max(lr, 0), 16)][min(max(lc, 0), 16)])
            if max(w) < 1 / 255.0:
                continue
            cand.append((idx, lt, max(w)))
        cand.sort()
        for idx, lt, w in cand:
            out.append(('layer', lt, idx, w))
        return q, out

    def show(tag, row, col):
        q, ps = passes_for(row, col)
        names = ['%s%s(%.2f)' % (tex(f), '' if k == 'base' else '#%d' % i, w)
                 for k, f, i, w in ps]
        promoted = ps and ps[0][0] == 'layer'
        print('    %-10s quad(%2d,%2d) quadrant %s%s : %s'
              % (tag, row, col, QNAME[q],
                 ' PROMOTED-OPAQUE' if promoted else '',
                 ' | '.join(names) if names else 'BARE'))

    for col in (4, 12, 20, 28):
        print('  --- horizontal centre line, col %d' % col)
        show('below', 15, col)
        show('above', 16, col)
    for row in (4, 12, 20, 28):
        print('  --- vertical centre line, row %d' % row)
        show('left', row, 15)
        show('right', row, 16)

    # how many quads either side of a line change their FIRST (visually
    # dominant) ingredient -- the number that decides whether the seam is real
    print()
    print('=== D. how many quad pairs across a centre line differ in their')
    print('    FIRST pass, and how many differ only because one side was')
    print('    PROMOTED (a lane invention) rather than because the record')
    print('    gave them different paint.')
    stats = {'pairs': 0, 'first-differs': 0, 'one-promoted': 0,
             'both-have-btxt': 0, 'first-differs-both-btxt': 0}
    def firstof(row, col):
        q, ps = passes_for(row, col)
        return q, (ps[0] if ps else None)
    for col in range(32):
        stats['pairs'] += 1
        qa, a = firstof(15, col)
        qb, b = firstof(16, col)
        if a and b:
            if base[qa] and base[qb]:
                stats['both-have-btxt'] += 1
                if a[1] != b[1]:
                    stats['first-differs-both-btxt'] += 1
            if a[1] != b[1]:
                stats['first-differs'] += 1
            if (a[0] == 'layer') != (b[0] == 'layer'):
                stats['one-promoted'] += 1
    for row in range(32):
        stats['pairs'] += 1
        qa, a = firstof(row, 15)
        qb, b = firstof(row, 16)
        if a and b:
            if base[qa] and base[qb]:
                stats['both-have-btxt'] += 1
                if a[1] != b[1]:
                    stats['first-differs-both-btxt'] += 1
            if a[1] != b[1]:
                stats['first-differs'] += 1
            if (a[0] == 'layer') != (b[0] == 'layer'):
                stats['one-promoted'] += 1
    for k in ('pairs', 'first-differs', 'one-promoted', 'both-have-btxt',
              'first-differs-both-btxt'):
        print('    %-26s %d' % (k, stats[k]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
