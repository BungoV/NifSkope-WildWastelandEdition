"""Lane CLOUD6c (CLOUDBAND1 leftover, RDR2 gap c, bungo 10-30): offline gates of edge refinement in the volume clouds'
march (RDR2's step refinement at cloud edges) through the real driver, no NifSkope. CLOUDBAND1's helpers (and through
them CLOUD5's and CLOUD2's) are imported, none of their gates run.

    python -I tests/spells/cloudedge_check.py <scratch dir> [--red off] [--only XBIQCLR] [--refusal] [--explore]

RESULT (CLOUD6c, 10-30): REFUSED. The refinement fixes the edges at the preset step count (X, L) but loses to plain
extra uniform steps at the same number of density reads (Q, every tier and view), costs 2.1-2.4x the reads (C) and
7.5-7.8x the GPU time of the march, so nothing ships: the refinement lives only in tests/cloudedge/shaders (a test
overlay laid over a scratch copy of res/shaders) and the shipping cloud_march*.prog are unchanged. --refusal makes the
exit code the refusal TEST: 0 iff X, I, L, R pass and Q fails for every candidate (the refinement is right, helps at
its own step count, and still loses at equal cost). A later lane that makes refinement win at equal cost must change
this test on purpose.

RDR2 (Bauer 2019, SIGGRAPH Advances "Creating the Atmospheric World of Red Dead Redemption 2", slide 50 and its note;
text at notes/fogheight1/rdr2/deck.txt lines 636-654): "Each time we hit the base shape of a cloud, we half the step
length and move the sample position back by that distance. We repeat this test with a fixed number of iterations (i_max)
... continue the integration process from there with the reduced step size. We return to the original step length after
leaving the cloud shape again." Slide: i_max = 3. Slide 33: their light is ONE secondary extinction sample toward the sun
(a lit sample costs ~2 density reads); ours reads 3 + 3 + 3 + 1 = 10 (Medium) / 6 + 6 + 3 + 1 = 16 (Ultra): the taps
toward the key light, CLOUD2C's taps down, CLOUDBAND1's 3 taps up, the sample.

The overlay (tests/cloudedge/shaders): cloud_march_refine.glsl replaces the uniform loop while cvRefine > 0: a coarse
sample that finds density after an empty one goes back to that empty sample, halves the gap cvRefine times (density reads
only), then steps 1 / 2^cvRefine of the coarse step through the edge (at least cvRefineSpan fine steps, on while dense when
cvRefineStay = 1); an empty fine sample returns to the coarse step; at most cvRefineCap steps, always with room to reach
the ray's end at the coarse step. cvRefineLight = 1: every fine sample lit (RDR2); > 1: the light's optical depths
(cvTaus) read every cvRefineLight fine steps and interpolated between (a fine sample ~1 density read).

Arms (the default program cloud_march_a.prog, every history pixel marched, stride 1, the blue noise jitter slices 0..15):
  old    = cvRefine 0: the uniform march (== today's shipping program, gate R; the red off makes the candidates this)
  rdr2   = levels 3, cap 2 x steps, span 8, stay 1, light 1 (every fine sample lit: RDR2's scheme)
  cheap  = the same with light 4 (the optical depths interpolated over 4 fine steps)
  ref    = cvRefine 0 at 16 x the preset's steps, the mean of the 16 jitter slices (the integral the march estimates)
Views at 1280 x 720 screen (Medium: 1/4 res history 320 x 180, 48 steps; Ultra: 1/2 res 640 x 360, 96 steps), the
Off sky (Low 1.5..3.5 km at coverage 0.6, Mid 5..6.5 km at 0.3), the sun at 30 deg, CLOUDBAND1's ambient on (the
default): "up18" = CLOUDBAND1's still camera (pitch 18 deg), "low5" = pitch 5 deg (long grazing rays: the coarse step
reaches ~1.2 km). Display levels as CLOUDBAND1 (light x the exposure that puts the reference's median cloud at 0.18, over
a mid sky, sRGB 8-bit levels of the luminance). Edge px = the reference's opacity in 0.03 .. 0.97.

Gates (bars written before the first run, CLOUD6c 10-30; the candidate list and the overlay were added after the first
runs, the bars were not changed):
  X edge      single-march error on edge px, rms over the 16 slices of (march - ref): cand <= 0.75 x old, every tier
              and view. The red off (cand = the old march) FAILS (ratio 1.0).
  B bias      the error left after the 16-slice mean (what the accumulated history converges to) on edge px:
              cand <= old + 0.25 levels.
  I interior  rms single-march error over the non-edge px: cand <= old + 0.5 levels.
  Q equal     equal cost: cand's edge error <= the uniform march's at the step count whose density reads per ray reach
              cand's (steps raised in 1/8 preset steps).
  C cost      mean density reads per ray (march + light taps + bisection, counted by the overlay's
              cloud_march_count.frag) cand <= 1.6 x old; max march samples per ray <= the cap. INFO: GPU ms of the march
              at 960 x 540 stride 1 (this machine, offline) for old, cand and the uniform march at 2 x steps.
  L live      the live history after 80 frames through the real resolve (CLOUDBAND1's still path, stride 4, the Bayer
              order, accumulation): rms error vs the reference on edge px, cand <= 0.9 x old.
  R red       the overlay at cvRefine 0 == the tree's shipping cloud_march / _a / _v, pixel for pixel (max |diff| 0).
"""
import ctypes, os, runpy, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudedge_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('XBIQCLR')
refusal = '--refusal' in args
explore = '--explore' in args
os.makedirs(scratch, exist_ok=True)

