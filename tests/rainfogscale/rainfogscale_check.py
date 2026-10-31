"""Lane FIX1031B clouds (parent CLOUDBAND1): the rain fog's SCALE. bungo's run3b pair (CommonwealthRain + Cloud Map
Ultra, rain fog on vs WW_CLOUD_RAINFOG_RED=old) moved the frame by a mean 0.6 level: not visible. Measured here on the
frame's own uniforms (frame_c3_rain_h150.txt = its notes line) and the game's FNAM (fnam_commonwealthrain.txt).

    python -I tests/rainfogscale/rainfogscale_check.py <scratch> [--red today|mono|notail|wiring] [--base <rev>]

Gates (bars written before the first run):
  M law     the dump's Marshall & Palmer (1948) extinction == the checker's own integral of the drop sizes
            (N0 e^(-Lambda D), Q 2, Simpson over 0..20 mm) at 7 rates, rel <= 1e-6; the rate factor s^0.63 == the
            integral's ratio rel <= 1e-6.
  V visible the rain fog's added fog share at 1 km on a level ray 300 m over the street (the column under the cloud
            base, above the air's 60 m ground layer), the map's mean cover: dA >= 0.10 (with the frame's ~60-level fog vs
            surface contrast: ~6 levels). Arms: green = Marshall-Palmer + the tails (RAINFOG2); red today = the one-drop
            law and no tails (main 5422be6c: the rain fog stops at the froxel grid's far edge, 441 m in this frame); red
            mono = the one-drop law with the tails; red notail = Marshall-Palmer without the tails. Each red must FAIL.
  D words   WW_CLOUD_RAINFOG_RED's words: mono -> 1, today -> 3, old / 1 -> no rain fog (redOn), others 0.
  W wiring  cellvolair.cpp hands the shaders Marshall-Palmer unless the red (defaultMpSigmaPerKm, mpRateFactor,
            redWords, RedNoTail). Red wiring = the base's file must FAIL.
INFO: the street-level table (the air alone already hides 99.9 % at 1 km there: its derived ground layer, V 442 m) and
the game's own FNAM alpha. Exit 0 only when every gate run passes.
"""
import math, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'rainfogscale_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
base = args[args.index('--base') + 1] if '--base' in args else '5422be6c'
os.makedirs(scratch, exist_ok=True)
UPM = 69.93007   # game units per metre (cellvolair.cpp kUnitsPerKm / 1000)
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-10s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


# ---- the dump: the C++ numbers ---------------------------------------------------------------------------------------
exe = os.path.join(scratch, 'rainfogscale_dump.exe')
env = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(HERE, 'rainfogscale_dump.cpp'), '-o', exe], env=env, capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])
D = {}
for ln in subprocess.run([exe], env=env, capture_output=True, text=True, check=True).stdout.splitlines():
    w = ln.split()
    D.setdefault(w[0], []).append(w[1:])
RATE = float(D['rain'][0][1])
MONO, MP = float(D['mono'][0][0]), float(D['mp'][0][0])
N0, LAM, EXP, Q = map(float, D['mpconst'][0])

# ---- M: Marshall-Palmer, integrated here -------------------------------------------------------------------------------
def mp_integral(R, n=20000, dmax=20.0):
    lam = LAM * R ** -EXP
    h = dmax / n
    s = 0.0
    for i in range(n + 1):
        Dm = i * h
        f = Q * math.pi / 4 * Dm * Dm * N0 * math.exp(-lam * Dm)
        s += f * (1 if i in (0, n) else (4 if i % 2 else 2))
    return s * h / 3 * 1e-6 * 1000.0   # mm^2 / m^3 -> 1 / km


worst = 0.0
for rr, sg in D['mprate']:
    worst = max(worst, abs(float(sg) / mp_integral(float(rr)) - 1))
fw = 0.0
for s, f in D['factor']:
    s, f = float(s), float(f)
    want = mp_integral(RATE * s) / mp_integral(RATE) if s > 0 else 0.0
    fw = max(fw, abs(f - want) / max(want, 1e-12))
gate('M law', worst <= 1e-6 and fw <= 1e-6,
     'Marshall-Palmer at %d rates: worst rel %.2g (<= 1e-6); rate factor worst rel %.2g; default rain %.1f mm/h: MP %.4f / km '
     '(visibility %.1f km) vs the 2-mm drops %.4f / km (%.1f km): x%.2f | 25 mm/h MP %.3f / km (visibility %.1f km)'
     % (len(D['mprate']), worst, fw, RATE, MP, 3.912 / MP, MONO, 3.912 / MONO, MP / MONO, mp_integral(25.0),
        3.912 / mp_integral(25.0)))

# ---- the frame ----------------------------------------------------------------------------------------------------------
line = [l for l in open(os.path.join(HERE, 'frame_c3_rain_h150.txt'), encoding='utf-8') if l.startswith('cell air:')][0]
F = {k: float(re.search(r'\b' + k + r'=([-0-9.e+]+)', line).group(1)) for k in ('sigma0', 'zb', 'H', 'ground', 'thick', 'zg', 'far', 'top')}
rf = re.search(r'rainfog=on\(share=([0-9.]+) sigma=([0-9.]+)/km base=([0-9.]+) cov=([0-9.]+)', line)
RF_BASE, RF_COV, RF_SIGMA_ECHO = float(rf.group(3)), float(rf.group(4)), float(rf.group(2))
K = {}
for ln in open(os.path.join(HERE, 'fnam_commonwealthrain.txt'), encoding='utf-8'):
    m = re.match(r'fogk hour=\S+ row=(\d+) v=(\S+)', ln)
    if m:
        K[int(m.group(1)) - 41] = [float(x) for x in m.group(2).split(',')]
