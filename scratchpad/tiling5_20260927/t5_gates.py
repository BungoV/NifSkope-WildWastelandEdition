"""TILING5 gates, on the PRODUCT's sheets (real bakes by t5_bake.sh).

    python t5_gates.py tiling FLOOR ARM [ARM...]   TILING4's repeat / swirl / grain gates,
                                                    both frozen sets, FLOOR = the per-sheet grain
                                                    rung (today's default bake)
    python t5_gates.py trans ARM [ARM...]           the layer-transition gate (vanilla beside)
    python t5_gates.py macro ARM [ARM...]           large-scale variance + saturation (vanilla beside)

TILING4's instruments (h1_sweep.score, t4_gates.decided) are imported UNCHANGED
from the main tree's scratchpad, read-only.

THE TRANSITION GATE (pre-registered before any height-blend sheet was scored):
per sheet, the texels where some LTEX layer's bilinear opacity is in [0.2,0.8]
over a base whose mean luminance differs from the layer's by >= 8 of 255 form the
transition zone Z; the texels where every layer is < 0.02 or > 0.98 form the
interior I.  The reading is rz = SD(hp in Z) / SD(hp in I), hp = the r=2 high-pass
TILING4's grain gate uses.  A crossfade averages two decorrelated grains in Z and
so reads rz < 1 ("soft, smeary transitions"); vanilla's own sheets read what they
read at the same places.  PASS per sheet: rz >= 0.9 x vanilla's rz on that sheet.

THE MACRO GATES: (a) large-scale luminance SD -- the SD of the sheet's luminance
after two r=32 box passes (about a 12 m Gaussian) -- median over the set <=
vanilla's median, and every sheet within vanilla's [min, max] over the set;
(b) mean HSV saturation per sheet >= the unmodified arm's.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = r'E:/Projects/NifskopeWildWastelandEdition/scratchpad'
T4 = os.path.join(MAIN, 'tiling4_20260912')
T3 = os.path.join(MAIN, 'tiling3_20260911')
T2 = os.path.join(MAIN, 'tiling2_20260911')
SP = os.path.join(MAIN, 'splat1_20260911')
for p in (T4, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402

CFG = json.load(open(os.path.join(T4, 'pool.json')))
SEL = [tuple(c) for c in CFG['selection']]
VAL = [tuple(c) for c in CFG['validation']]
BOSTON = [(x, y) for y in (-12, -8, -4) for x in (-8, -4, 0)]
CELL = 4096.0
RES = 512


def sheet(arm, cx, cy, boston=False):
    tag = 'boston' if boston else 'r_%d_%d_%d_%d' % (cx, cy, cx + 3, cy + 3)
    return os.path.join(HERE, 'out', arm, tag, 'tex', 'Commonwealth.4.%d.%d.DDS' % (cx, cy))


def rgb(path):
    return S.Dds(path).level(0)[:, :, :3].astype(np.float64)


def van_rgb(cx, cy):
    return rgb(S.van_sheet(cx, cy))


# ------------------------------------------------------------------ tiling
def cmd_tiling(arms):
    import h1_sweep as HS
    import t4_gates as G
    floor = arms[0]
    per = {a: {} for a in arms}
    for a in arms:
        for cx, cy in SEL + VAL:
            per[a]['%d,%d' % (cx, cy)] = HS.score(cx, cy, S.lum(S.Dds(sheet(a, cx, cy)).level(0)))
    van = {}
    for cx, cy in SEL + VAL:
        R = HS.ref(cx, cy)
        van['%d,%d' % (cx, cy)] = dict(hpsd=R['hpsd'], bands=R['bands'])
    L = ['TILING4 gates on the product, floor (G2 rung) = %s' % floor, '']
    out = {}
    for label, sheets in (('SELECTION', SEL), ('VALIDATION', VAL)):
        keys = ['%d,%d' % c for c in sheets]
        L.append(label)
        L.append(G.HEAD)
        for a in arms:
            r = G.decided(per[a], per[floor], van, keys)
            out.setdefault(a, {})[label] = {k: v for k, v in r.items() if not isinstance(v, list)}
            L.append(G.row(a, None, r))
        L.append('')
    L.append('REPEAT per sheet: amplitude (ceiling) / ratio (0.448)')
    L.append('   %-9s %7s ' % ('chunk', 'ceil') + ' '.join('%15s' % a[:15] for a in arms))
    for cx, cy in SEL + VAL:
        k = '%d,%d' % (cx, cy)
        R = HS.ref(cx, cy)
        ceil = HS.ABS_CEIL if R['ctrl'] < HS.ABS_CEIL else R['ctrl']
        cells = []
        for a in arms:
            s = per[a][k]
            cells.append('%6.3f/%5.3f%s' % (s['vis'], s['ratio'], ' ' if s['rep_ok'] else '*'))
        L.append('   %-9s %7.3f ' % (k, ceil) + ' '.join('%15s' % c for c in cells))
    L.append('   (* = RED)')
    L.append('')
    L.append('SWIRL per sheet: r (ceiling)')
    for cx, cy in SEL + VAL:
        k = '%d,%d' % (cx, cy)
        L.append('   %-9s %7.3f ' % (k, per[arms[0]][k]['swirl_ceil'])
                 + ' '.join('%8.3f%s' % (per[a][k]['swirl_r'], ' ' if per[a][k]['swirl_ok'] else '*')
                            for a in arms))
    L.append('')
    L.append('GRAIN hp SD per sheet (vanilla | arms)')
    for cx, cy in SEL + VAL:
        k = '%d,%d' % (cx, cy)
        L.append('   %-9s %7.3f | ' % (k, van[k]['hpsd'])
                 + ' '.join('%7.3f' % per[a][k]['hpsd'] for a in arms))
    txt = '\n'.join(L) + '\n'
    print(txt)
    tag = '_'.join(arms)
    with open(os.path.join(HERE, 'logs', 'tiling_%s.txt' % tag), 'w', newline='\n') as f:
        f.write(txt)
    with open(os.path.join(HERE, 'logs', 'tiling_%s.json' % tag), 'w', newline='\n') as f:
        json.dump(dict(summary=out, per={a: {k: {kk: vv for kk, vv in s.items()
                                                   if not isinstance(vv, (list, dict))}
                                               for k, s in per[a].items()} for a in arms}),
                  f, indent=1)


# ------------------------------------------------------------- transitions
_MEAN = {}


def mean_lum(form):
    import offline_bake as OB
    if form not in _MEAN:
        d, _ = OB.diffuse_of(form) if form else (None, '')
        if d is None:
            _MEAN[form] = None
        else:
            _MEAN[form] = float(S.lum(d.level(d.maxMip)).mean())
    return _MEAN[form]


def zones(cx0, cy0, dim=4, res=RES):
    """Z (transition) and I (interior) masks, row 0 = north, as offline_bake."""
    import offline_bake as OB
    e = OB.esm()
    span = float(dim) * CELL
    dom = OB.dominant_base(e, (cx0 // 4) * 4, (cy0 // 4) * 4, 4)
    cwX, cwY = cx0 * CELL, cy0 * CELL
    py, px = np.mgrid[0:res, 0:res]
    wy = cwY + (1.0 - (py + 0.5) / res) * span
    wx = cwX + ((px + 0.5) / res) * span
    Z = np.zeros((res, res), bool)
    I = np.zeros((res, res), bool)
    for ci in range(dim * dim):
        cx, cy = cx0 + ci % dim, cy0 + ci // dim
        land = e.lands.get((cx, cy))
        if not land:
            continue
        for q in range(4):
            x0 = cx * CELL + (2048.0 if q & 1 else 0.0)
            y0 = cy * CELL + (2048.0 if q & 2 else 0.0)
            m = (wx >= x0) & (wx < x0 + 2048.0) & (wy >= y0) & (wy < y0 + 2048.0)
            # stay 160 units off the quadrant lines: the cross-fade lives there
            inner = ((wx >= x0 + 160) & (wx < x0 + 2048 - 160)
                     & (wy >= y0 + 160) & (wy < y0 + 2048 - 160))
            if not m.any():
                continue
            qx = (wx[m] - x0) / 2048.0
            qy = (wy[m] - y0) / 2048.0
            base = land['base'][q] or dom
            bl = mean_lum(base)
            fx = np.clip(qx * 16.0, 0.0, 15.999)
            fy = np.clip(qy * 16.0, 0.0, 15.999)
            ix, iy = fx.astype(np.int64), fy.astype(np.int64)
            tx, ty = fx - ix, fy - iy
            z = np.zeros(qx.size, bool)
            mid = np.zeros(qx.size, bool)
            for lay in land['layers'][q]:
                op = np.asarray(lay['op'], np.float64)
                a = ((op[iy, ix] * (1 - tx) + op[iy, ix + 1] * tx) * (1 - ty)
                     + (op[iy + 1, ix] * (1 - tx) + op[iy + 1, ix + 1] * tx) * ty)
                a = np.clip(a, 0.0, 1.0)
                mid |= (a >= 0.02) & (a <= 0.98)
                ll = mean_lum(lay['ltex'] or dom)
                if bl is not None and ll is not None and abs(ll - bl) >= 8.0:
                    z |= (a >= 0.2) & (a <= 0.8)
            zz = np.zeros((res, res), bool)
            ii = np.zeros((res, res), bool)
            zz[m] = z
            ii[m] = ~mid
            Z |= zz & inner
            I |= ii & inner
    return Z, I


def hp(lum):
    return lum - S._box(lum, 2)


def rz(lum, Z, I):
    h = hp(lum)
    return float(h[Z].std() / max(h[I].std(), 1e-9))


def cmd_trans(arms):
    L = ['TRANSITION gate: rz = SD(hp in zone) / SD(hp in interior); PASS rz >= 0.9 x vanilla', '',
         '   %-9s %6s %6s %7s ' % ('chunk', 'nZ', 'nI', 'vanilla') + ' '.join('%10s' % a[:10] for a in arms)]
    npass = {a: 0 for a in arms}
    n = 0
    rows = {}
    for cx, cy in SEL + VAL:
        Z, I = zones(cx, cy)
        if Z.sum() < 500 or I.sum() < 500:
            L.append('   %-9s %6d %6d  (skipped: too few texels)' % ('%d,%d' % (cx, cy), Z.sum(), I.sum()))
            continue
        n += 1
        v = rz(S.lum(van_rgb(cx, cy)), Z, I)
        cells = []
        rows['%d,%d' % (cx, cy)] = dict(nZ=int(Z.sum()), nI=int(I.sum()), van=v)
        for a in arms:
            r = rz(S.lum(rgb(sheet(a, cx, cy))), Z, I)
            ok = r >= 0.9 * v
            npass[a] += ok
            rows['%d,%d' % (cx, cy)][a] = r
            cells.append('%9.3f%s' % (r, ' ' if ok else '*'))
        L.append('   %-9s %6d %6d %7.3f ' % ('%d,%d' % (cx, cy), Z.sum(), I.sum(), v) + ' '.join(cells))
    L.append('')
    for a in arms:
        L.append('%s: %d of %d sheets pass' % (a, npass[a], n))
    txt = '\n'.join(L) + '\n'
    print(txt)
    tag = '_'.join(arms)
    with open(os.path.join(HERE, 'logs', 'trans_%s.txt' % tag), 'w', newline='\n') as f:
        f.write(txt)
    with open(os.path.join(HERE, 'logs', 'trans_%s.json' % tag), 'w', newline='\n') as f:
        json.dump(rows, f, indent=1)


# ------------------------------------------------------------------- macro
def large_sd(img):
    l = S.lum(img)
    lo = S._box(S._box(l, 32), 32)
    return float(lo.std())


def large_chroma(img):
    """The same low-pass on the two opponent axes: large-scale hue/saturation SD."""
    o1 = img[:, :, 0] - img[:, :, 1]
    o2 = 0.5 * (img[:, :, 0] + img[:, :, 1]) - img[:, :, 2]
    a = S._box(S._box(o1, 32), 32)
    b = S._box(S._box(o2, 32), 32)
    return float(np.sqrt(a.var() + b.var()))


def sat_mean(img):
    mx = img.max(axis=2)
    mn = img.min(axis=2)
    s = np.where(mx > 1e-6, (mx - mn) / np.maximum(mx, 1e-6), 0.0)
    return float(s.mean())


def cmd_macro(arms):
    sets = [('FROZEN14', [(c, False) for c in SEL + VAL]), ('BOSTON9', [(c, True) for c in BOSTON])]
    L = ['MACRO gates: large-scale lum SD (two r=32 box passes) and mean HSV saturation', '']
    res = {}
    for label, items in sets:
        have = [a for a in arms if all(os.path.exists(sheet(a, c[0], c[1], b)) for c, b in items)]
        L.append('%s  arms present: %s' % (label, ' '.join(have)))
        L.append('   %-9s %8s ' % ('chunk', 'van') + ' '.join('%10s' % a[:10] for a in have)
                 + ' | sat: %6s ' % 'van' + ' '.join('%8s' % a[:8] for a in have))
        big = {a: [] for a in ['van'] + have}
        sat = {a: [] for a in ['van'] + have}
        means = {a: [] for a in ['van'] + have}
        chro = {a: [] for a in ['van'] + have}
        for (cx, cy), b in items:
            v = van_rgb(cx, cy)
            big['van'].append(large_sd(v))
            sat['van'].append(sat_mean(v))
            means['van'].append(float(S.lum(v).mean()))
            chro['van'].append(large_chroma(v))
            for a in have:
                im = rgb(sheet(a, cx, cy, b))
                big[a].append(large_sd(im))
                sat[a].append(sat_mean(im))
                means[a].append(float(S.lum(im).mean()))
                chro[a].append(large_chroma(im))
            L.append('   %-9s %8.3f ' % ('%d,%d' % (cx, cy), big['van'][-1])
                     + ' '.join('%10.3f' % big[a][-1] for a in have)
                     + ' | sat: %6.4f ' % sat['van'][-1] + ' '.join('%8.4f' % sat[a][-1] for a in have))
        L.append('   %-9s %8.3f ' % ('MEDIAN', float(np.median(big['van'])))
                 + ' '.join('%10.3f' % float(np.median(big[a])) for a in have))
        L.append('   %-9s %8.3f ' % ('SD(means)', float(np.std(means['van'])))
                 + ' '.join('%10.3f' % float(np.std(means[a])) for a in have)
                 + '   <- sheet-to-sheet (234 m - 1 km) luminance SD')
        L.append('   %-9s %8.3f ' % ('CHROMA med', float(np.median(chro['van'])))
                 + ' '.join('%10.3f' % float(np.median(chro[a])) for a in have)
                 + '   <- large-scale opponent-colour SD, median')
        vmin, vmax, vmed = min(big['van']), max(big['van']), float(np.median(big['van']))
        for a in have:
            inr = sum(1 for x in big[a] if vmin <= x <= vmax)
            L.append('   %s: large-scale median %.3f vs vanilla %.3f (%s); within vanilla range %d/%d; '
                     'sheet-mean SD %.3f vs vanilla %.3f'
                     % (a, float(np.median(big[a])), vmed,
                        'PASS' if float(np.median(big[a])) <= vmed else 'FAIL', inr, len(items),
                        float(np.std(means[a])), float(np.std(means['van']))))
        if have:
            base = have[0]
            for a in have[1:]:
                ok = sum(1 for x, y in zip(sat[a], sat[base]) if x >= y)
                worst = min(x - y for x, y in zip(sat[a], sat[base]))
                L.append('   saturation %s >= %s: %d/%d sheets (worst difference %+.5f)'
                         % (a, base, ok, len(items), worst))
        L.append('')
        res[label] = dict(big=big, sat=sat, means=means, chroma=chro)
    txt = '\n'.join(L) + '\n'
    print(txt)
    tag = '_'.join(arms)
    with open(os.path.join(HERE, 'logs', 'macro_%s.txt' % tag), 'w', newline='\n') as f:
        f.write(txt)
    with open(os.path.join(HERE, 'logs', 'macro_%s.json' % tag), 'w', newline='\n') as f:
        json.dump(res, f, indent=1)


if __name__ == '__main__':
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    cmd, arms = sys.argv[1], sys.argv[2:]
    {'tiling': cmd_tiling, 'trans': cmd_trans, 'macro': cmd_macro}[cmd](arms)