sys.argv = [os.path.join(HERE, 'cloudband_check.py'), scratch, '--only', 'Z']
B = runpy.run_path(os.path.join(HERE, 'cloudband_check.py'), run_name='cloudedge_helpers')
np, K, V = B['np'], B['K'], B['V']
S, C, g, gl = K.S, K.C, K.g, K.gl
SDIR = os.path.join(TREE, 'res', 'shaders')
TIERS, SW, SH, BAYER = B['TIERS'], B['SW'], B['SH'], B['BAYER']
display, exposure, run, mtexs, prog = B['display'], B['exposure'], B['run'], B['mtexs'], B['prog']
f_linkst = B['f_linkst']

# ---- the test overlay: a scratch copy of res/shaders with tests/cloudedge/shaders laid over it --------------------------
SX = os.path.join(scratch, 'shaders_edge')
if os.path.isdir(SX):
    shutil.rmtree(SX)
shutil.copytree(SDIR, SX)
OVL = os.path.join(TREE, 'tests', 'cloudedge', 'shaders')
for f in os.listdir(OVL):
    shutil.copyfile(os.path.join(OVL, f), os.path.join(SX, f))


def xprog(name=None, paths=None):
    p = C.build(SX, name, paths)
    ok = ctypes.c_int()
    f_linkst(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL overlay ' + (name or str(paths)))
    return p


PA = xprog('cloud_march_a.prog')
PCOUNT = xprog(None, [os.path.join(SX, 'cell_ao.vert'), os.path.join(SX, 'cloud_march_count.frag')])
results, qres, xres = [], [], []


def gate(name, ok, msg):
    results.append((name, ok))
    print('%-14s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


VIEWS = {'up18': 18.0, 'low5': 5.0}
OLD = (0, 0, 0, 0, 0)


def cands(st):
    """(levels, cap, span, stay, light); the red off turns every candidate into the old march"""
    if red == 'off':
        return {'rdr2': OLD, 'cheap': OLD}
    return {'rdr2': (3, st * 2, 8, 1, 1), 'cheap': (3, st * 2, 8, 1, 4)}


def refine_uni(steps, prm):
    lv, cap, span, stay, light = prm
    return {'cvSteps': steps, 'cvRefine': lv, 'cvRefineCap': cap, 'cvRefineSpan': span, 'cvRefineStay': stay,
            'cvRefineLight': light}


def march(pr, tier, view, steps, prm, slice_, tgt):
    div = TIERS[tier][0]
    hw, hh = (SW + div - 1) // div, (SH + div - 1) // div
    cam, pos = K.camera(0, VIEWS[view]), (0.0, 0.0, 0.05)
    u, calT = B['march_uni'](tier, cam, pos, hw, hh, 1, (0.0, 0.0), slice_, slice_, False, True)
    u.update(refine_uni(steps, prm))
    u['cvAmbDark'] = (1.0,)   # the default since CLOUDBAND2
    run(pr, tgt, u, mtexs(True), calT)
    return tgt


TG = {}


def target(tier):
    if tier not in TG:
        div = TIERS[tier][0]
        TG[tier] = K.Mrt((SW + div - 1) // div, (SH + div - 1) // div)
    return TG[tier]


def frames(tier, view, steps, prm, n=16):
    return [march(PA, tier, view, steps, prm, s, target(tier)).read() for s in range(n)]


REFS = {}


def reference(tier, view):
    if (tier, view) not in REFS:
        acc = 0.0
        for f in frames(tier, view, TIERS[tier][1] * 16, OLD):
            acc = acc + f
        ref = acc / 16.0
        a = 1.0 - ref[..., 3]
        REFS[(tier, view)] = (ref, exposure(ref), (a > 0.03) & (a < 0.97))
    return REFS[(tier, view)]


EMEMO = {}


def errors(tier, view, steps, prm):
    k = (tier, view, steps, prm)
    if k not in EMEMO:
        ref, expo, edge = reference(tier, view)
        fs = frames(tier, view, steps, prm)
        dref = display(ref, expo)
        se = np.mean([np.mean((display(f, expo) - dref)[edge] ** 2) for f in fs])
        si = np.mean([np.mean((display(f, expo) - dref)[~edge] ** 2) for f in fs])
        bias = float(np.sqrt(np.mean((display(sum(fs) / len(fs), expo) - dref)[edge] ** 2)))
        EMEMO[k] = (float(np.sqrt(se)), bias, float(np.sqrt(si)), fs[0])
    return EMEMO[k]


def counts(tier, view, steps, prm):
    """per ray (history pixel, slice 0): march samples, lit samples, density reads, refinements: mean and max"""
    a = march(PCOUNT, tier, view, steps, prm, 0, target(tier)).read(1).reshape(-1, 4)
    return a.mean(axis=0), a.max(axis=0)


def gpu_ms(steps, prm, view='low5', reps=20):
    W, H = 1920 // 2, 1080 // 2   # Ultra's history at 1080p, stride 1 (a harness frame; the live one marches 1/16)
    tgt = K.Mrt(W, H)
    begq = S.fn('glBeginQuery', None, ctypes.c_uint, ctypes.c_uint)
    endq = S.fn('glEndQuery', None, ctypes.c_uint)
    getq = S.fn('glGetQueryObjectui64v', None, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint64))
    genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(ctypes.c_uint))
    q = ctypes.c_uint()
    genq(1, ctypes.byref(q))
    cam, pos = K.camera(0, VIEWS[view]), (0.0, 0.0, 0.05)
    u, calT = B['march_uni']('ultra', cam, pos, W, H, 1, (0.0, 0.0), 0, 0, False, True)
    u.update(refine_uni(steps, prm))
    u['cvAmbDark'] = (1.0,)
    run(PA, tgt, u, mtexs(True), calT)
    gl.glFinish()
    begq(0x88BF, q.value)
    for _ in range(reps):
        gl.glDrawArrays(5, 0, 4)
    endq(0x88BF)
    ns = ctypes.c_uint64()
    getq(q.value, 0x8866, ctypes.byref(ns))
    return ns.value / 1e6 / reps


# ---- explore: a parameter grid, no gate (CLOUDEDGE_GRID="bm,lv,capm,span,stay,light;...", base steps x bm) ---------------
if explore:
    grid = os.environ.get('CLOUDEDGE_GRID', '1,3,2,8,1,1;1,3,2,8,1,4')
    for tier in ('medium', 'ultra'):
        st = TIERS[tier][1]
        for view in VIEWS:
            e0, c0 = errors(tier, view, st, OLD), counts(tier, view, st, OLD)[0]
            print('EXPLORE %s %s old: edge %.2f bias %.2f int %.2f | reads %.1f samp %.1f lit %.1f'
                  % (tier, view, e0[0], e0[1], e0[2], c0[2], c0[0], c0[1]), flush=True)
            for mult in (1.5, 2.0):
                e1, c1 = errors(tier, view, int(st * mult), OLD), counts(tier, view, int(st * mult), OLD)[0]
                print('EXPLORE %s %s old x%.1f: edge %.2f bias %.2f int %.2f | reads %.1f'
                      % (tier, view, mult, e1[0], e1[1], e1[2], c1[2]), flush=True)
            for e in grid.split(';'):
                bm, lv, cm, sp, sy, li = e.split(',')
                b = int(round(st * float(bm)))
                prm = (int(lv), int(round(b * float(cm))), int(sp), int(sy), int(li))
                e1, c1 = errors(tier, view, b, prm), counts(tier, view, b, prm)
                print('EXPLORE %s %s base%d lv%d cap%d span%d stay%d light%d: edge %.2f (%.3f) bias %.2f int %.2f | '
                      'reads %.1f (%.2fx) samp %.1f max %d lit %.1f ref %.2f'
                      % (tier, view, b, prm[0], prm[1], prm[2], prm[3], prm[4], e1[0], e1[0] / e0[0], e1[1], e1[2],
                         c1[0][2], c1[0][2] / c0[2], c1[0][0], int(c1[1][0]), c1[0][1], c1[0][3]), flush=True)
    sys.exit(0)

# ---- X, B, I, Q, C ------------------------------------------------------------------------------------------------------
for tier in ('medium', 'ultra'):
    st = TIERS[tier][1]
    if not (only & set('XBIQC')):
        break
    for view in VIEWS:
        ref, expo, edge = reference(tier, view)
        eo = errors(tier, view, st, OLD)
        co, cm = counts(tier, view, st, OLD)
        for cn, prm in cands(st).items():
            ee = errors(tier, view, st, prm)
            ce, cx = counts(tier, view, st, prm)
            tag = '%s %s %s' % (tier[:3], view, cn)
            info = ' (%d edge px of %d)%s' % (int(edge.sum()), edge.size, ' (red %s)' % red if red else '')
            if 'X' in only:
                gate('X ' + tag, ee[0] <= 0.75 * eo[0], 'single-march edge error %.2f vs old %.2f levels: ratio %.3f '
                     '(bar 0.75)%s' % (ee[0], eo[0], ee[0] / max(eo[0], 1e-9), info))
                xres.append(ee[0] <= 0.75 * eo[0])
            if 'B' in only:
                gate('B ' + tag, ee[1] <= eo[1] + 0.25, 'edge bias after the 16-slice mean %.2f vs old %.2f levels '
                     '(bar old + 0.25)' % (ee[1], eo[1]))
            if 'I' in only:
                gate('I ' + tag, ee[2] <= eo[2] + 0.5, 'non-edge single-march error %.2f vs old %.2f levels (bar old + 0.5)'
                     % (ee[2], eo[2]))
            if 'Q' in only:
                s2, cq = st, co
                while cq[2] < ce[2] and s2 < 4 * st:
                    s2 += max(st // 8, 1)
                    cq = counts(tier, view, s2, OLD)[0]
                eq = errors(tier, view, s2, OLD)
                gate('Q ' + tag, ee[0] <= eq[0], 'equal cost: %.2f levels at %.1f reads/ray vs the uniform march at %d steps '
                     '(%.1f reads/ray) %.2f levels' % (ee[0], ce[2], s2, cq[2], eq[0]))
                qres.append(ee[0] <= eq[0])
            if 'C' in only:
                cap = max(prm[1], st)
                gate('C ' + tag, ce[2] <= 1.6 * co[2] and cx[0] <= cap,
                     'density reads/ray %.1f vs old %.1f (%.2fx, bar 1.6x); march samples/ray mean %.1f max %d (cap %d) vs '
                     'old %.1f max %d; lit samples %.1f vs %.1f; refinements/ray %.2f' %
                     (ce[2], co[2], ce[2] / max(co[2], 1e-9), ce[0], int(cx[0]), cap, co[0], int(cm[0]), ce[1], co[1], ce[3]))
            B['png'](os.path.join(scratch, '%s_%s_%s.png' % (cn, tier, view)), display(ee[3], expo))
            B['png'](os.path.join(scratch, '%serr_%s_%s.png' % (cn, tier, view)), 128 + 8 * (display(ee[3], expo) - display(ref, expo)))
        B['png'](os.path.join(scratch, 'ref_%s_%s.png' % (tier, view)), display(ref, expo))
        B['png'](os.path.join(scratch, 'old_%s_%s.png' % (tier, view)), display(eo[3], expo))
        B['png'](os.path.join(scratch, 'olderr_%s_%s.png' % (tier, view)), 128 + 8 * (display(eo[3], expo) - display(ref, expo)))
    if 'C' in only:
        cs = cands(st)
        print('C %s cost     INFO  GPU march at 960 x 540 stride 1 (this machine, offline, low5): old %.3f ms, rdr2 %.3f ms, '
              'cheap %.3f ms, the uniform march at 2 x steps %.3f ms (the live frame marches 1/16 of these pixels)'
              % (tier[:3], gpu_ms(st, OLD), gpu_ms(st, cs['rdr2']), gpu_ms(st, cs['cheap']), gpu_ms(st * 2, OLD)), flush=True)

# ---- L: the live history through the real resolve (CLOUDBAND1's path) ------------------------------------------------------
if 'L' in only:
    PR = prog('cloud_resolve_a.prog')

    def live(tier, prm, nframes=80):
        st, div = TIERS[tier][1], TIERS[tier][0]
        hw, hh = (SW + div - 1) // div, (SH + div - 1) // div
        mt, hist = K.Mrt((hw + 3) // 4, (hh + 3) // 4), [K.Mrt(hw, hh), K.Mrt(hw, hh)]
        prev, cur = None, 0
        cam, pos = K.camera(0, VIEWS['up18']), (0.0, 0.0, 0.05)
        for fr in range(nframes):
            slot = BAYER[fr % 16]
            ph = (float(slot % 4), float(slot // 4))
            u, calT = B['march_uni'](tier, cam, pos, hw, hh, 4, ph, fr % 64, (fr // 16) % 64, False, True)
            u.update(refine_uni(st, prm))
            u['cvAmbDark'] = (1.0,)
            run(PA, mt, u, mtexs(True), calT)
            ru = {'cvHistSize': (float(hw), float(hh)), 'cvStride': (4.0,), 'cvPhase': ph, 'cvCam': cam, 'cvProj': u['cvProj'],
                  'cvCamKm': pos, 'cvHistOn': (1.0 if prev else 0.0,), 'cvReproj': (1.0,), 'cvAccClamp': (1.0,),
                  'cvAccAlpha': (B['alpha'](fr),)}
            p0 = prev or (cam, u['cvProj'], pos)
            ru.update({'cvPrevCam': p0[0], 'cvPrevProj': p0[1], 'cvPrevCamKm': p0[2]})
            run(PR, hist[cur], ru, [('cvMarch', 0x0DE1, mt.col), ('cvMarchDepth', 0x0DE1, mt.dep),
                                    ('cvPrev', 0x0DE1, hist[1 - cur].col), ('cvPrevDepth', 0x0DE1, hist[1 - cur].dep)])
            prev = (cam, u['cvProj'], pos)
            cur = 1 - cur
        return hist[1 - cur].read()

    for tier in ('medium', 'ultra'):
        st = TIERS[tier][1]
        ref, expo, edge = reference(tier, 'up18')
        dref = display(ref, expo)
        lo_ = live(tier, OLD)
        eo = float(np.sqrt(np.mean((display(lo_, expo) - dref)[edge] ** 2)))
        B['png'](os.path.join(scratch, 'live_old_%s.png' % tier), display(lo_, expo))
        for cn, prm in cands(st).items():
            le_ = live(tier, prm)
            ee = float(np.sqrt(np.mean((display(le_, expo) - dref)[edge] ** 2)))
            gate('L %s %s' % (tier[:3], cn), ee <= 0.9 * eo, 'live history after 80 frames, edge px rms error %.2f vs old '
                 '%.2f levels: ratio %.3f (bar 0.9)%s' % (ee, eo, ee / max(eo, 1e-9), ' (red %s)' % red if red else ''))
            B['png'](os.path.join(scratch, 'live_%s_%s.png' % (cn, tier)), display(le_, expo))

# ---- R: the overlay's old arm is today's shipping march, pixel for pixel ----------------------------------------------------
if 'R' in only:
    worst = []
    for name in ('cloud_march.prog', 'cloud_march_a.prog', 'cloud_march_v.prog'):
        pl, pb = xprog(name), prog(name)
        var = name.endswith('_v.prog')
        div = TIERS['medium'][0]
        hw, hh = (SW + div - 1) // div, (SH + div - 1) // div
        for view in VIEWS:
            outs = []
            for p in (pl, pb):
                cam, pos = K.camera(0, VIEWS[view]), (0.0, 0.0, 0.05)
                u, calT = B['march_uni']('medium', cam, pos, hw, hh, 1, (0.0, 0.0), 3, 3, var, not var)
                u.update(refine_uni(48, OLD))
                u['cvAmbDark'] = (1.0,)
                t = K.Mrt(hw, hh)
                run(p, t, u, mtexs(True), calT)
                outs.append((t.read(0), t.read(1)))
            d = max(float(np.abs(outs[0][0] - outs[1][0]).max()), float(np.abs(outs[0][1] - outs[1][1]).max()))
            worst.append((d, name, view))
    w = max(worst)
    gate('R red', w[0] == 0.0, 'the overlay at cvRefine 0 vs the shipping res/shaders: max |diff| %.3g over 3 programs x 2 '
         'views (worst %s %s)' % (w[0], w[1], w[2]))

holds = all(ok for _, ok in results)
if refusal:
    must = [ok for n, ok in results if n[0] in 'XILR']
    verdict = bool(must) and all(must) and bool(qres) and not any(qres)
    print('CLOUDEDGE-CHECK REFUSAL %s: %s%s' % ('HOLDS' if verdict else 'BROKEN',
          'the refinement helps at its own step count (X, I, L) and is exact when off (R), and the uniform march wins at '
          'equal cost in every case (Q)' if verdict else 'a must-pass gate failed or a candidate won Q (read the lines above)',
          (' (red %s)' % red) if red else ''))
    code = 0 if verdict else 1
else:
    print('CLOUDEDGE-CHECK %s%s' % ('PASS' if holds else 'FAIL', (' (red %s)' % red) if red else ''))
    code = 0 if holds else 1
if __name__ == '__main__':
    sys.exit(code)
