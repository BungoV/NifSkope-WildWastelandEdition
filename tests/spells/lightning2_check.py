"""Lane LIGHTNING2 (CLOUD6f leftovers LIGHTNINGFREQ1 + LIGHTNINGSKY1): offline gates, no NifSkope. The CPU side
src/gl/cloudlightning.h through tests/lightning2/lightning2_dump.cpp (g++), the real res/shaders/lookdev_clouds.frag in a
hidden GL context (this machine's driver), CLOUD2's helpers from cloudvol_check (imported, none of its gates run).

    python -I tests/spells/lightning2_check.py <scratch dir> [--red oldfreq|today] [--only QWOGKU]

Gates (bars written before the first run, LIGHTNING2 2026-10-31; a bar changed later says so here):
  Q law      the DATA frequency law (freqSchedule) over 400000 s, two seeds: the measured mean gap between strikes within
             0.93 .. 1.07 x 2 s x 255 / (255 - byte) for the bytes 133, 137, 250 (the vanilla storms' values); strictly
             longer gaps for higher bytes; byte 255 = no strike; share 0.5 at byte 133 = twice the gap (0.93 .. 1.07);
             every flash <= 1 s and inside its slot; gain 1 (full brightness). Red oldfreq (CLOUD6f's schedule) fails.
  W weathers the 107 vanilla weathers (tests/lightning2/weathers.tsv, from the masters): every storm weather (thunder
             SNAM) strikes; the radstorms (byte 133) every <= 6 s, the rains (byte 250) every >= 60 s; INFO the table.
             Red oldfreq (every storm every 10.7 s) fails.
  O old      the red is today's code: tests/cloudlightning's CLOUD6f dump built on the base's cloudlightning.h and on
             the tree's prints the same timing lines for two seeds, and the old wrappers == the Schedule forms with
             oldSchedule bit for bit (lightning2_dump oldsame: 0 differ).
  G ground   the flash on the ground (groundDalc, the DALC add): the six faces within 3 % of the checker's own numpy
             integral (a finer, wider grid of its own transcription of fieldAt) on every face > 1e-3 x the largest; the
             up-facing normal (travel Z-, index 5) the largest; the down-facing normal 0; the face toward the strike >
             the face away; a camera above the deck = all 0. Red today (no ground term) fails.
             AMENDED after the first run (no number moved): "up face the largest" was written for any strike, but with
             the strike 3 km off (the lit base 27 deg up) the face toward it is the larger one (0.0041 vs 0.0024), as
             the geometry says; the up-face bar now applies to a strike overhead (a second case), the 3 km case keeps
             toward > away and up > 0.
  K layers   lookdev_clouds.frag (the real file) on a storm view, the strike 4 km ahead: ldLtOn 0 and unset (a fresh
             program) == the base's frag bit for bit; a flash changes > 0 px and adds >= 0; GPU added light == the CPU
             twin (tex x fieldAt x colour / norm) on >= 200 px, relative |d| <= 2e-3. Red today (the base frag) fails.
  U under    straight under the strike, looking up: the added light / the layer texel's linear albedo == kFlashUnder
             (0.5, a white sunlit layer x 0.5) within 0.95 .. 1.05. Red today fails.
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'lightning2_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('QWOGKU')
BASE = os.environ.get('LIGHTNING2_BASE', '5422be6c')   # the batch head before LIGHTNING2
os.makedirs(scratch, exist_ok=True)
GPP = 'C:/msys64/ucrt64/bin/g++.exe'
ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


def build(src, exe, inc):
    r = subprocess.run([GPP, '-O2', '-std=c++17', '-I', inc, src, '-o', exe], env=ENV, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('BUILD FAIL %s %s' % (src, r.stderr[-2000:]))
    return exe


def run(exe, *a, inp=None):
    return subprocess.run([exe] + [str(x) for x in a], env=ENV, input=inp, capture_output=True, text=True, check=True,
                          timeout=300).stdout.split('\n')


DUMP = build(os.path.join(TREE, 'tests', 'lightning2', 'lightning2_dump.cpp'), os.path.join(scratch, 'lightning2_dump.exe'),
             os.path.join(TREE, 'src'))


def kv(line):
    p = line.split()
    return {p[i]: p[i + 1] for i in range(1, len(p) - 1, 2)}


def sched(law, byte, share, seed, t1):
    return kv([l for l in run(DUMP, 'sched', law, byte, share, seed, t1) if l.startswith('sched ')][0])


LAW = 0 if red == 'oldfreq' else 1


def want_gap(byte, share=1.0):
    return 2.0 * 255.0 / ((255.0 - byte) * share)


# ---- Q: the DATA frequency law ------------------------------------------------------------------------------------
if 'Q' in only:
    ok, rows = True, []
    gaps = {}
    for seed in (12345, 0x001CCE1F):
        for byte in (133, 137, 250):
            v = sched(LAW, byte, 1.0, seed, 400000)
            gp = float(v['meangap'])
            gaps[(seed, byte)] = gp
            r = gp / want_gap(byte)
            ok &= 0.93 <= r <= 1.07 and float(v['maxlen']) <= 1.0 and int(v['cross']) == 0 and float(v['gain']) == 1.0
            rows.append('seed %d byte %d: %s strikes gap %.2f s (want %.2f, ratio %.3f) longest %.3f s cross %s gain %s'
                        % (seed, byte, v['strikes'], gp, want_gap(byte), r, float(v['maxlen']), v['cross'], v['gain']))
        ok &= gaps[(seed, 133)] < gaps[(seed, 137)] < gaps[(seed, 250)]
        v = sched(LAW, 255, 1.0, seed, 400000)
        ok &= int(v['strikes']) == 0
        rows.append('seed %d byte 255: %s strikes' % (seed, v['strikes']))
        v = sched(LAW, 133, 0.5, seed, 400000)
        r = float(v['meangap']) / want_gap(133, 0.5)
        ok &= 0.93 <= r <= 1.07
        rows.append('seed %d byte 133 share 0.5: gap %.2f s (want %.2f, ratio %.3f)' % (seed, float(v['meangap']),
                                                                                       want_gap(133, 0.5), r))
    gate('Q law', ok, '; '.join(rows) + ' (bars 0.93 .. 1.07, flash <= 1 s, cross 0, gain 1, ordered, 255 = none)%s'
         % (' (red %s)' % red if red else ''))

# ---- W: the vanilla weathers --------------------------------------------------------------------------------------
if 'W' in only:
    rows = [l.rstrip('\n').split('\t') for l in open(os.path.join(TREE, 'tests', 'lightning2', 'weathers.tsv'))
            if not l.startswith('#')]
    h = {k: i for i, k in enumerate(rows[0])}
    ws = rows[1:]
    ok = len(ws) == 107
    table = []
    for w in ws:
        if w[h['storm']] != '1':
            continue
        byte = int(w[h['freq']])
        v = sched(LAW, byte, 1.0, 0x001CCE1F, 100000)
        every = float(v['meangap'])
        ok &= int(v['strikes']) > 0
        if byte == 133:
            ok &= every <= 6.0
        if byte == 250:
            ok &= every >= 60.0
        table.append('%s %d -> %.1f s' % (w[h['edid']], byte, every))
    nstorm = len(table)
    ok &= nstorm == 15
    gate('W weathers', ok, '%d weathers, %d storm: %s (bars radstorm <= 6 s, rain >= 60 s, every storm strikes)%s'
         % (len(ws), nstorm, '; '.join(table), ' (red %s)' % red if red else ''))

# ---- O: the red is today's code -----------------------------------------------------------------------------------
if 'O' in only:
    inc_old = os.path.join(scratch, 'base_src', 'gl')
    os.makedirs(inc_old, exist_ok=True)
    for fn in ('cloudlightning.h', 'cloudvolmath.h'):
        b = subprocess.run(['git', '-C', TREE, 'show', '%s:src/gl/%s' % (BASE, fn)], capture_output=True, check=True).stdout
        open(os.path.join(inc_old, fn), 'wb').write(b)
    cpp = os.path.join(TREE, 'tests', 'cloudlightning', 'cloudlightning_dump.cpp')
    e_old = build(cpp, os.path.join(scratch, 'cl_dump_base.exe'), os.path.join(scratch, 'base_src'))
    e_new = build(cpp, os.path.join(scratch, 'cl_dump_tree.exe'), os.path.join(TREE, 'src'))
    same, rows = True, []
    for seed in (12345, 0x001CCE1F):
        a = run(e_old, 'timing', seed, 0, 7200, 0.001)
        b = run(e_new, 'timing', seed, 0, 7200, 0.001)
        same &= a == b and len(a) > 3
        rows.append('seed %d: %d lines %s' % (seed, len(a), 'same' if a == b else 'DIFFER'))
    for seed in (12345, 0x001CCE1F):
        v = kv([l for l in run(DUMP, 'oldsame', seed, 20000) if l.startswith('oldsame ')][0])
        same &= int(v['differ']) == 0
        rows.append('oldsame seed %d: %s checked, %s differ' % (seed, v['checked'], v['differ']))
    gate('O old', same, '; '.join(rows))

# ---- G: the ground --------------------------------------------------------------------------------------------------
SIGMA, W_ALB = 40.0, 0.99
BASE_KM, TOP_KM = 1.5, 3.0


def mean_cosine():
    # V::msMeanCosine(dropletParams(kDropletUm)) through cloudvol_check's dump (its 'msg' line); none of its gates run
    sys.path.insert(0, HERE)
    sys.argv = [sys.argv[0], scratch, '--only', '-']
    import cloudvol_check as K   # noqa: E402
    return K


def fieldc(out):
    v = [float(x) for x in [l for l in out if l.startswith('fieldc ')][0].split()[1:]]
    return {'src': v[0:3], 'invDelta': v[3], 'r0sq': v[4], 'mirLo': v[5], 'mirHi': v[6], 'flux': v[7], 'norm': v[8],
            'power': v[9]}


def field_np(fc, p, d):
    """the checker's own transcription of cloudlightning.h fieldAt (float64), p and d (n, 3)"""
    import numpy as np
    zs = [fc['src'][2], fc['mirLo'] - fc['src'][2], fc['mirHi'] - fc['src'][2]]
    phi = np.zeros(len(p))
    gd = np.zeros(len(p))
    for i, z in enumerate(zs):
        a = p - np.array([fc['src'][0], fc['src'][1], z])
        r = np.sqrt((a * a).sum(1) + fc['r0sq'])
        e = np.exp(-r * fc['invDelta']) / r
        s = 1.0 if i == 0 else -1.0
        phi += s * e
        gd += s * (-e * (fc['invDelta'] + 1.0 / r) / r) * (a * d).sum(1)
    return fc['norm'] * np.maximum(phi + fc['flux'] * gd, 0.0)


