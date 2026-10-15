#!/usr/bin/env python3
"""Volumetric Air: the identities the air must keep, as a Python twin (lane VOLAIR1, 2026-10-13; src/gl/cellvolair.h).

  python3 tests/spells/volair1_check.py              green run, then every red (each must FAIL its check)
  python3 tests/spells/volair1_check.py --red NAME   one red only (exit 1 when it fails, as it must)

No game data: every medium and every fog record here is made up by a seeded random generator. The ESTIMATE is a
float64 port of what the shader and the CPU do (res/shaders/cell_volair.frag airOd / the froxel scheme,
res/shaders/lookdev_fog.glsl wwVolAirAt, src/gl/cellvolair.cpp vaFit); the REFERENCE shares nothing with it but
the medium's definition (sigma_t(z) = sigma0 x min(1, exp(-(z - zb) / H)), empty above the top) and the game's
fog formula (the FOG1-gated transcription, re-typed here):

  A1 optical depth  2,000 random segments through random media: the closed form vs a 200,001-point trapezoid of
                    the density along the segment                                       rel <= 1e-3 (+ 1e-9 abs)
  A2 chain          the transmittance over a ray = the product over its pieces (the integrate stage's identity),
                    1,000 random rays cut at random points                              |dT| <= 1e-9
  A3 furnace        the air lit by a uniform surround S, a surface of radiance S behind it: the froxel scheme
                    (64 exponential slices, 4 sub-steps, (1 - exp(-tau)) weights, front to back) and the apply
                    (colour x T + L, linear in log-depth) give S back at every distance  |out - S| / S <= 1e-6
  A4 phase          pi x the phase (Rayleigh + two Henyey-Greenstein lobes, colours over their summed luminance)
                    integrates to a luminance of pi over the sphere, 20 random records    |lum / pi - 1| <= 1e-4
                    and the six-axis L0 + L1 read of a uniform cube E returns E x the tint  rel <= 1e-9
  A5 fit            random fog records: the fitted air hides max at the slices' far edge (1 - T = min(max, 0.99),
                    T by quadrature), and its density above the height band over below it equals the game's
                    alpha there over below it (the game formula at both heights)        rel <= 1e-3 each
  A6 street haze    two made-up outdoor-shaped records: the air's 1 - T below the band within 0.10 of the game's
                    alpha at 2000, 3000 and 5000 units                                   |diff| <= 0.10

Reds (each must make its check FAIL; the default run proves it):
  flatod    the density at the segment's start taken for the whole segment             -> A1 FAIL
  nochain   the integrate stage sums the slices' transmittances instead of multiplying  -> A2 FAIL
  additive  the air ADDED on top of the surface, no extinction (lane VOLFOG1's law)     -> A3 FAIL
  no4pi     the Henyey-Greenstein lobes without their 1 / (4 pi)                        -> A4 FAIL
  wrongmed  the fit with twice the density and no height falloff                       -> A5 FAIL
  farfit    the density fitted at the full distance alone (the old fit)                 -> A6 FAIL
"""
import math
import os
import sys

import numpy as np

LUM = np.array([0.2126, 0.7152, 0.0722])
REDS = {'flatod': 'A1', 'nochain': 'A2', 'additive': 'A3', 'no4pi': 'A4', 'wrongmed': 'A5', 'farfit': 'A6'}


# ---------------------------------------------------------------- the estimate: ports of the shader / the CPU
def est_F(m, z):
    sigma0, zb, invH, top = m
    z = min(z, top)
    x = z - zb
    if x <= 0.0 or invH <= 0.0:
        return x
    u = x * invH
    e = u * (1.0 - u * (0.5 - u / 6.0)) if u < 1e-3 else 1.0 - math.exp(-u)
    return e / invH


def est_rho(m, z):
    sigma0, zb, invH, top = m
    if z > top:
        return 0.0
    return min(1.0, math.exp(-(z - zb) * invH)) if invH > 0.0 else 1.0


