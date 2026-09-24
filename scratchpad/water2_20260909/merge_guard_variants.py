#!/usr/bin/env python3
"""merge_guard_variants.py -- which guard keeps the ocean out of a painted body?

The known-answer control (`lodl --water-selftest`) found the same defect in rule
C's ADJACENT merge that bridge_variants.py found in rule D's bridge: an
inheriting component joins the painted component it touches with no statement of
which side is absorbed, so the synthetic worldspace's sea -- 73,728 texels,
touching the tidal reach of a 512-texel painted river at exactly the sea's
height -- came out named after the river.

Three candidate guards, measured end to end (rule C's merge AND rule D's
bridge, both), on the Commonwealth:

  EDGE   an inheriting component is not absorbed when it reaches the worldspace
         EDGE.  Principle: the ocean is not an unpainted reach of anything.
  SIZE   ... not when it is LARGER than the painted component.  Principle: a
         merge must not rename the larger water after the smaller.
  BOTH   neither.

Printed for each: the body count, the merges accepted and refused at each step,
and the five biggest bodies with the form they end up carrying.
"""

import collections
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
W1 = os.path.abspath(os.path.join(HERE, '..', 'water_20260909'))
sys.path.insert(0, W1)

import ccl                      # noqa: E402
import lodl_bulk as B           # noqa: E402
import water_model as WM        # noqa: E402
import census_water2 as C2      # noqa: E402

DEF = 0xFFFF


def build_components():
    """Rule C's components, from the .lodl, with the fields both merges need."""
    d = B.open_lodl(C2.LODL)
    words = B.bulk_height_words(d)
    heights = (words.astype(np.float32) - 32767.0) * d.quantum
    lo, hi, wh, wt, fl = B.cell_table(d)
    wet, whT = WM.wet_mask(heights, wh, (fl & 1) > 0, d.spc)
    hq = np.round(whT * 8.0).astype(np.int64)
    tq = WM.expand_cells(wt.astype(np.int64), d.spc)
    lab, n = ccl.label(wet, hq * 70000 + tq)
    print('rule C: %d components' % n)

    ys, xs = np.nonzero(wet)
    lb = lab[ys, xs]
    o = np.argsort(lb, kind='stable')
    lb_s, ys_s, xs_s = lb[o], ys[o], xs[o]
    i0 = np.searchsorted(lb_s, np.arange(1, n + 1), 'left')
    i1 = np.searchsorted(lb_s, np.arange(1, n + 1), 'right')
    comp = {}
    H, W = wet.shape
    for c in range(1, n + 1):
        a, b = i0[c - 1], i1[c - 1]
        if b <= a:
            continue
        by, bx = ys_s[a:b], xs_s[a:b]
        comp[c] = {
            'area': int(b - a), 'h': float(whT[by[0], bx[0]]),
            'type': int(tq[by[0], bx[0]]),
            'edge': bool(bx.min() == 0 or by.min() == 0
                         or bx.max() == W - 1 or by.max() == H - 1),
        }
    nb = WM.neighbour_bodies(lab, n)
    return d, lab, comp, nb


def guarded(inh, painted, mode):
    """May the INHERITING component `inh` be absorbed into `painted`?"""
    if mode in ('EDGE', 'BOTH') and inh['edge']:
        return False
    if mode in ('SIZE', 'BOTH') and inh['area'] >= painted['area']:
        return False
    return True


def run(d, lab, comp, nb, mode):
    uf = ccl.UF(max(comp) + 1)
    ambiguous = merged_c = refused_c = 0
    for c, rec in sorted(comp.items()):
        if rec['type'] != DEF:
            continue
        cand = [k for k in nb.get(c, ())
                if comp.get(k) and comp[k]['type'] != DEF
                and abs(comp[k]['h'] - rec['h']) < 0.01]
        if not cand:
            continue
        cand.sort(key=lambda k: (-comp[k]['area'], k))
        if len(cand) > 1:
            ambiguous += 1
        if guarded(rec, comp[cand[0]], mode):
            uf.union(c, cand[0])
            merged_c += 1
        else:
            refused_c += 1

    # rule-C bodies
    root = {c: uf.find(c) for c in comp}
    order, cb = {}, []
    for c in sorted(comp):
        r = root[c]
        if r not in order:
            order[r] = len(cb)
            cb.append({'area': 0, 'h': comp[c]['h'], 'types': collections.Counter(),
                       'edge': False, 'parts': 0, 'members': []})
        t = cb[order[r]]
        t['area'] += comp[c]['area']
        t['types'][comp[c]['type']] += comp[c]['area']
        t['edge'] |= comp[c]['edge']
        t['parts'] += 1
        t['members'].append(c)
    for t in cb:
        t['type'] = max(t['types'].items(), key=lambda kv: (kv[1], -kv[0]))[0]
    print('  %-4s rule C -> %d bodies (%d merged, %d refused, %d ambiguous)'
          % (mode, len(cb), merged_c, refused_c, ambiguous))

    # the rule-C label grid, then the exact bridge
    lut = np.zeros(lab.max() + 1, np.int32)
    for c in comp:
        lut[c] = order[root[c]] + 1
    blab = lut[lab].astype(np.int32)
    pairs = C2.pairs_within(blab, C2.GAP)
    duf = ccl.UF(len(cb) + 1)
    acc = ref = 0
    for (a, b) in sorted(pairs):
        A, Bb = cb[a - 1], cb[b - 1]
        if abs(A['h'] - Bb['h']) > 0.01:
            continue
        if A['type'] == Bb['type']:
            duf.union(a - 1, b - 1)
            acc += 1
            continue
        inhA, inhB = A['type'] == DEF, Bb['type'] == DEF
        if inhA and inhB:
            duf.union(a - 1, b - 1)
            acc += 1
        elif inhA or inhB:
            small = A if inhA else Bb
            big = Bb if inhA else A
            if guarded(small, big, mode):
                duf.union(a - 1, b - 1)
                acc += 1
            else:
                ref += 1
        else:
            ref += 1
    grp = {}
    for i in range(len(cb)):
        grp.setdefault(duf.find(i), []).append(i)
    rows = []
    for parts in grp.values():
        area = sum(cb[i]['area'] for i in parts)
        tc = collections.Counter()
        for i in parts:
            tc.update(cb[i]['types'])
        painted = [t for t in tc if t != DEF]
        ti = max(painted, key=lambda t: (tc[t], -t)) if painted else DEF
        maj = max(tc.items(), key=lambda kv: (kv[1], -kv[0]))[0]
        rows.append((area, ti, maj, len(parts)))
    rows.sort(reverse=True)
    dis = sum(1 for r in rows if r[1] != r[2])
    print('  %-4s rule D -> %d bodies (%d accepted, %d refused); %d bodies where '
          'the painted-majority and the majority-with-default forms disagree'
          % (mode, len(rows), acc, ref, dis))
    for r in rows[:5]:
        print('       %10d texels  painted form idx %5d  parts %d' % (r[0], r[1], r[3]))
    return len(rows)


def main():
    d, lab, comp, nb = build_components()
    for mode in ('NONE', 'EDGE', 'SIZE', 'BOTH'):
        run(d, lab, comp, nb, mode)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
