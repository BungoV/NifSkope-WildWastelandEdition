#!/usr/bin/env python3
"""census_water2.py -- lane WATER2's ORACLE for the C++ water writer.

It is rule D end to end, with the two corrections lane WATER2 measured
(scratchpad/water2_20260909/bridge_exact.py, bridge_effect.py,
bridge_variants.py) and argued in scratchpad/lane_water2_report.md:

  * the shore test is EXACT -- every texel pair within `gap`, found by scanning
    the disc of radius `gap` around every texel -- where WATER1's final_census.py
    compared DECIMATED point clouds (`bx[::area//4000]`), which can only
    overestimate a distance and so missed 327 of the 545 pairs that exist;
  * a merge in which exactly ONE side inherits the worldspace type -- rule C's
    adjacent merge AND rule D's bridge, both -- is accepted only when the
    INHERITING side is the SMALLER of the two.  Without it in the bridge, the
    exact test hands the Commonwealth's ocean -- 21,587,443 texels -- to
    `ExtMarshScumWater`, because a painted marsh comes within two texels of it;
    without it in rule C, the synthetic control's sea takes the form of the
    painted river reach it touches at its own height.

Everything else is rule D as `scratchpad/specs_20260909/spec_water.md` §2 and
§4 state it, including the class gate and the four flow sources.  The `near`
(64-texel drainage) relation is exact too, by the same bucket method.

Output: `census_water2.txt` (the fixed-format census the C++ `--water-census`
must reproduce line for line) and `census_water2.json` (every body record, for
the independent cross-check of the written body table).

Run it from this folder.  It builds rule C itself -- it used to import lane
WATER1's `labC.npy`, and cannot any more, because rule C's ADJACENT merge needed
the same direction clause the bridge needed and WATER1's script does not have
it.  The only thing it shares with anything is the .lodl decoder.
"""

import collections
import json
import math
import os
import pickle
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
W1 = os.path.abspath(os.path.join(HERE, '..', 'water_20260909'))
sys.path.insert(0, W1)

import ccl                      # noqa: E402
import lodl_bulk as B           # noqa: E402
import water_model as WM        # noqa: E402

LODL = r'E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl'
GAP = 2
NEAR = 64
DEF = 0xFFFF
TINY = 4
MIN_REPORT = 64
RULEC = [0, 0]


def disc(gap):
    """(dy, dx) with 0 < dy^2+dx^2 <= gap^2, taken on the dy >= 0 half-plane so
    every unordered texel pair is visited exactly once."""
    out = []
    for dy in range(0, gap + 1):
        for dx in range(-gap, gap + 1):
            if dy == 0 and dx <= 0:
                continue
            if dy * dy + dx * dx <= gap * gap:
                out.append((dy, dx))
    return out


