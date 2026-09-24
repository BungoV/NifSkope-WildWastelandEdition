#!/usr/bin/env python3
"""flow_feasibility.py -- can a direction be COMPUTED for each water body, and
where does a human stroke have to be asked for?

The census killed the two rules the design assumed:
  * 803 of 804 bodies carry exactly ONE water height, so there is no gradient
    INSIDE a body to read a direction off;
  * plateaus almost never touch, so the step BETWEEN two bodies is not an
    adjacency either -- it is a gap of dry land.

So this measures the candidates that are left, per body, and says which of them
answers:
  axis     the body's principal axis (unsigned) and how anisotropic it is;
  bed      the slope of the TERRAIN under the water along that axis -- a river
           bed that falls gives the sign the flat surface does not;
  drain    the nearest OTHER body within 64 texels and whether it is lower;
  vanilla  the WATR form's own NAM0 Linear Velocity, which FO4 already ships
           per water TYPE (so every lake sharing ExtLakeWater flows the same
           way -- which is the defect bungo is asking to fix).
"""

import collections
import json
import os
import pickle

import numpy as np

import lodl_bulk as B
import water_model as WM

HERE = os.path.dirname(os.path.abspath(__file__))
LODL = r'E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl'
MIN_AREA = 64
NEAR = 64          # texels, 8192 world units


