"""Lane CLOUD6h (CLOUD6g's leftover rows): offline gates of the volume clouds' resolve with History Age (the depth target's
second channel = frames since a trace's last march; an old trace that disagrees with this frame's march takes the march)
and Lit Clamp (Change Clamp compares the light too), through the real driver, no NifSkope. The live view is played frame
by frame through the REAL cloud_march*.prog + cloud_resolve*.prog by tests/spells/cloudband_check.py's helpers (imported,
none of its gates run).

    python -I tests/spells/cloudage_check.py <scratch dir> [--red noage|litblind] [--only ALRC]

Arms: today   = cloud_march.prog + cloud_resolve.prog, the Off path before CLOUD6g (CLOUDBAND1's base)
      before  = the accum twins with CLOUD6g's uniforms (Move Clamp + Change Clamp) and cvHistAge = cvLitReset = 0: the
                default before CLOUD6h
      default = the programs cloudvol.cpp draws for this env with every uniform it sets (cloudband_dump's lines)
      the same three on CLOUD5's Variety sky (Medium)
Reds: noage (WW_CLOUDVOL_HISTAGE=0: cvHistAge 0), litblind (WW_CLOUDVOL_LITCLAMP=0: cvLitReset 0); default == before.

Cases = CLOUDBAND1's gate G (Medium and Ultra at 1280 x 720, a camera turning 0.4 deg and moving 0.5 m a frame, or still
at the last frame's view; the field jumps at frame 40): thinning (Low 0.6 -> 0.05), shift (Low -> 0.35 + a 3 km wind
step); and a LIGHT change at frame 40 with the field kept (the sun's elevation 30 -> 10 deg and 30 -> 60 deg: the march's
key and sun direction). Error = mean |E| in display levels vs the 64-jitter reference of the new state over the px the jump
changed by > 8 levels, 16 frames after the jump.

Bars (written 10-31 after the variant runs in notes/fix1031a/cloud7/tmp/diag_*.log chose the constants; the brief's
target, the still-camera level 3.14 for the moving thinning, was measured out of reach: an arm that shows this frame's
bilinear march on EVERY reprojected px reads 7.86 there (7.63 read where the march samples sit), the floor for a trace
with no fresh march since the change, so the bar asks for a real drop instead):
  A age       moving thinning + shift, Medium + Ultra, and the Variety sky's moving shift: default <= before - 0.3 levels;
              still thinning + shift: default <= before + 0.05 (a still camera never makes an old trace). Red noage FAILS.
              INFO: the Variety sky's thinning is a full clearing there (0.04 both).
  L lit       the sun changes, moving AND still, Medium + Ultra + Variety Medium: default <= today + 1.0 level (CLOUD6g's T
              bar, now for the light; CLOUD6g's default read 6.59 vs today 3.88 with a still camera). Red litblind FAILS.
  R regress   no change: mean |E| on cloud px (reference opacity > 0.15) vs the 64-jitter reference, moving and still,
              Medium + Ultra + Variety Medium: default <= before + 0.3 levels.
  C cost      INFO: GPU ms of 20 resolves at Ultra's 1080p history (960 x 540), before vs default uniforms.
CLOUDBAND1's gates L M G Q F and CLOUD6g's S T R on the new default: tests/cloudband/run.sh, tests/cloudstale/run.sh.
Exit 0 only when every gate run passes.
"""
import os, runpy, sys

HERE = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(HERE, '..', '..', '..', 'cloudage_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ALRC')
os.makedirs(scratch, exist_ok=True)
sys.argv = [os.path.join(HERE, 'cloudband_check.py'), scratch, '--only', '-'] + (['--red', red] if red else [])
B = runpy.run_path(os.path.join(HERE, 'cloudband_check.py'), run_name='cloudage_helpers')
GL = B['live'].__globals__   # the helpers' own globals (run_path hands back a copy)
np, K, ctypes = B['np'], B['K'], B['ctypes']
GL['ARMS']['before'] = GL['ARMS']['default']
GL['ARMS']['before_v'] = GL['ARMS']['default_v']
_new_uni = GL['new_uni']