K = [K[i] for i in range(6)]


def game_alpha(d, z):   # lookdev_fog.glsl wwFogEval (notes/fogheight1/tools/measure.py, ported there)
    ramp = d * K[0][0] - K[0][2]
    f = min(max(ramp, 0.0), 1.0)
    p0 = min(max(z * K[5][0] - K[5][2], 0.0), 1.0)
    p1 = min(max(z * K[5][1] - K[5][3], 0.0), 1.0)
    hb = p0 + (p1 - p0) * f
    mx = K[2][3]
    clampT = min((f - 0.75) * 4.0 * (1.0 - mx) + mx, 1.0) if ramp > 0.75 else mx
    escape = f * 66.666672 if ramp < 0.015 else 1.0
    I = min(clampT, f ** K[1][3]) if f > 0.0 else 0.0
    return (hb * K[3][3] + (1.0 - hb)) * I * escape


def profile(hkm):   # rainfogmath.h profile
    return 1 + math.exp(-hkm / 0.3) if hkm >= 0 else 2 * max(1 + hkm / 0.15, 0)


def air_od(hm, d_km):   # wwAirRhoLaw on a level ray hm over the street (the ground layer stands on the terrain)
    z = hm * UPM
    rho = min(1.0, math.exp(-(z - F['zb']) / F['H'])) + F['ground'] * min(1.0, math.exp(-(z - F['zg']) / F['thick']))
    return F['sigma0'] * rho * d_km * 1000 * UPM if z <= F['top'] else 0.0


def rain_od(arm, hm, d_km, c):
    law = MONO if arm in ('today', 'mono') else MP
    reach = d_km if arm in ('green', 'mono') else min(d_km, F['far'] / UPM / 1000)   # no tails: the grid's far edge
    return law * c * profile((RF_BASE - hm * UPM) / UPM / 1000) * reach


def dA(arm, hm, d, c):
    oa = air_od(hm, d)
    return (1 - math.exp(-(oa + rain_od(arm, hm, d, c)))) - (1 - math.exp(-oa))


arm = red if red in ('today', 'mono', 'notail') else 'green'
print('frame: air sigma0 %.4g / u (%.3f / km) ground x%.3f over %.0f m, grid far %.0f m, air top %.0f m; rain fog base %.0f m, '
      'map cover %.3f, echo sigma %.4f / km (main\'s law %.4f)' % (F['sigma0'], F['sigma0'] * UPM * 1000, F['ground'],
                                                                F['thick'] / UPM, F['far'] / UPM, F['top'] / UPM,
                                                                RF_BASE / UPM, RF_COV, RF_SIGMA_ECHO, MONO))
print('INFO game FNAM (12:00) alpha at the street: ' + ', '.join('%g km %.3f' % (d, game_alpha(d * 1000 * UPM, 1.7 * UPM))
                                                              for d in (0.5, 1, 2, 3)))
print('INFO %5s %5s %6s | %6s | %8s %8s %8s %8s' % ('h m', 'd km', 'cover', 'air A', 'today', 'mono', 'notail', 'green'))
for hm in (2.0, 100.0, 300.0):
    for c in (RF_COV, 1.0):
        for d in (1.0, 2.0, 3.0):
            print('INFO %5.0f %5.1f %6.3f | %6.3f | %8.3f %8.3f %8.3f %8.3f' % (hm, d, c, 1 - math.exp(-air_od(hm, d)),
                  dA('today', hm, d, c), dA('mono', hm, d, c), dA('notail', hm, d, c), dA('green', hm, d, c)))
v = dA(arm, 300.0, 1.0, RF_COV)
gate('V visible', v >= 0.10, 'arm %s: added fog share at 1 km, 300 m over the street, cover %.3f: %.3f (>= 0.10); today %.3f'
     % (arm, RF_COV, v, dA('today', 300.0, 1.0, RF_COV)))

# ---- D: the red's words ----------------------------------------------------------------------------------------------
want = {"''": (0, 0), "'1'": (0, 1), "'old'": (0, 1), "'mono'": (1, 0), "'today'": (3, 0), "'mono,today'": (3, 0),
        "'x,mono'": (1, 0), "'monoo'": (0, 0), "'today2'": (0, 0)}
bad = [w for w in D['words'] if want.get(w[0]) != (int(w[1]), int(w[2]))]
gate('D words', not bad and len(D['words']) == len(want), '%d words, %d wrong %s' % (len(D['words']), len(bad), bad))

# ---- W: the wiring ---------------------------------------------------------------------------------------------------
if red == 'wiring':
    src = subprocess.run(['git', 'show', base + ':src/gl/cellvolair.cpp'], cwd=TREE, capture_output=True, text=True).stdout
else:
    src = open(os.path.join(TREE, 'src', 'gl', 'cellvolair.cpp'), encoding='utf-8').read()
need = ['RF::defaultMpSigmaPerKm()', 'RF::mpRateFactor(', 'RF::redWords(', 'RF::RedNoTail', 'mono ? RF::defaultSigmaPerKm()']
miss = [n for n in need if n not in src]
gate('W wiring', not miss, '%s: %d of %d hooks%s' % ('base ' + base if red == 'wiring' else 'tree', len(need) - len(miss),
                                                     len(need), (' missing ' + ', '.join(miss)) if miss else ''))
ok = all(results)
print('RAINFOGSCALE-CHECK %s (%d of %d)' % ('PASS' if ok else 'FAIL', sum(results), len(results)))
sys.exit(0 if ok else 1)
