"""Lane CLOUDLIGHT1 (CLOUD6c's leftover row: a cheaper lit sample): offline gates of the volume clouds' top light without
reads, through the real driver, no NifSkope. CLOUD6c's helpers (tests/spells/cloudedge_check.py: the overlay with the
CV_COUNT read counters, the 16x-steps reference, the display levels) are imported, none of their gates run.

    python -I tests/spells/cloudlight_check.py <scratch dir> [--red toptaps] [--only ENG]

A lit sample read 10 densities on Medium (3 key taps + CLOUD2C's 3 taps straight down + CLOUDBAND1's 3 taps up + the
sample) and 16 on Ultra (6 + 6 + 3 + 1); RDR2 lights with one sun-ward extinction sample (Bauer 2019 slide 33). Variants
measured (notes/fix1031a/cloud7/tmp/light1.log, tools/mklight.py): one tap down (gate R 0.634 / 0.705 vs MC: FAIL, the
red notop's numbers), one tap up (the converged picture +0.9 levels: refused), half the key taps (+2.6 levels: refused),
two taps down (-1 read), and the one shipped: the slab under the sample taken as uniform at the sample's own density down
to its band's base (at most the taps' reach), no read at all (cvTopOn 2, WW_CLOUDVOL_TOPCOLUMN).

Arms: today = cvTopOn 1 (CLOUD2C's taps), default = the shipped arm (cvTopOn 2; the red toptaps makes it the taps).
Views (cloudedge's): up18, low5 (a ground camera), plus "above": 4 km up (over the Low band), pitch -15 deg, the tops the
down taps light. Medium 48 steps and Ultra 96 at 1280 x 720, the Off sky, the sun at 30 deg.
Bars (written 10-31 after the variant runs above, before this checker's first run):
  E error    per tier and view: the converged picture (16x steps, 16 jitter slices) of default vs today's, rms <= 0.5
             display levels and |mean| <= 0.25 over cloud px (reference opacity > 0.03); the single march's error vs
             TODAY's reference <= today's own + 0.1 levels on edge and interior px.
  N reads    density reads per ray (cloudedge's CV_COUNT overlay) default <= 0.85 x today in every tier and view. The red
             toptaps FAILS (ratio 1.000).
  G gpu      INFO: GPU ms of the march at 960 x 540 stride 1 (Ultra taps, low5), today vs default, this machine offline.
The Monte Carlo gates on the default: tests/spells/cloudvol_check.py C (dark core), R (top: 0.957 / 1.253, today 0.996 /
1.275, bar 0.75 .. 1.33), D (flicker); tests/spells/cloudvar_check.py; tests/spells/cloudamb_check.py A (run.sh runs them).
Exit 0 only when every gate run passes.
"""
import os, runpy, sys

HERE = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(HERE, '..', '..', '..', 'cloudlight_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ENG')
os.makedirs(scratch, exist_ok=True)
sys.argv = [os.path.join(HERE, 'cloudedge_check.py'), scratch, '--only', 'Z']
E = runpy.run_path(os.path.join(HERE, 'cloudedge_check.py'), run_name='cloudlight_helpers')
G = E['errors'].__globals__
np = G['np']
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
    u['cvTopOn'] = (TOP['v'],)   # set on every draw: a program keeps its uniforms
    G['run'](pr, tgt, u, G['mtexs'](True), calT)
    return tgt


G['march'] = march
_mu = G['B']['march_uni']


def march_uni_top(*a, **kw):   # gpu_ms draws through cloudband's march_uni: the arm's cvTopOn there too
    u, calT = _mu(*a, **kw)
    u['cvTopOn'] = (TOP['v'],)
    return u, calT


G['B']['march_uni'] = march_uni_top
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s%s' % (name, 'PASS' if ok else 'FAIL', msg, ' (red %s)' % red if red else ''), flush=True)


OLD = G['OLD']
CASES = [(t, v) for t in ('medium', 'ultra') for v in ('up18', 'low5', 'above')]


def arm(top):
    TOP['v'] = top
    G['EMEMO'].clear()
    G['REFS'].clear()
    out = {}
    for tier, view in CASES:
        st = G['TIERS'][tier][1]
        ref, expo, edge = G['reference'](tier, view)
        fs = G['frames'](tier, view, st, OLD)
        c = G['counts'](tier, view, st, OLD)[0]
        out[(tier, view)] = dict(ref=ref, expo=expo, edge=edge, fs=fs, reads=float(c[2]))
    return out


T = arm(1.0)
D = arm(1.0 if red == 'toptaps' else 2.0)
disp = G['display']

if 'E' in only:
    for tier, view in CASES:
        t, d = T[(tier, view)], D[(tier, view)]
        expo, edge = t['expo'], t['edge']
        cl = (1.0 - t['ref'][..., 3]) > 0.03
        d0 = disp(t['ref'], expo)
        dd = disp(d['ref'], expo) - d0
        rms, mean = float(np.sqrt(np.mean(dd[cl] ** 2))), float(np.mean(dd[cl]))

        def err(fs, m):
            return float(np.sqrt(np.mean([np.mean((disp(f, expo) - d0)[m] ** 2) for f in fs])))
        et, ed = err(t['fs'], edge), err(d['fs'], edge)
        it, id_ = err(t['fs'], cl & ~edge), err(d['fs'], cl & ~edge)
        ok = rms <= 0.5 and abs(mean) <= 0.25 and ed <= et + 0.1 and id_ <= it + 0.1
        gate('E error', ok, '%s %s: converged default vs today rms %.2f mean %+.2f levels (bars 0.5, 0.25); single march '
             'vs today\'s reference: edge %.2f vs %.2f, interior %.2f vs %.2f (bar + 0.1)' % (tier, view, rms, mean, ed, et,
                                                                                            id_, it))
if 'N' in only:
    rows, ok = [], True
    for tier, view in CASES:
        rt, rd = T[(tier, view)]['reads'], D[(tier, view)]['reads']
        ok &= rd <= 0.85 * rt
        rows.append('%s %s %.1f vs %.1f (%.3f)' % (tier, view, rd, rt, rd / max(rt, 1e-9)))
    gate('N reads', ok, 'density reads per ray default vs today (bar <= 0.85 x): ' + '; '.join(rows))
if 'G' in only:
    ms = []
    for top in (1.0, 2.0):
        TOP['v'] = top
        ms.append((G['gpu_ms'](48, OLD), G['gpu_ms'](96, OLD)))
    print('G gpu        INFO  march 960 x 540 stride 1, Ultra taps, low5, this machine offline: taps %.2f / %.2f ms, column '
          '%.2f / %.2f ms (48 / 96 steps)'
          % (ms[0][0], ms[0][1], ms[1][0], ms[1][1]), flush=True)

print('CLOUDLIGHT-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
