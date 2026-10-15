#!/usr/bin/env python3
"""The air's ground layer from .recx files: the judge of tests/spells/volair1_ground.sh (lane VOLAIR1, 2026-10-14).

  python3 tests/spells/volair1_ground_check.py <out>     (VOLAIR1_RED=<name>: a red run, which must FAIL)

The density (src/gl/cellvolair.cpp VaLayer, res/shaders/lookdev_fog.glsl wwAirColLaw):
  rho(z) = (1 - w) B(z; zb, Hb) + w B(z; zn, Hn) + s B(z; zg, Hg),  B(z; z0, H) = min(1, exp(-(z - z0) / H))

Stages:
  A  twin      the closed-form column of the whole law (a float64 port of wwAirColLaw) vs a 200,001-point
               trapezoid of rho, 2,000 random layered media and segments                  rel <= 1e-6 (+ 1e-9 abs)
  N  near band the dumped near band (zn = K5.z / K5.x, 1/Hn from the records' top weight) and its share w re-fitted
               here (vaNearShare: least squares on the game's alpha, the grid of 1/50)   |dw| <= 0.04
  R  rungs     the census echo: form -> "form:...CommonwealthFoggy.recx", wild -> "wildcard:", edid -> "editorId:",
               none -> "none(no entry ...)", nosc -> "none(red nosidecar)", off -> "none(red noground)"
  V  values    form: strength 4 (the hours 17: 2 and 19: 6 at 18:00), 1/Hg = 1/600, zg = the near band's middle
               (0.5 + K5.z) / K5.x, w as none's; wild 5 / 300; edid 2.5 / 400; nosc and off: s = 0; off: w = 0
  C  merge     conf: a logged conflict naming b_, the winner a_ (strength 3); prov: ZMod's block wins over AMod's
               (the later provider by name), AMod reported as overridden, BMod (no groundFog block) not a claim
  T  applied   the transmittance over the froxels (the integrated volume's far slice): -ln T(form) + ln T(none) equals
               sigma0 s x the ground term's column along each froxel column's centre ray (closed form, here),
               median rel <= 2%, 95% of the columns within 5%; and nosc's picture is none's byte for byte
"""
import math
import os
import sys

import numpy as np

try:
    from PIL import Image
except ImportError:
    Image = None


class Gate:
    def __init__(self):
        self.rows = []

    def put(self, k, ok, msg):
        self.rows.append((k, bool(ok), msg))
        print('  %s %s  %s' % (k, 'PASS' if ok else 'FAIL', msg))

    def ok(self):
        return all(r[1] for r in self.rows)


def read_txt(p):
    d = {}
    for line in open(p, encoding='utf-8'):
        k, _, v = line.rstrip('\n').partition(' ')
        if not k:
            continue
        try:
            d[k] = np.array([float(x) for x in v.split()])
        except ValueError:
            d[k] = v
    return d


def read_vol(p):
    b = np.fromfile(p, dtype=np.uint8)
    c, r, s = np.frombuffer(b[:12].tobytes(), dtype=np.int32)
    v = np.frombuffer(b[12:].tobytes(), dtype=np.float32).reshape(2, s, r, c, 4)
    return v[0].astype(np.float64), v[1].astype(np.float64)


def col_one(lo, hi, z0, ih):
    """wwAirColOne, float64, arrays"""
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    out = np.where(hi > lo, hi - lo, 0.0)
    if ih <= 0.0:
        return out
    below = np.maximum(np.minimum(hi, z0) - lo, 0.0)
    a = np.maximum(lo, z0)
    w = np.maximum(hi - a, 0.0)
    u = w * ih
    e1 = np.where(u < 1e-3, 1.0 - u * (0.5 - u / 6.0), -np.expm1(-u) / np.where(u > 0, u, 1.0))
    above = np.exp(-(a - z0) * ih) * w * e1
    return np.where(hi > lo, below + np.where(hi > a, above, 0.0), 0.0)


def col_law(lo, hi, med, layer, ground):
    hi = np.minimum(hi, med[3])
    c = col_one(lo, hi, med[1], med[2])
    if layer[0] > 0:
        c = (1 - layer[0]) * c + layer[0] * col_one(lo, hi, layer[1], layer[2])
    if layer[3] > 0:
        c = c + layer[3] * col_one(lo, hi, ground[0], ground[1])
    return c


