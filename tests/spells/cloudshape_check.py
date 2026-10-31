"""Lane CLOUDSHAPE1: Cloud Shapes' offline gates in the tree (the look gate against RDR2 / reference frames lives in
notes/cloudshape1/tools/gate.py: those frames never go in the repo). Runs the REAL shaders in the hidden GL context
(CLOUD2's cloudvol_check helpers), the C++ side through tests/cloudshape/cloudshape_dump.cpp. No NifSkope.

    python -I tests/spells/cloudshape_check.py <scratch dir> [--red nocal|off]

Gates (bars written before the first run):
  L link      cloud_march_s, cloud_fly_s, cloud_shadowmap_s, cloud_flybake_s link (every reader of the density has its twin).
  M stats     the shape volume's mean / sd == cloudvolmath.h kShpMean / kShpSd within 0.002 (the shape normalises by them);
              the body field is uniform 0..1 (mean 0.5 +- 0.002, sd 0.2887 +- 0.002).
  T twin      cloud_common.glsl WW_CLOUD_SHAPE cloudDensity (probe mode 0, mip 0) == cloudvolmath.h shapeDensity on a 96 x 64
              vertical slice through the Low band, cumulus and cumulonimbus: max |d| <= 0.05, mean |d| <= 0.005 (8-bit
              texture weights vs exact float). Red off: the probe over the OLD density fails.
  A coverage  the column opacity from straight above over map stripes 0.1 .. 0.7, Low band, cumulus (T 2/3), stratocumulus
              (1/3) and cumulonimbus (1): |mean - map| <= 0.06 each (the shape tables are calibrated). Red nocal fails.
  B bound     cloud_flybake_s: every 0.25 km cell whose densest of 6^3 points is > 0 is marked occupied (the bound never cuts
              a cloud); a cumulus and a storm sky, 64 x 64 cells x 8 layers. No red (a bound either holds or not).
Exit 0 only when every gate run passes.
"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudshape_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
os.makedirs(scratch, exist_ok=True)
sys.path.insert(0, HERE)
sys.argv = [os.path.join(HERE, 'cloudvar_check.py'), scratch, '--only', 'Z']
import runpy   # noqa: E402
V = runpy.run_path(sys.argv[0], run_name='cloudshape_helpers')
np, K = V['np'], V['K']
C, g, gl = K.C, K.g, K.gl
SDIR = os.path.join(TREE, 'res', 'shaders')
ENV = V['ENV']
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


DUMP = os.path.join(scratch, 'cloudshape_dump.exe')
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(TREE, 'tests', 'cloudshape', 'cloudshape_dump.cpp'), '-o', DUMP], env=ENV,
                   capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])


def dump(*a):
    return subprocess.run([DUMP] + [str(x) for x in a], env=ENV, capture_output=True, text=True, check=True).stdout.split('\n')


# ---- L ----
links = {}
for name in ('cloud_march_s.prog', 'cloud_fly_s.prog', 'cloud_shadowmap_s.prog', 'cloud_flybake_s.prog'):
    try:
        links[name] = V['linked'](C.build(SDIR, name), name)
    except SystemExit as e:
        links[name] = None
        print(e)
gate('L link', all(links.values()), ' '.join('%s=%s' % (k, 'ok' if v else 'FAIL') for k, v in links.items()))

# ---- M ----
st = {l.split()[0]: [float(x) for x in l.split()[1:]] for l in dump('stats') if l}
sm, ssd = st['shape']
cm, csd = st['cell'][:2]
gate('M stats', abs(sm - 0.884) <= 0.002 and abs(ssd - 0.053) <= 0.002 and abs(cm - 0.5) <= 0.002 and abs(csd - 0.2887) <= 0.002,
     'shape mean %.4f sd %.4f (kShpMean 0.884, kShpSd 0.053); body field mean %.4f sd %.4f' % (sm, ssd, cm, csd))

binp = os.path.join(scratch, 'cell.bin')
dump('cell', binp)
cella = np.ascontiguousarray(np.fromfile(binp, np.float32).reshape(512, 512))
t = K.GLuint()
gl.glGenTextures(1, K.ctypes.byref(t))
gl.glBindTexture(0x0DE1, t)
gl.glTexImage2D(0x0DE1, 0, 0x822E, 512, 512, 0, 0x1903, 0x1406, cella.ctypes.data)
K.f_mip(0x0DE1)
for k, v in ((0x2801, 0x2703), (0x2800, 0x2601), (0x2802, 0x2901), (0x2803, 0x2901)):
    gl.glTexParameteri(0x0DE1, k, v)
CELL = t.value


def sky(T, low=(1.5, 3.5, 1.0), nocal=False):
    out = dump('cal', '%g' % T, low[0], low[1], low[2], 0, K.MID[0], K.MID[1], K.MID[2], 1 if nocal else 0)
    cal, typ = {}, {}
    for l in out:
        w = l.split()
        if w and w[0] == 'cal':
            cal[int(w[1])] = [float(x) for x in w[2:]]
        elif w and w[0] == 'type':
            typ[int(w[1])] = tuple(float(x) for x in w[2:6])
    return [cal[0], cal[1]], typ


def probe_prog(shape):
    f = os.path.join(scratch, 'probe_%s.frag' % ('s' if shape else 'old'))
    inc = os.path.join(TREE, 'tests', 'cloudvar', 'cloudvar_probe.glsl').replace('\\', '/')
    open(f, 'w').write('#version 410 core\n%s#include "%s"\n' % ('#define WW_CLOUD_SHAPE 1\n' if shape else '', inc))
    return V['linked'](C.build(SDIR, None, [os.path.join(SDIR, 'cell_ao.vert'), f]), 'probe')


P_S = probe_prog(True)
P_O = probe_prog(False)


def run_probe(prog, uni, w, h):
    tgt = K.Mrt(w, h)
    texs = V['vtexs'](K.trans, K.trans) + [('cvCell', 0x0DE1, CELL)]
    K.draw(prog, tgt, uni, texs)
    return tgt.read(0)


# ---- T ----
worst, means = 0.0, []
for T_, low in ((2.0 / 3.0, (1.5, 3.5, 1.0)), (1.0, (1.0, 9.0, 1.0))):
    cal, typ = sky(T_, low)
    nx, ny = 96, 64
    O, U, Vv = (0.3, 0.7, low[0]), (0.05, 0.0, 0.0), (0.0, 0.0, (low[1] - low[0]) / ny)
    cpu = []
    for l in dump('grid', '%g' % T_, low[0], low[1], low[2], 0, K.MID[0], K.MID[1], K.MID[2], 0, 0.45, 0.0, 0.0,
                  *O, *U, *Vv, nx, ny):
        w = l.split()
        if w and w[0] == 'g':
            cpu.append([float(x) for x in w[2:]])
    cpu = np.array(cpu)
    u = K.bands_uni(low, K.MID, cal, cov=(0.45, 0.0, 0.0))
    u.update({'cvShpT0': typ[0], 'cvShpT1': typ[1], 'prMode': 0, 'prO': O, 'prU': U, 'prV': Vv, 'prZ': (0.0, 1.0),
              'prNZ': 1, 'prEdge': (0.0,)})
    gpu = run_probe(P_O if red == 'off' else P_S, u, nx, ny)[..., 0]
    dd = np.abs(gpu - cpu)
    worst = max(worst, float(dd.max()))
    means.append(float(dd.mean()))
gate('T twin', worst <= 0.05 and max(means) <= 0.005, 'GPU vs CPU density, cumulus + cumulonimbus slices: max |d| %.4f '
     '(bar 0.05), mean %.5f (bar 0.005)%s' % (worst, max(means), ' [RED off: the old density]' if red == 'off' else ''))

# ---- A ----
msgs, worstA = [], 0.0
covs = [0.1, 0.3, 0.5, 0.7]
for T_ in (2.0 / 3.0, 1.0 / 3.0, 1.0):
    low = (1.0, 9.0, 1.0) if T_ >= 1.0 else (1.5, 3.5, 1.0)
    cal, typ = sky(T_, low, nocal=(red == 'nocal'))
    strip = np.zeros((1, 32 * len(covs), 4))
    for i, c in enumerate(covs):
        strip[0, i * 32:(i + 1) * 32, 0] = c
    K.MAP = K.tex2d(strip, linear=True)
    he = 48.0 * len(covs) / 2.0   # stripes 48 km wide (1.5 body-field periods; 0.5 tower period)
    PW, PH = 640, 160
    u = K.bands_uni(low, K.MID, cal, rect=(0.0, 0.0, 1.0 / (2 * he), 1.0))
    u.update(K.march_uni(K.camera(0, 10), (0, 0, 0.0), (PW, PH), 1, (0, 0), 30.0, drop=K.drop))
    u.update({'cvShpT0': typ[0], 'cvShpT1': typ[1], 'cvProbe': 1, 'cvProbeRect': (0.0, 0.0, 2 * he / PW, 96.0 / PH),
              'cvProbeBand': 0})
    tgt = K.Mrt(PW, PH)
    texs = V['vtexs'](K.trans, K.trans) + [('cvCell', 0x0DE1, CELL)]
    K.draw(links['cloud_march_s.prog'], tgt, u, texs)
    op = tgt.read()[..., 0]
    got = []
    for i, c in enumerate(covs):
        x0, x1 = int((i + 0.1) / len(covs) * PW), int((i + 0.9) / len(covs) * PW)
        m = float(op[:, x0:x1].mean())
        got.append(m)
        worstA = max(worstA, abs(m - c))
    msgs.append('T%.2f %s' % (T_, ' '.join('%.3f' % v for v in got)))
gate('A coverage', worstA <= 0.06, 'map %s -> %s; worst |d| %.3f (bar 0.06)%s' % (
    ' '.join('%.1f' % c for c in covs), '; '.join(msgs), worstA, ' [RED nocal]' if red == 'nocal' else ''))

# ---- B ----
bad, tot = 0, 0
for T_, low in ((2.0 / 3.0, (1.5, 3.5, 1.0)), (1.0, (1.0, 9.0, 1.0))):
    cal, typ = sky(T_, low)
    K.MAP = K.tex2d(np.zeros((1, 1, 4)))
    cell = 0.25
    for layer in range(8):
        z0 = low[0] + (low[1] - low[0]) * layer / 8.0
        u = K.bands_uni(low, K.MID, cal, cov=(0.45, 0.0, 0.0))
        u.update({'cvShpT0': typ[0], 'cvShpT1': typ[1], 'cfOrgKm': (1.3, 2.1, z0), 'cfCellKm': (cell,), 'cfLayer': 0,
                  'cfMargin': (0.02,), 'cfSlackKm': (0.0,)})
        occ = []
        for prb in (0, 1):
            u['cfProbe'] = prb
            occ.append(run_probe(links['cloud_flybake_s.prog'], u, 64, 64)[..., 0])
        miss = (occ[1] > 0.0) & (occ[0] < 0.5)
        bad += int(miss.sum())
        tot += int((occ[1] > 0.0).sum())
gate('B bound', bad == 0 and tot > 0, '%d of %d cells with density were marked empty (bar 0)' % (bad, tot))

print('CLOUDSHAPE-CHECK %s' % ('PASS' if all(results) else 'FAIL'))
sys.exit(0 if all(results) else 1)
