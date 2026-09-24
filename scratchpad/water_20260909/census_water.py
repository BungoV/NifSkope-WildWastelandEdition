#!/usr/bin/env python3
"""census_water.py -- lane WATER1's measured census of the Commonwealth's water.

Inputs, both MASTERS, neither of them our own judgement of our own output:
  E:\\Projects\\Fallout 4 Mods\\mods\\FO4CS\\Terrain\\Commonwealth.lodl   (v2)
  X:\\...\\Fallout 4\\Data\\Fallout4.esm  via esm_water.py's pickle

The control that has to pass first is control_synth.py.

Run:  python census_water.py [--playable] [--out report.txt]
"""

import argparse
import collections
import json
import os
import pickle
import sys
import time

import numpy as np

import ccl
import lodl_bulk as B
import water_model as WM

HERE = os.path.dirname(os.path.abspath(__file__))
LODL = r'E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl'
PKL = os.path.join(HERE, 'esm_water_0000003C.pkl')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lodl', default=LODL)
    ap.add_argument('--out', default=os.path.join(HERE, 'census_commonwealth.json'))
    ap.add_argument('--min-area', type=int, default=0)
    args = ap.parse_args()

    t0 = time.time()
    d = B.open_lodl(args.lodl)
    words = B.bulk_height_words(d)
    heights = (words.astype(np.float32) - 32767.0) * d.quantum
    del words
    lo, hi, wh, wt, fl = B.cell_table(d)
    watr = B.watr_table(d)
    defH, defT = B.default_water(d)
    esm = pickle.load(open(PKL, 'rb'))
    edid = {f: r['edid'] for f, r in esm['watr'].items()}
    print('loaded in %.1fs  cells %dx%d  texels %s  default water %.1f type %08X (%s)'
          % (time.time() - t0, d.cellsX, d.cellsY, heights.shape, defH, defT,
             edid.get(defT, '?')))

    hasWater = (fl & 1) > 0
    t0 = time.time()
    wet, whT, surf, nsurf, body, nbody = WM.segment(heights, wh, hasWater, wt, d.spc)
    print('segmented in %.1fs  wet texels %d (%.1f%%)  surfaces %d  bodies %d'
          % (time.time() - t0, int(wet.sum()), 100.0 * wet.sum() / wet.size, nsurf, nbody))

    tkey = WM.expand_cells(wt.astype(np.int32), d.spc)
    t0 = time.time()
    stats = WM.body_stats(wet, whT, body, nbody, surf, tkey, defH)
    print('body stats in %.1fs' % (time.time() - t0))

    nb = WM.neighbour_bodies(body, nbody)

    # class + naming
    for b, rec in stats.items():
        rec['class'] = WM.classify(rec, defH)
        ti = rec['type']
        rec['watr'] = int(watr[ti]) if ti != 0xFFFF else int(defT)
        rec['edid'] = edid.get(rec['watr'], '%08X' % rec['watr'])
        rec['inherits_default'] = (ti == 0xFFFF)
        rec['neighbours'] = sorted(nb.get(b, ()))
        # world-space bbox, in CELL coordinates, for a human to find it
        rec['cellx0'] = d.minX + rec['x0'] // d.spc
        rec['cellx1'] = d.minX + rec['x1'] // d.spc
        rec['celly0'] = d.minY + rec['y0'] // d.spc
        rec['celly1'] = d.minY + rec['y1'] // d.spc

    # a sea body's neighbours that are rivers make the OUTLET question answerable
    sea_labels = {b for b, r in stats.items() if r['class'] == 'sea'}
    for b, rec in stats.items():
        rec['touches_sea'] = sorted(set(rec['neighbours']) & sea_labels)

    # flow feasibility
    ys_all, xs_all = np.nonzero(wet)
    lb_all = body[ys_all, xs_all]
    order = np.argsort(lb_all, kind='stable')
    lb_s = lb_all[order]
    ys_s, xs_s = ys_all[order], xs_all[order]
    b0 = np.searchsorted(lb_s, np.arange(1, nbody + 1), 'left')
    b1 = np.searchsorted(lb_s, np.arange(1, nbody + 1), 'right')
    for b, rec in stats.items():
        i0, i1 = b0[b - 1], b1[b - 1]
        by, bx = ys_s[i0:i1], xs_s[i0:i1]
        hs = whT[by, bx]
        r, aniso = WM.height_monotonicity(by, bx, hs)
        rec['mono_r'] = round(r, 3)
        rec['aniso'] = round(aniso, 3)
        rec['drop'] = round(float(hs.max() - hs.min()), 2)
        rec['rule'] = flow_rule(rec)

    out = {
        'lodl': args.lodl,
        'cells': [int(d.minX), int(d.minY), int(d.maxX), int(d.maxY)],
        'spc': int(d.spc),
        'defaultWaterHeight': float(defH),
        'defaultWaterType': int(defT),
        'defaultWaterEdid': edid.get(defT, ''),
        'watr': [{'index': i, 'form': int(f), 'edid': edid.get(int(f), '')}
                 for i, f in enumerate(watr)],
        'wetTexels': int(wet.sum()),
        'totalTexels': int(wet.size),
        'nSurfaces': int(nsurf),
        'nBodies': int(nbody),
        'bodies': {str(k): v for k, v in stats.items()},
    }
    with open(args.out, 'w') as f:
        json.dump(out, f, indent=1, default=str)
    print('wrote', args.out)

    summarise(out, stats, d)
    np.save(os.path.join(HERE, 'body_lab.npy'), body.astype(np.int32))
    np.save(os.path.join(HERE, 'wet.npy'), wet)
    np.save(os.path.join(HERE, 'whT.npy'), whT.astype(np.float32))
    print('saved label/wet/waterheight grids for the flow lane')