def est_od(m, za, zb_, length, red=''):
    if red == 'flatod':
        return m[0] * length * est_rho(m, za)
    dz = zb_ - za
    if abs(dz) < 0.5:
        return m[0] * length * est_rho(m, 0.5 * (za + zb_))
    return m[0] * length * (est_F(m, zb_) - est_F(m, za)) / dz


def one_minus_e(tau):
    return tau * (1.0 - tau * (0.5 - tau / 6.0)) if tau < 1e-3 else 1.0 - math.exp(-tau)


def est_fit(K, red=''):
    """src/gl/cellvolair.cpp vaFit: sigma0, zb, 1/H (0 uniform), dFull, farD -- the street-range least squares in
    optical depth (64 log-spaced distances 500..5000 below the band), never thinner than the far-end fit"""
    dFull = (1.0 + K[0][2]) / K[0][0]
    mx = min(max(K[2][3], 0.0), 0.99)
    tauMax = -math.log(1.0 - mx) if mx > 0 else 0.0
    sFar = tauMax / dFull if dFull > 0 else 0.0
    zb, invH, band = 0.0, 0.0, False
    k5y, k5w, top = K[5][1], K[5][3], K[3][3]
    if k5y > 1e-9 and 0.0 < top < 1.0:
        z0, z1 = k5w / k5y, (1.0 + k5w) / k5y
        zb = z0
        invH = -math.log(max(top, 1e-3)) / max(z1 - z0, 1e-3)
        band = True
    zlow = zb - 10.0 if band else 0.0
    num = den = 0.0
    knee = (0.75 + K[0][2]) / K[0][0]    # past it the game's own end ramp to 1, not the air
    for i in range(64):
        d = 500.0 * 10.0 ** (i / 63.0)
        if d > knee and i > 0:
            break
        a = min(max(game_alpha(K, d, zlow), 0.0), max(mx, 1e-4))
        num += d * -math.log(1.0 - a)
        den += d * d
    sStreet = num / den if den > 0 else sFar
    # the record's curve ends inside the street range (an interior): the street fit alone; else never thinner than the far end
    sigma0 = sFar if red == 'farfit' else (sStreet if knee <= 5000.0 else max(sFar, sStreet))
    farD = min(dFull, tauMax / sigma0) if sigma0 > sFar and sigma0 > 0 else dFull
    if red == 'wrongmed':
        sigma0 *= 2.0
        invH = 0.0
    return sigma0, zb, invH, dFull, farD


def froxel_column(m, z0, dzdt, J, near, far, N=64, red=''):
    """stage 1 + stage 2 for one column with a constant light J (the furnace): returns L[k], T[k] at slice k's far edge"""
    logR = math.log(far / near)
    L = np.zeros(N)
    T = np.zeros(N)
    Lc, Tc = 0.0, 1.0
    for k in range(N):
        t0 = near * math.exp(logR * k / N)
        t1 = near * math.exp(logR * (k + 1) / N)
        acc, Ts = 0.0, 1.0
        for q in range(4):
            ta, tb = t0 + (t1 - t0) * q / 4.0, t0 + (t1 - t0) * (q + 1) / 4.0
            tau = est_od(m, z0 + dzdt * ta, z0 + dzdt * tb, tb - ta)
            acc += Ts * J * one_minus_e(tau)
            Ts *= math.exp(-tau)
        Lc += Tc * acc
        Tc = Tc + Ts if red == 'nochain' else Tc * Ts
        L[k], T[k] = Lc, Tc
    return L, T, logR


def apply_at(L, T, near, logR, d, N=64):
    """lookdev_fog.glsl wwVolAirAt along one column (the texture's slice axis is linear between layer centres)"""
    if d <= near:
        return 0.0, 1.0
    f = math.log(d / near) / logR * N
    if f < 1.0:
        return L[0] * f, 1.0 - (1.0 - T[0]) * f
    x = min(f, N) - 1.0  # layer coordinate: (f - 0.5) / N in GL = texel f - 1
    i0 = int(math.floor(x))
    i1 = min(i0 + 1, N - 1)
    w = x - i0
    return L[i0] * (1 - w) + L[i1] * w, T[i0] * (1 - w) + T[i1] * w