def new_uni(arm, accum):
    u = _new_uni('default' if arm.startswith('before') else arm, accum)
    if arm.startswith('before') and accum:   # CLOUD6g's resolve: History Age + Lit Clamp off, set explicitly
        u['cvHistAge'] = (0.0,)
        u['cvLitReset'] = (0.0,)
    return u


GL['new_uni'] = new_uni
SUN = {'el': None}   # SUN['el'](frame) = the march's sun elevation (None = the checkers' 30 deg)
_march_uni = GL['march_uni']


def march_uni(tier, cam, pos, hw, hh, stride, ph, frame, *a, **kw):
    u, calT = _march_uni(tier, cam, pos, hw, hh, stride, ph, frame, *a, **kw)
    if SUN['el'] is not None:
        sw = K.R.sundir(30, SUN['el'](frame))
        u['cvSunW'] = sw
        u['cvKeyW'] = sw
    return u, calT


GL['march_uni'] = march_uni
live, reference, display, exposure = GL['live'], GL['reference'], GL['display'], GL['exposure']
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s%s' % (name, 'PASS' if ok else 'FAIL', msg, ' (red %s)' % red if red else ''), flush=True)


JUMP, AFTER = 40, 16
FIELD = (('thinning', 0.05, (0.0, 0.0, 0.0), None, None), ('shift', 0.35, (3.0, 0.0, 0.0), None, None))
LIGHT = (('sun30to10', 0.6, (0.0, 0.0, 0.0), 30.0, 10.0), ('sun30to60', 0.6, (0.0, 0.0, 0.0), 30.0, 60.0))
SETS = (('medium', False), ('ultra', False), ('medium', True))


def mpath(fr):
    return K.camera(20 + 0.4 * fr, 18), (0.0005 * fr, 0.0, 0.05)


def spath(fr):
    return mpath(JUMP + AFTER - 1)


PATHS = (('moving', mpath), ('still', spath))


def an(a, var):
    return a + ('_v' if var else '')


def cases(tier, var, which):
    """{(path, case): {arm: mean |E|}}"""
    ref = an('today', var)
    out = {}
    for pname, path in PATHS:
        cam, pos = path(JUMP + AFTER - 1)
        for case, cov1, wind1, el0, el1 in which:
            SUN['el'] = None if el0 is None else (lambda f, e=el0: e)
            old = reference(ref, tier, cam, pos, n=16)
            expo = exposure(old)
            SUN['el'] = None if el1 is None else (lambda f, e=el1: e)
            new = reference(ref, tier, cam, pos, n=64, cov=cov1, wind=wind1)
            changed = np.abs(display(new, expo) - display(old, expo)) > 8.0

            def field(fr):
                return (0.6, (0.0, 0.0, 0.0)) if fr < JUMP else (cov1, wind1)
            SUN['el'] = None if el0 is None else (lambda f, a0=el0, a1=el1: a0 if f < JUMP else a1)
            e = {}
            for a in ('today', 'before', 'default'):
                L = live(an(a, var), tier, JUMP + AFTER, path, field)
                e[a] = float(np.abs(display(L, expo) - display(new, expo))[changed].mean())
            SUN['el'] = None
            out[(pname, case)] = e
            print('CASE         INFO  %s%s %s %s: %d px changed; %s' % (tier, ' variety' if var else '', pname, case,
                  int(changed.sum()), ', '.join('%s %.2f' % (a, e[a]) for a in e)), flush=True)
    return out


if 'A' in only:
    for tier, var in SETS:
        g = cases(tier, var, FIELD)
        rows, ok = [], True
        for (p, k), e in sorted(g.items()):
            if var and k == 'thinning':
                rows.append('%s %s %.2f vs before %.2f (INFO: a clearing there)' % (p, k, e['default'], e['before']))
                continue
            if var and p == 'still':
                want = e['before'] + 0.05
            else:
                want = e['before'] - 0.3 if p == 'moving' else e['before'] + 0.05
            ok &= e['default'] <= want
            rows.append('%s %s %.2f vs before %.2f (bar %.2f)' % (p, k, e['default'], e['before'], want))
        gate('A age', ok, '%s%s: %s' % (tier, ' variety' if var else '', '; '.join(rows)))

