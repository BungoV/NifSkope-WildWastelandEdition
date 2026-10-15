#!/usr/bin/env python3
"""Judge tests/spells/volpart1_cell.sh (lane VOLPART1, 2026-10-14): the volumetric particles against an independent
rebuild and against the air they sit in.

  python tests/spells/volpart1_check.py <out dir> <camera...>      (VOLPART1_RED=<red> when judging a red)

Every lit system's particles are probed at their centres (WW_CELL_VOLPART_DUMP, 8 RGBA a particle, the last frame
alone): 0 thin (the light the puff scatters toward the eye, per unit opacity: the air's J), 1 / 2 thick at the view's
+Z / -Z normal (wrap 0), 3 world P + the fog-frame height, 4 the eye's ray + its distance, 5 the integrated air the
draw applies, 6 the lit factor at +Z with the particle's own alpha, 7 the view point + alpha. The placed lights' records
come from the very buffer the shader reads (<dump>.lights); the air's volumes from WW_CELL_AIR_DUMP. The rebuild
re-types the law in numpy (float64) and shares nothing with the exe but those inputs.

Stages (bars fixed before the first run; "lit" = particles whose rebuilt thin luminance >= 1e-3 x the largest):
  X  present   every shot and dump the camera needs exists; >= 1 lit system with >= 4 particles dumped; no shader errors
  A  analytic  (ana: the directional light + the placed lights, no shadow) slot 0 = the rebuild (sun x its transmittance
               to the top x pi phase + every placed light: colour x radial curve x cone x shape x the air's transmittance
               to it x pi phase): median relative error < 1%, >= 99% of the lit particles within 3%
  L  lamps     (ana) near vs far: of the particles one light gives >= 90% of, every pair under the same light at
               distances differing >= 10%: the measured luminance ratio = the analytic irradiance ratio within 3%,
               on >= 95% of the pairs; >= 1 pair with an analytic ratio >= 1.2 (the lamp's falloff is really seen)
  T  tint      (ana) the particles one light gives >= 90% of, that light's colour >= 0.05 from grey in chromaticity: the
               measured chromaticity = (the light's colour x the phase)'s within 0.02 on >= 95%; >= 3 such particles
  N  normals   (ana) slots 1 and 2 = the rebuilt wrapped diffuse at +Z and -Z within the A bars, and they differ by
               >= 10% on >= 20% of the lit particles (the normal really turns the light)
  K  thickness (ana) slot 6 = mix(1, (1 - a) thin + a thick(w = 1 - a), influence) within the A bars, over the particles
               with a >= 0.05 (>= 4 of them)
  F  fog-over  (ana, full) slot 5 = the integrated volume read at the particle's column and distance (the rebuild's
               trilinear over the dumped volume): T within 2e-3 and L within 2% + 1e-4 x max on >= 99%; and the air is
               really there: T <= 0.98 on >= 25% of the particles
  G  air       (ana, full) the particle's light vs the air's at its froxel: J = inject.rgb / (1 - inject.a) at the
               particle's column (bilinear), trilinear in depth between the froxel centres (k + 0.5), over the
               particles nearer than 1000 units (>= 100; farther, a slice is deeper than a lamp's falloff: counted only); per-particle luminance ratio in [0.8, 1.25] on >= 90% of the
               particles whose J is >= 1e-3 x the largest; median chromaticity distance <= 0.02; log-correlation >= 0.9
  E  edge      the hard line where a card meets a surface: q12 (the soft cards' scene depth, card depth, half-depth h,
               the fade drawn; the dump reads the depth at the full-size pixel) = clamp((scene - (card - h)) / 2h) within
               2e-3 on >= 99%; >= 20 px on the ramp (a surface within h of the card: where the hard line was); the
               pictures: the fade changes >= 500 px (full vs nosoftp). (The first E, a hardness ratio over the whole
               picture, could not tell a contact line from a real occluder's edge in front of a beam: dropped 10-15.)
  Population bars (>= N px / particles) pool over the cameras and dumps: a stage passes when its law holds on every
  camera and its population is met on at least one.
  O  off       off.png (the row off) == before.png (the exe before the lane) byte for byte, or within the before exe's
               own flake (rung2: max 1, every differing pixel within 32 px of a flaking one)
  The volumetric cards (an "fx:" block: 8 slots + a "q" line of 4 more a pixel, sampled across the card):
  V  edge-on   q8 edge fade = smoothstep(0.05, 0.35, |N.V|) within 2e-3 on >= 99%; >= 10 px faded to <= 0.9; the alpha
               written <= edge x near x puff + 2e-3 on >= 95% (the dump pass's soft factor makes it an inequality)
  P  puff      q9 puff alpha = 1 - (1 - a)^(1.5 sqrt(1 - r^2)) within 2e-3 on >= 99%; the median optical depth gain at the
               centre (r < 0.35) over the rim (0.6..0.9) = the ball's path-length ratio within 3% (>= 4 px each)
  H  phase     one unit light on the eye's side (q10) and behind the card (q11) = m phase(c) + (1 - m) wrap(N.to, m)
               phase(c) / iso (c = -1 / +1, m = 1 - puff alpha, the air's three-lobe phase) within the A bars; >= 10 px
               whose predicted behind / front >= 1.2 (the backlight is really seen)
  M  near      q8 near fade = smoothstep(16, 128, depth) within 2e-3 on >= 99%; the depth = the view point's x
               fxDistScale; >= 20 px nearer than 128 (required at memden, whose camera stands in the steam)
  The thick rebuild (N, K) carries the phase normalised to its sphere average, as the shader does.
Reds: nolights -> A L G; white -> T; flat -> N; nothick -> K P; nofog -> F; nosoft -> E; noshadow -> G (full, if a
particle is in a shadow); noedge -> V; nophase -> H A; nonear -> M (memden).
"""
import os
import sys