def hg(c, g, red=''):
    d = 1.0 + g * g - 2.0 * g * c
    v = (1.0 - g * g) / (d * np.sqrt(d))
    return v if red == 'no4pi' else v / (4.0 * math.pi)


def phase_pi(rec, c, red=''):
    air, fwd, back = rec[0:3], rec[3:6], rec[6:9]
    lum = float(LUM @ (air + fwd + back))
    c = np.asarray(c)[..., None]
    return math.pi * ((air / lum) * (3.0 / (16.0 * math.pi)) * (1.0 + c * c)
                      + (fwd / lum) * hg(c, rec[9], red) + (back / lum) * hg(c, rec[10], red))


def cube_scatter(E, d, tint, g):
    fl = (2.0 / 3.0) * E.sum(0)
    l1 = (E[0] - E[1]) * d[0] + (E[2] - E[3]) * d[1] + (E[4] - E[5]) * d[2]
    return 0.25 * np.maximum(fl + 3.0 * g * l1, 0.0) * tint


# ---------------------------------------------------------------- the reference
def ref_rho(m, z):
    sigma0, zb, invH, top = m
    r = np.where(invH > 0, np.minimum(1.0, np.exp(-(z - zb) * invH)), 1.0)
    return np.where(z > top, 0.0, r)


def ref_od(m, za, zb_, length, n=200001):
    t = np.linspace(0.0, 1.0, n)
    s = m[0] * ref_rho(m, za + (zb_ - za) * t)
    return float(np.trapz(s, dx=1.0 / (n - 1))) * length


def game_alpha(K, d, z):
    """the game's fog alpha (lookdev_fog.glsl wwFogEval, re-typed)"""
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


class Gate:
    def __init__(self):
        self.res = {}

    def put(self, key, ok, text):
        self.res.setdefault(key, []).append(ok)
        print('  %s  %s  %s' % ('PASS' if ok else 'FAIL', key, text), flush=True)


def rand_medium(rng):
    sigma0 = 10 ** rng.uniform(-5.5, -2.5)
    zb = rng.uniform(-300.0, 600.0)
    invH = 0.0 if rng.random() < 0.3 else 1.0 / rng.uniform(40.0, 4000.0)
    top = zb + rng.uniform(-200.0, 6000.0)
    return sigma0, zb, invH, top