def rho_law(z, med, layer, ground):
    def B(z0, ih):
        return np.minimum(1.0, np.exp(-(z - z0) * ih)) if ih > 0 else np.ones_like(z)
    r = (1 - layer[0]) * B(med[1], med[2]) + layer[0] * B(layer[1], layer[2]) + layer[3] * B(ground[0], ground[1])
    return np.where(z > med[3], 0.0, r)


def game_alpha(K, d, z):
    ramp = d * K[0][0] - K[0][2]
    f = min(max(ramp, 0.0), 1.0)
    p0 = min(max(z * K[5][0] - K[5][2], 0.0), 1.0)
    p1 = min(max(z * K[5][1] - K[5][3], 0.0), 1.0)
    hb = p0 + (p1 - p0) * f
    mx = K[2][3]
    clampT = min((f - 0.75) * 4.0 * (1.0 - mx) + mx, 1.0) if ramp > 0.75 else mx
    escape = f * 66.666672 if ramp < 0.015 else 1.0
    I = min(clampT, f ** K[1][3]) if f > 0 else 0.0
    return (hb * K[3][3] + (1.0 - hb)) * I * escape


def near_share(K, sigma0, zb, ihb, dfull):
    """vaNearShare re-typed: (w, zn, 1/Hn)"""
    k5x, k5z, top = K[5][0], K[5][2], K[3][3]
    if not (k5x > 1e-9 and sigma0 > 0 and 0 < top < 1 and ihb > 0):
        return 0.0, 0.0, 0.0
    n0, n1 = k5z / k5x, (1 + k5z) / k5x
    ihn = -math.log(max(top, 1e-3)) / max(n1 - n0, 1e-3)
    if abs(n0 - zb) < 0.5 and abs(ihn - ihb) <= 1e-4 * ihb:
        return 0.0, n0, ihn
    b1 = zb - math.log(max(top, 1e-3)) / ihb
    lo, hi = min(n0, zb), max(n1, b1)
    span = max(hi - lo, 1.0)
    mx = min(max(K[2][3], 1e-4), 0.99)
    knee = (0.75 + K[0][2]) / K[0][0]
    dend = max(min(knee, dfull), 600.0)
    zs = [lo - 0.25 * span + 1.5 * span * j / 23.0 for j in range(24)]
    ds = [500.0 * (dend / 500.0) ** (i / 47.0) for i in range(48)]
    game = [[min(max(game_alpha(K, d, z), 0.0), mx) for d in ds] for z in zs]
    best, bw = None, 0.0
    for k in range(51):
        w = k / 50.0
        e = 0.0
        for j, z in enumerate(zs):
            r = (1 - w) * min(1.0, math.exp(-(z - zb) * ihb)) + w * min(1.0, math.exp(-(z - n0) * ihn))
            for i, d in enumerate(ds):
                a = min(1.0 - math.exp(-sigma0 * r * d), mx) - game[j][i]
                e += a * a
        if best is None or e < best - 1e-12:
            best, bw = e, w
    return bw, n0, ihn


