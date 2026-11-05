# lane FOGLEAK1 gate G1: the fog leak fix's law, offline (no window, no GPU). Usage:
#   python -I tests/features/air/fogleak/fogleak_model_check.py [--red dalcenv|nosungate]
# Exit 0 = every row inside its bar; 1 = a row outside (a red must exit 1).
# The frame: the c3 Foggy noon church frame of FOGLEAK-R (its notes/fogleak-r/tools/fogleak_model.py, the inputs and
# their sources there): sun colour 1.51084,1.31303,1.04735 linear, Ts 0.42, ground layer V 164 m, the fallback god-ray
# record's phase, the DALC up/down bytes, the SKY2 tracer's street sky share per axis (bounce_reference, Concord street,
# 24 probes x 6 axes; open ground = up 1, sides 0.5, down 0).
# The reference: the air's env share occluded as the street occludes it (the tracer's per-axis share). The shader's
# fixed law (cell_volair.frag airLeakEnv): fade x the two-side scatter of the volume's irradiance (E+, E- along the
# ray; for an L1 field this IS volCubeScatter of the six axes) + (1 - fade) x the flat DALC, the volume's E = the
# DALC x the street share per axis (what an ideal volume holds). Pre-registered bars (FOGLEAK-R RESEARCH sec 5):
#   shadowed air J fixed / ref in [0.8, 1.2]; every facade ray's L fixed / ref <= 1.2; the roofed air past the
#   cascades (sky visibility toward the sun 0.05) J fixed / ref <= 1.2.
import math
import sys

import numpy as np

UNIT_M = 1.0 / 70.0
SIG = 3.912 / (164.0 / UNIT_M)
TS = 0.42
SUN_LUM = 0.2126 * 1.51084 + 0.7152 * 1.31303 + 0.0722 * 1.04735
# the tracer's per-axis street share (+x -x +y -y +z -z), FOGLEAK-R model output; open ground per axis
AX = np.array([0.173, 0.160, 0.183, 0.158, 0.402, 0.002])
OPEN = np.array([0.5, 0.5, 0.5, 0.5, 1.0, 0.0])
K = AX.mean() / 0.5
G_FWD = 0.75 * 2.0 / (0.2126 * 0.60 + 0.7152 * 1.52 + 0.0722 * 3.31 + 3.0)	# volGi.w: the record's mean cosine (fwd lobe share x g)


def lin(b):
    return np.asarray(b) ** 2.2