def main():
    d = B.open_lodl(LODL)
    words = B.bulk_height_words(d)
    heights = (words.astype(np.float32) - 32767.0) * d.quantum
    lo, hi, wh, wt, fl = B.cell_table(d)
    wet, whT = WM.wet_mask(heights, wh, (fl & 1) > 0, d.spc)
    lab = np.load(os.path.join(HERE, 'labC.npy'))
    remap = json.load(open(os.path.join(HERE, 'ruleC_map.json')))['roots']
    bodies = json.load(open(os.path.join(HERE, 'bodies_ruleC.json')))['bodies']

    # component label -> merged body id
    maxc = lab.max()
    lut = np.zeros(maxc + 1, np.int32)
    for c, b in remap.items():
        lut[int(c)] = b
    blab = lut[lab]

    esm = pickle.load(open(os.path.join(HERE, 'esm_water_0000003C.pkl'), 'rb'))
    watr = B.watr_table(d)
    defH, defT = B.default_water(d)
    vel = {}
    for f, r in esm['watr'].items():
        if r['NAM0']:
            import struct
            vel[f] = struct.unpack('<3f', r['NAM0'])

    big = {int(k): v for k, v in bodies.items() if v['area'] >= MIN_AREA}
    print('bodies with area >= %d texels: %d of %d' % (MIN_AREA, len(big), len(bodies)))

    ys, xs = np.nonzero(wet)
    lb = blab[ys, xs]
    order = np.argsort(lb, kind='stable')
    lb_s, ys_s, xs_s = lb[order], ys[order], xs[order]
    lo_i = np.searchsorted(lb_s, sorted(big), 'left')
    hi_i = np.searchsorted(lb_s, sorted(big), 'right')
    keys = sorted(big)

    pts = {}
    for k, a, b in zip(keys, lo_i, hi_i):
        by, bx = ys_s[a:b], xs_s[a:b]
        rec = big[k]
        ax, aniso = WM.principal_axis(by, bx)
        t = bx * ax[0] + by * ax[1]
        bed = heights[by, bx].astype(np.float64)
        depth = whT[by, bx] - heights[by, bx]
        if bed.std() > 0 and t.std() > 0:
            r_bed = float(np.corrcoef(t, bed)[0, 1])
            slope = float(np.polyfit(t, bed, 1)[0])          # units per texel
            drop = slope * (t.max() - t.min())
        else:
            r_bed, slope, drop = 0.0, 0.0, 0.0
        rec['axis'] = [round(float(ax[0]), 3), round(float(ax[1]), 3)]
        rec['aniso'] = round(float(aniso), 3)
        rec['bed_r'] = round(r_bed, 3)
        rec['bed_drop'] = round(float(drop), 1)
        rec['depth_mean'] = round(float(depth.mean()), 1)
        rec['depth_max'] = round(float(depth.max()), 1)
        rec['length_texels'] = int(t.max() - t.min() + 1)
        ti = rec['typeIdx']
        form = int(watr[ti]) if ti != 0xFFFF else int(defT)
        rec['nam0'] = [round(v, 3) for v in vel.get(form, (0, 0, 0))[:2]]
        # a subsampled point cloud for the proximity pass
        step = max(1, by.size // 1500)
        pts[k] = np.stack([bx[::step].astype(np.float32), by[::step].astype(np.float32)])

    # nearest other body within NEAR texels
    for k in keys:
        big[k]['near'] = []
    for i, k in enumerate(keys):
        ka = big[k]
        for k2 in keys[i + 1:]:
            kb = big[k2]
            if (ka['cellx0'] - kb['cellx1']) * d.spc > NEAR or (kb['cellx0'] - ka['cellx1']) * d.spc > NEAR:
                continue
            if (ka['celly0'] - kb['celly1']) * d.spc > NEAR or (kb['celly0'] - ka['celly1']) * d.spc > NEAR:
                continue
            pa, pb = pts[k], pts[k2]
            dx = pa[0][:, None] - pb[0][None, :]
            dy = pa[1][:, None] - pb[1][None, :]
            dmin = float(np.sqrt((dx * dx + dy * dy).min()))
            if dmin <= NEAR:
                ka['near'].append((k2, round(dmin, 1), kb['h']))
                kb['near'].append((k, round(dmin, 1), ka['h']))

    for k in keys:
        r = big[k]
        r['lower_near'] = [n for n in r['near'] if n[2] < r['h'] - 0.01]
        r['verdict'] = verdict(r)

    json.dump({str(k): v for k, v in big.items()},
              open(os.path.join(HERE, 'flow_feasibility.json'), 'w'), indent=1, default=str)

    print()
    print('== verdicts (bodies >= %d texels) ==' % MIN_AREA)
    c = collections.Counter(r['verdict'] for r in big.values())
    for k, v in c.most_common():
        print('  %-12s %4d' % (k, v))
    print()
    print('  %-4s %-26s %8s %8s %6s %6s %8s %6s %6s %-11s %s'
          % ('id', 'edid', 'area', 'height', 'aniso', 'bed_r', 'bed_drop', 'depth',
             'near', 'verdict', 'cells'))
    for k, r in sorted(big.items(), key=lambda kv: -kv[1]['area']):
        near = min([n[1] for n in r['near']], default=-1)
        print('  %-4d %-26s %8d %8.1f %6.2f %+6.2f %8.1f %6.1f %6.1f %-11s (%d..%d, %d..%d)'
              % (k, r['edid'][:26], r['area'], r['h'], r['aniso'], r['bed_r'],
                 r['bed_drop'], r['depth_mean'], near, r['verdict'],
                 r['cellx0'], r['cellx1'], r['celly0'], r['celly1']))


def verdict(r):
    """Which rule can serve this body's flow, on the numbers measured here."""
    if r['aniso'] < 0.5 and not r['lower_near']:
        return 'zero'                      # round-ish, no outlet: a lake
    if r['lower_near'] and r['aniso'] >= 0.5:
        return 'drain'                     # axis + a lower neighbour to point at
    if abs(r['bed_r']) >= 0.7 and abs(r['bed_drop']) >= 64.0 and r['aniso'] >= 0.5:
        return 'bed'                       # axis + a bed that falls one way
    if r['aniso'] >= 0.5:
        return 'stroke'                    # axis known, SIGN unknown
    return 'stroke-all'                    # neither axis nor sign


if __name__ == '__main__':
    main()