if 'G' in only or 'K' in only or 'U' in only:
    K = mean_cosine()
    import numpy as np
    MSG = K.MSG

def ground_case(sx):
    src = (sx, 0.0, BASE_KM + 0.35 * (TOP_KM - BASE_KM))
    gp = (0.0, 0.0, 0.002)
    out = run(DUMP, 'ground', SIGMA, W_ALB, MSG, BASE_KM, TOP_KM, *src, *gp)
    fc = fieldc(out)
    gl_ = [l for l in out if l.startswith('ground ')][0].split()
    e = [float(x) for x in gl_[1:7]]
    deck = float(gl_[8])
    if red == 'today':
        e = [0.0] * 6   # today: the cell's ambient takes no flash
    # the checker's own integral: 600 x 600 over +- 12 km round the strike
    n, half = 600, 12.0
    xs = src[0] - half + (np.arange(n) + 0.5) * (2 * half / n)
    ys = src[1] - half + (np.arange(n) + 0.5) * (2 * half / n)
    X, Y = np.meshgrid(xs, ys)
    P = np.stack([X.ravel(), Y.ravel(), np.full(X.size, deck)], 1)
    V = P - np.array(gp)
    r2 = (V * V).sum(1)
    r = np.sqrt(r2)
    Wd = V / r[:, None]
    under = field_np(fc, np.array([[src[0], src[1], deck]]), np.array([[0.0, 0.0, 1.0]]))[0]
    L = field_np(fc, P, Wd) / under
    k = L * (V[:, 2] / r) * (2 * half / n) ** 2 / r2
    nrm = np.array([[-1, 0, 0], [1, 0, 0], [0, -1, 0], [0, 1, 0], [0, 0, -1], [0, 0, 1.0]])
    mine = [float((k * np.maximum(Wd @ nrm[a], 0)).sum()) * 0.5 / math.pi for a in range(6)]
    mx = max(mine)
    rel = [abs(e[a] - mine[a]) / mine[a] for a in range(6) if mine[a] > 1e-3 * mx]
    ok = bool(rel) and max(rel) <= 0.03 and e[4] == 0.0 and e[5] > 0.0
    msg = 'strike %.0f km off: dalc %s vs mine %s, worst rel %.4f; down face %.3g' % (
        sx, ' '.join('%.5f' % x for x in e), ' '.join('%.5f' % x for x in mine), max(rel) if rel else float('nan'), e[4])
    if sx > 0:
        ok &= e[1] > e[0]
        msg += '; toward %.5f > away %.5f' % (e[1], e[0])
    else:
        ok &= e[5] == max(e)
        msg += '; up face %.5f the largest' % e[5]
    return ok, msg, src


