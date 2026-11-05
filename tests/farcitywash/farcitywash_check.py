"""FARCITYWASH1 (bungo 11-02): the far Boston skyline from the Concord hatch, CommonwealthOvercast noon, washed out next
to vanilla's haze. Offline gate on the curve: the veil (the light the haze adds) over far geometry, ours vs the game's
own fog (its FNAM curve x its fog colour), along the rays of the camera concord_hatch_boston.

The frame is the shot's census (run5c3 row c3_wb5_tend): the medium, the sun, the DALC, the phase record (OvercastDay
GDRY), the fog K rows (weather --fog dump). Our light per ray: the aerial perspective (tests/features/sky/atmo_model.py, the
min rule of atmo_apfog.glsl) and the cell air (wwAirRhoLaw's ground layer over a terrain falling toward Boston, the
inject's J = D vis Ts pi p(c) + the env cube, x airVeilK of cell_volair.frag).

Rows (bars pre-registered before the first run):
  G1 the shader and the CPU carry the law (text): airVeilK's reference, its 4 uses, the uniform, the red.
  G2 far veil / the game's: 0.6..1.6 at the roof, the base and the 0.8 km ground point (3 Boston ground heights).
     The red (k = 1, today) must read > 2.0 at the roof.
  G3 one-sided: k <= 1 everywhere; a dim light (night) k == 1, the veil bit-equal to the red's.
  G4 shadows kept: lit / shadowed air J the same with and without the scale.
usage: python -I farcitywash_check.py [--red] < /dev/null   (--red: judge k = 1 as the fix; must FAIL)
"""
import math, os, re, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'features', 'sky'))  # sky helpers
import atmo_model as A

RED = '--red' in sys.argv
Y = np.array([0.2126, 0.7152, 0.0722])
UPKM = 69930.07
# the frame (census of c3_wb5_tend)
SIG0, S, THICK, ZG = 5.8099e-06, 5.347, 4475.8, 64.0
D = np.array([0.78300, 0.67723, 0.53602])
SUN = np.array([0.0, -0.83352, 0.55249])
DALC_UP = np.array([0.3399, 0.4550, 0.5835])
DALC_DN = np.array([0.1620, 0.2118, 0.2528])
K_AP = 1.083
# CommonwealthOvercast fog K rows 41..46 (fogheight1 dumps), hour 12 and hour 0
K12 = [[4.02576507e-06, 3.33333337e-05, 0.0064412239, -0.49786666], [0.0370307118, 0.0581897497, 0.103635222, 0.280000001],
       [0.0242244489, 0.0356160067, 0.0717622265, 0.949999988], [0.0891963467, 0.106161371, 0.151059911, 0.975000024],
       [0.0915212929, 0.106161371, 0.157282665, 0.0], [3.33333337e-05, 4.99999987e-05, -0.49786666, -0.496800005]]
K0 = [[4.01929265e-06, 3.33333337e-05, 0.00482315104, -0.49786666], [0.00411633914, 0.00411649002, 0.00715515157, 0.280000001],
      [0.00143323885, 0.00226312527, 0.00455982937, 0.949999988], [0.00411638105, 0.00604098802, 0.00902163796, 0.975000024],
      [0.00293250545, 0.00502853189, 0.00837325118, 0.0], [3.33333337e-05, 4.99999987e-05, -0.49786666, -0.496800005]]
# OvercastDay GDRY: back rgb, fwd rgb, air/back/fwd scale, back g, fwd g (vaPhase: luminance weights summing to 1)
GDRY = ((0.349, 0.561, 0.624), (0.173, 0.345, 0.396), 0.01, 1.0, 1.0, 0.423, 0.687)
# the camera: eye 46 m over the Concord hatch, the Mass Fusion roof
EYE_G = 6182.6
EYE = np.array([-60895.7, 73875.3, EYE_G + 46 * 69.93])
TGT = np.array([18943.9, -7167.9, 8600.0])
DH = float(np.linalg.norm((TGT - EYE)[:2]))	# the terrain falls to Boston's ground over 0.8 of it
FAILS = []


def check(name, ok, msg):
    print('%s %s: %s' % ('PASS' if ok else 'FAIL', name, msg))
    if not ok:
        FAILS.append(name)