def run(red):
    rng = np.random.default_rng(20261013)
    G = Gate()
    # A1 -----------------------------------------------------------------------------------------------------------
    worst = 0.0
    for _ in range(2000):
        m = rand_medium(rng)
        length = 10 ** rng.uniform(0.0, 4.5)
        za = rng.uniform(-600.0, 5000.0)
        dz = length * rng.uniform(-1.0, 1.0) * (0.0 if rng.random() < 0.1 else 1.0)
        e = est_od(m, za, za + dz, length, red)
        r = ref_od(m, za, za + dz, length)
        worst = max(worst, abs(e - r) / (abs(r) + 1e-9))
    G.put('A1', worst <= 1e-3, 'optical depth, closed form vs quadrature, 2,000 segments: worst rel %.3g (bar 1e-3)' % worst)
    # A2 -----------------------------------------------------------------------------------------------------------
    worst = 0.0
    for _ in range(1000):
        m = rand_medium(rng)
        z0, dzdt, length = rng.uniform(-200, 3000), rng.uniform(-1, 1), 10 ** rng.uniform(1, 4.5)
        cuts = np.sort(np.concatenate([[0.0, length], rng.uniform(0, length, rng.integers(1, 12))]))
        whole = math.exp(-est_od(m, z0, z0 + dzdt * length, length))
        Ts = [math.exp(-est_od(m, z0 + dzdt * a, z0 + dzdt * b, b - a)) for a, b in zip(cuts[:-1], cuts[1:])]
        prod = sum(Ts) if red == 'nochain' else float(np.prod(Ts))
        worst = max(worst, abs(prod - whole))
    G.put('A2', worst <= 1e-9, 'transmittance over a ray = the product over its pieces, 1,000 rays: worst |dT| %.3g' % worst)
    # A3 -----------------------------------------------------------------------------------------------------------
    worst = 0.0
    S = 0.37
    for _ in range(200):
        m = rand_medium(rng)
        m = (m[0] * 10.0, m[1], m[2], m[3])  # thicker: the air must matter
        far = 10 ** rng.uniform(3.0, 5.0)
        z0, dzdt = rng.uniform(-200, 2000), rng.uniform(-0.6, 0.6)
        L, T, logR = froxel_column(m, z0, dzdt, S, 16.0, far, red=red)
        for d in np.concatenate([[1.0, 16.0, 17.0, far, 2 * far], 10 ** rng.uniform(1.0, math.log10(2 * far), 20)]):
            l, t = apply_at(L, T, 16.0, logR, d)
            out = S + l if red == 'additive' else S * t + l
            worst = max(worst, abs(out - S) / S)
    G.put('A3', worst <= 1e-6, 'furnace: colour x T + L = the surround at every distance, 200 media x 25 depths: worst rel %.3g' % worst)
    # A4 -----------------------------------------------------------------------------------------------------------
    worst = 0.0
    c = np.linspace(-1.0, 1.0, 400001)
    for _ in range(20):
        rec = np.concatenate([rng.uniform(0.05, 3.0, 9), [rng.uniform(0.0, 0.9), rng.uniform(-0.6, 0.0)]])
        p = phase_pi(rec, c, red) @ LUM
        integ = 2.0 * math.pi * float(np.trapz(p, c))   # the luminance of pi x the phase over the sphere
        worst = max(worst, abs(integ / math.pi - 1.0))
    E = np.tile(rng.uniform(0.1, 2.0, 3), (6, 1))
    tint = rng.uniform(0.5, 1.5, 3)
    d = rng.normal(size=3)
    d /= np.linalg.norm(d)
    cs = cube_scatter(E, d, tint, 0.4)
    cube = float(np.max(np.abs(cs - E[0] * tint) / (E[0] * tint)))
    G.put('A4', worst <= 1e-4 and cube <= 1e-9, 'phase over the sphere: worst |lum / pi - 1| %.3g (bar 1e-4); uniform cube read: rel %.3g' % (worst, cube))
    # A5 -----------------------------------------------------------------------------------------------------------
    worst_a, worst_h, worst_t, n_thin = 0.0, 0.0, 0.0, 0
    for _ in range(40):
        start, full = rng.uniform(0, 3000), 0
        full = start + rng.uniform(2000, 60000)
        k0x = 1.0 / (full - start)
        k0z = start * k0x
        mx = rng.uniform(0.2, 0.995)
        z0, z1 = rng.uniform(-200, 800), 0
        z1 = z0 + rng.uniform(100, 4000)
        k5y = 1.0 / (z1 - z0)
        k5w = z0 * k5y
        top = rng.uniform(0.02, 0.9)
        K = [[k0x, 0, k0z, 0], [0.5, 0.5, 0.5, rng.uniform(0.5, 2.0)], [0.5, 0.5, 0.5, mx], [0.5, 0.5, 0.5, top],
             [0.5, 0.5, 0.5, 0], [k5y * 0.5, k5y, k5w * 0.5, k5w]]
        sigma0, zb, invH, dFull, farD = est_fit(K, red)
        m = (sigma0, zb, invH, 1e9)
        zlow = z0 - 50.0
        Tfull = math.exp(-ref_od(m, zlow, zlow, farD))
        if farD < dFull * (1.0 - 1e-6):
            worst_a = max(worst_a, abs((1.0 - Tfull) - min(mx, 0.99)) / min(mx, 0.99))
        else:
            # the street fit thinner than the far end: the air at the full distance within the street bar of max
            n_thin += 1
            worst_t = max(worst_t, abs((1.0 - Tfull) - min(mx, 0.99)))
        # the game at its full distance (ramp 1: the height blend is the far pair's alone): its alpha at the band's
        # top over its alpha below the band
        ga = game_alpha(K, dFull, z1) / game_alpha(K, dFull, z0 - 10.0)
        # the air: its density at the same two heights, from the optical depth of 1-unit vertical pieces (quadrature)
        ra = ref_od(m, z1 - 0.5, z1 + 0.5, 1.0, 2001) / ref_od(m, z0 - 10.5, z0 - 9.5, 1.0, 2001)
        worst_h = max(worst_h, abs(ra - ga) / ga)
    G.put('A5', worst_a <= 1e-3 and worst_t <= 0.10 and worst_h <= 1e-3,
          'fit: 1 - T(far edge) vs the game\'s max, worst rel %.3g; %d records thinner than the far end, 1 - T(full) vs max worst |diff| %.3f (bar 0.10); density above / below the band vs the game\'s alpha ratio, worst rel %.3g (40 records)'
          % (worst_a, n_thin, worst_t, worst_h))
    # A6 -----------------------------------------------------------------------------------------------------------
    # two made-up records shaped like an outdoor fog (a near start, a far full distance, a low power that front-loads
    # the opacity): the air's haze 1 - T over a horizontal ray below the band (quadrature) within 0.10 of the game's
    # alpha at 2000, 3000 and 5000 units. The far-end fit leaves it ~1/10 of the game's there.
    worst6, txt = 0.0, []
    # A third, shaped like an interior's (Vault111Cryo: start 500, full 4600, power 1.2, max 0.7, no band): judged only
    # up to the curve's end ramp (ramp 0.75), past which the game ramps to 1 on its own
    for start, full, pw, mx, top in ((550.0, 175000.0, 0.13, 1.0, 0.97), (1200.0, 250000.0, 0.31, 0.85, 0.97),
                                     (500.0, 4600.0, 1.2, 0.7, 1.0)):
        k0x = 1.0 / (full - start)
        K = [[k0x, 0, start * k0x, 0], [0.5, 0.5, 0.5, pw], [0.5, 0.5, 0.5, mx], [0.5, 0.5, 0.5, top],
             [0.5, 0.5, 0.5, 0], [4.0e-5, 4.6e-5, -0.497, -0.497]]
        sigma0, zb, invH, dFull, farD = est_fit(K, red)
        m = (sigma0, zb, invH, 1e9)
        zlow = zb - 50.0
        for d in (2000.0, 3000.0, 5000.0):
            if d > (0.75 + K[0][2]) / K[0][0]:
                continue
            air = 1.0 - math.exp(-ref_od(m, zlow, zlow, d))
            game = game_alpha(K, d, zlow)
            worst6 = max(worst6, abs(air - game))
            txt.append('%.0f/%.0f: %.3f vs %.3f' % (start, d, air, game))
    G.put('A6', worst6 <= 0.10, 'street haze, air vs game alpha (start/d): %s; worst |diff| %.3f (bar 0.10)' % ('; '.join(txt), worst6))
    return G


