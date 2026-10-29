"""Lane CLOUD5: offline gates of Cloud Variety (cloud types, weather noise, curl-displaced erosion) through the real driver,
no NifSkope. The density is res/shaders/cloud_common.glsl's WW_CLOUD_VARIETY cloudDensity (tests/cloudvar/cloudvar_probe.frag)
and its CPU twin src/gl/cloudvolmath.h varietyDensity (tests/cloudvar/cloudvar_dump.cpp, g++); the noise bytes and CLOUD2's
helpers come from cloudvol_check (imported, none of its gates run).

    python -I tests/spells/cloudvar_check.py <scratch dir> [--red onetype|nocurl|nocal|shortlight] [--only TSAVCE]

Gates (bars written before the first run, lane CLOUD5 2026-10-29; a bar changed later says so here):
  T twin      GPU density == the CPU twin on two fixed point grids (a storm sky across 0..8.5 km and a clear sky at
              x -37, y 212 km: the wraps): weather noise and curl max |d| <= 2e-4; density median |d| <= 0.002, p99
              <= 0.03, points where one side is 0 and the other > 0.01 <= 1 %.
  S sweep     the type sweep: the horizontal-mean density over height at T = 0, 1/3, 2/3, 1 (no swing): tops rise
              strictly with T, stratus top <= 0.8 km over the base, cumulonimbus top >= 5 km over it, and each
              neighbour pair's normalised profiles differ by L1 >= 0.2.
  A coverage  the column opacity from above == the map: stripes 0.1 .. 0.9 at T = 0, 2/3, 1 (Low) and the Mid band, no
              swing: |mean - map| <= 0.05 each (CLOUD2's bar); a uniform map 0.3 / 0.5 / 0.7 with the swing and the
              type spread on, over 192 km: |mean - map| <= 0.06. Red nocal fails.
  V variety   one fixed sky (coverage 0.45, Mid off), storm (type 0.9 +- 0.25) and clear (0.6 +- 0.2), 96 km at 0.15 km:
              V1 the cloud tops' IQR (columns of opacity > 0.5) >= 1.0 km storm, >= 0.5 km clear; V2 the std of the 12 km
              tiles' mean opacity >= 0.06 (big masses beside small puffs); V3 the field's correlation with itself one
              shape period (6 km) along x <= 0.9 (no tiling). TODAY's density (CLOUD2's cloudDensity, the same map, the
              band 1.5..3.0 km) must FAIL each of V1..V3, else the metric does not separate and the gate FAILS.
              V4 wisps: the high-passed density's correlation at one detail period (0.6 km) on a slice inside thick
              cumulus, with the curl / without it, <= 0.75 (4 windows of 6 km at 0.03 km). Red onetype fails V1; red
              nocurl fails V4.
  C towers    CLOUD2B's dark core for a 6 km tower: a slab 1.5..7.5 km of tau 60, sun 60 deg, seen from below with
              cloud_march_v.prog at the light reach cloudvol.cpp gives a cumulonimbus sky (cloudvolmath.h varLightKm):
              the underside within 0.5 .. 1.6 x a Monte Carlo of the same slab (same droplet phase). Red shortlight (the
              preset's 1.2 km reach) fails (a tower base too bright: the column above it unseen).
  E cost      GPU ms of the march at 1080p per preset, today's cloud_march.prog vs cloud_march_v.prog, clear and storm
              skies: INFO; says OVER when Ultra's variety march is above the brief's 1.5 ms.
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudvar_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('TSAVCE')
os.makedirs(scratch, exist_ok=True)

sys.path.insert(0, HERE)
sys.argv = [sys.argv[0], scratch, '--only', '-']   # cloudvol_check: its helpers and dump, none of its gates
import cloudvol_check as K   # noqa: E402
import numpy as np            # noqa: E402

S, C, g, gl = K.S, K.C, K.g, K.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = os.path.join(TREE, 'res', 'shaders')
TOWER, ERODE, CURL, ANVIL, SWING = 7.0, 0.35, 0.15, 1.0, 0.9   # cloudvolmath.h kVarTowerKm, kVarErode, kVarCurlKm, ...
ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
DUMPV = os.path.join(scratch, 'cloudvar_dump.exe')
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(TREE, 'tests', 'cloudvar', 'cloudvar_dump.cpp'), '-o', DUMPV], env=ENV,
                   capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])
f_linkst = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))


def linked(p, name):
    ok = GLint()
    f_linkst(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL ' + name)
    return p


VERT = os.path.join(SDIR, 'cell_ao.vert')
P_NEW = linked(C.build(SDIR, None, [VERT, os.path.join(TREE, 'tests', 'cloudvar', 'cloudvar_probe.frag')]), 'probe')
P_OLD = linked(C.build(SDIR, None, [VERT, os.path.join(TREE, 'tests', 'cloudvar', 'cloudvar_probe_old.frag')]), 'probe_old')
P_MV = linked(C.build(SDIR, 'cloud_march_v.prog'), 'cloud_march_v.prog')
for name in ('cloud_fly_v.prog', 'cloud_shadowmap_v.prog', 'cloud_flybake_v.prog'):   # every twin links here too
    linked(C.build(SDIR, name), name)


def dumpv(*a):
    return subprocess.run([DUMPV] + ['%.9g' % x if isinstance(x, float) else str(x) for x in a], env=ENV,
                          capture_output=True, text=True, check=True).stdout.split('\n')


def parse_cal(out):
    calT, calM = [None] * 7, None   # cloudvolmath.h kVarCal (MERGE1029C: 4 -> 7 anchors)
    for l in out:
        w = l.split()
        if w and w[0] == 'calT':
            calT[int(w[1])] = [float(x) for x in w[2:]]
        elif w and w[0] == 'calM':
            calM = [float(x) for x in w[1:]]
    return calT, calM


def type_depth(T, depth=1.5, tower=TOWER):
    """cloudvolmath.h typeDepthKm, transcribed (gate S checks it against the dump)"""
    u = min(max(T, 0.0), 1.0) * 3.0
    if u < 1.0:
        return depth * (0.3 + 0.3 * u)
    if u < 2.0:
        return depth * (0.6 + 0.4 * (u - 1.0))
    return depth + (max(tower, depth) - depth) * (u - 2.0)


def light_km(tmax, pre, depth=1.5, tower=TOWER):
    """cloudvolmath.h varLightKm, transcribed (gate S checks it against the dump)"""
    return min(max(0.8 * type_depth(tmax, depth, tower), pre), max(pre, 6.0))


def sky(type_, spread, cov=0.45, sheet=0.0, swing=SWING, curl=CURL, base0=1.5, depth=1.5, dens0=1.0,
        base1=5.0, top1=6.5, dens1=0.0, covmid=0.3, nocal=False):
    s = dict(type=type_, spread=spread, cov=cov, sheet=sheet, swing=swing, curl=curl, base0=base0, depth=depth,
             dens0=dens0, base1=base1, top1=top1, dens1=dens1, covmid=covmid)
    s['calT'], s['calM'] = parse_cal(dumpv('cal', base0, dens0, depth, base1, top1, dens1, curl, 1 if nocal else 0))
    s['top0'] = base0 + type_depth(min(1.0, type_ + spread), depth)
    return s


def var_uni(s, rect=(0.0, 0.0, 1.0, 0.0)):
    u = K.bands_uni((s['base0'], s['top0'], s['dens0']), (s['base1'], s['top1'], s['dens1']), [K.cal[0], s['calM']],
                    cov=(s['cov'], s['covmid'], 0.0), rect=rect, sheet=s['sheet'])
    u.update({'cvVarA': (s['type'], s['spread'], s['swing'], s['depth']), 'cvVarB': (TOWER, ERODE, s['curl'], ANVIL)})
    return u


def set_calT(prog, calT):
    l = g.loc(prog, b'cvCalT')
    if l >= 0:
        flat = [v for row in calT for v in row]
        K.f_u1fv(l, len(flat), (ctypes.c_float * len(flat))(*flat))


BLANK = None


def run(prog, w, h, uni, calT=None, maptex=None, extra=None):
    global BLANK
    if BLANK is None:
        BLANK = K.tex2d(np.zeros((1, 1, 4)))
    tgt = K.Mrt(w, h)
    g.bindfb(0x8D40, tgt.fb)
    gl.glViewport(0, 0, w, h)
    g.use(prog)
    K.setuni(prog, uni)
    if calT is not None:
        set_calT(prog, calT)
    sh, de = K.textures()
    K.bind(prog, [('cvShape', 0x806F, sh), ('cvDetail', 0x806F, de), ('cvMap', 0x0DE1, maptex or BLANK)] + (extra or []))
    gl.glDrawArrays(5, 0, 4)
    return tgt.read(0)


def probe(s, mode, O, U, V, w, h, zr=(0.0, 1.0), nz=96, edge=0.02, old=False, rect=(0.0, 0.0, 1.0, 0.0), maptex=None,
          oldcal=None):
    pu = {'prMode': mode, 'prO': O, 'prU': U, 'prV': V, 'prZ': zr, 'prNZ': nz, 'prEdge': (edge,)}
    if old:
        u = K.bands_uni((s['base0'], s['base0'] + s['depth'], s['dens0']), (s['base1'], s['top1'], s['dens1']), oldcal,
                        cov=(s['cov'], s['covmid'], 0.0), rect=rect, sheet=s['sheet'])
        u.update(pu)
        return run(P_OLD, w, h, u, maptex=maptex)
    u = var_uni(s, rect)
    u.update(pu)
    return run(P_NEW, w, h, u, s['calT'], maptex)


results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg))


ONE = red == 'onetype'
NOCURL = red == 'nocurl'
CURL_G = 0.0 if NOCURL else CURL

# ---- T: the twin ---------------------------------------------------------------------------------------------------------
if 'T' in only:
    worst_nc, dens = 0.0, []
    for (ty, sp, O, U, V) in ((0.9, 0.25, (0.3, 3.7, 1.45), (0.061, 0.013, 0.0), (0.017, 0.0, 0.11)),
                              (0.6, 0.2, (-37.3, 211.9, 1.45), (0.047, -0.021, 0.0), (0.0, 0.009, 0.05))):
        nx, ny = 96, 64
        s = sky(ty, sp, cov=0.55, sheet=0.1, dens1=0.6)
        out = dumpv('grid', s['base0'], s['dens0'], s['depth'], s['base1'], s['top1'], s['dens1'], s['curl'], ty, sp,
                    SWING, s['cov'], s['covmid'], s['cov'] * s['sheet'], *O, *U, *V, nx, ny)
        cg = np.array([[float(x) for x in l.split()[2:]] for l in out if l.startswith('g ')])
        cn = np.array([[float(x) for x in l.split()[2:]] for l in out if l.startswith('n ')])
        cc = np.array([[float(x) for x in l.split()[2:]] for l in out if l.startswith('c ')]).reshape(ny, nx, 3)
        gd = probe(s, 0, O, U, V, nx, ny)[..., 0]
        gn = probe(s, 2, O, U, V, nx, ny)
        worst_nc = max(worst_nc, float(np.abs(gn[..., 0] - cn).max()), float(np.abs(gn[..., 1:4] - cc).max()))
        dens.append((gd, cg))
    gd = np.concatenate([a.ravel() for a, b in dens])
    cg = np.concatenate([b.ravel() for a, b in dens])
    dd = np.abs(gd - cg)
    zero = float(np.mean(((gd <= 0) & (cg > 0.01)) | ((cg <= 0) & (gd > 0.01))))
    live = (gd > 0) | (cg > 0)
    med = float(np.median(dd[live])) if live.any() else 0.0
    p99 = float(np.percentile(dd[live], 99)) if live.any() else 0.0
    gate('T twin', worst_nc <= 2e-4 and med <= 0.002 and p99 <= 0.03 and zero <= 0.01,
         'noise + curl worst |d| %.2g (bar 2e-4); density over %d live points: median |d| %.4f (bar 0.002), p99 %.4f '
         '(bar 0.03), zero disagreement %.2f %% (bar 1 %%); GPU mean %.4f, CPU mean %.4f'
         % (worst_nc, int(live.sum()), med, p99, 100 * zero, float(gd.mean()), float(cg.mean())))

# ---- S: the type sweep ---------------------------------------------------------------------------------------------------
if 'S' in only:
    dz, nzs = 0.05, 150
    rows, tops, profs = [], [], []
    lo = dumpv('light')
    depth_d = {float(l.split()[1]): float(l.split()[2]) for l in lo if l.startswith('depth ')}
    twin_d = max(abs(type_depth(t) - v) for t, v in depth_d.items())
    twin_d = max([twin_d] + [abs(light_km(float(w[1]), float(w[4])) - float(w[5])) for w in
                             (l.split() for l in lo if l.startswith('light '))])
    for T in (0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0):
        s = sky(T, 0.0, cov=0.6, swing=0.0, curl=CURL_G)
        s['top0'] = s['base0'] + type_depth(T)
        acc = np.zeros(nzs)
        for y in (0.37, 1.9, 3.3, 4.6):
            img = probe(s, 0, (0.0, y, s['base0']), (6.0 / 128, 0.0, 0.0), (0.0, 0.0, dz), 128, nzs)[..., 0]
            acc += img.mean(axis=1)
        prof = acc / 4.0
        m = float(prof.max())
        above = np.nonzero(prof > 0.02 * m)[0] if m > 0 else np.array([0])
        top = float((above.max() + 1) * dz)
        tops.append(top)
        profs.append(prof / max(prof.sum(), 1e-9))
        rows.append('T %.2f top %.2f km peak %.2f km mean %.3f' % (T, top, float(np.argmax(prof) * dz), float(prof.mean())))
    l1 = [float(np.abs(profs[i] - profs[i + 1]).sum()) for i in range(3)]
    ok = all(tops[i] < tops[i + 1] for i in range(3)) and tops[0] <= 0.8 and tops[3] >= 5.0 and min(l1) >= 0.2 \
        and twin_d <= 1e-4
    gate('S sweep', ok, '; '.join(rows) + ' | neighbour L1 %s (bar 0.2); typeDepthKm twin |d| %.1g'
         % (' '.join('%.2f' % v for v in l1), twin_d))

# ---- A: coverage == the map ----------------------------------------------------------------------------------------------
if 'A' in only:
    covs = [0.1, 0.3, 0.5, 0.7, 0.9]
    strip = np.zeros((1, 160, 4))
    for i, c in enumerate(covs):
        strip[0, i * 32:(i + 1) * 32, 0] = c
        strip[0, i * 32:(i + 1) * 32, 1] = c
    mtex = K.tex2d(strip, linear=True)
    he, W, H = 15.0, 600, 240
    rect = (0.0, 0.0, 1.0 / (2 * he), 1.0)
    worst, msgs = 0.0, []
    nocal = red == 'nocal'
    for label, T, band in (('stratus', 0.0, 0), ('cumulus', 2.0 / 3.0, 0), ('cumulonimbus', 1.0, 0), ('mid', 0.0, 1)):
        if band == 0:
            s = sky(T, 0.0, swing=0.0, curl=CURL_G, nocal=nocal)
            zr = (s['base0'], s['base0'] + type_depth(T) + 0.01)
        else:
            s = sky(0.6, 0.0, swing=0.0, curl=CURL_G, dens0=0.0, dens1=0.6, nocal=nocal)
            zr = (s['base1'], s['top1'])
        op = probe(s, 1, (0.0, 0.0, 0.0), (30.0 / W, 0.0, 0.0), (0.0, 12.0 / H, 0.0), W, H, zr=zr, nz=128, rect=rect,
                   maptex=mtex)[..., 0]
        got = []
        for i, c in enumerate(covs):
            x0, x1 = int((i * 6 + 1) / 30 * W), int((i * 6 + 5) / 30 * W)
            m = float(op[:, x0:x1].mean())
            got.append(m)
            worst = max(worst, abs(m - c))
        msgs.append('%s %s' % (label, ' '.join('%.3f' % v for v in got)))
    sw_worst, sw = 0.0, []
    for c in (0.3, 0.5, 0.7):
        s = sky(0.75, 0.25, cov=c, curl=CURL_G, nocal=nocal)
        n = 384
        op = probe(s, 1, (0.0, 0.0, 0.0), (192.0 / n, 0.0, 0.0), (0.0, 192.0 / n, 0.0), n, n,
                   zr=(s['base0'], s['top0'] + 0.01), nz=96)[..., 0]
        m = float(op.mean())
        sw.append('%.2f->%.3f' % (c, m))
        sw_worst = max(sw_worst, abs(m - c))
    gate('A coverage', worst <= 0.05 and sw_worst <= 0.06,
         'stripes 0.1 .. 0.9 -> %s; worst |d| %.3f (bar 0.05) | uniform maps with the swing and spread, 192 km: %s, '
         'worst |d| %.3f (bar 0.06)' % ('; '.join(msgs), worst, ' '.join(sw), sw_worst))


# ---- V: the variety metric -----------------------------------------------------------------------------------------------
def field(s, old=False, oldcal=None, n=640, span=96.0, org=(0.0, 0.0)):
    zr = (s['base0'], s['base0'] + type_depth(1.0) + 0.05)
    img = probe(s, 1, (org[0], org[1], 0.0), (span / n, 0.0, 0.0), (0.0, span / n, 0.0), n, n, zr=zr, nz=96, edge=0.02,
                old=old, oldcal=oldcal)
    return img[..., 0], img[..., 1]


def metrics(op, top, base, px):
    cl = op > 0.5
    tt = top[cl & (top > 0)] - base
    iqr = float(np.percentile(tt, 75) - np.percentile(tt, 25)) if tt.size > 16 else 0.0
    t = int(round(12.0 / px))
    n = op.shape[0] // t
    tiles = op[:n * t, :n * t].reshape(n, t, n, t).mean(axis=(1, 3))
    lag = int(round(6.0 / px))
    a, b = op[:, :-lag].ravel(), op[:, lag:].ravel()
    corr = float(np.corrcoef(a, b)[0, 1]) if a.std() > 0 and b.std() > 0 else 1.0
    return iqr, float(tiles.std()), corr, float(cl.mean())


def hp_corr(op, lagpx, box):
    k = np.ones(box) / box
    bl = np.apply_along_axis(lambda r: np.convolve(r, k, mode='same'), 1, op)
    bl = np.apply_along_axis(lambda c: np.convolve(c, k, mode='same'), 0, bl)
    h = (op - bl)[box:-box, box:-box]
    cx = np.corrcoef(h[:, :-lagpx].ravel(), h[:, lagpx:].ravel())[0, 1]
    cy = np.corrcoef(h[:-lagpx, :].ravel(), h[lagpx:, :].ravel())[0, 1]
    return float(0.5 * (cx + cy))


if 'V' in only:
    oldcal, _ = K.dump([(1.5, 3.0, 1.0, 0.0), (5.0, 6.5, 0.6, 0.0)])
    px = 96.0 / 640
    lines, ok_green, today_fails = [], True, True
    for label, ty, sp, b1, b2 in (('storm', 0.9, 0.25, 1.0, 0.06), ('clear', 0.6, 0.2, 0.5, 0.06)):
        s = sky(2.0 / 3.0 if ONE else ty, 0.0 if ONE else sp, curl=CURL_G)
        op, top = field(s)
        iqr, tstd, corr, frac = metrics(op, top, s['base0'], px)
        g_ok = iqr >= b1 and tstd >= b2 and corr <= 0.9
        ok_green = ok_green and g_ok
        so = sky(ty, sp)
        op0, top0 = field(so, old=True, oldcal=oldcal)
        iqr0, tstd0, corr0, frac0 = metrics(op0, top0, so['base0'], px)
        t_fail = iqr0 < b1 and tstd0 < b2 and corr0 > 0.9
        today_fails = today_fails and t_fail
        lines.append('%s: variety V1 tops IQR %.2f km (bar %.1f) V2 tile std %.3f (bar %.2f) V3 6 km corr %.3f (bar 0.9) '
                     'cloudy %.2f %s | today %.2f km, %.3f, %.3f, cloudy %.2f %s'
                     % (label, iqr, b1, tstd, b2, corr, frac, 'pass' if g_ok else 'FAIL', iqr0, tstd0, corr0, frac0,
                        'fails every bar' if t_fail else 'PASSES A BAR'))
    # V4: the curl's wisps (the detail's tiling broken): the density on a horizontal slice inside thick cumulus (cov
    # 0.85, hf 0.33: the shape term is smooth over 0.6 km there, so the high-passed density is the eroded detail), 4
    # windows of 6 km at 0.03 km
    hp = {}
    for arm, cu in (('curl', CURL_G), ('nocurl', 0.0)):
        s = sky(2.0 / 3.0, 0.0, cov=0.85, swing=0.0, curl=cu)
        cs = []
        for org in ((3.0, 5.0), (31.0, 17.0), (55.0, 61.0), (80.0, 9.0)):
            d = probe(s, 0, (org[0], org[1], s['base0'] + 0.5), (0.03, 0.0, 0.0), (0.0, 0.03, 0.0), 200, 200)[..., 0]
            cs.append(hp_corr(d, 20, 21))
        hp[arm] = float(np.mean(cs))
    ratio = hp['curl'] / max(hp['nocurl'], 1e-6)
    lines.append('V4 detail-period corr with curl %.3f / without %.3f = %.3f (bar 0.75)' % (hp['curl'], hp['nocurl'], ratio))
    gate('V variety', ok_green and today_fails and ratio <= 0.75, ' || '.join(lines))

# ---- C: a tower's dark base vs Monte Carlo -------------------------------------------------------------------------------
if 'C' in only:
    reach = 1.2 if red == 'shortlight' else light_km(1.0, 1.2)
    W, H = 64, 64
    tgt = K.Mrt(W, H)
    slab = (1.5, 7.5, 1.0)
    dens = 0.25
    tau = K.SIGMA * dens * (slab[1] - slab[0])
    sun_el = 60.0
    mu0 = math.sin(math.radians(sun_el))
    s = sky(1.0, 0.0)
    u = var_uni(s)
    u.update(K.bands_uni(slab, (9.0, 9.1, 0.0), K.cal, cov=(1.0, 0.0, 0.0)))
    u.update(K.march_uni(K.camera(180, 89.5), (0, 0, 0.5), (W, H), 1, (0, 0), sun_el, steps=256, taps=6, slab=dens,
                         drop=K.drop, vfov=10.0, lightKm=reach))
    u['cvAmbKm'] = (1.5,)
    g.bindfb(0x8D40, tgt.fb)
    gl.glViewport(0, 0, W, H)
    g.use(P_MV)
    K.setuni(P_MV, u)
    set_calT(P_MV, s['calT'])
    K.bind(P_MV, K.march_texs(K.trans, K.trans))
    gl.glDrawArrays(5, 0, 4)
    o = tgt.read()
    T = K.A.transmittance(K.sky.P, np.array([6360.0 + slab[0]]), np.array([mu0]), 400)[0]
    lum = lambda c: float(np.dot(c, [0.2126, 0.7152, 0.0722]))
    under = lum(o[24:40, 24:40, :3].reshape(-1, 3).mean(0)) / lum(T)
    ref = K.mc_underside(tau, mu0, K.drop, n=40000)
    ratio = under / max(ref, 1e-12)
    gate('C towers', 0.5 <= ratio <= 1.6,
         'tau %.0f slab 1.5..7.5 km, light reach %.2f km: underside %.4g x sun vs Monte Carlo %.4g (ratio %.3f, bar '
         '0.5 .. 1.6)' % (tau, reach, under, ref, ratio))

# ---- E: cost ------------------------------------------------------------------------------------------------------------
if 'E' in only:
    q = GLuint()
    S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(q))
    begq = S.fn('glBeginQuery', None, GLenum, GLuint)
    endq = S.fn('glEndQuery', None, GLenum)
    getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
    sv = K.sky.view_lut(192, 108, 6360.05, K.R.sundir(30, 35))
    lines, over = [], False
    for skyname, ty, sp in (('clear', 0.6, 0.2), ('storm', 0.9, 0.25)):
        s = sky(ty, sp, cov=0.6, dens1=0.6)
        for name, div, steps, taps, plk, mx in (('medium', 4, 48, 3, 1.0, 60.0), ('high', 4, 64, 6, 1.2, 80.0),
                                                ('ultra', 2, 96, 6, 1.2, 100.0)):
            W, H = 1920 // div, 1080 // div
            mw, mh = (W + 3) // 4, (H + 3) // 4
            march = K.Mrt(mw, mh)
            cam = K.camera(10, 8)
            ms = []
            for arm in ('today', 'variety'):
                base = K.march_uni(cam, (0, 0, 0.05), (W, H), 4, (1.0, 2.0), 35.0, sun_az=30, steps=steps, taps=taps,
                                   drop=K.drop, amb=0.05, ap=1.0, maxKm=mx,
                                   lightKm=plk if arm == 'today' else light_km(min(1.0, ty + sp), plk))
                if arm == 'today':
                    u = K.bands_uni((1.5, 3.0, 1.0), (5.0, 6.5, 0.6), K.cal, cov=(0.6, 0.3, 0.0))
                    prog = K.P_MARCH
                else:
                    u = var_uni(s)
                    u['cvAmbKm'] = (5.0,)
                    prog = P_MV
                u.update(base)
                g.bindfb(0x8D40, march.fb)
                gl.glViewport(0, 0, mw, mh)
                g.use(prog)
                K.setuni(prog, u)
                if arm == 'variety':
                    set_calT(prog, s['calT'])
                K.bind(prog, K.march_texs(K.trans, sv))
                gl.glDrawArrays(5, 0, 4)
                gl.glFinish()
                reps = 20
                begq(0x88BF, q)
                for _ in range(reps):
                    gl.glDrawArrays(5, 0, 4)
                endq(0x88BF)
                ns = ctypes.c_uint64()
                getq(q, 0x8866, ctypes.byref(ns))
                ms.append(ns.value / 1e6 / reps)
            if name == 'ultra' and ms[1] > 1.5:
                over = True
            lines.append('%s %s march today %.3f ms, variety %.3f ms (x%.2f)' % (skyname, name, ms[0], ms[1],
                                                                              ms[1] / max(ms[0], 1e-9)))
    print('E cost       INFO  ' + '; '.join(lines) + ('  OVER: Ultra variety above 1.5 ms' if over else '') +
          ' (this machine\'s GPU, offline, march only: the resolve and composites do not change)')

print('CLOUDVAR-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