def fog_eval(K, d, z):
    """lookdev_fog.glsl wwFogEval: alpha, colour"""
    ramp = d * K[0][0] - K[0][2]
    f = min(max(ramp, 0.0), 1.0)
    p0 = min(max(z * K[5][0] - K[5][2], 0.0), 1.0)
    p1 = min(max(z * K[5][1] - K[5][3], 0.0), 1.0)
    hb = p0 + (p1 - p0) * f
    mx = K[2][3]
    clampT = min((f - 0.75) * 4.0 * (1.0 - mx) + mx, 1.0) if ramp > 0.75 else mx
    escape = f * 66.666672 if ramp < 0.015 else 1.0
    I = min(clampT, f ** K[1][3]) if f > 0.0 else 0.0
    weight = hb * K[3][3] + (1.0 - hb)
    lo = np.array(K[1][:3]) * (1 - I) + np.array(K[3][:3]) * I
    hi = np.array(K[2][:3]) * (1 - I) + np.array(K[4][:3]) * I
    return weight * I * escape, lo * (1 - hb) + hi * hb


def hg(c, g):
    return (1 - g * g) / (4 * math.pi * (1 + g * g - 2 * g * c) ** 1.5)


def phase_pi(c):
    back, fwd, a, bs, fs, bg, fg = GDRY
    air = tuple(k * a for k in (0.18, 0.46, 1.0))
    la, lf, lb = (float(Y @ np.array(v)) for v in (air, tuple(x * fs for x in fwd), tuple(x * bs for x in back)))
    t = la + lf + lb
    return math.pi * (la / t * 3 / (16 * math.pi) * (1 + c * c) + lf / t * hg(c, fg) + lb / t * hg(c, bg))


def env_cube():
    """volCubeScatter of the six DALC axes for a level ray: their mean (sides: the affine rows' mean of up and down)"""
    side = (0.5 * (DALC_UP + DALC_DN)) ** 2.2
    return (DALC_UP ** 2.2 + DALC_DN ** 2.2 + 4 * side) / 6.0


def veil_k(K, Jenv, t, z, d_lum):
    """cell_volair.frag airVeilK"""
    _, fc = fog_eval(K, t, z)
    rec = float(Y @ np.maximum(fc, 0.0))
    op = 0.25 * d_lum + float(Y @ Jenv)
    return rec / op if (op > rec and op > 1e-8) else 1.0


def air_ray(K, end, zb, d_col, env, fix, n=400, vis=1.0):
    """(T, L) of the cell air from the eye to `end`, terrain falling linearly to Boston ground zb"""
    seg = end - EYE
    dist = float(np.linalg.norm(seg))
    dirw = seg / dist
    c = float(dirw @ SUN)
    ppi = phase_pi(c)
    T, L = 1.0, np.zeros(3)
    dl = dist / n
    for i in range(n):
        tt = (i + 0.5) / n
        x = tt * dist
        z = EYE[2] + seg[2] * tt
        g = EYE_G + (zb - EYE_G) * min(x / (0.8 * DH), 1.0)
        h = z - (g + ZG)
        e = min(1.0, math.exp(-h / THICK))
        dtau = SIG0 * (1.0 + S * e) * dl
        Ts = math.exp(-SIG0 * S * THICK * e / max(SUN[2], 0.02))	# airTupSun: the ground layer only
        k = veil_k(K, env, x, z - EYE_G, float(Y @ d_col)) if fix else 1.0
        J = k * (d_col * vis * Ts * ppi + env)
        L = L + T * (1.0 - math.exp(-dtau)) * J
        T *= math.exp(-dtau)
    return T, L, c


def ap_ray(end):
    P = A.params(haze=A.haze_from_fog(250000.0, 0.95, 64.0, 10000.0))
    seg = end - EYE
    dkm = float(np.linalg.norm(seg)) / UPKM
    v = seg / np.linalg.norm(seg)
    ms = A.MsTable(P, nu=16, nv=16, k=6, n=12)
    Lap, _, thr = A.integrate(P, A.RB + EYE[2] / UPKM, v[None, :], SUN, 48, psi=ms, dmax=np.array([dkm]), want_T=True)
    return float(thr[0].mean()), math.pi * K_AP * Lap[0]


def combine(Tap, Lap, Tair, Lair):
    """atmo_apfog.glsl: T = min, the air's residual over the AP"""
    tr = min(1.0, Tair / max(Tap, 1e-6))
    s = min(1.0, (1 - tr) / max(1 - Tair, 1e-6)) if tr < 1 else 0.0
    return Tap * tr, Lap * tr + Lair * s


