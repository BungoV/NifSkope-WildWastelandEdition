"""FOGCOST1: the FOGLEAK1 air inject's cost offline, in the LIVE define set and scene shape (fogleak_check's G2 cost
row runs one volume and no cascades: +0.87 ms; RT preset Low runs 3 cascades: +3.16 here, +4.0 in the live inject).

usage: python -I tests/features/air/fogleak/fogleak_costbench.py <tree> [--shaders DIR] [--casc 0|3] [--lights N] [--reps 9] [--sets a,b,...]
  sets (each a label:defines:leakred:terms):
    off      RTDDGI (+CASCADES)                 leakred 0  terms 15
    on       RTDDGI (+CASCADES) + FOGLEAK       leakred 0  terms 15
    onred7   on with every red bit (dalcenv+nosungate+lightleak): code compiled in, never run
    dalc / nosun / noleak  one red bit each (the live hold's reds)
    sun8     on with the red sun8 (64): the sun's visibility from all 8 probes (FOGLEAK1-COST, the old read)
  --gate-sun (FOGLEAK1-COST, bars written before the run): with sets on, nosun, sun8 (casc 3, lights 1), the sun gate's
    cost on - nosun <= 0.10 ms, and the red's sun8 - nosun >= 0.25 ms (the old 8-probe read, FOGCOST1's 0.47).
The scene: tests/features/air/fogleak/fogleak_check.py's (160 x 90 x 64 froxels, camera at 0 looking -z), imported for its GL
helpers. --casc 3: WW_CELL_RTDDGI_CASCADES with the c3 hold's three cascades (16^3 @ 273, 16x13x16 @ 547,
16x7x16 @ 1093 round the camera; the rig's y is the world's height). --lights N: N unshadowed omni lights in the
frustum (seeded), radius 600..2000. Prints the median GPU ms of the 64 inject draws per set, then on - off.
"""
import os, sys, ctypes

args = sys.argv[1:]
TREE = os.path.abspath(args[0])
def opt(n, d):
    return args[args.index(n) + 1] if n in args else d
SH = opt('--shaders', os.path.join(TREE, 'res', 'shaders'))
CASC = int(opt('--casc', '3'))
NL = int(opt('--lights', '40'))
REPS = int(opt('--reps', '9'))
SETS = opt('--sets', 'off,on,onred7,dalc,nosun,noleak').split(',')

sys.argv = [sys.argv[0], '--only', 'X']
sys.path.insert(0, os.path.join(TREE, 'tests', 'features', 'air', 'fogleak'))
_exit = sys.exit
sys.exit = lambda *a: None      # its last line exits with the (empty) gate verdict
import fogleak_check as F   # noqa: E402  (builds its own two programs from the tree; we build ours below)
sys.exit = _exit
import numpy as np          # noqa: E402

F.SDIR = SH
BASE = ['WW_CELL_RTDDGI'] + (['WW_CELL_RTDDGI_CASCADES'] if CASC else [])
SPEC = {
    'off': (BASE, 0), 'on': (BASE + ['WW_CELL_FOGLEAK'], 0), 'onred7': (BASE + ['WW_CELL_FOGLEAK'], 7),
    'dalc': (BASE + ['WW_CELL_FOGLEAK'], 1), 'nosun': (BASE + ['WW_CELL_FOGLEAK'], 2),
    'noleak': (BASE + ['WW_CELL_FOGLEAK'], 4), 'sun8': (BASE + ['WW_CELL_FOGLEAK'], 64),
}
for s in SETS:
    if ':' in s:
        lab, d, r = s.split(':')
        SPEC[lab] = (BASE + [x for x in d.split('+') if x], int(r))

# volumes: one (fogleak_check's C volume) or three cascades round the camera
E = lambda d: 0.1 + 0.0 * d[:, 2]
# --vols: which volumes (c0,c1,c2 = the hold's cascades; C = fogleak_check's volume C); --casc 1 with --vols C runs the
# cascades' code on one volume (code cost apart from coverage)
SHAPES = {'c0': ((16, 16, 16), 273.3), 'c1': ((16, 13, 16), 546.7), 'c2': ((16, 7, 16), 1093.3)}
VOLS = opt('--vols', 'c0,c1,c2' if CASC else 'C').split(',')
if CASC and len(VOLS) > 1 and CASC != len(VOLS):
    raise SystemExit('--casc must equal the volume count')
if CASC:
    vols = []
    for nm in VOLS:
        if nm == 'C':
            vols.append(F.Volume((-4000.0, -6000.0, -17000.0), 1000.0, (17, 13, 17), E, 0.6, wall=2500.0))
            continue
        (nx, ny, nz), st = SHAPES[nm]
        o = (-st * (nx - 1) / 2, -st * (ny - 1) / 2, -st * (nz - 1) / 2)
        vols.append(F.Volume(o, st, (nx, ny, nz), E, 0.6, wall=2500.0))
    data = np.concatenate([v.data for v in vols], 0)
    bases = np.cumsum([0] + [len(v.data) for v in vols])[:-1]
    ATLAS = F.tex_buffer(data)