def main():
    out = sys.argv[1]
    red = os.environ.get('VOLAIR1_RED', '')
    G = Gate()
    # A ------------------------------------------------------------------------------------------------------------
    rng = np.random.default_rng(20261014)
    worst = 0.0
    for _ in range(2000):
        zb = rng.uniform(-300, 600)
        med = (1.0, zb, 0.0 if rng.random() < 0.2 else 1 / rng.uniform(40, 4000), zb + rng.uniform(-200, 6000))
        layer = (rng.uniform(0, 1) if rng.random() < 0.7 else 0.0, rng.uniform(-300, 600), 1 / rng.uniform(40, 4000),
                 rng.uniform(0, 8) if rng.random() < 0.7 else 0.0)
        ground = (rng.uniform(-200, 400), 1 / rng.uniform(50, 2000))
        lo = rng.uniform(-1000, 3000)
        hi = lo + rng.uniform(0.01, 8000)
        top = min(hi, med[3])    # the density ends at the top: the trapezoid stops there (no step inside it)
        zz = np.linspace(lo, top, 200001) if top > lo else None
        trap = getattr(np, 'trapezoid', None) or np.trapz
        ref = float(trap(rho_law(zz, med, layer, ground), zz)) if zz is not None else 0.0
        est = float(col_law(lo, hi, med, layer, ground))
        worst = max(worst, abs(est - ref) / (abs(ref) + 1e-9) if abs(ref) > 1e-9 else abs(est - ref))
    G.put('A', worst <= 1e-6, 'the closed-form column of the law vs a trapezoid, 2000 random layered media: worst rel %.3g (bar 1e-6)' % worst)

    def has(*names):
        return all(os.path.exists(os.path.join(out, n)) for n in names)

    t = {k: read_txt(os.path.join(out, k + '.bin.txt')) for k in ('none', 'form', 'nosc', 'off', 'wild', 'edid', 'conf', 'prov')
         if has(k + '.bin.txt')}
    if len(t) < 8:
        G.put('-', False, 'missing dumps: %s' % ', '.join(k for k in ('none', 'form', 'nosc', 'off', 'wild', 'edid', 'conf', 'prov') if k not in t))
        print('VOLAIR1 GROUND VERDICT: %s' % ('FAIL' if not red else 'RED CONTROL FAILED AS IT MUST'))
        return 1 if not red else 0
    K = [list(t['none']['fogK%d' % k]) for k in range(6)]
    am = t['none']['airMed']
    dfull = (1 + K[0][2]) / K[0][0]
    # N ------------------------------------------------------------------------------------------------------------
    w, zn, ihn = near_share(K, float(am[0]), float(am[1]), float(am[2]), dfull)
    ln = t['none']['airLayer']
    okn = abs(ln[0] - w) <= 0.04 and abs(ln[1] - zn) <= 1e-3 * max(1, abs(zn)) and abs(ln[2] - ihn) <= 1e-4 * max(ihn, 1e-12) and ln[3] == 0
    G.put('N', okn, 'near band: w %.2f (re-fit %.2f), zn %.2f (%.2f), 1/Hn %.6g (%.6g), s %.3g (must be 0)' % (ln[0], w, ln[1], zn, ln[2], ihn, ln[3]))
    # R ------------------------------------------------------------------------------------------------------------
    rc = {k: str(v['recx']) if 'recx' in v else '' for k, v in t.items()}
    okr = (rc['form'].startswith('form:') and rc['form'].endswith('/CommonwealthFoggy.recx')
           and rc['wild'].startswith('wildcard:') and rc['edid'].startswith('editorId:')
           and rc['none'].startswith('none(no entry') and rc['nosc'] == 'none(red nosidecar)' and rc['off'] == 'none(red noground)')
    G.put('R', okr, 'rungs: ' + '; '.join('%s=%s' % (k, rc[k]) for k in ('form', 'wild', 'edid', 'none', 'nosc', 'off')))
    # V ------------------------------------------------------------------------------------------------------------
    zg = (0.5 + K[5][2]) / K[5][0]

    def lay(k):
        return t[k]['airLayer'], t[k]['airGround']

    def near(a, b, tol=1e-5):
        return abs(a - b) <= tol * max(1.0, abs(b))
    msgs, okv = [], True
    for k, s_, th in (('form', 4.0, 600.0), ('wild', 5.0, 300.0), ('edid', 2.5, 400.0)):
        L, Gd = lay(k)
        o = near(L[3], s_) and near(Gd[1], 1 / th, 1e-5) and near(Gd[0], zg, 1e-4) and L[0] == ln[0]
        okv = okv and o
        msgs.append('%s s %.4g (%.4g) Hg %.1f (%.0f) zg %.1f (%.1f) w %.2f' % (k, L[3], s_, 1 / Gd[1] if Gd[1] > 0 else 0, th, Gd[0], zg, L[0]))
    Ln, _ = lay('nosc')
    Lo, _ = lay('off')
    o = Ln[3] == 0 and Ln[0] == ln[0] and not Lo.any()
    okv = okv and o
    msgs.append('nosc s %.3g w %.2f; off layer %s' % (Ln[3], Ln[0], ' '.join('%.3g' % x for x in Lo)))
    G.put('V', okv, '; '.join(msgs))
    # C ------------------------------------------------------------------------------------------------------------
    cf = str(t['conf'].get('recxConflict', ''))
    Lc, _ = lay('conf')
    okc = 'a_CommonwealthFoggy.recx' in rc['conf'] and 'b_CommonwealthFoggy.recx' in cf and near(Lc[3], 3.0)
    Lp, Gp = lay('prov')
    pf = str(t['prov'].get('recxConflict', ''))
    okc = okc and rc['prov'].startswith('form:ZMod/') and near(Lp[3], 3.5) and near(Gp[1], 1 / 350.0) and pf.startswith('overrides:') \
        and '/AMod/' in pf and '/BMod/' not in pf
    G.put('C', okc, 'conflict: winner %s, s %.3g (3); %s; providers: winner %s, s %.3g (3.5); %s' % (rc['conf'], Lc[3], cf, rc['prov'], Lp[3], pf))
    # T ------------------------------------------------------------------------------------------------------------
    okt, msg = False, 'no volumes'
    if has('none.bin', 'form.bin'):
        _, In = read_vol(os.path.join(out, 'none.bin'))
        _, If = read_vol(os.path.join(out, 'form.bin'))
        tf = t['form']
        cols, rows, N = (int(x) for x in tf['dims'])
        near_, logR = tf['slices']
        p00, p11 = tf['proj']
        fv, ds = tf['fogView'], float(tf['fogDistScale'][0])
        jj, ii = np.meshgrid(np.arange(rows), np.arange(cols), indexing='ij')
        dv = np.stack([((ii + 0.5) / cols * 2 - 1) / p00, ((jj + 0.5) / rows * 2 - 1) / p11, -np.ones((rows, cols))], -1)
        dv /= np.linalg.norm(dv, axis=-1, keepdims=True)
        dzdt = (dv @ fv[:3]) / ds
        zc = float(fv[3])
        far = near_ * math.exp(logR)
        Lf, Gf = lay('form')
        medf = tf['airMed']
        za, zz = zc + dzdt * near_, zc + dzdt * far
        lo, hi = np.minimum(za, zz), np.maximum(za, zz)
        cg = col_one(lo, np.minimum(hi, medf[3]), Gf[0], Gf[1])
        dz = np.abs(zz - za)
        flatc = dz < 0.5
        zm = 0.5 * (za + zz)
        rho_g = np.where(zm > medf[3], 0.0, np.minimum(1.0, np.exp(-(zm - Gf[0]) * Gf[1])))
        pred = float(medf[0]) * Lf[3] * (far - near_) * np.where(flatc, rho_g, cg / np.where(flatc, 1.0, dz))
        Tn, Tf = In[N - 1, ..., 3], If[N - 1, ..., 3]
        use = (Tn > 1e-5) & (Tf > 1e-5) & (pred > 1e-3)
        got = -np.log(np.where(use, Tf, 1.0)) + np.log(np.where(use, Tn, 1.0))
        if use.sum() >= 0.25 * use.size and Lf[3] > 0:
            rel = np.abs(got[use] - pred[use]) / pred[use]
            okt = float(np.median(rel)) <= 0.02 and float(np.mean(rel <= 0.05)) >= 0.95
            msg = '%d/%d columns judged; the ground term\'s optical depth vs the closed form: median rel %.4f, %.3f within 5%%; predicted median %.3f' % (
                use.sum(), use.size, float(np.median(rel)), float(np.mean(rel <= 0.05)), float(np.median(pred[use])))
        else:
            msg = 'only %d/%d columns to judge (s %.3g): no ground layer applied' % (use.sum(), use.size, Lf[3])
        if Image and has('none.png', 'nosc.png'):
            same = np.array_equal(np.asarray(Image.open(os.path.join(out, 'none.png'))), np.asarray(Image.open(os.path.join(out, 'nosc.png'))))
            okt = okt and same
            msg += '; nosc picture == none picture: %s' % same
    G.put('T', okt, msg)
    ok = G.ok()
    if red:
        print('VOLAIR1 GROUND VERDICT: %s (red %s)' % ('RED CONTROL FAILED AS IT MUST' if not ok else 'RED NOT CAUGHT', red))
        return 0 if not ok else 1
    print('VOLAIR1 GROUND VERDICT: %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
