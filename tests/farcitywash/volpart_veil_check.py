"""FARCITYWASH1-PART (lane FOGENV1, 11-02): the volume particles take the air's veil.

FARCITYWASH1 caps the daytime air's sky-lit share (the sun x the phase + the env cube) at the records' fog colour
(cell_volair.frag airVeilK, red WW_CELL_AIR_RED=veilphys). The particles (cell_volpart.glsl vpLight) kept the physical
level, so a daytime puff read ~1/k (~3x at Overcast noon) over the air around it. Now vpVeilK, the same law and guards,
scales the puff's sky-lit share (thin and thick); the lamps keep their own level.

Rows (bars pre-registered before the first run), the frame of farcitywash_check.py (c3 Overcast noon):
  V1 the law in the tree (text): vpVeilK's body equals airVeilK's, one call on (jEnv, t, z), 2 uses on the sky shares,
     jEnv takes the GI and ambient cube scatter (2 adds), the lamps unscaled, the uniform set from the air's veilPhys.
  V2 a puff at a froxel (sunlit and shadowed, t 2..114k, z 0..2400) over the air's sky-lit J there: within 1e-9 of 1.
     The red (the old puff, k = 1 beside the capped air) must read > 2.0 somewhere.
  V3 the night (a dim light): k == 1, the puff bit-equal to the old one.
  V4 red veilphys (both physical): puff == air, k == 1.
usage: python -I volpart_veil_check.py [--red] < /dev/null   (--red: the old puff; must FAIL)
"""
import math
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.normpath(os.path.join(HERE, '..', '..'))
RED = '--red' in sys.argv

# the frame and laws of farcitywash_check.py (its source without the run line)
src = open(os.path.join(HERE, 'farcitywash_check.py'), 'rb').read().decode('utf-8')
src = src.replace('sys.exit(main())', '')
FW = {'__file__': os.path.join(HERE, 'farcitywash_check.py'), '__name__': 'farcitywash_twin'}
sys_argv = sys.argv
sys.argv = [sys.argv[0]]
exec(compile(src, 'farcitywash_check.py', 'exec'), FW)
sys.argv = sys_argv
Y, D, SUN, K12, K0 = FW['Y'], FW['D'], FW['SUN'], FW['K12'], FW['K0']
veil_k, phase_pi, env_cube = FW['veil_k'], FW['phase_pi'], FW['env_cube']
SIG0, S, THICK = FW['SIG0'], FW['S'], FW['THICK']
FAILS = []


def check(name, ok, msg):
    print('%s %s: %s' % ('PASS' if ok else 'FAIL', name, msg))
    if not ok:
        FAILS.append(name)


def body(text, name, args):
    m = re.search(r'float %s\( %s \)\r?\n\{(.*?)\r?\n\}' % (name, re.escape(args)), text, re.S)
    return re.sub(r'\s+', ' ', m.group(1)).strip() if m else None


def v1():
    sh = os.path.join(TREE, 'res', 'shaders')
    fr = open(os.path.join(sh, 'features', 'air', 'cell_volair.frag'), 'rb').read().decode('utf-8')
    vp = open(os.path.join(sh, 'features', 'air', 'cell_volpart.glsl'), 'rb').read().decode('utf-8')
    cpp = open(os.path.join(TREE, 'src', 'features', 'air', 'cellvolair.cpp'), 'rb').read().decode('utf-8')
    ba, bp = body(fr, 'airVeilK', 'vec3 Jenv, float t, float z'), body(vp, 'vpVeilK', 'vec3 Jenv, float t, float z')
    same = ba is not None and ba == bp
    calls = len(re.findall(r'float vk = vpVeilK\( jEnv, t, z \);', vp))
    uses = len(re.findall(r'\+= vk \* airEnvColour\( sky(Thin|Thick), t, z, dirView \);', vp))
    adds = len(re.findall(r'jEnv \+= volCubeScatter\( (Eg|Am), dW \);', vp))
    lamps = ('thin += e * vpPhase( dot( L, dW ) );' in vp
             and 'thick += e * vpWrap( dot( N, L ), w ) * vpPhaseN( dot( L, dW ) );' in vp)
    uni = 'uniform bool volVeil;' in vp
    cp = 'prog->uni1b( "volVeil", air && !vs().veilPhys );' in cpp
    check('V1 law in tree', same and calls == 1 and uses == 2 and adds == 2 and lamps and uni and cp,
          'body equal %s, calls %d (1), uses %d (2), jEnv adds %d (2), lamps unscaled %s, uniform %s, cpu %s'
          % (same, calls, uses, adds, lamps, uni, cp))


def sky_j(k, d_col, env, vis, Ts, ppi):
    return k * (d_col * vis * Ts * ppi + env)


def main():
    v1()
    env = env_cube()
    dl = float(Y @ D)
    worst, worst_red = 0.0, 0.0
    for t in (2000.0, 20000.0, 50000.0, 113800.0):
        for z in (0.0, 800.0, 2400.0):
            for vis in (1.0, 0.0):
                for c in (-0.6, 0.0, 0.59, 0.95):
                    e = min(1.0, math.exp(-z / THICK))
                    Ts = math.exp(-SIG0 * S * THICK * e / max(SUN[2], 0.02))
                    ppi = phase_pi(c)
                    ka = veil_k(K12, env, t, z, dl)	# the air's froxel
                    kp = 1.0 if RED else veil_k(K12, env, t, z, dl)	# the puff at that froxel (red: the old puff)
                    r = float(Y @ sky_j(kp, D, env, vis, Ts, ppi)) / float(Y @ sky_j(ka, D, env, vis, Ts, ppi))
                    worst = max(worst, abs(r - 1.0))
                    worst_red = max(worst_red, float(Y @ sky_j(1.0, D, env, vis, Ts, ppi))
                                    / float(Y @ sky_j(ka, D, env, vis, Ts, ppi)))
    check('V2 puff = air', worst < 1e-9, 'max |puff / air - 1| %.3g (< 1e-9)' % worst)
    check('V2 red reads the mismatch', worst_red > 2.0, 'old puff / air at most %.2f (> 2.0)' % worst_red)
    dimD, dimE = D * 0.004, env * 0.004
    kn = 1.0 if RED else veil_k(K0, dimE, 113800.0, 2400.0, float(Y @ dimD))
    pn, pn0 = sky_j(kn, dimD, dimE, 1.0, 1.0, phase_pi(0.3)), sky_j(1.0, dimD, dimE, 1.0, 1.0, phase_pi(0.3))
    check('V3 night unchanged', kn == 1.0 and np.array_equal(pn, pn0), 'night k %.3f, puff equal %s'
          % (kn, np.array_equal(pn, pn0)))
    # veilphys: volVeil false in both programs, so both k = 1
    pv, av = sky_j(1.0, D, env, 1.0, 1.0, phase_pi(0.59)), sky_j(1.0, D, env, 1.0, 1.0, phase_pi(0.59))
    check('V4 veilphys both physical', np.array_equal(pv, av), 'puff == air at k 1')
    print('k at the city (t 113800, z 2400): %.3f' % veil_k(K12, env, 113800.0, 2400.0, dl))
    print('RESULT %s (%d fail)%s' % ('PASS' if not FAILS else 'FAIL', len(FAILS), ' [red]' if RED else ''))
    return 0 if not FAILS else 1


sys.exit(main())