def flow_rule(rec):
    """Which automatic flow rule can serve this body, and how confident."""
    if rec['class'] == 'lake':
        return 'zero'
    if rec['nsurf'] > 1 and abs(rec['mono_r']) >= 0.7:
        return 'gradient'
    if rec['nsurf'] > 1:
        return 'gradient-weak'
    if rec['class'] == 'sea':
        return 'zero-or-tide'
    if rec['touches_sea']:
        return 'centreline'
    return 'stroke'


def summarise(out, stats, d):
    print()
    print('== class counts ==')
    c = collections.Counter(r['class'] for r in stats.values())
    for k, v in c.most_common():
        print('  %-6s %5d' % (k, v))
    print('== area distribution (texels; 1 texel = 128x128 world units) ==')
    areas = sorted((r['area'] for r in stats.values()), reverse=True)
    if areas:
        q = np.percentile(areas, [50, 90, 99])
        print('  max %d   p99 %.0f   p90 %.0f   median %.0f   min %d   total %d'
              % (areas[0], q[2], q[1], q[0], areas[-1], sum(areas)))
        buckets = collections.Counter()
        for a in areas:
            if a < 4:
                buckets['<4'] += 1
            elif a < 16:
                buckets['4-15'] += 1
            elif a < 64:
                buckets['16-63'] += 1
            elif a < 1024:
                buckets['64-1k'] += 1
            elif a < 65536:
                buckets['1k-64k'] += 1
            else:
                buckets['>=64k'] += 1
        for k in ['<4', '4-15', '16-63', '64-1k', '1k-64k', '>=64k']:
            print('    %-7s %d' % (k, buckets.get(k, 0)))
    print('== the 25 biggest bodies ==')
    print('  %-5s %-26s %-6s %9s %5s %8s %6s %5s %s'
          % ('id', 'edid', 'class', 'area', 'surf', 'drop', 'mono', 'anis', 'cells'))
    for b, r in sorted(stats.items(), key=lambda kv: -kv[1]['area'])[:25]:
        print('  %-5d %-26s %-6s %9d %5d %8.1f %+6.2f %5.2f (%d..%d, %d..%d)'
              % (b, r['edid'][:26], r['class'], r['area'], r['nsurf'], r['drop'],
                 r['mono_r'], r['aniso'], r['cellx0'], r['cellx1'], r['celly0'], r['celly1']))
    print('== stepped vs flat ==')
    riv = [r for r in stats.values() if r['class'] == 'river']
    print('  rivers %d   stepped (>1 surface) %d   flat (1 surface) %d'
          % (len(riv), sum(1 for r in riv if r['nsurf'] > 1),
             sum(1 for r in riv if r['nsurf'] == 1)))
    print('== flow rule ==')
    fr = collections.Counter(r['rule'] for r in stats.values())
    for k, v in fr.most_common():
        print('  %-14s %5d' % (k, v))
    print('== per WATR form ==')
    per = collections.defaultdict(lambda: [0, 0])
    for r in stats.values():
        per[r['edid']][0] += 1
        per[r['edid']][1] += r['area']
    for k, (n, a) in sorted(per.items(), key=lambda kv: -kv[1][1]):
        print('  %-28s bodies %5d  texels %10d' % (k, n, a))


if __name__ == '__main__':
    main()
