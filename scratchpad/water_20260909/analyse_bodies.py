#!/usr/bin/env python3
"""analyse_bodies.py -- the body rule, measured three ways, and the flow graph.

The first census (census_water.py) keyed bodies on WATER TYPE and found 804 of
them, 534 of which inherit the worldspace default -- a river is chopped wherever
a cell was never given an XCWT.  It also found that 803 of 804 carry exactly ONE
water height, which kills the "steps inside a river" hypothesis outright: FO4's
water is a set of FLAT plateaus, and every step is a step BETWEEN two touching
plateaus.

So this script measures the three candidate body rules on the same grid --

  A  connectivity + equal type            (the first census)
  B  connectivity + equal height          (a surface)
  C  connectivity + equal (height, type), then MERGE an inheriting component
     into the adjacent painted component it shares a height with

-- and builds the plateau adjacency graph (who touches whom, and at what height
difference) that the flow rule has to run on.
"""

import collections
import json
import os
import sys

import numpy as np

import ccl
import lodl_bulk as B
import water_model as WM

HERE = os.path.dirname(os.path.abspath(__file__))
LODL = r'E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl'
DEFAULT_TYPE_IDX = 0xFFFF


def load():
    d = B.open_lodl(LODL)
    words = B.bulk_height_words(d)
    heights = (words.astype(np.float32) - 32767.0) * d.quantum
    lo, hi, wh, wt, fl = B.cell_table(d)
    return d, heights, wh, wt, (fl & 1) > 0


