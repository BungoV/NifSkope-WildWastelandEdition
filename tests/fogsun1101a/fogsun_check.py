"""Lane FIX1101A (atmo): the fog-sun fault (manager 11-01, LATE ITEM 18's thick-fog picture: no shafts, the ground's sun
erased). A twin of the sun's transmittance the air (cell_airmed.glsl airTupSun) and the surfaces (lookdev_fog.glsl
wwVolAirSunT) take, on the frame's own medium (hold h2 c3 CommonwealthFoggy 12:00 `cell air:` census: sigma0 3.88757e-05
/u, flat band zb -9936 H 789958.6, top 29811.8, ground layer 5.667 x over 4195.8 u at zg 64; sun mu 0.55249), and the
Clear noon control (sigma0 0.531 /km, ground layer 0.25 x over 300 u).
  shipped (airSunLayer / flag 128): only the derived ground layer dims the sun;
  red suncolumn (WW_CELL_AIR_RED=suncolumn): the whole fitted column to the scene's top (FOGSUN1 as it was).
Gates (stated before the twin ran, Foggy): lit / shadowed air in-scatter at the street facing the sun >= 1.25; the
ground's direct sun >= 10 % of the record's; Ts <= 1. Ambient = the Foggy Day DALC mean (linear), phase = the fallback
god-ray record. Plus text checks: both shaders carry the layer-only column (the twin follows the shipped code).
Measured 11-01: shipped 1.622 / 19.7 % PASS; red 1.082 / 2.6 % FAIL. Clear: Ts 0.674 -> 0.999.
usage: python -I fogsun_check.py [--red suncolumn]   (exit 0 = all gates pass)"""
import sys, os, math
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
RED = '--red' in sys.argv
U_KM = 0.0142875e-3


def col_one(lo, hi, z0, invH):   # lookdev_fog.glsl wwAirColOne
    if hi <= lo:
        return 0.0
    if invH <= 0:
        return hi - lo
    below = max(min(hi, z0) - lo, 0.0)
    a = max(lo, z0)
    if hi <= a:
        return below
    w = hi - a
    u = w * invH
    e1 = (1 - math.exp(-u)) / u if u > 1e-3 else 1 - u * (0.5 - u / 6)
    return below + math.exp(-(a - z0) * invH) * w * e1


def hg(c, g):
    return (1 - g * g) / (4 * math.pi * (1 + g * g - 2 * g * c) ** 1.5)


def pi_phase(c):
    return math.pi * (0.326 * 3 / (16 * math.pi) * (1 + c * c) + 0.449 * hg(c, 0.75) + 0.225 * hg(c, 0.0))


fails = 0


def gate(name, ok, msg):
    global fails
    print('%s %s: %s' % ('PASS' if ok else 'FAIL', name, msg))
    fails += 0 if ok else 1


lin = lambda b: (np.array(b) / 255.0) ** 2.2
Yw = np.array([0.2126, 0.7152, 0.0722])
mu = 0.55249
z = 64.0 + 1.7 * 70
ph = pi_phase(math.cos(math.asin(mu)))
FOG = dict(s0=3.88757e-05, zb=-9936.0, invH=1 / 789958.6, top=29811.8, s=5.667, Hg=4195.8, zg=64.0,
           sun=np.array([1.48587, 1.28516, 1.01719]),
           dalc=[(83, 97, 113), (73, 88, 101), (69, 89, 100), (86, 100, 117), (70, 94, 113), (168, 201, 238)])
CLR = dict(s0=0.531 * U_KM, zb=-9936.0, invH=1 / 789958.6, top=29811.8, s=0.25, Hg=300.0, zg=64.0)


def Ts(c, red):
    band = col_one(z, c['top'], c['zb'], c['invH'])
    grd = c['s'] * col_one(z, c['top'], c['zg'], 1.0 / c['Hg'])
    return math.exp(-c['s0'] * ((band + grd) if red else grd) / mu)


T = Ts(FOG, RED)
D = float(Yw @ FOG['sun'])
A = float(Yw @ lin(FOG['dalc']).mean(0))
shaft = (A + D * T * ph) / A
gate('shaft', shaft >= 1.25, 'Foggy noon: lit / shadowed air column %.3f (bar 1.25), Ts %.4f' % (shaft, T))
gate('ground-sun', T >= 0.10, 'Foggy noon: the ground\'s direct sun %.1f %% of the record\'s (bar 10)' % (100 * T))
gate('never-above', T <= 1.0 and Ts(CLR, RED) <= 1.0, 'Ts <= 1 (Clear %.4f)' % Ts(CLR, RED))
# the shipped shaders carry the rule the twin computes (the red is the switch, not a shader copy)
am = open(os.path.join(TREE, 'res/shaders/cell_airmed.glsl'), encoding='latin-1').read()
lf = open(os.path.join(TREE, 'res/shaders/lookdev_fog.glsl'), encoding='latin-1').read()
va = open(os.path.join(TREE, 'res/shaders/cell_volair.frag'), encoding='latin-1').read()
vp = open(os.path.join(TREE, 'res/shaders/cell_volpart.glsl'), encoding='latin-1').read()
gate('shader-air', 'airLayer.w * wwAirColOne( z, airMed.w, airGround.x + airGz, airGround.y )' in am, 'cell_airmed.glsl airTupSun: the ground layer only')
gate('shader-surface', 'volAirLayer.w * wwAirColOne( lo, volAirMed.w, g.x, g.y )' in lf and '( volAirFlags & 128 ) != 0' in lf,
     'lookdev_fog.glsl wwVolAirSunT: flag 128 = the ground layer only')
gate('shader-sites', va.count('airTupSun(') == 3 and 'airTupSun(' in vp and 'airTup( z, to.z )' not in va,
     'cell_volair.frag: 3 sun sites on airTupSun; cell_volpart.glsl too')
print('%s: %d gate(s) failed%s' % ('GATE OK' if not fails else 'GATE FAIL', fails, ' (red suncolumn)' if RED else ''))
sys.exit(1 if fails else 0)
