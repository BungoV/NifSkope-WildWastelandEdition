"""Lane CLOUD6g (CLOUDBAND1's leftover row): offline gates of the volume clouds' resolve under camera motion and partial
cloud changes, through the real driver, no NifSkope. The live view is played frame by frame through the REAL
cloud_march*.prog + cloud_resolve*.prog by tests/spells/cloudband_check.py's helpers (imported, none of its gates run).

    python -I tests/spells/cloudstale_check.py <scratch dir> [--red staleread|looseclamp] [--only SRTC]

Arms: today       = cloud_march.prog + cloud_resolve.prog, the uniforms before CLOUD6g (the Off path, CLOUDBAND1's base)
      accum       = the accum twins, the uniforms before CLOUD6g (the default before CLOUD6g)
      default     = the programs cloudvol.cpp draws for this env with the uniforms it sets (cloudband_dump's lines):
                    Accumulation + Move Clamp (cvMoveClamp) + Change Clamp (cvAccReset / cvAccResetSd = cloudvolmath.h
                    kChangeReset / kChangeResetSd)
      default_off = the Off programs with this env's Move Clamp (the old path of Accumulation, as WW_CLOUDVOL_ACCUM=0 draws)
Reds: staleread  (WW_CLOUDVOL_MOVECLAMP=0: the reprojected pixels read the history unclamped, the old read)
      looseclamp (WW_CLOUDVOL_CHANGECLAMP=0: CLOUDBAND1's variance clip alone)

Case = CLOUDBAND1's gate G: Medium (and Ultra) at 1280 x 720, a camera turning 0.4 deg and moving 0.5 m a frame (or still
at the last frame's view), the field jumps at frame 40: clearing (Low 0.6 -> 0, Mid 0.3 -> 0), thinning (Low -> 0.05),
shift (Low -> 0.35 + a 3 km wind step). Error = mean |E| in display levels vs the 64-jitter reference of the NEW field
over the px the jump changed by > 8 levels, 16 frames after the jump (one refresh of every history pixel).

Gates (bars written at 00:02 10-31 after a baseline of today / accum only, BEFORE any variant ran; notes/fix1030c/cloud6g/
tmp/bars.txt):
  S stale     moving clearing: default_off <= 2.0 levels AND default <= 2.0 (a still camera reads 0.00; today 28.90,
              accum 28.97); default_off no worse than today in moving and still thinning + shift. Red staleread fails.
  T clamp     default <= today + 1.0 level in thinning + shift, moving AND still. Red looseclamp fails (accum: still
              thinning 12.68 vs today 3.09).
  R regress   no field change, the same paths: mean |E| on cloud px (reference opacity > 0.15) vs the 64-jitter
              reference: default <= accum + 0.3 levels, default_off <= today + 0.3, moving and still.
  C cost      INFO: GPU ms of 20 resolves at Ultra's 1080p history (960 x 540), the programs with and without the uniforms.
Amendment (after the variant runs, before this checker's first run): T and R run at Ultra too (Medium alone was the
pre-registered tier; Ultra's still shift is the tightest case: 0.025 failed it, the shipped 0.0125 holds). Bars unchanged.
CLOUDBAND1's gates L M G Q F on the new default: tests/cloudband/run.sh (cloudband_check's default arms draw the new
uniforms). Exit 0 only when every gate run passes.
"""
import os, runpy, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(HERE, '..', '..', '..', 'cloudstale_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('SRTC')
os.makedirs(scratch, exist_ok=True)
sys.argv = [os.path.join(HERE, 'cloudband_check.py'), scratch, '--only', '-'] + (['--red', red] if red else [])
B = runpy.run_path(os.path.join(HERE, 'cloudband_check.py'), run_name='cloudstale_helpers')
np, K, ctypes = B['np'], B['K'], B['ctypes']
B['ARMS']['default_off'] = B['ARMS']['today']
live, reference, display, exposure = B['live'], B['reference'], B['display'], B['exposure']
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s%s' % (name, 'PASS' if ok else 'FAIL', msg, ' (red %s)' % red if red else ''), flush=True)


JUMP, AFTER = 40, 16
CASES = (('clearing', 0.0, (0.0, 0.0, 0.0)), ('thinning', 0.05, (0.0, 0.0, 0.0)), ('shift', 0.35, (3.0, 0.0, 0.0)))


def mpath(fr):
    return K.camera(20 + 0.4 * fr, 18), (0.0005 * fr, 0.0, 0.05)


def spath(fr):
    return mpath(JUMP + AFTER - 1)


PATHS = (('moving', mpath), ('still', spath))
ARMS = ('today', 'accum', 'default', 'default_off')


def ghost(tier):
    """{(path, case): {arm: mean |E|}}"""
    out = {}
    for pname, path in PATHS:
        cam, pos = path(JUMP + AFTER - 1)
        old = reference('today', tier, cam, pos, n=16)
        expo = exposure(old)
        for case, cov1, wind1 in CASES:
            def field(fr):
                return (0.6, (0.0, 0.0, 0.0)) if fr < JUMP else (cov1, wind1)
            new = reference('today', tier, cam, pos, n=64, cov=cov1, wind=wind1)
            changed = np.abs(display(new, expo) - display(old, expo)) > 8.0
            e = {}
            for a in ARMS:
                L = live(a, tier, JUMP + AFTER, path, field)
                e[a] = float(np.abs(display(L, expo) - display(new, expo))[changed].mean())
            out[(pname, case)] = e
            print('CASE         INFO  %s %s %s: %d px changed; %s' % (tier, pname, case, int(changed.sum()),
                                                                 ', '.join('%s %.2f' % (a, e[a]) for a in ARMS)), flush=True)
    return out


G = {}
if only & set('ST'):
    G['medium'] = ghost('medium')
if 'T' in only:
    G['ultra'] = ghost('ultra')

if 'S' in only:
    g = G['medium']
    c = g[('moving', 'clearing')]
    ok = c['default_off'] <= 2.0 and c['default'] <= 2.0
    worse = [(p, k, g[(p, k)]['default_off'], g[(p, k)]['today']) for p in ('moving', 'still') for k in ('thinning', 'shift')
             if g[(p, k)]['default_off'] > g[(p, k)]['today']]
    gate('S stale', ok and not worse, 'moving clearing 16 frames after: Off path %.2f, default %.2f levels (bar <= 2.0; '
         'today %.2f, accum %.2f; still camera %.2f); Off path worse than today in %s'
         % (c['default_off'], c['default'], c['today'], c['accum'], g[('still', 'clearing')]['today'],
            ', '.join('%s %s %.2f > %.2f' % w for w in worse) if worse else 'no thinning / shift case'))

if 'T' in only:
    for tier in ('medium', 'ultra'):
        g = G[tier]
        rows, ok = [], True
        for p in ('moving', 'still'):
            for k in ('thinning', 'shift'):
                e = g[(p, k)]
                ok &= e['default'] <= e['today'] + 1.0
                rows.append('%s %s %.2f vs today %.2f (accum %.2f)' % (p, k, e['default'], e['today'], e['accum']))
        gate('T clamp', ok, '%s: default <= today + 1.0 level: %s' % (tier, '; '.join(rows)))

if 'R' in only:
    for tier in ('medium', 'ultra'):
        rows, ok = [], True
        for pname, path in PATHS:
            cam, pos = path(JUMP + AFTER - 1)
            ref = reference('today', tier, cam, pos, n=64)
            expo = exposure(ref)
            m = (1.0 - ref[..., 3]) > 0.15
            e = {}
            for a in ARMS:
                L = live(a, tier, JUMP + AFTER, path)
                e[a] = float(np.abs(display(L, expo) - display(ref, expo))[m].mean())
            ok &= e['default'] <= e['accum'] + 0.3 and e['default_off'] <= e['today'] + 0.3
            rows.append('%s default %.2f vs accum %.2f, Off %.2f vs today %.2f' % (pname, e['default'], e['accum'],
                                                                                 e['default_off'], e['today']))
        gate('R regress', ok, '%s, no change, mean |E| on cloud px (bar + 0.3): %s' % (tier, '; '.join(rows)))

if 'C' in only:
    S, g = K.S, K.g
    begq = S.fn('glBeginQuery', None, ctypes.c_uint, ctypes.c_uint)
    endq = S.fn('glEndQuery', None, ctypes.c_uint)
    genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(ctypes.c_uint))
    getq = S.fn('glGetQueryObjectui64v', None, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint64))
    finish = K.gl.glFinish
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
    for name, extra in (('cloud_resolve.prog', {'cvMoveClamp': (0.0,)}), ('cloud_resolve.prog', {'cvMoveClamp': (1.0,)}),
                        ('cloud_resolve_a.prog', {'cvAccAlpha': (0.25,), 'cvAccClamp': (1.0,), 'cvMoveClamp': (0.0,),
                                                  'cvAccReset': (0.0,)}),
                        ('cloud_resolve_a.prog', {'cvAccAlpha': (0.25,), 'cvAccClamp': (1.0,), 'cvMoveClamp': (1.0,),
                                                  'cvAccReset': (B['DEF_RESET'],), 'cvAccResetSd': (B['DEF_RESETSD'],)})):
        ru = {'cvHistSize': (float(hw), float(hh)), 'cvStride': (4.0,), 'cvPhase': (1.0, 2.0), 'cvCam': cam1,
              'cvProj': proj, 'cvCamKm': pos1, 'cvHistOn': (1.0,), 'cvReproj': (1.0,), 'cvPrevCam': cam0,
              'cvPrevProj': proj, 'cvPrevCamKm': pos0}
        ru.update(extra)
        B['run'](B['P'][name], hist[0], ru, texs)
        finish()
        ns = []
        for rep in range(5):
            begq(0x88BF, q)
            for _ in range(20):
                K.gl.glDrawArrays(5, 0, 4)
            endq(0x88BF)
            t = ctypes.c_uint64()
            getq(q, 0x8866, ctypes.byref(t))
            ns.append(t.value / 20.0)
        rows.append('%s %s %.4f ms' % (name, 'new' if extra['cvMoveClamp'][0] else 'old', min(ns) * 1e-6))
    print('C cost       INFO  one resolve at 960 x 540 (Ultra 1080p history), this machine\'s GPU offline, best of 5 x 20: '
          + '; '.join(rows), flush=True)

print('CLOUDSTALE-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