if 'G' in only:
    ok1, m1, src = ground_case(3.0)
    ok2, m2, _ = ground_case(0.0)
    above = run(DUMP, 'ground', SIGMA, W_ALB, MSG, BASE_KM, TOP_KM, *src, 0.0, 0.0, 2.0)
    ea = [float(x) for x in [l for l in above if l.startswith('ground ')][0].split()[1:7]]
    ok3 = all(x == 0.0 for x in ea)
    gate('G ground', ok1 and ok2 and ok3, '%s; %s; camera above the deck: %s (bar rel 0.03)%s' % (
        m1, m2, ' '.join('%.3g' % x for x in ea), ' (red %s)' % red if red else ''))

# ---- K / U: the 2D layers -------------------------------------------------------------------------------------------
if 'K' in only or 'U' in only:
    C, S, g, gl = K.C, K.S, K.g, K.gl
    SDIR = os.path.join(TREE, 'res', 'shaders')
    VERT = os.path.join(TREE, 'tests', 'lightning2', 'layer_quad.vert')
    f_linkst = S.fn('glGetProgramiv', None, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_int))

    def linked(p, name):
        okv = ctypes.c_int()
        f_linkst(p, 0x8B82, ctypes.byref(okv))
        if not okv.value:
            raise SystemExit('LINK FAIL ' + name)
        return p

    BDIR = os.path.join(scratch, 'base_shaders')
    os.makedirs(BDIR, exist_ok=True)
    for fn in ('lookdev_clouds.frag', 'lookdev_output.glsl', 'uniforms.glsl'):
        b = subprocess.run(['git', '-C', TREE, 'show', '%s:res/shaders/%s' % (BASE, fn)], capture_output=True, check=True).stdout
        open(os.path.join(BDIR, fn), 'wb').write(b)
    P_NEW = linked(C.build(SDIR, None, [VERT, os.path.join(SDIR, 'lookdev_clouds.frag')]), 'lookdev_clouds.frag')
    P_BASE = linked(C.build(SDIR, None, [VERT, os.path.join(BDIR, 'lookdev_clouds.frag')]), 'base lookdev_clouds.frag')
    P_USE = P_BASE if red == 'today' else P_NEW
    TEXV = 0.8
    TEX = K.tex2d(np.full((4, 4, 4), [TEXV, TEXV, TEXV, 1.0]), linear=False)
    f_um3 = K.f_um3

    def setmat3(prog, name, m):
        l = g.loc(prog, name.encode())
        if l >= 0:
            a = np.ascontiguousarray(np.asarray(m, np.float32))
            f_um3(l, 1, 0, a.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))

    def layer(prog, W, H, cam, proj, extra):
        tgt = K.Mrt(W, H)
        u = {'texBlend': (0.0,), 'colLin': (0.3, 0.3, 0.3), 'alpha': (1.0,), 'skyScale': (1.0,), 'probe': 0,
             'sceneExposure': (1.0,), 'viewTransform': 3, 'tProj': proj}
        u.update(extra)
        g.bindfb(0x8D40, tgt.fb)
        gl.glViewport(0, 0, W, H)
        g.use(prog)
        K.setuni(prog, u)
        setmat3(prog, 'tCam', cam)
        K.bind(prog, [('Tex', 0x0DE1, TEX), ('Tex2', 0x0DE1, TEX)])
        gl.glDrawArrays(5, 0, 4)
        return tgt.read()

    def view(yaw, pitch, W, H, vfov=60.0):
        cam = K.camera(yaw, pitch)
        f = 1.0 / math.tan(math.radians(vfov) / 2)
        return cam, (f * H / W, f, 0.0, 0.0)

    def dirs_of(cam, proj, W, H):
        ys, xs = np.mgrid[0:H, 0:W]
        q = np.stack([(xs + 0.5) / W * 2 - 1, (ys + 0.5) / H * 2 - 1], -1)
        v = np.stack([(q[..., 0] + proj[2]) / proj[0], (q[..., 1] + proj[3]) / proj[1], -np.ones(q.shape[:2])], -1)
        M = np.asarray(cam).reshape(3, 3).T
        d = v @ M.T
        return d / np.linalg.norm(d, axis=-1, keepdims=True)

    TEXL = ((TEXV + 0.055) / 1.055) ** 2.4   # the frag's srgbToLinear of the texel

    def flash_uni(fc, camKm, deck, on=1):
        col = fc['norm'] * fc['power']   # level 1, white, unit sunK = pi x skyScale (the weather's own colours)
        return {'ldLtOn': on, 'ldLtSrc': (fc['src'][0], fc['src'][1], fc['src'][2], fc['invDelta']),
                'ldLtPrm': (fc['r0sq'], fc['mirLo'], fc['mirHi'], fc['flux']), 'ldLtCol': (col, col, col),
                'ldLtCam': (camKm[0], camKm[1], camKm[2], deck)}, col

    camKm = (0.0, 0.0, 0.002)
    deck = BASE_KM + 1.0 / SIGMA