def pairs_within(lab, gap):
    """{(a, b): min distance in texels} for every pair of labels within `gap`."""
    H, W = lab.shape
    best = {}
    for (dy, dx) in disc(gap):
        y0, y1 = 0, H - dy
        x0, x1 = (0, W - dx) if dx >= 0 else (-dx, W)
        a = lab[y0:y1, x0:x1]
        b = lab[y0 + dy:y1 + dy, x0 + dx:x1 + dx]
        m = (a != 0) & (b != 0) & (a != b)
        if not m.any():
            continue
        dist = math.sqrt(dy * dy + dx * dx)
        pa = a[m].astype(np.int64)
        pb = b[m].astype(np.int64)
        lo = np.minimum(pa, pb)
        hi = np.maximum(pa, pb)
        for k in np.unique(lo * 1000000 + hi).tolist():
            kk = (int(k // 1000000), int(k % 1000000))
            if kk not in best or dist < best[kk]:
                best[kk] = dist
    return best


def boundary_points(lab):
    """Texels of a labelled body with a 4-neighbour that is not the same body."""
    m = lab != 0
    d = np.zeros_like(m)
    d[:, :-1] |= lab[:, :-1] != lab[:, 1:]
    d[:, 1:] |= lab[:, 1:] != lab[:, :-1]
    d[:-1, :] |= lab[:-1, :] != lab[1:, :]
    d[1:, :] |= lab[1:, :] != lab[:-1, :]
    d[0, :] = d[-1, :] = True
    d[:, 0] = d[:, -1] = True
    return m & d


def near_pairs(lab, keep, radius):
    """Exact {(a, b): (distance, contact x, contact y)} for every pair of labels
    in `keep` whose texels come within `radius`.  Bucketed at `radius`, so only
    the 3x3 neighbourhood of buckets can hold a pair inside the radius (two
    texels two buckets apart are at least `radius`+1 apart)."""
    bnd = boundary_points(lab)
    ys, xs = np.nonzero(bnd)
    lb = lab[ys, xs]
    sel = np.isin(lb, list(keep))
    ys, xs, lb = ys[sel], xs[sel], lb[sel]
    by, bx = ys // radius, xs // radius
    nbx = int(bx.max()) + 1
    key = by.astype(np.int64) * nbx + bx
    order = np.argsort(key, kind='stable')
    ys, xs, lb, key = ys[order], xs[order], lb[order], key[order]
    uk, start = np.unique(key, return_index=True)
    end = np.append(start[1:], len(key))
    bucket = {int(k): (a, b) for k, a, b in zip(uk, start, end)}

    best = {}

    def consider(i0, i1, j0, j1, same):
        ay, ax, al = ys[i0:i1], xs[i0:i1], lb[i0:i1]
        cy, cx, cl = ys[j0:j1], xs[j0:j1], lb[j0:j1]
        dy = ay[:, None] - cy[None, :]
        dx = ax[:, None] - cx[None, :]
        dd = dy * dy + dx * dx
        diff = al[:, None] != cl[None, :]
        if not diff.any():
            return
        dd = np.where(diff, dd, 1 << 30)
        inr = dd <= radius * radius
        if not inr.any():
            return
        ii, jj = np.nonzero(inr)
        for i, j in zip(ii.tolist(), jj.tolist()):
            a, b = int(al[i]), int(cl[j])
            k = (a, b) if a < b else (b, a)
            dist = math.sqrt(float(dd[i, j]))
            if k not in best or dist < best[k][0]:
                # contact point: the midpoint of the closest pair
                best[k] = (dist, 0.5 * (float(ax[i]) + float(cx[j])),
                           0.5 * (float(ay[i]) + float(cy[j])))

    for k, (i0, i1) in bucket.items():
        kby, kbx = k // nbx, k % nbx
        for ddy in (0, 1):
            for ddx in (-1, 0, 1):
                if ddy == 0 and ddx < 0:
                    continue
                k2 = (kby + ddy) * nbx + (kbx + ddx)
                if kbx + ddx < 0 or kbx + ddx >= nbx or k2 not in bucket:
                    continue
                j0, j1 = bucket[k2]
                consider(i0, i1, j0, j1, k2 == k)
    return best


def principal(n, sx, sy, sxx, syy, sxy):
    """Unit vector along the longest extent and the anisotropy 1 - l1/l0."""
    if n < 3:
        return (1.0, 0.0), 0.0
    mx, my = sx / n, sy / n
    cxx = sxx / n - mx * mx
    cyy = syy / n - my * my
    cxy = sxy / n - mx * my
    # numpy's cov uses the n-1 denominator; the eigenvectors are unchanged by a
    # common scale and the anisotropy is a ratio, so this matches.
    tr = cxx + cyy
    det = cxx * cyy - cxy * cxy
    disc_ = max(tr * tr / 4.0 - det, 0.0)
    l0 = tr / 2.0 + math.sqrt(disc_)
    l1 = tr / 2.0 - math.sqrt(disc_)
    if abs(cxy) > 1e-12:
        vx, vy = l0 - cyy, cxy
    elif cxx >= cyy:
        vx, vy = 1.0, 0.0
    else:
        vx, vy = 0.0, 1.0
    m = math.hypot(vx, vy)
    if m == 0:
        vx, vy, m = 1.0, 0.0, 1.0
    aniso = 0.0 if l0 <= 0 else 1.0 - l1 / l0
    return (vx / m, vy / m), aniso


def main():
    d = B.open_lodl(LODL)
    words = B.bulk_height_words(d)
    heights = (words.astype(np.float32) - 32767.0) * d.quantum
    lo, hi, wh, wt, fl = B.cell_table(d)
    wet, whT = WM.wet_mask(heights, wh, (fl & 1) > 0, d.spc)

    # ---- rule C, built here rather than loaded ------------------------
    #
    # The oracle used to import lane WATER1's `labC.npy` and its rule-C map.
    # It cannot any more: rule C's merge needed the same DIRECTION clause the
    # bridge needed (the known-answer control found it -- the sea touches a
    # painted river reach at its own height and was absorbed by it), and
    # WATER1's script does not have it. So rule C is built here, and the oracle
    # shares nothing with the writer but the .lodl decoder.
    hq = np.round(whT * 8.0).astype(np.int64)
    tq = WM.expand_cells(wt.astype(np.int64), d.spc)
    lab, nC = ccl.label(wet, hq * 70000 + tq)
    ys0, xs0 = np.nonzero(wet)
    lb0 = lab[ys0, xs0]
    o0 = np.argsort(lb0, kind='stable')
    lbs, yss, xss = lb0[o0], ys0[o0], xs0[o0]
    c0 = np.searchsorted(lbs, np.arange(1, nC + 1), 'left')
    c1 = np.searchsorted(lbs, np.arange(1, nC + 1), 'right')
    comp = {}
    H, Wd = wet.shape
    for c in range(1, nC + 1):
        a, b = c0[c - 1], c1[c - 1]
        if b <= a:
            continue
        by, bx = yss[a:b], xss[a:b]
        comp[c] = {'area': int(b - a), 'h': float(whT[by[0], bx[0]]),
                   'type': int(tq[by[0], bx[0]]),
                   'x0': int(bx.min()), 'x1': int(bx.max()),
                   'y0': int(by.min()), 'y1': int(by.max()),
                   'edge': bool(bx.min() == 0 or by.min() == 0
                                or bx.max() == Wd - 1 or by.max() == H - 1)}
    nb = WM.neighbour_bodies(lab, nC)
    cuf = ccl.UF(nC + 1)
    ambiguous = mergedC = refusedC = 0
    for c in sorted(comp):
        rec = comp[c]
        if rec['type'] != DEF:
            continue
        cand = [k for k in nb.get(c, ())
                if comp.get(k) and comp[k]['type'] != DEF
                and abs(comp[k]['h'] - rec['h']) < 0.01]
        if not cand:
            continue
        cand.sort(key=lambda k: (-comp[k]['area'], k))
        # the inheriting side is absorbed, and only when it is the SMALLER
        if rec['area'] >= comp[cand[0]]['area']:
            refusedC += 1
            continue
        if len(cand) > 1:
            ambiguous += 1
        cuf.union(c, cand[0])
        mergedC += 1
    root = {c: cuf.find(c) for c in comp}
    seq, info = {}, {}
    for c in sorted(comp):
        r = root[c]
        if r not in seq:
            seq[r] = len(seq) + 1
            info[seq[r]] = {'area': 0, 'h': comp[c]['h'], 'types': collections.Counter(),
                            'edge': False, 'parts': 0}
        t = info[seq[r]]
        t['area'] += comp[c]['area']
        t['types'][comp[c]['type']] += comp[c]['area']
        t['edge'] = t['edge'] or comp[c]['edge']
        t['parts'] += 1
    for t in info.values():
        # the rule-C body's own type is the majority INCLUDING the default,
        # which is what decides whether it counts as inheriting in the bridge
        t['typeIdx'] = max(t['types'].items(), key=lambda kv: (kv[1], -kv[0]))[0]
        t['inherits'] = t['typeIdx'] == DEF
    lut = np.zeros(lab.max() + 1, np.int32)
    for c in comp:
        lut[c] = seq[root[c]]
    blab = lut[lab].astype(np.int32)
    keys = sorted(info)
    RULEC[0], RULEC[1] = nC, len(keys)
    print('rule C: %d components -> %d bodies (%d merged, %d refused, %d ambiguous)'
          % (nC, len(keys), mergedC, refusedC, ambiguous))

    # ---- rule D, the corrected bridge ---------------------------------
    pr = pairs_within(blab, GAP)
    uf = ccl.UF(max(keys) + 2)
    accepted = refused = 0
    for (a, b) in sorted(pr):
        A, Bb = info[a], info[b]
        if abs(A['h'] - Bb['h']) > 0.01:
            continue
        if A['typeIdx'] == Bb['typeIdx']:
            uf.union(a, b)
            accepted += 1
            continue
        inhA, inhB = A['inherits'], Bb['inherits']
        if inhA and inhB:
            uf.union(a, b)
            accepted += 1
        elif inhA or inhB:
            small = A if inhA else Bb
            big = Bb if inhA else A
            if small['area'] < big['area']:
                uf.union(a, b)
                accepted += 1
            else:
                refused += 1
        else:
            refused += 1
    grp = {}
    for k in keys:
        grp.setdefault(uf.find(k), []).append(k)

    # IDs by DESCENDING AREA (spec 4.1 step 6), ties by the smallest part id
    order = sorted(grp.values(),
                   key=lambda parts: (-sum(info[k]['area'] for k in parts), min(parts)))
    idOf = {}
    for i, parts in enumerate(order):
        for k in parts:
            idOf[k] = i + 1
    m = np.zeros(max(keys) + 2, np.int32)
    for k in keys:
        m[k] = idOf[k]
    dlab = m[blab]
    nBodies = len(order)

    # ---- per-body records ---------------------------------------------
    esm = pickle.load(open(os.path.join(W1, 'esm_water_0000003C.pkl'), 'rb'))
    edid = {f: r['edid'] for f, r in esm['watr'].items()}
    nam0 = {f: struct.unpack('<3f', r['NAM0'])[:2]
            for f, r in esm['watr'].items() if r['NAM0']}
    watr = B.watr_table(d)
    defH, defT = B.default_water(d)

    ys, xs = np.nonzero(dlab > 0)
    lb = dlab[ys, xs]
    o = np.argsort(lb, kind='stable')
    lb_s, ys_s, xs_s = lb[o], ys[o], xs[o]
    gk = list(range(1, nBodies + 1))
    a0 = np.searchsorted(lb_s, gk, 'left')
    a1 = np.searchsorted(lb_s, gk, 'right')

    recs = {}
    for g, a, b in zip(gk, a0, a1):
        by, bx = ys_s[a:b], xs_s[a:b]
        parts = order[g - 1]
        tc = collections.Counter()
        for k in parts:
            tc.update(info[k]['types'])
        painted = [t for t in tc if t != DEF]
        ti = max(painted, key=lambda t: (tc[t], -t)) if painted else DEF
        form = int(watr[ti]) if ti != DEF else int(defT)
        n = by.size
        fx = bx.astype(np.float64)
        fy = by.astype(np.float64)
        ax, aniso = principal(n, fx.sum(), fy.sum(), (fx * fx).sum(),
                              (fy * fy).sum(), (fx * fy).sum())
        t = fx * ax[0] + fy * ax[1]
        bed = heights[by, bx].astype(np.float64)
        r_bed = float(np.corrcoef(t, bed)[0, 1]) if bed.std() > 0 and t.std() > 0 else 0.0
        slope = float(np.polyfit(t, bed, 1)[0]) if bed.std() > 0 and t.std() > 0 else 0.0
        w = int(bx.max() - bx.min() + 1)
        hgt = int(by.max() - by.min() + 1)
        recs[g] = {
            'id': g, 'area': int(n), 'h': float(info[parts[0]]['h']),
            'edid': edid.get(form, '%08X' % form), 'form': form,
            'typeIdx': int(ti), 'inherits': ti == DEF, 'parts': len(parts),
            'cellx0': int(d.minX + bx.min() // d.spc), 'cellx1': int(d.minX + bx.max() // d.spc),
            'celly0': int(d.minY + by.min() // d.spc), 'celly1': int(d.minY + by.max() // d.spc),
            'edge': bool(bx.min() == 0 or by.min() == 0
                         or bx.max() == wet.shape[1] - 1 or by.max() == wet.shape[0] - 1),
            'elong': round(max(w, hgt) ** 2 / float(n), 2),
            'aniso': round(float(aniso), 3),
            'axis': [round(ax[0], 4), round(ax[1], 4)],
            'bed_r': round(r_bed, 3), 'bed_drop': round(slope * (t.max() - t.min()), 1),
            'nam0': [round(v, 4) for v in nam0.get(form, (0.0, 0.0))],
            'tiny': bool(n < TINY),
            'mx': float(fx.mean()), 'my': float(fy.mean()),
        }

    # ---- the exact 64-texel drainage relation -------------------------
    big = set(g for g in gk if recs[g]['area'] >= 16)
    np_ = near_pairs(dlab, big, NEAR)
    for g in gk:
        recs[g]['lower_near'] = []
    for (a, b), (dist, cxp, cyp) in np_.items():
        ra, rb = recs[a], recs[b]
        if rb['h'] < ra['h'] - 0.01:
            ra['lower_near'].append([b, round(dist, 2), rb['h'], cxp, cyp])
        if ra['h'] < rb['h'] - 0.01:
            rb['lower_near'].append([a, round(dist, 2), ra['h'], cxp, cyp])
    for g in gk:
        recs[g]['lower_near'].sort(key=lambda e: e[1])

    for g in gk:
        r = recs[g]
        r['class'] = classify(r)
        r['flow_source'], r['flow'] = flow_of(r)

    with open(os.path.join(HERE, 'census_water2.json'), 'w') as f:
        json.dump({'bodies': {str(k): v for k, v in recs.items()},
                   'gap': GAP, 'near': NEAR, 'accepted': accepted,
                   'refused': refused, 'nBodies': nBodies}, f, indent=1)
    np.save(os.path.join(HERE, 'body_id_plane.npy'), dlab.astype(np.uint16))
    report(d, recs, wet, accepted, refused, defH, defT, edid)
    return 0


def classify(r):
    if r['edge']:
        return 'sea'
    if r['elong'] >= 6.0 or r['lower_near']:
        return 'river'
    return 'lake'


def flow_of(r):
    """(source byte, [vx, vy]) -- spec 4.2, first rule that answers wins.
    0 none, 1 the form's NAM0, 2 bed slope, 3 drain, 4 user stroke."""
    if r['class'] == 'sea':
        return 0, [0.0, 0.0]
    ax = r['axis']
    if r['lower_near'] and r['aniso'] >= 0.5:
        # toward the contact, projected on the body's own axis
        b, dist, h, cx, cy = r['lower_near'][0]
        # the sign is the contact seen from the body's own CENTROID, projected
        # on the axis -- an absolute projection would point the same way for
        # every body in the eastern half of the worldspace
        sgn = 1.0 if ((cx - r['mx']) * ax[0] + (cy - r['my']) * ax[1]) >= 0 else -1.0
        spd = math.hypot(*r['nam0'])
        return 3, [round(ax[0] * sgn * spd, 4), round(ax[1] * sgn * spd, 4)]
    if abs(r['bed_r']) >= 0.7 and abs(r['bed_drop']) >= 64.0 and r['aniso'] >= 0.5:
        sgn = -1.0 if r['bed_r'] > 0 else 1.0     # downhill
        spd = math.hypot(*r['nam0'])
        return 2, [round(ax[0] * sgn * spd, 4), round(ax[1] * sgn * spd, 4)]
    if r['aniso'] < 0.5 and not r['lower_near']:
        return 0, [0.0, 0.0]                       # a round lake with no outlet
    if r['nam0'][0] or r['nam0'][1]:
        return 1, [r['nam0'][0], r['nam0'][1]]
    return 0, [0.0, 0.0]


def report(d, recs, wet, accepted, refused, defH, defT, edid):
    P = print
    out = []

    def say(s=''):
        out.append(s)
        P(s)

    say('== INPUT ==')
    say('  cells %d..%d x %d..%d = %d  spc %d  wet texels %d'
        % (d.minX, d.maxX, d.minY, d.maxY, d.cellsX * d.cellsY, d.spc, int(wet.sum())))
    say('  worldspace default water height %.1f type %08X %s'
        % (defH, defT, edid.get(defT, '?')))
    say('  bridge gap %d texels: %d merges accepted, %d refused' % (GAP, accepted, refused))
    say('  rule C: %d components -> %d bodies' % (RULEC[0], RULEC[1]))
    say()
    say('== BODIES ==')
    say('  total %d' % len(recs))
    for t in (1, 4, 16, 64, 256, 1024, 65536):
        say('    area >= %6d texels : %4d'
            % (t, sum(1 for r in recs.values() if r['area'] >= t)))
    c = collections.Counter(r['class'] for r in recs.values())
    say('  class: ' + '  '.join('%s %d' % (k, c[k]) for k in ('sea', 'river', 'lake')))
    c2 = collections.Counter(r['class'] for r in recs.values() if r['area'] >= MIN_REPORT)
    say('  class (area >= %d): ' % MIN_REPORT
        + '  '.join('%s %d' % (k, c2[k]) for k in ('sea', 'river', 'lake')))
    say('  bodies assembled from more than one component: %d'
        % sum(1 for r in recs.values() if r['parts'] > 1))
    say('  TINY (< %d texels): %d' % (TINY, sum(1 for r in recs.values() if r['tiny'])))
    say()
    say('== PER WATR FORM ==')
    per = collections.defaultdict(lambda: [0, 0])
    for r in recs.values():
        per[r['edid']][0] += 1
        per[r['edid']][1] += r['area']
    for k, (n, a) in sorted(per.items(), key=lambda kv: -kv[1][1]):
        say('  %-28s bodies %4d  texels %9d' % (k, n, a))
    say()
    say('== FLOW SOURCE ==')
    names = {0: 'none', 1: 'form NAM0', 2: 'bed', 3: 'drain', 4: 'stroke'}
    fr = collections.Counter(r['flow_source'] for r in recs.values())
    say('  all bodies : ' + '  '.join('%s %d' % (names[k], fr[k]) for k in sorted(fr)))
    fr2 = collections.Counter(r['flow_source'] for r in recs.values()
                              if r['area'] >= MIN_REPORT)
    say('  area >= %d : ' % MIN_REPORT
        + '  '.join('%s %d' % (names[k], fr2[k]) for k in sorted(fr2)))
    say()
    say('== THE 20 BIGGEST BODIES ==')
    say('  %-4s %-26s %-6s %9s %8s %5s %6s %6s %-9s %s'
        % ('id', 'edid', 'class', 'area', 'height', 'parts', 'elong', 'aniso',
           'flow', 'cells'))
    for g, r in sorted(recs.items(), key=lambda kv: -kv[1]['area'])[:20]:
        say('  %-4d %-26s %-6s %9d %8.1f %5d %6.1f %6.2f %-9s (%d..%d, %d..%d)'
            % (g, r['edid'][:26], r['class'], r['area'], r['h'], r['parts'],
               r['elong'], r['aniso'], names[r['flow_source']],
               r['cellx0'], r['cellx1'], r['celly0'], r['celly1']))
    with open(os.path.join(HERE, 'census_water2.txt'), 'w') as f:
        f.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    raise SystemExit(main())