if 'L' in only:
    for tier, var in SETS:
        g = cases(tier, var, LIGHT)
        rows, ok = [], True
        for (p, k), e in sorted(g.items()):
            ok &= e['default'] <= e['today'] + 1.0
            rows.append('%s %s %.2f vs today %.2f (before %.2f)' % (p, k, e['default'], e['today'], e['before']))
        gate('L lit', ok, '%s%s, default <= today + 1.0: %s' % (tier, ' variety' if var else '', '; '.join(rows)))

if 'R' in only:
    for tier, var in SETS:
        rows, ok = [], True
        for pname, path in PATHS:
            cam, pos = path(JUMP + AFTER - 1)
            ref = reference(an('today', var), tier, cam, pos, n=64)
            expo = exposure(ref)
            m = (1.0 - ref[..., 3]) > 0.15
            e = {}
            for a in ('before', 'default'):
                L = live(an(a, var), tier, JUMP + AFTER, path)
                e[a] = float(np.abs(display(L, expo) - display(ref, expo))[m].mean())
            ok &= e['default'] <= e['before'] + 0.3
            rows.append('%s default %.2f vs before %.2f' % (pname, e['default'], e['before']))
        gate('R regress', ok, '%s%s, no change, mean |E| on cloud px (bar + 0.3): %s' % (
            tier, ' variety' if var else '', '; '.join(rows)))

if 'C' in only:
    S = K.S
    begq = S.fn('glBeginQuery', None, ctypes.c_uint, ctypes.c_uint)
    endq = S.fn('glEndQuery', None, ctypes.c_uint)
    genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(ctypes.c_uint))
    getq = S.fn('glGetQueryObjectui64v', None, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint64))
    hw, hh = 960, 540
    march, hist = K.Mrt(hw // 4, hh // 4), [K.Mrt(hw, hh), K.Mrt(hw, hh)]
    q = ctypes.c_uint()
    genq(1, ctypes.byref(q))
    cam0, pos0 = mpath(0)
    cam1, pos1 = mpath(1)
    proj = (1.0 / np.tan(np.radians(30)) * hh / hw, 1.0 / np.tan(np.radians(30)), 0.0, 0.0)
    texs = [('cvMarch', 0x0DE1, march.col), ('cvMarchDepth', 0x0DE1, march.dep), ('cvPrev', 0x0DE1, hist[1].col),
            ('cvPrevDepth', 0x0DE1, hist[1].dep)]
    rows = []
    for name in ('before', 'default'):
        ru = {'cvHistSize': (float(hw), float(hh)), 'cvStride': (4.0,), 'cvPhase': (1.0, 2.0), 'cvCam': cam1,
              'cvProj': proj, 'cvCamKm': pos1, 'cvHistOn': (1.0,), 'cvReproj': (1.0,), 'cvPrevCam': cam0,
              'cvPrevProj': proj, 'cvPrevCamKm': pos0, 'cvAccAlpha': (0.25,), 'cvAccClamp': (1.0,)}
        ru.update(new_uni(name, True))
        GL['run'](GL['P']['cloud_resolve_a.prog'], hist[0], ru, texs)
        K.gl.glFinish()
        ns = []
        for rep in range(5):
            begq(0x88BF, q)
            for _ in range(20):
                K.gl.glDrawArrays(5, 0, 4)
            endq(0x88BF)
            t = ctypes.c_uint64()
            getq(q, 0x8866, ctypes.byref(t))
            ns.append(t.value / 20.0)
        rows.append('%s %.4f ms' % (name, min(ns) * 1e-6))
    print('C cost       INFO  one accum resolve at 960 x 540 (Ultra 1080p history), this machine\'s GPU offline, best of '
          '5 x 20 (an empty history: the old-trace test runs where the age says so): ' + '; '.join(rows), flush=True)

print('CLOUDAGE-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
