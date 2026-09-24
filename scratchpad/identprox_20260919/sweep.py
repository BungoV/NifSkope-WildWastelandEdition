#!/usr/bin/env python3
"""IDENTPROX (S) -- the proximity-join sweep on chunk 4.4.-12.

(a) who may join x (b) gap x (c) measure, with the five numbers per cell and
the two failure counts that decide anything.

THE CONTROL RUNS FIRST.  Cell (arch, aabb, 16) is the SHIPPED rule, so it must
reproduce the shipped file exactly -- 588 groups, 468 singletons, largest 205,
and no group of the simulation spanning two groups of the file or the other way
round.  If that line does not print PASS, no other row in the table means
anything.

THE TWO FAILURE COUNTS
  SHATTER      a "one building" reference still split into more than one
               identity.  The references are the Creation Kit XLYR layers on
               this chunk whose editor id carries bld/building/tower/bldg --
               `lodlevels_20260919/ident_notes.md` measured those as 97% one
               building -- plus the hand list bungo named (DN135_GwinnettExt,
               AndrewStation, Theater47_Bld01).  On this chunk that is 5
               layers holding 582 of the 2,449 placements.
  OVER-MERGE   an identity too big to be one building.  Two counts, because
               they fail differently:
                 extent  the identity's XY footprint is wider than the WIDEST
                         single reference building on this chunk (measured:
                         6,110 u, DN135_GwinnettExt).  Not a typed 6,000.
                 2 refs  the identity holds placements of TWO different
                         reference buildings -- the sharp one, it cannot be
                         explained away by a long building.
                 layers  the identity spans two different CK layers of any
                         kind.  South Boston is laid out by CITY BLOCK and a
                         street runs between two blocks, so this is the count
                         that says "a row of houses has been welded".
"""
import json
import numpy as np
import sys
import time

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
if LANE not in sys.path:
    sys.path.insert(0, LANE)
import join as JN                                           # noqa: E402

GAPS = (16.0, 32.0, 64.0, 128.0, 256.0)
WHO = ('arch', 'kit', 'all', 'attach')
MEAS = ('aabb', 'obb', 'mesh', 'meshv')
# the cells worth arguing about, for the per-building detail block
SHORT = (('arch', 'aabb', 16.0), ('arch', 'aabb', 256.0),
         ('kit', 'aabb', 16.0), ('kit', 'aabb', 64.0), ('kit', 'aabb', 128.0),
         ('kit', 'mesh', 64.0), ('kit', 'mesh', 128.0),
         ('all', 'mesh', 16.0), ('all', 'mesh', 32.0), ('all', 'mesh', 64.0),
         ('all', 'obb', 16.0), ('all', 'aabb', 16.0),
         ('attach', 'mesh', 32.0), ('attach', 'mesh', 64.0),
         ('attach', 'meshv', 64.0), ('attach', 'obb', 16.0))