import numpy as np

try:
    from PIL import Image
except ImportError:
    Image = None

PI = 3.14159265
G_RESOLVED = 1000.0  # G judges particles nearer than this (game units): beyond, a froxel slice is deeper than ~115 units
LUM = np.array([0.2126, 0.7152, 0.0722])


# ---------------------------------------------------------------- reading
def read_vp(p):
    """the particle dump: (blocks, frame); a block = dict of uniforms + 'p' (n x 8 x 4)"""
    blocks, cur, frame = [], None, None
    for line in open(p, encoding='utf-8'):
        k, _, v = line.strip().partition(' ')
        if not k:
            continue
        if k == 'frame':
            frame = int(v)
            continue
        if k == 'system':
            cur = {'head': v, 'p': []}
            blocks.append(cur)
            continue
        if k == 'p':
            cur['p'].append([float(x) for x in v.split()])
            continue
        if k == 'q':
            cur.setdefault('qq', []).append([float(x) for x in v.split()])
            continue
        try:
            cur[k] = np.array([float(x) for x in v.split()])
        except ValueError:
            cur[k] = v
    for b in blocks:
        b['p'] = np.array(b['p'], dtype=np.float64).reshape(-1, 8, 4)
        if 'qq' in b:
            b['qq'] = np.array(b['qq'], dtype=np.float64)
            b['qq'] = b['qq'].reshape(len(b['qq']), -1, 4)
    return blocks, frame


def read_lights(p):
    L = []
    for line in open(p, encoding='utf-8'):
        if line.startswith('light '):
            L.append([float(x) for x in line.split()[1:]])
    return np.array(L, dtype=np.float64).reshape(-1, 8, 4)


def read_txt(p):
    d = {}
    for line in open(p, encoding='utf-8'):
        k, _, v = line.strip().partition(' ')
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


