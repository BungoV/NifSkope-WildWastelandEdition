"""Lane CLOUD6d: one cloud profile set per weather (WW_CLOUDVOL_PROFILESET, the default) instead of CLOUD5's type swing
(towers beside puffs), gated offline through the real driver, no NifSkope. The density is res/shaders/cloud_common.glsl's
WW_CLOUD_VARIETY cloudDensity (CLOUD5's probe tests/cloudvar/cloudvar_probe.frag, reused through cloudvar_check, none of
its gates run); each weather class's type and spread come from the REAL rule (src/gl/cloudmath.h lowTypeOf, through
tests/cloudprofile/cloudprofile_dump.cpp), so the gate judges what cloudmap.cpp hands the march.

    python -I tests/spells/cloudprofile_check.py <scratch dir> [--red typemix] [--only PCVWD] [--src <dir>]

Weather classes (notes/fix1030c/cloud6d census_profiles.py over the 107 vanilla WTHR records, MEASURED 10-30): the DATA
class byte picks the type; Low coverage = the class mean of the census rung over the weathers with Low cloud, the Low
band's own depth = the class mean of the role heights (cloudmath.h bandsOf):
    Pleasant (flags 1, 38 with Low cloud)  cov 0.16 depth 1.33 km   type 0.60 +- 0.20 today
    Rainy    (flags 4, 12)                 cov 0.53 depth 1.50 km   type 0.90 +- 0.25 today (the thunder byte is > 0 in
                                                                     every vanilla weather: every rain is a storm)
    Cloudy   (flags 2, 7)                  cov 0.64 depth 1.50 km   type 0.45 +- 0.20 today
    Snow     (flags 8, 8; the radstorms)   cov 0.49 depth 1.50 km   type 0.30 +- 0.15 today
Each class: a uniform map at its coverage, the weather noise's coverage swing on (CLOUD5's 0.9), Mid band off, the whole
384 km wrap of the weather noise at 0.5 km (768 x 768 columns), each column marched over the Low band (96 steps).

Gates (bars written before the first run, lane CLOUD6d 2026-10-30; a bar changed later says so here):
  P profile set   the cloud tops of the substantial columns (column opacity > 0.5; top = the highest point of density
                  > 0.02, over the band base): p90 / p10 <= 1.67 in EVERY class. 1.67 = the smallest depth step between
                  two neighbouring families (cumulus / stratocumulus = 1.0 / 0.6, cloudvolmath.h typeDepthKm): a sky whose
                  tall clouds stand more than one family step over its short ones mixes families. The red typemix (today's
                  +- spread) must FAIL P in every class whose census local-type depth ratio is over 1.67 (Pleasant 3.42,
                  Rainy 3.94, Cloudy 1.73), else the metric does not separate and the gate FAILS. Snow (1.58): INFO.
  C coverage      the mean column opacity == the map coverage, |d| <= 0.06 (CLOUD5's gate A bar for the swing arm).
  V variety kept  the std of the 12 km tiles' mean opacity (big masses beside small puffs, CLOUD5's V2 measure) >= 0.8 x
                  the red's in every class: the profile set must keep the masses the coverage swing makes.
  W wiring        the source: cloudmap.cpp hands lowTypeOf the switch (wwCloudVolProfileSet()), cloudvol.cpp reads
                  WW_CLOUDVOL_PROFILESET through defaultOnSwitch inside cs() (live: re-read on any WW_CLOUDVOL* set) and
                  the red word typemix. Red: the base tree's sources (run.sh: git show 0e7c2c3a) FAIL.
  D default       the switch rule: unset / empty / 1 / on -> on, 0 / off / false / no -> off (defaultOnSwitch).
  INFO            the share of substantial columns whose top is over 1.67 x the median top (towers among the rest), the
                  Low band's top (cloudmap.cpp raises it to the tallest type: the march's span), the cloudy share.
Exit 0 only when every gate run passes.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudprofile_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('PCVWD')
src = args[args.index('--src') + 1] if '--src' in args else os.path.join(TREE, 'src', 'gl')
os.makedirs(scratch, exist_ok=True)
results = []

ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
DUMP = os.path.join(scratch, 'cloudprofile_dump.exe')
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(TREE, 'tests', 'cloudprofile', 'cloudprofile_dump.cpp'), '-o', DUMP], env=ENV,
                   capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])


def dump(*a):
    return subprocess.run([DUMP] + [str(x) for x in a], env=ENV, capture_output=True, text=True,
                          check=True).stdout.split()


def switch_on(value):
    return dump('switch', *([] if value is None else [value]))[1] == 'on'


def class_type(flags, thunder, one_set):
    w = dump('type', flags, thunder, 1 if one_set else 0)
    return float(w[1]), float(w[3])


# the arm cloudvol.cpp draws for this env: the switch (default on) unless WW_CLOUDVOL_RED holds typemix
env_v = os.environ.get('WW_CLOUDVOL_PROFILESET')
env_red = 'typemix' in [w.strip() for w in os.environ.get('WW_CLOUDVOL_RED', '').lower().split(',')]
ONE_SET = switch_on(None if env_v is None else env_v.strip().lower()) and not env_red and red != 'typemix'

# ---- D: the switch rule ----------------------------------------------------------------------------------------------
if 'D' in only:
    want = {None: True, '': True, '1': True, 'on': True, 'yes': True, '0': False, 'off': False, 'false': False,
            'no': False}
    bad = [k for k, v in want.items() if switch_on(k) != v]
    results.append(not bad)
    print('D default    %s  unset/empty/1/on/yes -> on, 0/off/false/no -> off%s' % (
        'PASS' if not bad else 'FAIL', (' (wrong: %s)' % bad) if bad else ''))

# ---- W: the wiring in the sources ------------------------------------------------------------------------------------
if 'W' in only:
    cm = open(os.path.join(src, 'cloudmap.cpp'), encoding='latin1').read()
    cv = open(os.path.join(src, 'cloudvol.cpp'), encoding='latin1').read()
    w1 = re.search(r'lowTypeOf\([^;]*wwCloudVolProfileSet\(\)\s*\)\s*;', cm) is not None
    m = re.search(r'CvState & cs\(\)\s*\{(.*?)\n\}', cv, re.S)
    w2 = bool(m) and re.search(r'"WW_CLOUDVOL_PROFILESET"', m.group(1)) is not None and 'defaultOnSwitch' in m.group(1)
    w3 = re.search(r'bool wwCloudVolProfileSet\(\)[^}]*red\( "typemix" \)', cv, re.S) is not None
    ok = w1 and w2 and w3
    results.append(ok)
    print('W wiring     %s  cloudmap lowTypeOf(.., wwCloudVolProfileSet()) %s; cs() reads WW_CLOUDVOL_PROFILESET via '
          'defaultOnSwitch %s; wwCloudVolProfileSet() honours RED=typemix %s  (src %s)' % (
              'PASS' if ok else 'FAIL', w1, w2, w3, src))

if not (only & set('PCV')):
    print('CLOUDPROFILE-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
    sys.exit(0 if all(results) else 1)

# ---- the GPU arms --------------------------------------------------------------------------------------------------------
sys.argv = [sys.argv[0], os.path.join(scratch, 'cv'), '--only', '-']   # cloudvar_check: helpers + programs, no gates
sys.path.insert(0, HERE)
import cloudvar_check as CV   # noqa: E402
import numpy as np            # noqa: E402

CLASSES = (   # name, flags, thunder (the census's commonest byte), Low coverage, Low depth km, census local-type ratio
    ('Pleasant', 1, 137, 0.16, 1.33, 3.42),
    ('Rainy', 4, 250, 0.53, 1.50, 3.94),
    ('Cloudy', 2, 137, 0.64, 1.50, 1.73),
    ('Snow', 8, 133, 0.49, 1.50, 1.58),
)
SPAN, NPX, NZ = 384.0, 768, 96
PX = SPAN / NPX


def field(T, sp, cov, depth, zr):
    s = CV.sky(T, sp, cov=cov, depth=depth, curl=CV.CURL)
    img = CV.probe(s, 1, (0.0, 0.0, 0.0), (PX, 0.0, 0.0), (0.0, PX, 0.0), NPX, NPX, zr=zr, nz=NZ, edge=0.02)
    return s, img[..., 0], img[..., 1]


def stats(s, op, top):
    cl = op > 0.5
    tt = top[cl & (top > 0)] - s['base0']
    if tt.size < 64:
        return dict(ratio=float('inf'), p10=0, p50=0, p90=0, towers=0, mean=float(op.mean()), tile=0, cloudy=float(cl.mean()), n=tt.size)
    p10, p50, p90 = np.percentile(tt, [10, 50, 90])
    t = int(round(12.0 / PX))
    n = op.shape[0] // t
    tiles = op[:n * t, :n * t].reshape(n, t, n, t).mean(axis=(1, 3))
    return dict(ratio=float(p90 / max(p10, 1e-6)), p10=float(p10), p50=float(p50), p90=float(p90),
                towers=float((tt > 1.67 * p50).mean()), mean=float(op.mean()), tile=float(tiles.std()),
                cloudy=float(cl.mean()), n=int(tt.size))


P_ok, C_ok, V_ok, sep_ok = True, True, True, True
lines = []
for name, flags, thunder, cov, depth, census_ratio in CLASSES:
    Tg, sg = class_type(flags, thunder, ONE_SET)
    Tr, sr = class_type(flags, thunder, False)
    zr = (1.5, 1.5 + CV.type_depth(min(1.0, Tr + sr), depth) + 0.05)
    s_g, op_g, top_g = field(Tg, sg, cov, depth, zr)
    s_r, op_r, top_r = field(Tr, sr, cov, depth, zr)
    G, R = stats(s_g, op_g, top_g), stats(s_r, op_r, top_r)
    p = G['ratio'] <= 1.67
    c = abs(G['mean'] - cov) <= 0.06
    v = G['tile'] >= 0.8 * R['tile']
    red_fails = R['ratio'] > 1.67
    must_sep = census_ratio > 1.67
    P_ok &= p
    C_ok &= c
    V_ok &= v
    if must_sep and not red_fails:
        sep_ok = False
    lines.append('%-8s arm type %.3f+-%.3f: tops p10/p50/p90 %.2f/%.2f/%.2f km ratio %.2f %s | cov %.3f vs map %.2f (d %+.3f) '
                 '%s | tile std %.4f vs red %.4f %s | towers %.3f cloudy %.3f n %d lowtop %.2f km\n'
                 '         red typemix %.3f+-%.3f: tops %.2f/%.2f/%.2f km ratio %.2f %s | cov %.3f (d %+.3f) | towers %.3f '
                 'cloudy %.3f lowtop %.2f km%s'
                 % (name, Tg, sg, G['p10'], G['p50'], G['p90'], G['ratio'], 'pass' if p else 'FAIL', G['mean'], cov,
                    G['mean'] - cov, 'pass' if c else 'FAIL', G['tile'], R['tile'], 'pass' if v else 'FAIL',
                    G['towers'], G['cloudy'], G['n'], s_g['top0'] - s_g['base0'], Tr, sr, R['p10'], R['p50'],
                    R['p90'], R['ratio'], 'fails P' if red_fails else 'PASSES P', R['mean'], R['mean'] - cov,
                    R['towers'], R['cloudy'], s_r['top0'] - s_r['base0'],
                    '' if must_sep else ' (census ratio %.2f <= 1.67: separation INFO)' % census_ratio))

arm = 'profile set' if ONE_SET else 'typemix (the old mix)'
print('arm: %s (WW_CLOUDVOL_PROFILESET=%s, RED typemix %s)' % (arm, env_v, env_red or red == 'typemix'))
for l in lines:
    print(l)
if 'P' in only:
    ok = P_ok and sep_ok
    results.append(ok)
    print('P profile    %s  every class p90/p10 <= 1.67 %s; the red typemix fails P where the census asks %s' % (
        'PASS' if ok else 'FAIL', P_ok, sep_ok))
if 'C' in only:
    results.append(C_ok)
    print('C coverage   %s  |mean opacity - map| <= 0.06 every class' % ('PASS' if C_ok else 'FAIL'))
if 'V' in only:
    results.append(V_ok)
    print('V variety    %s  12 km tile std >= 0.8 x the red every class' % ('PASS' if V_ok else 'FAIL'))
print('CLOUDPROFILE-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