def main():
    d, heights, wh, wt, hasWater = load()
    wet, whT = WM.wet_mask(heights, wh, hasWater, d.spc)
    hq = np.round(whT * 8.0).astype(np.int64)
    tq = WM.expand_cells(wt.astype(np.int64), d.spc)

    labA, nA = ccl.label(wet, tq.astype(np.int32))
    labB, nB = ccl.label(wet, hq.astype(np.int32))
    labC, nC = ccl.label(wet, hq * 70000 + tq)
    print('rule A  type-keyed        %6d bodies' % nA)
    print('rule B  height-keyed      %6d bodies' % nB)
    print('rule C  (height,type)     %6d components before the merge' % nC)

    # -- rule C's merge -----------------------------------------------------
    nb = WM.neighbour_bodies(labC, nC)
    ys, xs = np.nonzero(wet)
    lb = labC[ys, xs]
    order = np.argsort(lb, kind='stable')
    lb_s, ys_s, xs_s = lb[order], ys[order], xs[order]
    i0 = np.searchsorted(lb_s, np.arange(1, nC + 1), 'left')
    i1 = np.searchsorted(lb_s, np.arange(1, nC + 1), 'right')
    comp = {}
    for c in range(1, nC + 1):
        a, b = i0[c - 1], i1[c - 1]
        if b <= a:
            continue
        by, bx = ys_s[a:b], xs_s[a:b]
        comp[c] = {
            'area': int(b - a), 'h': float(whT[by[0], bx[0]]),
            'type': int(tq[by[0], bx[0]]),
            'x0': int(bx.min()), 'x1': int(bx.max()),
            'y0': int(by.min()), 'y1': int(by.max()),
            'cx': float(bx.mean()), 'cy': float(by.mean()),
            'edge': bool(bx.min() == 0 or by.min() == 0
                         or bx.max() == wet.shape[1] - 1 or by.max() == wet.shape[0] - 1),
        }

    uf = ccl.UF(nC + 1)
    ambiguous = 0
    for c, rec in comp.items():
        if rec['type'] != DEFAULT_TYPE_IDX:
            continue
        painted = [n for n in nb.get(c, ())
                   if comp.get(n) and comp[n]['type'] != DEFAULT_TYPE_IDX
                   and abs(comp[n]['h'] - rec['h']) < 0.01]
        if len(painted) == 1:
            uf.union(c, painted[0])
        elif len(painted) > 1:
            ambiguous += 1
            # merge into the largest, and count it as a decision the user may undo
            painted.sort(key=lambda n: -comp[n]['area'])
            uf.union(c, painted[0])
    roots = {c: uf.find(c) for c in comp}
    merged = len(set(roots.values()))
    print('rule C  after merging inheriting components into a same-height painted'
          ' neighbour: %6d bodies  (%d of the merges had more than one candidate)'
          % (merged, ambiguous))

    # -- the merged bodies --------------------------------------------------
    remap = {}
    bodies = {}
    for c, r in roots.items():
        b = remap.setdefault(r, len(remap) + 1)
        rec = comp[c]
        t = bodies.setdefault(b, {'area': 0, 'h': rec['h'], 'types': collections.Counter(),
                                  'x0': 1 << 30, 'x1': -1, 'y0': 1 << 30, 'y1': -1,
                                  'edge': False, 'parts': 0, 'sx': 0.0, 'sy': 0.0})
        t['area'] += rec['area']
        t['types'][rec['type']] += rec['area']
        t['x0'] = min(t['x0'], rec['x0'])
        t['x1'] = max(t['x1'], rec['x1'])
        t['y0'] = min(t['y0'], rec['y0'])
        t['y1'] = max(t['y1'], rec['y1'])
        t['edge'] |= rec['edge']
        t['parts'] += 1
        t['sx'] += rec['cx'] * rec['area']
        t['sy'] += rec['cy'] * rec['area']
    for b, t in bodies.items():
        t['cx'] = t['sx'] / t['area']
        t['cy'] = t['sy'] / t['area']
        t['type'] = t['types'].most_common(1)[0][0]

    # adjacency between merged bodies
    badj = collections.defaultdict(set)
    for c, ns in nb.items():
        if c not in roots:
            continue
        a = remap[roots[c]]
        for n in ns:
            if n in roots:
                bb = remap[roots[n]]
                if bb != a:
                    badj[a].add(bb)
                    badj[bb].add(a)

    watr = B.watr_table(d)
    import pickle
    esm = pickle.load(open(os.path.join(HERE, 'esm_water_0000003C.pkl'), 'rb'))
    edid = {f: r['edid'] for f, r in esm['watr'].items()}
    defH, defT = B.default_water(d)

    def name(t):
        return edid.get(int(watr[t]) if t != DEFAULT_TYPE_IDX else int(defT), '?')

    out = {}
    for b, t in bodies.items():
        lower = [n for n in badj[b] if bodies[n]['h'] < t['h'] - 0.01]
        higher = [n for n in badj[b] if bodies[n]['h'] > t['h'] + 0.01]
        out[b] = {
            'area': t['area'], 'h': t['h'], 'parts': t['parts'],
            'edid': name(t['type']), 'typeIdx': t['type'],
            'inherits': t['type'] == DEFAULT_TYPE_IDX,
            'cellx0': d.minX + t['x0'] // d.spc, 'cellx1': d.minX + t['x1'] // d.spc,
            'celly0': d.minY + t['y0'] // d.spc, 'celly1': d.minY + t['y1'] // d.spc,
            'edge': t['edge'],
            'nlower': len(lower), 'nhigher': len(higher),
            'lower': sorted(lower), 'higher': sorted(higher),
            'ndeg': len(badj[b]),
            'elong': round(max(t['x1'] - t['x0'] + 1, t['y1'] - t['y0'] + 1) ** 2
                           / float(t['area']), 2),
        }
    json.dump({'bodies': {str(k): v for k, v in out.items()},
               'ruleA': nA, 'ruleB': nB, 'ruleC_pre': nC, 'ruleC': merged,
               'ambiguousMerges': ambiguous},
              open(os.path.join(HERE, 'bodies_ruleC.json'), 'w'), indent=1)
    np.save(os.path.join(HERE, 'labC.npy'), labC)
    with open(os.path.join(HERE, 'ruleC_map.json'), 'w') as f:
        json.dump({'roots': {str(k): remap[v] for k, v in roots.items()}}, f)

    print()
    print('== rule C bodies: class from the plateau graph ==')
    cls = collections.Counter()
    for b, r in out.items():
        r['class'] = classify(r)
        cls[r['class']] += 1
    for k, v in cls.most_common():
        print('  %-8s %5d' % (k, v))

    print()
    print('  %-4s %-26s %-8s %8s %8s %5s %4s %4s %5s %s'
          % ('id', 'edid', 'class', 'area', 'height', 'parts', 'lo', 'hi', 'elong', 'cells'))
    for b, r in sorted(out.items(), key=lambda kv: -kv[1]['area'])[:30]:
        print('  %-4d %-26s %-8s %8d %8.1f %5d %4d %4d %5.1f (%d..%d, %d..%d)'
              % (b, r['edid'][:26], r['class'], r['area'], r['h'], r['parts'],
                 r['nlower'], r['nhigher'], r['elong'],
                 r['cellx0'], r['cellx1'], r['celly0'], r['celly1']))

    print()
    print('== areas ==')
    ar = sorted((r['area'] for r in out.values()), reverse=True)
    print('  bodies %d  >=64 texels %d  >=1024 texels %d  <4 texels %d'
          % (len(ar), sum(1 for a in ar if a >= 64), sum(1 for a in ar if a >= 1024),
             sum(1 for a in ar if a < 4)))
    print()
    print('== per WATR form (rule C) ==')
    per = collections.defaultdict(lambda: [0, 0])
    for r in out.values():
        per[r['edid']][0] += 1
        per[r['edid']][1] += r['area']
    for k, (n, a) in sorted(per.items(), key=lambda kv: -kv[1][1]):
        print('  %-28s bodies %5d  texels %10d' % (k, n, a))


def classify(r):
    if r['edge']:
        return 'sea'
    if r['nlower'] >= 1 or r['nhigher'] >= 1:
        return 'river' if (r['elong'] >= 6.0 or r['nlower'] + r['nhigher'] >= 2) else 'pond'
    return 'lake'


if __name__ == '__main__':
    main()