else:
    if VOLS[0] == 'C':
        vols = [F.Volume((-4000.0, -6000.0, -17000.0), 1000.0, (17, 13, 17), E, 0.6, wall=2500.0)]
    else:
        (nx, ny, nz), st = SHAPES[VOLS[0]]
        o = (-st * (nx - 1) / 2, -st * (ny - 1) / 2, -st * (nz - 1) / 2)
        vols = [F.Volume(o, st, (nx, ny, nz), E, 0.6, wall=2500.0)]
    bases = [0]
    ATLAS = vols[0].tex

rng = np.random.default_rng(7)
T = np.zeros((max(NL, 1) * 8, 4))
for i in range(NL):
    z = -rng.uniform(500.0, 8000.0)
    x = rng.uniform(-0.8, 0.8) * -z / F.P00
    y = rng.uniform(-0.8, 0.8) * -z / F.P11
    T[i * 8 + 0] = [x, y, z, rng.uniform(600.0, 2000.0)]
    T[i * 8 + 1] = [1.0, 1.0, 1.0, -2.0]
    T[i * 8 + 3] = [0.0, 1.0, 1.0, 0.0]
    T[i * 8 + 4] = [-1.0, 0.0, 0.0, 0.0]
LB = F.tex_buffer(T)

q = (F.GLuint * 1)()
F.f_genq(1, q)


def setup(p, units, leak):
    F.uniforms(p, units, vols[0], 15, leak, LB)
    F.setu(p, 'cellLightCount', NL)
    if CASC:
        F.setu(p, 'cellDdgiCount', len(vols))
        for c, v in enumerate(vols):
            o, s, (nx, ny, nz) = v.origin, v.step, v.dims
            F.setu(p, 'cellDdgiCOrigin[%d]' % c, (o[0], o[1], o[2], s))
            F.setu(p, 'cellDdgiCStep[%d]' % c, (s, s, s, 0.3))
            F.setu(p, 'cellDdgiCDims[%d]' % c, (nx, ny, nz, v.nP), ints=True)
            F.setu(p, 'cellDdgiCBase[%d]' % c, int(bases[c]))
    if 'cellDdgiAtlas' in units:
        F.G.active(0x84C0 + units['cellDdgiAtlas'])
        F.gl.glBindTexture(0x8C2A, ATLAS)


def timed(p, units, leak):
    setup(p, units, leak)
    F.G.bindfb(0x8D40, F.target())
    F.gl.glViewport(0, 0, F.COLS, F.ROWS)
    l = F.G.loc(p, b'volLayer')
    ms = []
    for rep in range(REPS + 2):
        F.gl.glFinish()
        F.f_begq(0x88BF, q[0])
        for k in range(F.SL):
            F.G.u1i(l, k)
            F.gl.glDrawArrays(5, 0, 4)
        F.f_endq(0x88BF)
        r = ctypes.c_uint64()
        F.f_getq(q[0], 0x8866, ctypes.byref(r))
        if rep >= 2:
            ms.append(r.value / 1e6)
    return float(np.median(ms))


progs = {}
res = {}
for s in SETS:
    lab = s.split(':')[0]
    defs, leak = SPEC[lab]
    key = tuple(defs)
    if key not in progs:
        progs[key] = F.build(defs)
    res[lab] = timed(*progs[key], leak)
# interleave a second pass to see drift
res2 = {}
for s in SETS:
    lab = s.split(':')[0]
    defs, leak = SPEC[lab]
    res2[lab] = timed(*progs[tuple(defs)], leak)
off = min(res.get('off', 0.0), res2.get('off', 0.0)) if 'off' in res else None
print('COSTBENCH shaders=%s casc=%d lights=%d reps=%d' % (SH, CASC, NL, REPS))
for s in SETS:
    lab = s.split(':')[0]
    m = min(res[lab], res2[lab])
    print('  %-10s %7.3f ms (passes %.3f / %.3f)%s' % (lab, m, res[lab], res2[lab],
          '' if off is None or lab == 'off' else '  +%.3f vs off' % (m - off)))
if '--gate-sun' in args:
    b = {k: min(res[k], res2[k]) for k in ('on', 'nosun', 'sun8')}
    g1, g2 = b['on'] - b['nosun'], b['sun8'] - b['nosun']
    ok = g1 <= 0.10 and g2 >= 0.25
    print('%s sun gate cost: on - nosun %.3f ms (<= 0.10), red sun8 - nosun %.3f ms (>= 0.25)' % ('PASS' if ok else 'FAIL', g1, g2))
    sys.exit(0 if ok else 1)
