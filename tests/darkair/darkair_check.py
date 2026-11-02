#!/usr/bin/env python3
"""Lane DARKAIR1 gate: outdoors the air's ambient (the weather's DALC) takes the sky the ray-traced volume sees up from
the froxel (cell_volair.frag airSkyView), on the REAL res/shaders/cell_volair.prog, offline through this machine's GL
driver (the scene, the synthetic volume and the uniforms of tests/fogleak/fogleak_check.py, exec'd up to its programs).

    python -I tests/darkair/darkair_check.py [--base <a res/shaders dir of the base>] [--red]

Programs: NEW = WW_CELL_RTDDGI + WW_CELL_AIR_SKYVIEW (the opt-in loader define; no FOGLEAK: FOGLEAK1's own read replaces the env share there). RED = the same program
with volEnvOccRed 1 (WW_CELL_AIR_RED=openenv). --red runs every gate with NEW set to the red: the D2 rows must FAIL.
The synthetic volume's tiles hold one sky visibility (alpha) on every texel: 1.0 = open ground, 0.36 = the c3 street
(the SKY2 tracer's normalised sky share k 0.359, notes/fogleak-r/RESEARCH.md 4).
Bars (written before the first run):
  D0 identity  --base given: BASE (RTDDGI) == RED, BASE (RTDDGI) == the tree's RTDDGI program without the define (off),
               and BASE (no RTDDGI) == NEW (no RTDDGI), sun + ambient, every froxel
               (|d| <= 1e-5 x the value + 1e-7 x max: fogleak_check's same()).
  D1 open      alpha 1.0, sun + ambient: NEW == RED on every froxel (lit air in the open unchanged).
  D2 street    alpha 0.36, the ambient only: inside the volume NEW / RED median 0.35 .. 0.37 and p1 >= 0.34; outside
               the volume NEW == RED.
  D3 sun       alpha 0.36, the sun only: NEW == RED on every froxel (the sun's share is not touched).
  D4 interior  alpha 0.36, cellInterior 1, the ambient only: NEW == RED (indoors keeps its own rules).
  C cost       INFO: GPU ms of the 64 inject draws (sun + ambient), NEW vs RED, median of 9.
Exit 0 only when every gate passes.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
argv = sys.argv[1:]
BASE = argv[argv.index('--base') + 1] if '--base' in argv else ''
REDRUN = '--red' in argv

FL = os.path.join(TREE, 'tests', 'fogleak', 'fogleak_check.py')
src = open(FL, encoding='latin-1').read()
cut = src.index("P_ON, U_ON = build(")
sys.argv = [FL]   # fogleak_check reads its own args at the top
g = {'__name__': 'fogleak_check_lib', '__file__': FL}
exec(compile(src[:cut], FL, 'exec'), g)

np, math, ctypes = g['np'], g['math'], g['ctypes']
build, uniforms, inject, same, lum, Volume = g['build'], g['uniforms'], g['inject'], g['same'], g['lum'], g['Volume']
G, GLuint, gl, setu = g['G'], g['GLuint'], g['gl'], g['setu']
results = []


def gate(name, ok, msg):
    results.append(bool(ok))
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


P_NEW, U_NEW = build(['WW_CELL_RTDDGI', 'WW_CELL_AIR_SKYVIEW'])
A_FLAT = g['A_FLAT']
E0 = 0.3 * math.pi * A_FLAT


def vol(alpha):
    return Volume((-4000.0, -24000.0, -36000.0), 4000.0, (19, 13, 12), lambda d: E0 * (1.0 + 0.5 * d[:, 2]), alpha)


def run(p, u, V, terms, red, interior=False):
    uniforms(p, u, V, terms, 0, interior=interior)
    setu(p, 'volEnvOccRed', int(red))   # after uniforms(): fogleak_check sets it 1 for its OFF arm
    return inject(p)


def new(V, terms, interior=False):
    return run(P_NEW, U_NEW, V, terms, 1 if REDRUN else 0, interior)


def red(V, terms, interior=False):
    return run(P_NEW, U_NEW, V, terms, 1, interior)


tag = ' [red run]' if REDRUN else ''
every = np.ones((g['SL'], g['ROWS'], g['COLS']), bool)

if BASE:
    g['SDIR'] = os.path.abspath(BASE)
    P_B, U_B = build(['WW_CELL_RTDDGI'])
    P_B0, U_B0 = build([])
    g['SDIR'] = os.path.join(TREE, 'res', 'shaders')
    P_N0, U_N0 = build([])
    P_OFF, U_OFF = build(['WW_CELL_RTDDGI'])
    V = vol(0.36)
    b = run(P_B, U_B, V, 1 | 8, 0)
    r = red(V, 1 | 8)
    n1, d1 = same(r, b, every)
    b0 = run(P_B0, U_B0, V, 1 | 8, 0)
    n0 = run(P_N0, U_N0, V, 1 | 8, 0)
    n2, d2 = same(n0, b0, every)
    uniforms(P_OFF, U_OFF, V, 1 | 8, 0)
    n3, d3 = same(inject(P_OFF), b, every)
    gate('D0 identity', n1 == 0 and n2 == 0 and n3 == 0 and float(lum(b[..., :3]).max()) > 0.0,
         'base RTDDGI vs red: %d differ (max %.2e); base vs new without RTDDGI: %d differ (max %.2e); base vs off: %d '
         'differ (max %.2e); lit max %.3g' % (n1, d1, n2, d2, n3, d3, float(lum(b[..., :3]).max())))

V1 = vol(1.0)
a, b = new(V1, 1 | 8), red(V1, 1 | 8)
n, d = same(a, b, every)
gate('D1 open', n == 0 and float(lum(b[..., :3]).max()) > 0.0,
     'alpha 1.0, sun + ambient: %d of %d froxels differ (max %.2e of max)%s' % (n, every.sum(), d, tag))

V36 = vol(0.36)
lo, hi = V36.lo_hi_fade1()
inside = g['all_in'](lo, hi, 1100.0)
outside = g['all_where'](lambda P: P[..., 0] <= -5100.0)
a, b = new(V36, 8), red(V36, 8)
r = lum(a[..., :3]) / np.maximum(lum(b[..., :3]), 1e-30)
n, d = same(a, b, outside)
med = float(np.median(r[inside])) if inside.any() else -1.0
p1 = float(np.percentile(r[inside], 1)) if inside.any() else -1.0
gate('D2 street', inside.sum() > 100 and outside.sum() > 100 and 0.35 <= med <= 0.37 and p1 >= 0.34 and n == 0,
     'alpha 0.36, ambient only: inside %d froxels new/red median %.4f p1 %.4f p99 %.4f; outside %d froxels: %d differ%s'
     % (inside.sum(), med, p1, float(np.percentile(r[inside], 99)) if inside.any() else -1, outside.sum(), n, tag))

a, b = new(V36, 1), red(V36, 1)
n, d = same(a, b, every)
gate('D3 sun', n == 0 and float(lum(b[..., :3]).max()) > 0.0,
     'alpha 0.36, sun only: %d differ (max %.2e)%s' % (n, d, tag))

a, b = new(V36, 8, True), red(V36, 8, True)
n, d = same(a, b, every)
gate('D4 interior', n == 0, 'alpha 0.36, interior, ambient only: %d differ (max %.2e)%s' % (n, d, tag))

q = (GLuint * 1)()
g['f_genq'](1, q)


def timed(redv):
    uniforms(P_NEW, U_NEW, V36, 1 | 8, 0)
    setu(P_NEW, 'volEnvOccRed', redv)
    G.bindfb(0x8D40, g['target']())
    gl.glViewport(0, 0, g['COLS'], g['ROWS'])
    l = G.loc(P_NEW, b'volLayer')
    ms = []
    for rep in range(9):
        gl.glFinish()
        g['f_begq'](0x88BF, q[0])
        for k in range(g['SL']):
            G.u1i(l, k)
            gl.glDrawArrays(5, 0, 4)
        g['f_endq'](0x88BF)
        rr = ctypes.c_uint64()
        g['f_getq'](q[0], 0x8866, ctypes.byref(rr))
        ms.append(rr.value / 1e6)
    return float(np.median(ms))


tr, tn = timed(1), timed(0)
gate('C cost', True, 'INFO inject 160x90x64, sun + ambient, median of 9: red %.3f ms, new %.3f ms (+%.3f)' % (tr, tn, tn - tr))
print('DARKAIR-CHECK %s (%d of %d)%s' % ('PASS' if results and all(results) else 'FAIL', sum(results), len(results), tag))
sys.exit(0 if results and all(results) else 1)