def g1():
    fr = open(os.path.join(TREE, 'res', 'shaders', 'features', 'air', 'cell_volair.frag'), 'rb').read().decode('utf-8')
    cpp = open(os.path.join(TREE, 'src', 'gl', 'cellvolair.cpp'), 'rb').read().decode('utf-8')
    m = re.search(r'float airVeilK\( vec3 Jenv, float t, float z \)\r?\n\{(.*?)\r?\n\}', fr, re.S)
    body = m.group(1) if m else ''
    ok = ('0.25 * dot( cellDirColor, lumK ) + dot( Jenv, lumK )' in body and 'rec / open : 1.0' in body
          and 'wwFogEval( t, z, hb, fc )' in body and 'cellInterior' in body)
    uses = len(re.findall(r'\bvk \* ', fr))
    calls = len(re.findall(r'float vk = airVeilK\( Jenv, tm, zc \+ dzdt \* tm \);', fr))
    cp = 'prog->uni1b( "volVeil", !s.veilPhys );' in cpp and 'r == "veilphys"' in cpp
    check('G1 law in tree', ok and uses == 3 and calls == 2 and cp,
          'body %s, vk uses %d (3), calls %d (2), cpu %s' % (ok, uses, calls, cp))


def main():
    g1()
    env = env_cube()
    fix = not RED
    print('env lum %.4f, D lum %.4f, k at the city (fix) see rows' % (float(Y @ env), float(Y @ D)))
    worst_red = 0.0
    for zb in (0.0, 1500.0, 3000.0):
        rows = (('roof', TGT), ('base', np.array([TGT[0], TGT[1], zb + 300.0]))
                , ('0.8km', EYE + 0.5 * (np.array([TGT[0], TGT[1], 0.0]) - EYE) * np.array([1, 1, 0])))
        for name, end in rows:
            end = end.copy()
            if name == '0.8km':
                end[2] = EYE_G + (zb - EYE_G) * min(0.5 / 0.8, 1.0) + 300.0
            Tap, Lap = ap_ray(end)
            Ta, La, c = air_ray(K12, end, zb, D, env, fix)
            Ta0, La0, _ = air_ray(K12, end, zb, D, env, False)
            T, L = combine(Tap, Lap, Ta, La)
            T0, L0 = combine(Tap, Lap, Ta0, La0)
            d = float(np.linalg.norm(end - EYE))
            a, fc = fog_eval(K12, d, end[2] - EYE_G)
            Lv = float(Y @ (a * fc))
            r, r0 = float(Y @ L) / Lv, float(Y @ L0) / Lv
            if name == 'roof':
                worst_red = max(worst_red, r0)
            check('G2 %s zb %.0f' % (name, zb), 0.6 <= r <= 1.6,
                  'd %.2f km cos %.2f: T ours %.3f game %.3f, veil ours %.4f (today %.4f) game %.4f -> %.2f (today %.2f)'
                  % (d / UPKM, c, T, 1 - a, float(Y @ L), float(Y @ L0), Lv, r, r0))
    check('G2 red reads the wash', worst_red > 2.0, 'today / game at the roof %.2f (> 2.0)' % worst_red)
    # G3: one-sided, and the night (a dim light): k == 1, bit-equal
    ks = [veil_k(K12, env * s, t, z, float(Y @ D) * s) for s in (0.05, 0.2, 1.0, 3.0)
          for t in (2000.0, 20000.0, 113800.0) for z in (0.0, 2400.0)]
    dimD, dimE = D * 0.004, env * 0.004
    Tn, Ln, _ = air_ray(K0, TGT, 1500.0, dimD, dimE, fix)
    Tn0, Ln0, _ = air_ray(K0, TGT, 1500.0, dimD, dimE, False)
    kn = veil_k(K0, dimE, 113800.0, 2400.0, float(Y @ dimD))
    check('G3 one-sided', max(ks) <= 1.0 and kn == 1.0 and np.array_equal(Ln, Ln0),
          'max k %.3f, min k %.3f, night k %.3f, night veil equal %s' % (max(ks), min(ks), kn, np.array_equal(Ln, Ln0)))
    # G4: lit / shadowed J ratio unchanged (k takes no visibility)
    k = veil_k(K12, env, 50000.0, 1000.0, float(Y @ D)) if fix else 1.0
    c = 0.59
    lit, sh = k * (D * phase_pi(c) + env), k * env
    lit0, sh0 = D * phase_pi(c) + env, env
    q, q0 = float(Y @ lit) / float(Y @ sh), float(Y @ lit0) / float(Y @ sh0)
    check('G4 shadows kept', abs(q / q0 - 1) < 1e-9, 'lit/shadow %.4f vs today %.4f (k %.3f)' % (q, q0, k))
    print('RESULT %s (%d fail)%s' % ('PASS' if not FAILS else 'FAIL', len(FAILS), ' [red]' if RED else ''))
    return 0 if not FAILS else 1


sys.exit(main())