if 'K' in only:
    W, H = 160, 90
    cam, proj = view(0, 25, W, H)
    src = (4.0, 0.0, BASE_KM + 0.35 * (TOP_KM - BASE_KM))
    fc = fieldc(run(DUMP, 'field', SIGMA, W_ALB, MSG, BASE_KM, TOP_KM, *src, math.pi))
    fu, col = flash_uni(fc, camKm, deck)
    base_o = layer(P_BASE, W, H, cam, proj, {})
    rows, ok = [], True
    for tag in ('unset', 'off'):
        prog = linked(C.build(SDIR, None, [VERT, os.path.join(SDIR, 'lookdev_clouds.frag')]), 'fresh') if tag == 'unset' else P_NEW
        o = layer(prog, W, H, cam, proj, {} if tag == 'unset' else dict(fu, ldLtOn=0))
        dn = int(np.count_nonzero(o != base_o))
        ok &= dn == 0
        rows.append('ldLtOn %s: %d values differ' % (tag, dn))
    on = layer(P_USE, W, H, cam, proj, fu)
    add = (on[..., :3] - base_o[..., :3])[..., 1]
    ctrl = int(np.count_nonzero(np.any(on != base_o, axis=2)))
    ok &= ctrl > 0 and float(add.min()) >= 0.0
    rows.append('flash: %d px change, min added %.3g' % (ctrl, float(add.min())))
    D = dirs_of(cam, proj, W, H)
    pick = [(y, x) for y in range(1, H, 3) for x in range(1, W, 3) if D[y, x, 2] > 0.05]
    pts = []
    for y, x in pick:
        d = D[y, x]
        t = (deck - camKm[2]) / d[2]
        pts.append((y, x, (camKm[0] + d[0] * t, camKm[1] + d[1] * t, deck), d))
    inp = ''.join('%.9g %.9g %.9g %.9g %.9g %.9g\n' % (*p, *d) for _, _, p, d in pts)
    fv = [float(l.split()[1]) for l in run(DUMP, 'field', SIGMA, W_ALB, MSG, BASE_KM, TOP_KM, *src, math.pi, inp=inp)
          if l.startswith('f ')]
    want = np.array([TEXL * f * col / fc['norm'] for f in fv])
    got = np.array([add[y, x] for y, x, _, _ in pts])
    keep = want > 1e-3 * want.max() if want.size and want.max() > 0 else np.zeros(0, bool)
    worst = float((np.abs(got[keep] - want[keep]) / want[keep]).max()) if keep.any() else 1.0
    ok &= int(keep.sum()) >= 200 and worst <= 2e-3
    rows.append('twin on %d px worst rel %.2g (bar 2e-3)' % (int(keep.sum()), worst))
    gate('K layers', ok, '; '.join(rows) + '%s' % (' (red %s)' % red if red else ''))

if 'U' in only:
    W, H = 33, 33
    cam, proj = view(0, 89.9, W, H, vfov=2.0)
    src = (0.0, 0.0, BASE_KM + 0.35 * (TOP_KM - BASE_KM))
    fc = fieldc(run(DUMP, 'field', SIGMA, W_ALB, MSG, BASE_KM, TOP_KM, *src, math.pi))
    fu, col = flash_uni(fc, camKm, deck)
    off = layer(P_BASE, W, H, cam, proj, {})
    on = layer(P_USE, W, H, cam, proj, fu)
    r = float((on[16, 16, 1] - off[16, 16, 1]) / TEXL)
    ok = 0.95 * 0.5 <= r <= 1.05 * 0.5
    gate('U under', ok, 'added / texel albedo straight under the strike %.4f (want kFlashUnder 0.5 x 0.95 .. 1.05)%s'
         % (r, ' (red %s)' % red if red else ''))

print('LIGHTNING2-CHECK %s%s' % ('PASS' if results and all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if results and all(results) else 1)