def lum(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


up = lum(lin([0.6635, 0.7881, 0.9255]))
dn = lum(lin([0.2765, 0.3687, 0.4396]))
side = lum(lin([(0.6635 + 0.2765) / 2, (0.7881 + 0.3687) / 2, (0.9255 + 0.4396) / 2]))
E6 = np.array([side] * 4 + [up, dn])


def hg(c, g):
    return (1 - g * g) / (4 * math.pi * (1 + g * g - 2 * g * c) ** 1.5)


def phase_pi(c):
    air, fwd, back = 0.2126 * 0.60 + 0.7152 * 1.52 + 0.0722 * 3.31, 2.0, 1.0
    s = air + fwd + back
    return math.pi * (air / s * 3 / (16 * math.pi) * (1 + c * c) + fwd / s * hg(c, 0.75) + back / s * hg(c, 0.0))


def cube_scatter(E, d):
    fl = (2.0 / 3.0) * E.sum()
    l1 = (E[0] - E[1]) * d[0] + (E[2] - E[3]) * d[1] + (E[4] - E[5]) * d[2]
    return 0.25 * max(fl + 3.0 * G_FWD * l1, 0.0)


def two_side(E, d):
    """the shader's read: irradiance of the L1 field fitted to the six axes at +d and -d, then the two-side form"""
    c0 = E.mean()
    c1 = np.array([(E[0] - E[1]) / 2, (E[2] - E[3]) / 2, (E[4] - E[5]) / 2])
    ep, em = c0 + c1 @ d, c0 - c1 @ d
    return 0.25 * max(2.0 * (ep + em) + 3.0 * G_FWD * (ep - em), 0.0)


def env_fixed(d, red, fade=1.0):
    occl = E6 * np.where(OPEN > 0, AX / np.maximum(OPEN, 1e-9), K)
    dalc = cube_scatter(E6, d)
    if 'dalcenv' in red:
        return dalc
    return fade * two_side(occl, d) + (1.0 - fade) * dalc


def env_ref(d):
    return cube_scatter(E6 * np.where(OPEN > 0, AX / np.maximum(OPEN, 1e-9), K), d)


def sun_gate(vs, fade, red):
    if 'nosungate' in red:
        return 1.0
    return 1.0 + (min(max((vs - 0.1) / 0.3, 0.0), 1.0) - 1.0) * fade


def ray_L(D, frac_shadow, j_lit, j_sh):
    n = 2000
    t = (np.arange(n) + 0.5) / n * D
    T = np.exp(-SIG * t)
    J = np.where(t >= D * (1 - frac_shadow), j_sh, j_lit)
    return float((T * SIG * J * (D / n)).sum())


def main():
    red = sys.argv[sys.argv.index('--red') + 1].split(',') if '--red' in sys.argv else []
    fails = 0
    j_sun = SUN_LUM * TS * phase_pi(0.858)
    # 1: the shadowed air's env J along level rays (four headings) and up / down
    for name, d in (('+x', [1, 0, 0]), ('-y', [0, -1, 0]), ('diag', [0.7071, 0.7071, 0]), ('up45', [0, 0.7071, 0.7071]), ('down45', [0.7071, 0, -0.7071])):
        d = np.array(d, float)
        r = env_fixed(d, red) / env_ref(d)
        ok = 0.8 <= r <= 1.2
        fails += not ok
        print('env J %-6s fixed/ref %.3f  bar [0.8, 1.2]  %s' % (name, r, 'ok' if ok else 'FAIL'))
    # 2: the facade rays of FOGLEAK-R (camera 110 u above the street, sun el 34.9 deg)
    d = np.array([0.0, -1.0, 0.0])
    worst = 0.0
    for H in (800, 1200, 2000):
        for D in (1000, 2000, 3000, 5000):
            f = min(1.0, 1.43 * (H - 110) / D)
            lf = ray_L(D, f, j_sun + env_fixed(d, red), env_fixed(d, red))
            lr = ray_L(D, f, j_sun + env_ref(d), env_ref(d))
            worst = max(worst, lf / lr)
    ok = worst <= 1.2
    fails += not ok
    print('facade rays L fixed/ref worst %.3f  bar <= 1.2  %s' % (worst, 'ok' if ok else 'FAIL'))
    # 3: the roofed air past the cascades: today vis 1; the volume's sky visibility toward the sun 0.05, fade 1
    vis = sun_gate(0.05, 1.0, red)	# the reference: no sun under the roof
    ok = vis <= 1e-6
    fails += not ok
    print('roofed air past the cascades: sun vis %.3f (ref 0)  sun J %.3f (today %.3f)  %s' % (vis, j_sun * vis, j_sun, 'ok' if ok else 'FAIL'))
    # 4: open ground keeps the sun (sky visibility 0.6 -> 1) and outside the volume nothing moves (fade 0 = today)
    vo = sun_gate(0.6, 1.0, red)
    ok = abs(vo - 1.0) < 1e-6 and abs(sun_gate(0.05, 0.0, red) - 1.0) < 1e-6 and abs(env_fixed(d, red, 0.0) - cube_scatter(E6, d)) < 1e-9
    fails += not ok
    print('open ground sun vis %.3f, fade 0 = today  %s' % (vo, 'ok' if ok else 'FAIL'))
    print('G1 %s (%d rows outside)%s' % ('PASS' if fails == 0 else 'FAIL', fails, ' red ' + ','.join(red) if red else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
