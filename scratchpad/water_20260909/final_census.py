#!/usr/bin/env python3
"""final_census.py -- the census and flow-feasibility tables lane WATER1 reports.

Body rule D, which is what the spec proposes:
  1. wet texel   = terrain height < the cell's resolved water height, in a cell
                   with Has Water;
  2. component   = 4-connected wet texels with equal water HEIGHT and equal
                   water TYPE;
  3. merge       an inheriting component into the same-height painted component
                   it touches (a river reach whose cells were never given an
                   XCWT);
  4. bridge      two components at the same height whose shores are within 2
                   texels (256 world units), when their types match or one
                   inherits -- the 128-unit sample grid pinches a river shut at
                   bridges and narrows;
  5. class       sea    reaches the worldspace edge
                 river  elongation >= 6, or a lower body within 64 texels
                 lake   everything else.

Every number this prints goes into scratchpad/lane_water1_report.md.
"""

import collections
import json
import os
import pickle
import struct

import numpy as np

import ccl
import lodl_bulk as B
import water_model as WM

HERE = os.path.dirname(os.path.abspath(__file__))
LODL = r'E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl'
GAP = 2
NEAR = 64
MIN_REPORT = 64


def main():
    d = B.open_lodl(LODL)
    words = B.bulk_height_words(d)
    heights = (words.astype(np.float32) - 32767.0) * d.quantum
    lo, hi, wh, wt, fl = B.cell_table(d)
    wet, whT = WM.wet_mask(heights, wh, (fl & 1) > 0, d.spc)
    lab = np.load(os.path.join(HERE, 'labC.npy'))
    remap = json.load(open(os.path.join(HERE, 'ruleC_map.json')))['roots']
    bodies = json.load(open(os.path.join(HERE, 'bodies_ruleC.json')))['bodies']
    lut = np.zeros(lab.max() + 1, np.int32)
    for c, b in remap.items():
        lut[int(c)] = b
    blab = lut[lab]

    ys, xs = np.nonzero(wet)
    lb = blab[ys, xs]
    o = np.argsort(lb, kind='stable')
    lb_s, ys_s, xs_s = lb[o], ys[o], xs[o]
    keys = sorted(int(k) for k in bodies)
    a0 = np.searchsorted(lb_s, keys, 'left')
    a1 = np.searchsorted(lb_s, keys, 'right')
    info = {k: bodies[str(k)] for k in keys}
    pts = {}
    for k, a, b in zip(keys, a0, a1):
        by, bx = ys_s[a:b], xs_s[a:b]
        st = max(1, by.size // 4000)
        pts[k] = np.stack([bx[::st].astype(np.float32), by[::st].astype(np.float32)])

    # step 4: bridge
    uf = ccl.UF(max(keys) + 2)
    accepted = refused = 0
    for i, k in enumerate(keys):
        A = info[k]
        for k2 in keys[i + 1:]:
            Bb = info[k2]
            if abs(A['h'] - Bb['h']) > 0.01:
                continue
            if (A['cellx0'] - Bb['cellx1']) * d.spc > GAP or (Bb['cellx0'] - A['cellx1']) * d.spc > GAP:
                continue
            if (A['celly0'] - Bb['celly1']) * d.spc > GAP or (Bb['celly0'] - A['celly1']) * d.spc > GAP:
                continue
            pa, pb = pts[k], pts[k2]
            dx = pa[0][:, None] - pb[0][None, :]
            dy = pa[1][:, None] - pb[1][None, :]
            if float(np.sqrt((dx * dx + dy * dy).min())) > GAP:
                continue
            if (A['typeIdx'] == Bb['typeIdx']) or A['inherits'] or Bb['inherits']:
                uf.union(k, k2)
                accepted += 1
            else:
                refused += 1
    group = {}
    for k in keys:
        group.setdefault(uf.find(k), []).append(k)
    gid = {r: i + 1 for i, r in enumerate(sorted(group))}
    dlab = np.zeros_like(blab)
    m = np.zeros(max(keys) + 2, np.int32)
    for r, ks in group.items():
        for k in ks:
            m[k] = gid[r]
    dlab = m[blab]

    esm = pickle.load(open(os.path.join(HERE, 'esm_water_0000003C.pkl'), 'rb'))
    edid = {f: r['edid'] for f, r in esm['watr'].items()}
    nam0 = {f: struct.unpack('<3f', r['NAM0'])[:2] for f, r in esm['watr'].items() if r['NAM0']}
    watr = B.watr_table(d)
    defH, defT = B.default_water(d)

    recs = {}
    ys, xs = np.nonzero(dlab > 0)
    lb = dlab[ys, xs]
    o = np.argsort(lb, kind='stable')
    lb_s, ys_s, xs_s = lb[o], ys[o], xs[o]
    gkeys = sorted(gid.values())
    a0 = np.searchsorted(lb_s, gkeys, 'left')
    a1 = np.searchsorted(lb_s, gkeys, 'right')
    for g, a, b in zip(gkeys, a0, a1):
        by, bx = ys_s[a:b], xs_s[a:b]
        parts = group[[r for r in group if gid[r] == g][0]]
        h = info[parts[0]]['h']
        tcount = collections.Counter()
        for k in parts:
            tcount[info[k]['typeIdx']] += info[k]['area']
        painted = [t for t in tcount if t != 0xFFFF]
        ti = max(painted, key=lambda t: tcount[t]) if painted else 0xFFFF
        form = int(watr[ti]) if ti != 0xFFFF else int(defT)
        ax, aniso = WM.principal_axis(by, bx)
        t = bx * ax[0] + by * ax[1]
        bed = heights[by, bx].astype(np.float64)
        depth = (whT[by, bx] - heights[by, bx]).astype(np.float64)
        r_bed = float(np.corrcoef(t, bed)[0, 1]) if bed.std() > 0 and t.std() > 0 else 0.0
        slope = float(np.polyfit(t, bed, 1)[0]) if bed.std() > 0 and t.std() > 0 else 0.0
        w = bx.max() - bx.min() + 1
        hgt = by.max() - by.min() + 1
        recs[g] = {
            'area': int(by.size), 'h': float(h), 'edid': edid.get(form, '%08X' % form),
            'form': form, 'typeIdx': int(ti), 'inherits': ti == 0xFFFF,
            'parts': len(parts),
            'cellx0': int(d.minX + bx.min() // d.spc), 'cellx1': int(d.minX + bx.max() // d.spc),
            'celly0': int(d.minY + by.min() // d.spc), 'celly1': int(d.minY + by.max() // d.spc),
            'edge': bool(bx.min() == 0 or by.min() == 0 or bx.max() == wet.shape[1] - 1
                         or by.max() == wet.shape[0] - 1),
            'elong': round(max(w, hgt) ** 2 / float(by.size), 2),
            'aniso': round(float(aniso), 3),
            'axis': [round(float(ax[0]), 3), round(float(ax[1]), 3)],
            'bed_r': round(r_bed, 3), 'bed_drop': round(slope * (t.max() - t.min()), 1),
            'depth_mean': round(float(depth.mean()), 1),
            'depth_max': round(float(depth.max()), 1),
            'shore_len_texels': int(w + hgt),
            'nam0': [round(v, 3) for v in nam0.get(form, (0.0, 0.0))],
        }
        st = max(1, by.size // 4000)
        pts[('g', g)] = np.stack([bx[::st].astype(np.float32), by[::st].astype(np.float32)])

    # nearest lower body within NEAR texels
    big = [g for g in gkeys if recs[g]['area'] >= 16]
    for g in gkeys:
        recs[g]['near'] = []
    for i, g in enumerate(big):
        A = recs[g]
        for g2 in big[i + 1:]:
            Bb = recs[g2]
            if (A['cellx0'] - Bb['cellx1']) * d.spc > NEAR or (Bb['cellx0'] - A['cellx1']) * d.spc > NEAR:
                continue
            if (A['celly0'] - Bb['celly1']) * d.spc > NEAR or (Bb['celly0'] - A['celly1']) * d.spc > NEAR:
                continue
            pa, pb = pts[('g', g)], pts[('g', g2)]
            dx = pa[0][:, None] - pb[0][None, :]
            dy = pa[1][:, None] - pb[1][None, :]
            dm = float(np.sqrt((dx * dx + dy * dy).min()))
            if dm <= NEAR:
                A['near'].append([g2, round(dm, 1), Bb['h']])
                Bb['near'].append([g, round(dm, 1), A['h']])
    for g in gkeys:
        r = recs[g]
        r['lower_near'] = [n for n in r['near'] if n[2] < r['h'] - 0.01]
        r['class'] = classify(r)
        r['flow'] = flow_rule(r)

    json.dump({'bodies': {str(k): v for k, v in recs.items()},
               'gap': GAP, 'accepted': accepted, 'refused': refused,
               'nBodies': len(recs)},
              open(os.path.join(HERE, 'census_ruleD.json'), 'w'), indent=1)
    np.save(os.path.join(HERE, 'body_id_plane.npy'), dlab.astype(np.uint16))

    report(d, recs, wet, wh, wt, fl, accepted, refused, edid, watr, defH, defT)


def classify(r):
    if r['edge']:
        return 'sea'
    if r['elong'] >= 6.0 or r['lower_near']:
        return 'river'
    return 'lake'


def flow_rule(r):
    if r['class'] == 'sea':
        return 'zero'
    if r['aniso'] < 0.5 and not r['lower_near']:
        return 'zero'
    if r['lower_near'] and r['aniso'] >= 0.5:
        return 'drain'
    if abs(r['bed_r']) >= 0.7 and abs(r['bed_drop']) >= 64.0 and r['aniso'] >= 0.5:
        return 'bed'
    return 'stroke'


def report(d, recs, wet, wh, wt, fl, accepted, refused, edid, watr, defH, defT):
    P = print
    P('== INPUT ==')
    P('  Commonwealth.lodl v%d  cells %d..%d x %d..%d = %d  spc %d  texel %.0f units'
      % (d.version, d.minX, d.maxX, d.minY, d.maxY, d.cellsX * d.cellsY, d.spc, 4096.0 / d.spc))
    P('  worldspace default water height %.1f  type %08X %s (WRLD DNAM/NAM3/NAM4)'
      % (defH, defT, edid.get(defT, '?')))
    P('  cells with Has Water %d of %d (%.2f%%)'
      % (int((fl & 1).astype(bool).sum()), fl.size, 100.0 * (fl & 1).astype(bool).sum() / fl.size))
    P('  cells inheriting the default water TYPE  %d (%.2f%%)'
      % (int((wt == 0xFFFF).sum()), 100.0 * (wt == 0xFFFF).sum() / wt.size))
    P('  cells inheriting the default water HEIGHT %d (%.2f%%)'
      % (int((wh == defH).sum()), 100.0 * (wh == defH).sum() / wh.size))
    P('  distinct water heights %d   WATR forms interned %d'
      % (len(np.unique(wh)), d.nWatr))
    P('  wet texels %d of %d (%.1f%%)' % (int(wet.sum()), wet.size, 100.0 * wet.sum() / wet.size))
    P('  bridge merges accepted %d, refused (two painted types) %d' % (accepted, refused))
    P()
    P('== BODIES (rule D) ==')
    P('  total %d' % len(recs))
    for t in (1, 4, 16, 64, 256, 1024, 65536):
        P('    area >= %6d texels : %4d' % (t, sum(1 for r in recs.values() if r['area'] >= t)))
    c = collections.Counter(r['class'] for r in recs.values())
    P('  class: ' + '  '.join('%s %d' % kv for kv in c.most_common()))
    c2 = collections.Counter(r['class'] for r in recs.values() if r['area'] >= MIN_REPORT)
    P('  class (area >= %d): ' % MIN_REPORT + '  '.join('%s %d' % kv for kv in c2.most_common()))
    nsteps = sum(1 for r in recs.values() if r['parts'] > 1)
    P('  bodies assembled from more than one component: %d' % nsteps)
    P()
    P('== PER WATR FORM ==')
    per = collections.defaultdict(lambda: [0, 0, set()])
    for r in recs.values():
        per[r['edid']][0] += 1
        per[r['edid']][1] += r['area']
        per[r['edid']][2].add((r['cellx0'], r['celly0']))
    for k, (n, a, w) in sorted(per.items(), key=lambda kv: -kv[1][1]):
        P('  %-28s bodies %4d  texels %9d' % (k, n, a))
    P()
    P('== FLOW RULE ==')
    fr = collections.Counter(r['flow'] for r in recs.values())
    P('  all bodies : ' + '  '.join('%s %d' % kv for kv in fr.most_common()))
    fr2 = collections.Counter(r['flow'] for r in recs.values() if r['area'] >= MIN_REPORT)
    P('  area >= %d : ' % MIN_REPORT + '  '.join('%s %d' % kv for kv in fr2.most_common()))
    P()
    P('== THE BODIES A HUMAN MUST STROKE (area >= %d, flow = stroke) ==' % MIN_REPORT)
    P('  %-4s %-26s %9s %8s %6s %6s %8s %s'
      % ('id', 'edid', 'area', 'height', 'aniso', 'bed_r', 'bed_drop', 'cells'))
    for g, r in sorted(recs.items(), key=lambda kv: -kv[1]['area']):
        if r['area'] < MIN_REPORT or r['flow'] != 'stroke':
            continue
        P('  %-4d %-26s %9d %8.1f %6.2f %+6.2f %8.1f (%d..%d, %d..%d)'
          % (g, r['edid'][:26], r['area'], r['h'], r['aniso'], r['bed_r'], r['bed_drop'],
             r['cellx0'], r['cellx1'], r['celly0'], r['celly1']))
    P()
    P('== THE 20 BIGGEST BODIES ==')
    P('  %-4s %-26s %-6s %9s %8s %5s %6s %6s %7s %-7s %s'
      % ('id', 'edid', 'class', 'area', 'height', 'parts', 'elong', 'aniso', 'depth', 'flow', 'cells'))
    for g, r in sorted(recs.items(), key=lambda kv: -kv[1]['area'])[:20]:
        P('  %-4d %-26s %-6s %9d %8.1f %5d %6.1f %6.2f %7.1f %-7s (%d..%d, %d..%d)'
          % (g, r['edid'][:26], r['class'], r['area'], r['h'], r['parts'], r['elong'],
             r['aniso'], r['depth_mean'], r['flow'],
             r['cellx0'], r['cellx1'], r['celly0'], r['celly1']))


if __name__ == '__main__':
    main()