def png(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(np.int32)


# ---------------------------------------------------------------- the law, re-typed
class Air:
    def __init__(self, b):
        self.m = b['airMed']
        self.ph = [b['volPhase[%d]' % k] for k in range(3)]

    def F(self, z):
        m = self.m
        z = min(z, m[3])
        x = z - m[1]
        if x <= 0.0 or m[2] <= 0.0:
            return x
        return -np.expm1(-x * m[2]) / m[2]

    def rho(self, z):
        m = self.m
        if z > m[3]:
            return 0.0
        return min(1.0, np.exp(-(z - m[1]) * m[2])) if m[2] > 0 else 1.0

    def od(self, za, zb, ln):
        dz = zb - za
        if abs(dz) < 0.5:
            return self.m[0] * ln * self.rho(0.5 * (za + zb))
        return self.m[0] * ln * (self.F(zb) - self.F(za)) / dz

    def tup(self, z, mu):
        m = self.m
        if z >= m[3]:
            return 1.0
        mu = max(mu, 0.02)
        return np.exp(-m[0] * (self.F(m[3]) - self.F(z)) / mu)

    def phase_pi(self, c):
        def hg(c, g):
            d = 1.0 + g * g - 2.0 * g * c
            return (1.0 - g * g) / (4.0 * PI * d * np.sqrt(d))
        p = self.ph
        return PI * (p[0][:3] * (3.0 / (16.0 * PI)) * (1.0 + c * c) + p[1][:3] * hg(c, p[1][3]) + p[2][:3] * hg(c, p[2][3]))

    def iso(self):
        """the phase's sphere average (x pi): the three weights summed / 4 (the thick term's normaliser)"""
        return np.maximum((self.ph[0][:3] + self.ph[1][:3] + self.ph[2][:3]) * 0.25, 1e-6)


def radial(d, r, bse, red):
    x = min(max(d / max(r, 0.001), 0.0), 1.0)
    xe = x ** bse[2] if bse[2] > 0 else 1.0
    k = 1.0 - min(max(bse[1] * xe + bse[0], 0.0), 1.0)
    return k if (int(red) & 1) else k ** 2.2


def shape_in(t, P):
    sh = t[1][3]
    if sh > -2.5:
        return True
    if sh > -3.5:
        return np.dot(P - t[0][:3], t[2][:3]) >= 0.0
    k = [np.dot(t[5 + j][:3], P) + t[5 + j][3] for j in range(3)]
    return all(abs(x) <= 1.0 for x in k)


def light_terms(b, lights, P, z, dW):
    """per placed light: (index, e rgb (no phase), L, phase-pi rgb)"""
    air = Air(b)
    out = []
    zoff = z - P[2]
    for i in range(int(b['cellLightCount'][0])):
        t = lights[i]
        Lv = t[0][:3] - P
        d = np.linalg.norm(Lv)
        if d >= t[0][3] or d < 0.001:
            continue
        L = Lv / d
        if not shape_in(t, P):
            continue
        a = radial(d, t[0][3], t[3][:3], b['cellRed'][0])
        if t[1][3] > -1.5:
            base = min(max(1.0 - (1.0 - np.dot(-L, t[2][:3])) / max(1.0 - t[1][3], 1e-4), 0.0), 1.0)
            a *= min(base ** max(t[2][3], 1e-3), 1.0)
        if a <= 0.0:
            continue
        e = t[1][:3] * a * np.exp(-air.od(z, t[0][2] + zoff, d))
        out.append((i, e, L, air.phase_pi(np.dot(L, dW))))
    return out


def rebuild(b, lights, q, N=None, w=0.0):
    """(thin, thick at world normal N with wrap w, [(light, thin share rgb)]) at a probe q (8 x 4), terms 1 + 2, no shadow"""
    air = Air(b)
    P, z, dW = q[3][:3], q[3][3], q[4][:3]
    terms = int(b['volTerms'][0])
    thin, thick, per = np.zeros(3), np.zeros(3), []
    wrap = lambda nl: max(nl + w, 0.0) / (1.0 + w)
    if (terms & 1) and b['cellHasDir'][0] > 0.5:
        to = b['cellDirTo'] / np.linalg.norm(b['cellDirTo'])
        e = b['cellDirColor'] * air.tup(z, to[2])
        s = e * air.phase_pi(np.dot(to, dW))
        thin += s
        per.append(('sun', s, b['cellDirColor']))
        if N is not None:
            thick += e * wrap(np.dot(N, to)) * air.phase_pi(np.dot(to, dW)) / air.iso()
    if terms & 2:
        for i, e, L, ph in light_terms(b, lights, P, z, dW):
            thin += e * ph
            per.append((i, e * ph, lights[i][1][:3]))
            if N is not None:
                thick += e * wrap(np.dot(N, L)) * ph / air.iso()
    return thin, thick, per


def zw(b):
    v = np.array([b['cellRow[0]'][2], b['cellRow[1]'][2], b['cellRow[2]'][2]])
    return v / np.linalg.norm(v)


def lum(c):
    return float(np.dot(LUM, c))


def chroma(c):
    s = float(np.sum(c))
    return np.array([c[0] / s, c[1] / s]) if s > 1e-12 else np.array([1 / 3, 1 / 3])


def rel(got, want):
    return np.linalg.norm(got - want) / max(np.linalg.norm(want), 1e-12)


# ---------------------------------------------------------------- the air's volume
def trilinear2(vol, u, v, k):
    """bilinear in the column of layer k (texel centres at +0.5, clamped)"""
    N, R, C = vol.shape[:3]
    x = np.clip(u * C - 0.5, 0, C - 1)
    y = np.clip(v * R - 0.5, 0, R - 1)
    x0, y0 = int(np.floor(x)), int(np.floor(y))
    x1, y1 = min(x0 + 1, C - 1), min(y0 + 1, R - 1)
    fx, fy = x - x0, y - y0
    return ((vol[k, y0, x0] * (1 - fx) + vol[k, y0, x1] * fx) * (1 - fy)
            + (vol[k, y1, x0] * (1 - fx) + vol[k, y1, x1] * fx) * fy)


def vol_at(integ, t, d, u, v):
    near, logR = t['slices']
    N = integ.shape[0]
    f = np.log(max(d, 1e-6) / near) / logR * N
    if d <= near:
        return np.array([0.0, 0.0, 0.0, 1.0])
    if f < 1.0:
        lo = trilinear2(integ, u, v, 0)
        out = np.concatenate([lo[:3] * f, [1.0 - (1.0 - lo[3]) * f]])
    else:
        z = np.clip(min(f, N) - 1.0, 0, N - 1)
        z0 = int(np.floor(z))
        z1 = min(z0 + 1, N - 1)
        fz = z - z0
        out = trilinear2(integ, u, v, z0) * (1 - fz) + trilinear2(integ, u, v, z1) * fz
    out[:3] = np.maximum(out[:3], 0.0)
    out[3] = min(max(out[3], 0.0), 1.0)
    return out


def proj_uv(b, pv):
    pr = b['proj']
    w = -pv[2]
    return pr[0] * pv[0] / w * 0.5 + 0.5, pr[1] * pv[1] / w * 0.5 + 0.5


# ---------------------------------------------------------------- stages
def particles(blocks, lit_only=True):
    """(block, probe) of every particle of a lit, blended system"""
    out = []
    for b in blocks:
        if lit_only and (int(b['fxLitMode'][0]) == 0):
            continue
        for q in b['p']:
            out.append((b, q))
    return out


def stage_analytic(blocks, lights, res):
    ps = particles(blocks)
    rows = []
    for b, q in ps:
        thin, _, per = rebuild(b, lights, q)
        Z = zw(b)
        _, thP, _ = rebuild(b, lights, q, Z, 0.0)
        _, thM, _ = rebuild(b, lights, q, -Z, 0.0)
        a = q[7][3]
        m = 1.0 - min(max(a, 0.0), 1.0)
        th6, tk6, _ = rebuild(b, lights, q, Z, m)
        inf = b['lightingInfluence'][0]
        selfLit = (int(b['fxLitMode'][0]) & 8) != 0 or (b['cellInterior'][0] < 0.5 and b['cellHasDir'][0] < 0.5)
        lit6 = np.ones(3) if selfLit else 1.0 + (m * th6 + (1 - m) * tk6 - 1.0) * inf
        rows.append(dict(b=b, q=q, thin=thin, per=per, thP=thP, thM=thM, lit6=lit6, a=a))
    if not rows:
        return rows
    top = max(lum(r['thin']) for r in rows)
    for r in rows:
        r['lit'] = lum(r['thin']) >= 1e-3 * top and top > 0
    return rows


def smooth(e0, e1, x):
    t = np.clip((np.asarray(x, dtype=np.float64) - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def card_rows(blocks):
    """every sampled pixel of a volumetric effect card: (block, p 8 x 4, q 4 x 4)"""
    out = []
    for b in blocks:
        if 'qq' not in b or not b['head'].startswith('fx:'):
            continue
        for p, q in zip(b['p'], b['qq']):
            out.append((b, p, q))
    return out


def stage_cards(t, blocks, red, judge, cam=''):
    """V edge-on fade, P puff thickness, H phase (light behind vs in front), M near-camera fade, over the cards' pixels.
    q 8 = (|N.V|, edge fade, near fade, depth in game units); 9 = (puff q.xy, alpha in, puff alpha);
    10 = (one unit light on the eye side, N.(-dW)); 11 = (one unit light behind the card, the alpha written);
    12 = (the scene's depth, the card's, the slab half-depth h, all game units; the soft fade drawn)"""
    cs = card_rows(blocks)
    if not cs:
        if red in ('', 'noedge', 'nothick', 'nophase', 'nonear'):
            judge('V', False, '%s: no volumetric card pixels dumped' % t)
        return
    air = Air(cs[0][0])
    if red in ('', 'noedge'):
        nv = np.array([q[0][0] for _, _, q in cs])
        fe = np.array([q[0][1] for _, _, q in cs])
        want = smooth(0.05, 0.35, nv)
        ok = np.abs(fe - want) <= 2e-3
        seen = int((want <= 0.9).sum())
        # the fade really reaches the alpha (11.w carries the dump pass's own soft factor, so <= only)
        prod = np.array([q[0][1] * q[0][2] * q[1][3] for _, _, q in cs])
        aw = np.array([q[3][3] for _, _, q in cs])
        le = float((aw <= prod + 2e-3).mean())
        okz = float(ok[want <= 0.9].mean()) if seen else 1.0
        judge('V', ok.mean() >= 0.99 and okz >= 0.99 and le >= 0.95, pop=seen >= 10, msg=
              '%s: edge fade vs smoothstep(0.05, 0.35, |N.V|) over %d px: within 2e-3 %.4f; px faded to <= 0.9: %d; '
              'within 2e-3 there %.4f; alpha written <= edge x near x puff: %.4f' % (t, len(cs), float(ok.mean()), seen, okz, le))
    if red in ('', 'nothick'):
        qx = np.array([q[1][:2] for _, _, q in cs])
        a0 = np.array([q[1][2] for _, _, q in cs])
        ap = np.array([q[1][3] for _, _, q in cs])
        r2 = (qx ** 2).sum(1)
        l = np.sqrt(np.maximum(1.0 - r2, 0.0))
        law = 1.0 - (1.0 - np.clip(a0, 0.0, 0.999)) ** (1.5 * l)
        okA = float((np.abs(ap - law) <= 2e-3).mean())
        use = (a0 > 0.02) & (a0 < 0.95)
        tau = np.where(use, -np.log(np.maximum(1.0 - ap, 1e-9)) / np.maximum(-np.log(1.0 - np.clip(a0, 0, 0.999)), 1e-9), np.nan)
        r = np.sqrt(r2)
        ce, ri = use & (r < 0.35), use & (r > 0.6) & (r < 0.9)
        if ce.sum() >= 4 and ri.sum() >= 4:
            got = float(np.median(tau[ce]) / max(np.median(tau[ri]), 1e-9))
            ana = float(np.median(1.5 * l[ce]) / max(np.median(1.5 * l[ri]), 1e-9))
        else:
            got = ana = float('nan')
        judge('P', okA >= 0.99 and not abs(got / ana - 1.0) > 0.03, pop=ce.sum() >= 4 and ri.sum() >= 4, msg=
              '%s: puff alpha vs 1-(1-a)^(1.5 sqrt(1-r^2)) over %d px: within 2e-3 %.4f; centre/rim optical depth %.4f vs '
              'the ball\'s path-length ratio %.4f (%d centre, %d rim px)' % (t, len(cs), okA, got, ana, int(ce.sum()), int(ri.sum())))
    if red in ('', 'nophase'):
        iso = air.iso()
        ef, eb, ratios = [], [], []
        for _, _, q in cs:
            m = 1.0 - min(max(q[1][3], 0.0), 1.0)
            wrap = lambda nl: max(nl + m, 0.0) / (1.0 + m)
            nf = q[2][3]
            pf, pb = air.phase_pi(-1.0), air.phase_pi(1.0)
            wf = m * pf + (1.0 - m) * wrap(nf) * pf / iso
            wb = m * pb + (1.0 - m) * wrap(-nf) * pb / iso
            ef.append(rel(q[2][:3], wf))
            eb.append(rel(q[3][:3], wb))
            ratios.append(lum(wb) / max(lum(wf), 1e-12))
        okF, mF, wF = bars(ef)
        okB, mB, wB = bars(eb)
        big = int((np.array(ratios) >= 1.2).sum())
        judge('H', okF and okB, pop=big >= 10, msg='%s: one light in front: median %.4f within 3%% %.4f; behind: median %.4f '
              'within 3%% %.4f; px whose predicted behind/front >= 1.2: %d (median ratio %.3f, g %s)'
              % (t, mF, wF, mB, wB, big, float(np.median(ratios)), [round(float(air.ph[k][3]), 3) for k in (1, 2)]))
    if red in ('', 'nosoft'):
        sl = [(b, q) for b, _, q in cs if q.shape[0] >= 5 and int(b['fxSoft'][0]) != 0]
        if sl:
            sw = np.array([q[4][0] for _, q in sl])
            cw = np.array([q[4][1] for _, q in sl])
            hh = np.array([q[4][2] for _, q in sl])
            S = np.array([q[4][3] for _, q in sl])
            want = np.clip((sw - (cw - hh)) / (2.0 * np.maximum(hh, 1e-6)), 0.0, 1.0)
            good = np.abs(S - want) <= 2e-3
            ok = float(good.mean())
            rz = (want > 0.02) & (want < 0.98)
            ramp = int(rz.sum())
            okz = float(good[rz].mean()) if ramp else 1.0
            judge('E', ok >= 0.99 and okz >= 0.99, pop=ramp >= 20, msg='%s: soft slab fade vs clamp((scene - (card - h)) / 2h) over %d px of soft '
                  'cards: within 2e-3 %.4f; px on the ramp (a surface within h of the card): %d, within 2e-3 there %.4f; h %s'
                  % (t, len(sl), ok, ramp, okz, sorted(set(np.round(hh, 1).tolist()))[:4]))
        else:
            judge('E', False, pop=False, msg='%s: no soft card pixels (slot 12) dumped' % t)
    if red in ('', 'nonear'):
        dep = np.array([q[0][3] for _, _, q in cs])
        fn = np.array([q[0][2] for _, _, q in cs])
        dv = np.array([-p[7][2] * b['fxDistScale'][0] for b, p, _ in cs])
        okd = float((np.abs(dep - dv) <= 1e-3 * np.maximum(dv, 1.0)).mean())
        good = np.abs(fn - smooth(16.0, 128.0, dep)) <= 2e-3
        ok = float(good.mean())
        close = int((dep < 128.0).sum())
        okz = float(good[dep < 128.0].mean()) if close else 1.0  # the fade's own zone, judged alone too
        judge('M', ok >= 0.99 and okz >= 0.99 and okd >= 0.99, pop=close >= 20, msg=
              '%s: near fade vs smoothstep(16, 128, depth) over %d px: within 2e-3 %.4f; depth = the view point\'s %.4f; '
              'px nearer than 128: %d (nearest %.1f), within 2e-3 there %.4f' % (t, len(cs), ok, okd, close, float(dep.min()), okz))


def bars(errs):
    errs = np.asarray(errs)
    if errs.size == 0:
        return False, float('nan'), float('nan')
    med, w3 = float(np.median(errs)), float((errs <= 0.03).mean())
    return med < 0.01 and w3 >= 0.99, med, w3


def main():
    out = sys.argv[1]
    cams = sys.argv[2:]
    red = os.environ.get('VOLPART1_RED', '')
    verdict, pops = {}, {}

    def judge(stage, ok, msg, pop=True):
        """ok: the law holds on what is there; pop: the population the stage needs is there (pooled: any camera/dump)"""
        verdict.setdefault(stage, []).append(bool(ok))
        pops.setdefault(stage, []).append(bool(pop))
        print('  %s %-4s %s%s' % (stage, 'PASS' if ok and pop else ('ok' if ok else 'FAIL'), msg,
                                 '' if pop else '  [population short here: pooled over the cameras]'))

    for cam in cams:
        d = os.path.join(out, cam)
        print('== %s' % cam)
        need = {'': ['ana', 'full', 'nosoftp', 'hide', 'off', 'before', 'rung2'], 'nolights': ['ana', 'full'],
                'white': ['ana'], 'flat': ['ana'], 'nothick': ['ana'], 'nofog': ['ana'], 'nosoft': ['full'],
                'noshadow': ['full'], 'noedge': ['ana'], 'nophase': ['ana'], 'nonear': ['ana']}[red]
        miss = [t for t in need if not os.path.exists(os.path.join(d, t + '.png')) or os.path.getsize(os.path.join(d, t + '.png')) == 0]
        errs = 0
        for t in need:
            n = os.path.join(d, t + '.notes')
            if os.path.exists(n):
                errs += open(n, encoding='utf-8', errors='replace').read().lower().count('errors during shader compilation')
        dumps = {}
        for t in ('ana', 'full'):
            vp = os.path.join(d, t + '.vp')
            if t in need and os.path.exists(vp) and os.path.exists(vp + '.lights') and os.path.exists(os.path.join(d, t + '.air')):
                blocks, frame = read_vp(vp)
                dumps[t] = (blocks, frame, read_lights(vp + '.lights'), read_vol(os.path.join(d, t + '.air')),
                            read_txt(os.path.join(d, t + '.air.txt')))
            elif t in need:
                miss.append(t + '.vp')
        nlit = {t: sum(len(b['p']) for b in v[0] if int(b['fxLitMode'][0]) != 0) for t, v in dumps.items()}
        judge('X', not miss and errs == 0 and all(n >= 4 for n in nlit.values()),
              'missing %s, shader errors %d, lit particles dumped %s, frames %s'
              % (miss or 'none', errs, nlit, {t: v[1] for t, v in dumps.items()}))
        for t, (blocks, frame, lights, vols, atxt) in dumps.items():
            heads = sorted(set(b['head'].split(' block')[0] for b in blocks))
            print('    %s: %d blocks (%s), %d lights, terms %s, csm %s, shadow %s, red %s' % (
                t, len(blocks), ', '.join(heads[:6]), len(lights), blocks[0]['volTerms'][0] if blocks else '-',
                blocks[0]['volCsm'][0] if blocks else '-', blocks[0]['cellShadowOn'][0] if blocks else '-',
                blocks[0]['volPartRed'][0] if blocks else '-'))

        # ---- the analytic stages (ana)
        if 'ana' in dumps:
            blocks, _, lights, _, _ = dumps['ana']
            rows = stage_analytic(blocks, lights, None)
            lit = [r for r in rows if r['lit']]
            if red in ('', 'nolights'):
                ok, med, w3 = bars([rel(r['q'][0][:3], r['thin']) for r in lit])
                judge('A', ok and len(lit) >= 4, 'thin vs the rebuild over %d lit particles: median %.4f, within 3%% %.4f'
                      % (len(lit), med, w3))
                # L: near / far under one light
                groups = {}
                for r in lit:
                    tot = lum(r['thin'])
                    for k, s, col in r['per']:
                        if k != 'sun' and lum(s) >= 0.9 * tot:
                            groups.setdefault(k, []).append(r)
                pairs, okp, big = 0, 0, 0
                for k, g in groups.items():
                    for i in range(len(g)):
                        for j in range(i + 1, len(g)):
                            di = np.linalg.norm(lights[k][0][:3] - g[i]['q'][3][:3])
                            dj = np.linalg.norm(lights[k][0][:3] - g[j]['q'][3][:3])
                            if abs(di - dj) < 0.1 * max(di, dj):
                                continue
                            want = lum(g[i]['thin']) / max(lum(g[j]['thin']), 1e-12)
                            got = lum(g[i]['q'][0][:3]) / max(lum(g[j]['q'][0][:3]), 1e-12)
                            pairs += 1
                            okp += abs(got / want - 1.0) <= 0.03
                            big += max(want, 1 / want) >= 1.2
                judge('L', pairs > 0 and okp >= 0.95 * pairs and big >= 1,
                      'near/far pairs under one light: %d (%d lights), within 3%% %d, analytic ratio >= 1.2: %d'
                      % (pairs, len(groups), okp, big))
            if red in ('', 'white'):
                n, okc, dmax = 0, 0, 0.0
                for r in lit:
                    tot = lum(r['thin'])
                    for k, s, col in r['per']:
                        if lum(s) >= 0.9 * tot and np.linalg.norm(chroma(col) - np.array([1 / 3, 1 / 3])) >= 0.05:
                            dd = float(np.linalg.norm(chroma(r['q'][0][:3]) - chroma(r['thin'])))
                            n += 1
                            okc += dd <= 0.02
                            dmax = max(dmax, dd)
                judge('T', okc >= 0.95 * n, pop=n >= 3, msg='particles under one coloured light: %d, chromaticity (vs the rebuilt total) within 0.02: %d '
                      '(worst %.4f)' % (n, okc, dmax))
            if red in ('', 'flat'):
                okP, mP, wP = bars([rel(r['q'][1][:3], r['thP']) for r in lit])
                okM, mM, wM = bars([rel(r['q'][2][:3], r['thM']) for r in lit])
                turn = np.mean([rel(r['thP'], r['thM']) >= 0.1 if lum(r['thP'] + r['thM']) > 0 else False for r in lit]) if lit else 0
                judge('N', okP and okM and turn >= 0.2, 'thick +Z: median %.4f within 3%% %.4f; -Z: median %.4f within 3%% %.4f; '
                      '+Z vs -Z differ >= 10%% on %.3f' % (mP, wP, mM, wM, turn))
            if red in ('', 'nothick'):
                th = [r for r in lit if r['a'] >= 0.05]
                ok, med, w3 = bars([rel(r['q'][6][:3], r['lit6']) for r in th])
                judge('K', ok or not th, pop=len(th) >= 4, msg='lit factor with alpha as thickness over %d particles (a >= 0.05, median a '
                      '%.3f): median %.4f, within 3%% %.4f' % (len(th), np.median([r['a'] for r in th]) if th else 0, med, w3))

        # ---- the air stages (ana, full)
        for t, (blocks, _, lights, (inj, integ), atxt) in dumps.items():
            ps = particles(blocks)
            if red in ('', 'nofog'):
                okT, okL, fogged, n = 0, 0, 0, 0
                Lmax = max([float(np.max(vol_at(integ, atxt, q[4][3], *proj_uv(b, q[7][:3]))[:3])) for b, q in ps] or [0])
                for b, q in ps:
                    want = vol_at(integ, atxt, q[4][3], *proj_uv(b, q[7][:3]))
                    got = q[5]
                    n += 1
                    okT += abs(got[3] - want[3]) <= 2e-3
                    okL += bool(np.all(np.abs(got[:3] - want[:3]) <= 0.02 * np.abs(want[:3]) + 1e-4 * Lmax))
                    fogged += want[3] <= 0.98
                judge('F', n > 0 and okT >= 0.99 * n and okL >= 0.99 * n and fogged >= 0.25 * n,
                      '%s: the air the draw applies vs the integrated volume, %d particles: T ok %d, L ok %d; T <= 0.98: %d'
                      % (t, n, okT, okL, fogged))
            if red in ('', 'nolights', 'noshadow') and not (red == 'noshadow' and t != 'full') and not (red == 'nolights' and t != 'ana'):
                near, logR = atxt['slices']
                N = inj.shape[0]
                rs, cd, lg = [], [], []
                Js = []
                for b, q in ps:
                    dd = q[4][3]
                    f = np.log(max(dd, 1e-6) / near) / logR * N
                    # the froxel centres sit at slice k + 0.5: J trilinear in depth between the two nearest
                    z = float(np.clip(f - 0.5, 0, N - 1))
                    z0 = int(z)
                    z1, fz = min(z0 + 1, N - 1), z - int(z)
                    s0, s1 = trilinear2(inj, *proj_uv(b, q[7][:3]), z0), trilinear2(inj, *proj_uv(b, q[7][:3]), z1)
                    if 1.0 - s0[3] < 1e-6 or 1.0 - s1[3] < 1e-6:
                        continue
                    J = s0[:3] / (1.0 - s0[3]) * (1.0 - fz) + s1[:3] / (1.0 - s1[3]) * fz
                    Js.append((J, q[0][:3], dd))
                top = max([lum(J) for J, _, _ in Js] or [0])
                far = 0
                for J, th, dd in Js:
                    if top <= 0 or lum(J) < 1e-3 * top:
                        continue
                    if dd >= G_RESOLVED:  # the froxel (a slice ~12% of its distance deep) no longer resolves a lamp
                        far += 1
                        continue
                    rs.append(lum(th) / lum(J))
                    cd.append(float(np.linalg.norm(chroma(th) - chroma(J))))
                    if lum(th) > 0:
                        lg.append((np.log(lum(th)), np.log(lum(J))))
                rs = np.array(rs)
                within = float(((rs >= 0.8) & (rs <= 1.25)).mean()) if rs.size else 0.0
                corr = float(np.corrcoef(np.array(lg).T)[0, 1]) if len(lg) > 2 and np.std(np.array(lg)[:, 1]) > 0 else float('nan')
                cm = float(np.median(cd)) if cd else float('nan')
                ok = rs.size >= 100 and within >= 0.9 and cm <= 0.02 and (corr >= 0.9 or np.isnan(corr) and within >= 0.99)
                judge('G', ok, '%s: particle J vs the air\'s J at its froxel over %d particles nearer than %d (%d farther, not '
                      'judged): median ratio %.4f, in [0.8, 1.25] %.3f, median chromaticity distance %.4f, log-correlation %.3f'
                      % (t, rs.size, G_RESOLVED, far, float(np.median(rs)) if rs.size else float('nan'), within, cm, corr))

        # ---- the card volume stages (every dump with card pixels)
        for t, v in dumps.items():
            if card_rows(v[0]) or t == 'ana':
                stage_cards(t, v[0], red, judge, cam)

        # ---- the pictures
        if Image is not None and red == '':
            pth = {t: os.path.join(d, t + '.png') for t in ('full', 'nosoftp', 'hide')}
            if all(os.path.exists(p) for p in pth.values()):
                im = {t: png(x) for t, x in pth.items()}
                if len(set(a.shape for a in im.values())) > 1:
                    judge('E', False, 'picture sizes differ %s' % {t: a.shape for t, a in im.items()})
                else:
                    dS = int((np.abs(im['full'] - im['nosoftp']).max(-1) > 2).sum())
                    dH = int((np.abs(im['full'] - im['hide']).max(-1) > 2).sum())
                    judge('E', True, pop=dS >= 500, msg='pictures: the soft fade changes %d px (full vs nosoftp); the cards %d px '
                          '(full vs hide)' % (dS, dH))
        if Image is not None and red == '':
            p = {t: os.path.join(d, t + '.png') for t in ('off', 'before', 'rung2')}
            if all(os.path.exists(x) for x in p.values()):
                a, b, c = png(p['off']), png(p['before']), png(p['rung2'])
                if len({a.shape, b.shape, c.shape}) > 1:
                    judge('O', False, 'picture sizes differ: off %s before %s rung2 %s (a floored window: shoot again)'
                          % (a.shape, b.shape, c.shape))
                    continue
                same = open(p['off'], 'rb').read() == open(p['before'], 'rb').read()
                diff = np.abs(a - b).max(-1)
                flake = np.abs(b - c).max(-1)
                if same:
                    ok, msg = True, 'off == before byte for byte'
                else:
                    near = np.zeros_like(flake, dtype=bool)
                    for y, x in zip(*np.nonzero(flake)):
                        near[max(0, y - 32):y + 33, max(0, x - 32):x + 33] = True
                    m = diff > 0
                    ok = bool(diff.max() <= 1 and (m <= near).all())
                    msg = 'off vs before: %d px differ (max %d); before vs rung2: %d px flake' % (int(m.sum()), int(diff.max()), int((flake > 0).sum()))
                judge('O', ok, msg)

    want_fail = {'nolights': 'ALG', 'white': 'T', 'flat': 'N', 'nothick': 'KP', 'nofog': 'F', 'nosoft': 'E', 'noshadow': 'G',
                 'noedge': 'V', 'nophase': 'HA', 'nonear': 'M'}
    passed = {k: all(v) and any(pops[k]) for k, v in verdict.items()}
    allok = all(passed.values()) and verdict
    if red:
        failed = [s for s in want_fail[red] if s in passed and not passed[s]]
        print('RED %s: stages failed %s (wanted any of %s) -> %s' % (red, failed or 'none', want_fail[red],
                                                                      'RED OK' if failed else 'RED DID NOT FAIL'))
        return 0 if failed else 1
    print('VOLPART1 %s' % ('PASS' if allok else 'FAIL'), passed)
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