def main():
    t0 = time.time()
    log = open(LANE + '/sweep.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    P = JN.Placements(verbose=False)
    pairs, gaps = JN.candidates(P, verbose=False)
    refs = JN.reference_layers(P)
    # the over-merge extent threshold, MEASURED off the reference buildings
    ex = []
    for nm, sel in refs.items():
        lo = np.nanmin(P.lo[sel], axis=0)
        hi = np.nanmax(P.hi[sel], axis=0)
        ex.append(float(max(hi[0] - lo[0], hi[1] - lo[1])))
    OVER = float(max(ex))
    p('reference "one building" layers on this chunk: %d, holding %d of %d placements'
      % (len(refs), sum(len(v) for v in refs.values()), P.n))
    for nm, sel in sorted(refs.items(), key=lambda kv: -len(kv[1])):
        lo = np.nanmin(P.lo[sel], axis=0)
        hi = np.nanmax(P.hi[sel], axis=0)
        p('   %-24s %4d placements, XY footprint %5.0f x %5.0f u'
          % (nm, len(sel), hi[0] - lo[0], hi[1] - lo[1]))
    p('OVER-MERGE extent threshold = %.0f u, the widest of those, not a typed number' % OVER)
    p('')

    # ---------------- the control
    sim = JN.identity(P, pairs, gaps, 'arch', 'aabb', 16.0)
    import collections
    m = collections.defaultdict(set)
    m2 = collections.defaultdict(set)
    for i in range(P.n):
        m[sim[i]].add(int(P.shipped[i]))
        m2[int(P.shipped[i])].add(int(sim[i]))
    a = len(np.unique(sim))
    b = len(np.unique(P.shipped))
    sz = np.bincount(np.unique(sim, return_inverse=True)[1])
    bad = sum(1 for v in m.values() if len(v) > 1) + sum(1 for v in m2.values() if len(v) > 1)
    ok = (a == b == 588 and int((sz == 1).sum()) == 468 and int(sz.max()) == 205 and bad == 0)
    p('CONTROL (arch, aabb, 16 u = the shipped rule): %d groups, %d singletons, largest %d, '
      '%d partition disagreements with the file -- %s'
      % (a, int((sz == 1).sum()), int(sz.max()), bad, 'PASS' if ok else 'FAIL'))
    if not ok:
        p('the control FAILED; the table below is not trustworthy')
    p('')

    rows = []
    hdr = ('%-7s %-6s %5s | %6s %6s %7s %9s | %7s %6s %6s %6s'
           % ('who', 'meas', 'gap', 'groups', 'singl', 'largest', 'lrg extent',
              'shatter', 'ext>', '2refs', 'layers'))
    p(hdr)
    p('-' * len(hdr))
    for who in WHO:
        for meas in MEAS:
            for gp in GAPS:
                ident = JN.identity(P, pairs, gaps, who, meas, gp)
                st = JN.score(P, ident, refs, OVER)
                st.update(dict(who=who, measure=meas, gap=gp))
                rows.append(st)
                p('%-7s %-6s %5.0f | %6d %6d %7d %9.0f | %4d/%-2d %6d %6d %6d'
                  % (who, meas, gp, st['groups'], st['singletons'], st['largest'],
                     st['largest_extent'], st['shatter'], st['nrefs'],
                     st['over_extent'], st['over_two_refs'], st['cross_layer']))
            p('')
    json.dump(dict(over_threshold=OVER, refs={k: [int(x) for x in v] for k, v in refs.items()},
                   rows=rows), open(LANE + '/sweep.json', 'w'), indent=1)

    # ---- the highway run, per cell: does the whole run become ONE identity?
    low = np.array([m2_.lower() for m2_ in P.model])
    hw = np.array([('hwdouble' in s or 'hwonramp' in s) for s in low])
    deck = np.array([('hwdouble' in s) for s in low])
    p('')
    p('THE HIGHWAY under each cell: identities over the 10 deck pieces / over all 19 '
      'deck+ramp pieces, and whether a building got swallowed with them')
    p('%-7s %-6s %5s | %5s %5s | %s' % ('who', 'meas', 'gap', 'deck', 'deck+ramp',
                                        'buildings sharing an identity with the highway'))
    hwrows = []
    for who in WHO:
        for meas in MEAS:
            for gp in GAPS:
                ident = JN.identity(P, pairs, gaps, who, meas, gp)
                nd = len(set(int(x) for x in ident[deck]))
                nh = len(set(int(x) for x in ident[hw]))
                ids = set(int(x) for x in ident[hw])
                swallowed = []
                for nm, sel in refs.items():
                    if len(set(int(x) for x in ident[sel]) & ids):
                        swallowed.append(nm)
                # how many NON-highway placements share an identity with the highway
                other = int(sum(1 for i in range(P.n)
                                if (not hw[i]) and int(ident[i]) in ids))
                hwrows.append(dict(who=who, measure=meas, gap=gp, deck_ids=nd,
                                   hw_ids=nh, others=other, swallowed=swallowed))
                p('%-7s %-6s %5.0f | %5d %5d | %d other placements%s'
                  % (who, meas, gp, nd, nh, other,
                     ('  <-- ' + ', '.join(swallowed)) if swallowed else ''))
    json.dump(hwrows, open(LANE + '/sweep_hwy.json', 'w'), indent=1)

    # ---- per-reference-building detail, for the cells worth arguing about
    p('')
    p('PER-BUILDING DETAIL -- identities each reference building is split into')
    names = sorted(refs, key=lambda k: -len(refs[k]))
    p('%-22s | %s' % ('cell', ' '.join('%-10s' % n[:10] for n in names)))
    for cell in SHORT:
        ident = JN.identity(P, pairs, gaps, *cell)
        cnt = [len(set(int(x) for x in ident[refs[n]])) for n in names]
        p('%-22s | %s' % ('%s %s %.0f' % cell,
                          ' '.join('%-10d' % c for c in cnt)))
    p('')
    p('%.1f s' % (time.time() - t0))
    log.close()


if __name__ == '__main__':
    main()
