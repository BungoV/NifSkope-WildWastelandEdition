"""Lane CLOUDSTEPS1 (CLOUD6c's leftover row: Medium / High at more steps): offline gates of the march's step count
(src/gl/cloudvolmath.h marchSteps, through a dump) through the real driver, no NifSkope. CLOUD6c's helpers
(tests/spells/cloudedge_check.py: the CV_COUNT read counters, the 16x-steps reference, the display levels) are imported.

    python -I tests/spells/cloudsteps_check.py <scratch dir> [--red oldsteps] [--only ERW]

Arms: main    = CLOUD2C's taps (cvTopOn 1) at the preset's steps (Medium 48, High 64: cloudvol.cpp preset())
      default = what cloudvol.cpp draws for this env: the read-free top column (cvTopOn 2, CLOUDLIGHT1) at the dump's steps
The rule (bars written 10-31 after notes/fix1031a/cloud7/tmp/steps1.log measured 48 / 60 / 72 / 96 Medium and
64 / 80 / 96 / 128 High, before this checker's first run): the most steps whose reads stay at main's cost.
  R reads    density reads per ray default <= main in every view (up18, low5: CLOUD6c's ground views; above: 4 km up, pitch
             -15 deg), Medium and High: the step raise costs no more reads than main. (GPU ms INFO.)
  E error    the single march's edge error (display levels vs the 16x-steps reference) default <= 0.8 x main and the
             interior error <= main, up18 and low5. The red oldsteps (the dump's preset steps) FAILS (ratio ~1.0).
  W wiring   cloudvol.cpp hands cvSteps V::marchSteps( tier, preset, d.moreSteps ), moreSteps needs the column and honours
             RED=oldsteps, cs() reads WW_CLOUDVOL_MORESTEPS through defaultOnSwitch. Red: the base sources (563dcf97).
Exit 0 only when every gate run passes.
"""
import ctypes, os, re, runpy, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudsteps_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ERW')
src = args[args.index('--src') + 1] if '--src' in args else os.path.join(TREE, 'src', 'gl')
os.makedirs(scratch, exist_ok=True)
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s%s' % (name, 'PASS' if ok else 'FAIL', msg, ' (red %s)' % red if red else ''), flush=True)


if 'W' in only:
    cv = open(os.path.join(src, 'cloudvol.cpp'), encoding='latin1').read()
    w1 = re.search(r'p->uni1i\( "cvSteps", V::marchSteps\( d\.tier, pr\.steps, d\.moreSteps \) \);', cv) is not None
    w2 = re.search(r'd\.moreSteps = s\.moreSteps != 0 && d\.topColumn && !red\( "oldsteps" \);', cv) is not None
    m = re.search(r'CvState & cs\(\)\s*\{(.*?)\n\}', cv, re.S)
    w3 = bool(m) and '"WW_CLOUDVOL_MORESTEPS"' in m.group(1) and 's.moreSteps = V::defaultOnSwitch' in m.group(1)
    gate('W wiring', w1 and w2 and w3, 'cvSteps via marchSteps %s; moreSteps needs the column + RED=oldsteps %s; cs() reads '
         'WW_CLOUDVOL_MORESTEPS %s (src %s)' % (w1, w2, w3, src))
if not (only & set('ER')):
    print('CLOUDSTEPS-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
    sys.exit(0 if all(results) else 1)

ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
DUMP = os.path.join(scratch, 'cloudsteps_dump.exe')
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(TREE, 'tests', 'cloudsteps', 'cloudsteps_dump.cpp'), '-o', DUMP], env=ENV,
                   capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])
DENV = dict(ENV, WW_CLOUDVOL_MORESTEPS='0') if red == 'oldsteps' else ENV
PRESET = {'medium': (2, 48), 'high': (3, 64)}
STEPS = {}
for tier, (ti, ps) in PRESET.items():
    w = subprocess.run([DUMP, str(ti), str(ps)], env=DENV, capture_output=True, text=True, check=True).stdout.split()
    STEPS[tier] = int(w[1])
print('DEFAULT      INFO  steps: medium %d (preset 48), high %d (preset 64)' % (STEPS['medium'], STEPS['high']), flush=True)

sys.argv = [os.path.join(HERE, 'cloudedge_check.py'), scratch, '--only', 'Z']
E = runpy.run_path(os.path.join(HERE, 'cloudedge_check.py'), run_name='cloudsteps_helpers')
G = E['errors'].__globals__
np = G['np']
G['TIERS']['high'] = (4, 64, 6, 1.2, 80.0)   # cloudvol.cpp preset(3)
G['VIEWS']['above'] = -15.0
POS = {'above': (0.0, 0.0, 4.0)}
TOP = {'v': 1.0}


def march(pr, tier, view, steps, prm, slice_, tgt):
    TIERS, SW, SH, K, B = G['TIERS'], G['SW'], G['SH'], G['K'], G['B']
    div = TIERS[tier][0]
    hw, hh = (SW + div - 1) // div, (SH + div - 1) // div
    cam, pos = K.camera(0, G['VIEWS'][view]), POS.get(view, (0.0, 0.0, 0.05))
    u, calT = B['march_uni'](tier, cam, pos, hw, hh, 1, (0.0, 0.0), slice_, slice_, False, True)
    u.update(G['refine_uni'](steps, prm))
    u['cvAmbDark'] = (1.0,)
    u['cvTopOn'] = (TOP['v'],)
    G['run'](pr, tgt, u, G['mtexs'](True), calT)
    return tgt


G['march'] = march
OLD = G['OLD']
res = {}
for top, arm in ((1.0, 'main'), (2.0, 'default')):
    TOP['v'] = top
    G['EMEMO'].clear()
    G['REFS'].clear()
    for tier in PRESET:
        st = PRESET[tier][1] if arm == 'main' else STEPS[tier]
        for view in ('up18', 'low5', 'above'):
            e = G['errors'](tier, view, st, OLD) if view != 'above' else (0.0, 0.0, 0.0)
            c = G['counts'](tier, view, st, OLD)[0]
            res[(arm, tier, view)] = (e, float(c[2]), st)

if 'R' in only:
    rows, ok = [], True
    for tier in PRESET:
        for view in ('up18', 'low5', 'above'):
            rd, rm = res[('default', tier, view)][1], res[('main', tier, view)][1]
            ok &= rd <= rm
            rows.append('%s %s %.1f @%d vs %.1f @%d' % (tier, view, rd, res[('default', tier, view)][2], rm,
                                                        res[('main', tier, view)][2]))
    gate('R reads', ok, 'reads per ray default <= main: ' + '; '.join(rows))
if 'E' in only:
    for tier in PRESET:
        rows, ok = [], True
        for view in ('up18', 'low5'):
            ed, em = res[('default', tier, view)][0], res[('main', tier, view)][0]
            ok &= ed[0] <= 0.8 * em[0] and ed[2] <= em[2]
            rows.append('%s edge %.2f vs %.2f (%.3f, bar 0.8) interior %.2f vs %.2f' % (view, ed[0], em[0],
                                                                                    ed[0] / max(em[0], 1e-9), ed[2], em[2]))
        gate('E error', ok, '%s %d vs %d steps: %s' % (tier, STEPS[tier], PRESET[tier][1], '; '.join(rows)))

print('CLOUDSTEPS-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
