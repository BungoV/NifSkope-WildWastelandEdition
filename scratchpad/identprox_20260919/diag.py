#!/usr/bin/env python3
"""IDENTPROX -- name the OVER-MERGES.

A count of "1 identity wider than 6,110 u" means nothing on its own.  This
prints WHICH identity, what is in it, and which Creation Kit layers it spans,
for the cells the recommendation will argue about -- so "a street got welded"
is a statement about named placements and not a number.
"""
import numpy as np
import sys

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
if LANE not in sys.path:
    sys.path.insert(0, LANE)
import join as JN                                           # noqa: E402

CELLS = (('arch', 'aabb', 16.0),
         ('all', 'obb', 16.0),
         ('all', 'mesh', 32.0),
         ('all', 'mesh', 64.0),
         ('attach', 'mesh', 32.0),
         ('all', 'aabb', 16.0))


def main():
    P = JN.Placements(verbose=False)
    pairs, gaps = JN.candidates(P, verbose=False)
    refs = JN.reference_layers(P)
    OVER = 6110.0
    log = open(LANE + '/overmerge.txt', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')

    for cell in CELLS:
        ident = JN.identity(P, pairs, gaps, *cell)
        u, inv = np.unique(ident, return_inverse=True)
        p('')
        p('=== %s %s %.0f u -- %d identities' % (cell + (len(u),)))
        rows = []
        for g in range(len(u)):
            sel = np.nonzero(inv == g)[0]
            if len(sel) < 2:
                continue
            lo = np.nanmin(P.lo[sel], axis=0)
            hi = np.nanmax(P.hi[sel], axis=0)
            ext = float(max(hi[0] - lo[0], hi[1] - lo[1]))
            lays = set(int(x) for x in P.layer[sel] if x)
            rows.append((ext, len(sel), sel, lays))
        rows.sort(key=lambda r: -r[0])
        for ext, n, sel, lays in rows[:3]:
            nm = [P.layer_name.get(l, '?') for l in lays]
            # the three commonest base models in it
            import collections
            c = collections.Counter(P.model[i].split('\\')[-1] for i in sel)
            p('   widest identity: %5.0f u, %4d placements, %d CK layers %s%s'
              % (ext, n, len(lays), sorted(nm)[:4],
                 '   OVER (> %.0f u)' % OVER if ext > OVER else ''))
            p('      commonest bases: %s'
              % ', '.join('%s x%d' % (k, v) for k, v in c.most_common(4)))
        # how many identities hold placements of TWO reference buildings
        two = 0
        for g in range(len(u)):
            sel = np.nonzero(inv == g)[0]
            hits = [nm for nm, r in refs.items() if len(set(sel) & set(r.tolist()))]
            if len(hits) > 1:
                two += 1
                p('   TWO REFERENCE BUILDINGS in one identity: %s (%d placements)'
                  % (', '.join(hits), len(sel)))
        if not two:
            p('   no identity holds two reference buildings')
    log.close()


if __name__ == '__main__':
    main()