def main():
    args = sys.argv[1:]
    red = os.environ.get('VOLAIR1_RED', '')
    if len(args) >= 2 and args[0] == '--red':
        red = args[1]
    if red:
        if red not in REDS:
            print('unknown red %s (%s)' % (red, ', '.join(REDS)))
            return 2
        print('VOLAIR1 twin, red %s (must fail %s)' % (red, REDS[red]))
        G = run(red)
        hit = not all(G.res.get(REDS[red], [True]))
        print('RED %s: %s' % (red, 'FAILED as it must (%s)' % REDS[red] if hit else 'DID NOT FAIL -- the check is blind'))
        return 1 if hit else 0
    print('VOLAIR1 twin, green')
    G = run('')
    failed = sorted(k for k, v in G.res.items() if not all(v))
    print('VERDICT %s' % ('PASS' if not failed else 'FAIL (%s)' % ', '.join(failed)))
    blind = []
    for r in REDS:
        print('-- red %s (must fail %s)' % (r, REDS[r]))
        Gr = run(r)
        if all(Gr.res.get(REDS[r], [True])):
            blind.append(r)
    print('REDS %s' % ('all failed as they must' if not blind else 'BLIND: %s' % ', '.join(blind)))
    return 0 if not failed and not blind else 1


if __name__ == '__main__':
    sys.exit(main())
