#!/usr/bin/env python3
"""Lane CLOUD6e (CLOUDBAND1 leftover, RDR2 gap e): the rain fog under the rain clouds, gated offline. Runs the REAL
res/shaders/cell_volair.prog (the air's froxel pass, inject stage) through this machine's GL driver in a hidden 4.1 core
context (shader_link_check.py / atmo_lut_check.py), and the CPU law through tests/rainfog/rainfog_dump.cpp (g++). No
NifSkope, no game data (the weather census is the derived fixture tests/rainfog/census_rain.tsv).

    python -I tests/spells/rainfog_check.py <scratch> [--red today|flagrule|wiring] [--only ZIPHSMDKWC] [--base <sha>]

RDR2 (Bauer 2019, SIGGRAPH Advances, slide 19 + note 19): "rain fog beneath clouds, where placement is derived off the
density in the cloud map. A height-based falloff is applied to connect the fog to the cloud." No numbers given.

Gates (bars written before the first run; the camera grid 64 x 36 columns x 64 exponential slices, 60 deg vertical fov;
the cloud map 256^2 RGBA32F over 48 km, the Low base 1.5 km over the ground; the air's own medium off (sigma0 0) except
in Z and C, so a froxel's T is the rain fog's alone):
  Z clear     a weather that does not rain draws today's air EXACTLY: the new program with rfOn 0 (every rain uniform
              set, the map bound) vs today's program (cell_volair.frag at --base), the air's medium on: every froxel's
              RGBA bit for bit, 3 cameras. Bar: max |diff| == 0.
  I integral  rfOn 1, a smooth random coverage map, 3 cameras (ground level, ground 25 deg up, 1 km up looking 40 deg
              down): every slice's T vs a 64-point-per-slice quadrature of the same law (bilinear map emulated):
              max |dT| <= 0.01 a slice, the column's T at the far end |dT| <= 0.02.
  P place     half map covered (c 1 at x < -4 km), half clear (c 0 at x > +4 km): level rays 2 m over the ground into
              each half: the clear side's optical depth over 6..8 km <= 0.02 x the covered side's, the covered side's
              within 3 % of sigma x p(h) x length (follows the coverage above).
  S stripes   1 km up looking straight down over 4 x 4 blocks of random coverage (smoothed): the columns' optical depth to
              the ground vs c(x, y) x the profile's integral: correlation >= 0.99, slope within 3 % of 1.
  H height    on the ground looking straight up under full cover: each slice's -ln T / length vs sigma p(h) at its middle
              within 2 % (slices < 120 m long, below the base); slices wholly above base + 0.15 km: T == 1 exactly.
  M magnitude the dump's sigma == the checker's own drop extinction (Atlas 1973 fall speed, water mass, the rain's
              flux, Q 2) rel 1e-9; the precipitation reds do not reach it (sigmared == sigma).
  D rules     the dump's profile == p(h) transcribed (1e-12); the share rule (pct for the new weather, 1 - pct for the
              leaving one, 0 for none); the switch (unset on, 0/off/false/no off) and red (1/on/old) words.
  K census    the 107 vanilla weathers' fixture through the rule (rains = the record links a precipitation shader,
              MNAM): 10 rain, the 4 Rainy-flag weathers with no shader do not. Red flagrule (DATA flag 4) FAILS.
  W wiring    esmweather.cpp reads MNAM, lookdevstage.cpp's wwLookdevWeatherRains decides on it, cellvolair.cpp sets
              the uniforms before the far grid keys on them, cell_volair.frag adds rfSigma in the sub-step loop before
              `scatters`. Red wiring (the --base sources) FAILS.
  C cost      GPU ms of the inject pass at 160 x 90 x 64 (1080p), 20 reps: today / rfOn 0 / rfOn 1 (INFO; bars rfOn 0
              <= today + 0.02 ms, rfOn 1 <= today + 0.15 ms).
Reds: today (today's program for I P S H: no rain fog, every one FAILS), flagrule (K), wiring (W).
Amendments after the first run (before any red run), both measurement resolution, not the law:
  M  rel 1e-9 -> 1e-5: precipmath.h's water mass constant is 0.5236 (pi / 6 to 4 digits): rel 2.3e-6.
  H  the 2 % bar judges only slices of od >= 1e-4; the near slices (od 1e-6..1e-4) are judged by |d od| <= 3e-7: the
     program's T is float32 (6e-8 a step near 1), which read 2.3 % on the first slices.

Exit 0 only when every gate run passes.
"""
import ctypes, math, os, re, shutil, subprocess, sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'rainfog_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ZIPHSMDKWC')
BASE = args[args.index('--base') + 1] if '--base' in args else '0e7c2c3a'
os.makedirs(scratch, exist_ok=True)

import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = os.path.join(TREE, 'res', 'shaders')
KU = 0.0142875 / 1000.0          # atmomath.h kUnitKm (km per game unit)
UPK = 1.0 / KU
ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ.get('PATH', ''))
results = []


def gate(name, ok, msg):
    results.append(bool(ok))
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


# ---- the CPU numbers (rainfogmath.h) through the dump ------------------------------------------------------------------
def dump():
    exe = os.path.join(scratch, 'rainfog_dump.exe')
    r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                        os.path.join(TREE, 'tests', 'rainfog', 'rainfog_dump.cpp'), '-o', exe], env=ENV,
                       capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])
    out = subprocess.run([exe], capture_output=True, text=True, env=ENV, check=True, timeout=60).stdout.split('\n')
    D = {'profile': [], 'share': [], 'switch': [], 'red': []}
    for l in out:
        w = l.split()
        if not w:
            continue
        if w[0] in ('rain', 'sigma', 'sigmared', 'law'):
            D[w[0]] = [float(x) for x in w[1:]]
        elif w[0] == 'profile':
            D['profile'].append((float(w[1]), float(w[2])))
        elif w[0] == 'share':
            D['share'].append((int(w[1]), int(w[2]), int(w[3]), float(w[4]), float(w[5])))
        elif w[0] in ('switch', 'red'):
            D[w[0]].append((w[1].strip("'"), int(w[2])))
    return D


DD = dump()
SIGMA_KM = DD['sigma'][0]
EXCESS, FALLKM, ABOVEKM, EDGE = DD['law']


def p_of(hkm):   # the checker's own transcription of rainfogmath.h profile
    hkm = np.asarray(hkm, dtype=np.float64)
    below = 1.0 + EXCESS * np.exp(-np.maximum(hkm, 0.0) / FALLKM)
    above = (1.0 + EXCESS) * np.maximum(1.0 + hkm / ABOVEKM, 0.0)
    return np.where(hkm >= 0.0, below, above)


# ---- GL -----------------------------------------------------------------------------------------------------------------
G = C.Gl()
f_u4f = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
f_u3i = S.fn('glUniform3i', None, GLint, GLint, GLint, GLint)
f_um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
f_pget = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
f_active = S.fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_int),
                ctypes.POINTER(GLint), ctypes.POINTER(GLenum), ctypes.c_char_p)
f_genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))
f_begq = S.fn('glBeginQuery', None, GLenum, GLuint)
f_endq = S.fn('glEndQuery', None, GLenum)
f_getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
SAMPLERS = set(range(0x8B5D, 0x8B65)) | set(range(0x8DC0, 0x8DDA)) | set(range(0x9108, 0x910E)) | set(range(0x900C, 0x9010))


def program(sdir):
    p = C.build(sdir, 'cell_volair.prog')
    ok = GLint()
    f_pget(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL cell_volair.prog in ' + sdir)
    # every sampler on its own unit (an unset sampler of another type on unit 0 drops the draw)
    n = GLint()
    f_pget(p, 0x8B86, ctypes.byref(n))
    G.use(p)
    units, unit = {}, 1
    for i in range(n.value):
        ln, sz, ty = ctypes.c_int(), GLint(), GLenum()
        buf = ctypes.create_string_buffer(256)
        f_active(p, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), buf)
        if ty.value in SAMPLERS:
            name = buf.value.decode()
            G.u1i(G.loc(p, name.encode()), unit)
            units[name] = unit
            unit += 1
    return p, units


def tex_map(img):
    a = np.ascontiguousarray(np.dstack([img, np.zeros_like(img), np.zeros_like(img), np.zeros_like(img)]).astype(np.float32))
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x0DE1, t)
    gl.glTexImage2D(0x0DE1, 0, 0x8814, img.shape[1], img.shape[0], 0, 0x1908, 0x1406, a.ctypes.data)
    for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F), (0x813D, 0)):
        gl.glTexParameteri(0x0DE1, k, v)
    return t.value


def basis(yaw_deg, pitch_deg):
    yaw, pit = math.radians(yaw_deg), math.radians(pitch_deg)
    fwd = np.array([math.cos(pit) * math.cos(yaw), math.cos(pit) * math.sin(yaw), math.sin(pit)])
    ref = np.array([0, 0, 1.0]) if abs(fwd[2]) < 0.99 else np.array([0, 1.0, 0])
    right = np.cross(fwd, ref)
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    return np.stack([right, up, -fwd])   # rows: view axes in world (view = R (w - pos))


class Scene:
    def __init__(self, pos_km, yaw, pitch, img, cols=64, rows=36, far_km=8.0, half_km=24.0, base_km=1.5, cov_out=0.5):
        self.pos = np.array(pos_km, dtype=np.float64) * UPK
        self.R = basis(yaw, pitch)
        self.img = np.asarray(img, dtype=np.float64)
        self.cols, self.rows = cols, rows
        self.p11 = 1.0 / math.tan(math.radians(30.0))
        self.p00 = self.p11 * rows / cols
        self.near, self.far = 16.0, far_km * UPK
        self.logR = math.log(self.far / self.near)
        self.he = half_km * UPK
        self.cx, self.cy = self.pos[0], self.pos[1]
        self.base = base_km * UPK
        self.cov_out = cov_out
        Rt = self.R.T   # world = pos + Rt v
        M = np.zeros((4, 4))
        M[0, :3] = Rt[0] / (2 * self.he)
        M[0, 3] = (self.pos[0] - self.cx + self.he) / (2 * self.he)
        M[1, :3] = Rt[1] / (2 * self.he)
        M[1, 3] = (self.pos[1] - self.cy + self.he) / (2 * self.he)
        M[2, :3] = -Rt[2] * KU
        M[2, 3] = (self.base - self.pos[2]) * KU
        M[3, 3] = 1.0
        self.M = M

    def dirs(self):
        c = (np.arange(self.cols) + 0.5) / self.cols * 2 - 1
        r = (np.arange(self.rows) + 0.5) / self.rows * 2 - 1
        X, Y = np.meshgrid(c / self.p00, r / self.p11)
        d = np.dstack([X, Y, -np.ones_like(X)])
        return d / np.linalg.norm(d, axis=2, keepdims=True)   # rows x cols x 3 (view)

    def edges(self):
        k = np.arange(65)
        return self.near * np.exp(self.logR * k / 64.0)

    def sample_c(self, u, v):   # GL_LINEAR + CLAMP_TO_EDGE on the R channel, then the edge fade to the scalar
        n = self.img.shape[0]
        x, y = u * n - 0.5, v * n - 0.5
        x0, y0 = np.floor(x), np.floor(y)
        fx, fy = x - x0, y - y0
        x0, y0 = x0.astype(int), y0.astype(int)
        cl = lambda a: np.clip(a, 0, n - 1)
        I = self.img
        c = (I[cl(y0), cl(x0)] * (1 - fx) * (1 - fy) + I[cl(y0), cl(x0 + 1)] * fx * (1 - fy)
             + I[cl(y0 + 1), cl(x0)] * (1 - fx) * fy + I[cl(y0 + 1), cl(x0 + 1)] * fx * fy)
        e = np.maximum(np.abs(u - 0.5), np.abs(v - 0.5)) * 2
        t = np.clip((e - (1 - EDGE)) / EDGE, 0, 1)
        w = t * t * (3 - 2 * t)
        return c * (1 - w) + self.cov_out * w

    def sigma_units(self, pv):   # per game unit at view points (..., 3)
        q = pv @ self.M[:3, :3].T + self.M[:3, 3]
        return SIGMA_KM * KU * self.sample_c(q[..., 0], q[..., 1]) * p_of(q[..., 2])

    def ref_T(self, n=64):
        """per slice T by an n-point midpoint quadrature: rows x cols x 64"""
        d = self.dirs()
        e = self.edges()
        out = np.zeros((self.rows, self.cols, 64))
        for k in range(64):
            t0, t1 = e[k], e[k + 1]
            ts = t0 + (np.arange(n) + 0.5) / n * (t1 - t0)
            pv = d[:, :, None, :] * ts[None, None, :, None]
            od = self.sigma_units(pv).sum(axis=2) * (t1 - t0) / n
            out[:, :, k] = np.exp(-od)
        return out


def run(prog, units, sc, rf_on=1, air=None, maptex=None, layers=64):
    """the inject stage's RGBA per slice: rows x cols x layers x 4"""
    w, h = sc.cols, sc.rows
    if not hasattr(run, 'tg') or run.tg[2:] != (w, h):
        t, fb = G.target(w, h)
        run.tg = (t, fb, w, h)
    fb = run.tg[1]
    G.bindfb(0x8D40, fb)
    gl.glViewport(0, 0, w, h)
    G.use(prog)
    L = lambda n: G.loc(prog, n.encode())
    G.u1i(L('volStage'), 1)
    f_u3i(L('volDims'), w, h, 64)
    G.u2f(L('volProj'), sc.p00, sc.p11)
    f_u4f(L('volSlices'), sc.near, sc.logR, 64.0, 1.0)
    up = sc.R[:, 2]   # world up in view coords (view = R w: the z row of R^T)
    f_u4f(L('fogView'), up[0], up[1], up[2], sc.pos[2])
    G.u1f(L('fogDistScale'), 1.0)
    G.u1i(L('fogOn'), 0)
    a = air or (0.0, 0.0, 0.0, 1e9)
    f_u4f(L('airMed'), *a)
    for n in ('airLayer', 'airGround', 'volDrift'):
        f_u4f(L(n), 0.0, 0.0, 0.0, 0.0)
    for n in ('airTerrOn', 'volTemporal', 'volDepthSkip', 'volRed', 'volFarMode', 'volLayerBase', 'volTerms'):
        G.u1i(L(n), 0)
    G.u1f(L('volJitter'), 0.0)
    G.u1i(L('rfOn'), rf_on)
    f_u4f(L('rfLaw'), SIGMA_KM * KU, EXCESS, 1.0 / FALLKM, 1.0 / ABOVEKM)
    G.u2f(L('rfFar'), sc.cov_out, EDGE)
    m = np.ascontiguousarray(sc.M.T.astype(np.float32))   # column-major
    f_um4(L('rfFromView'), 1, 0, m.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
    if 'rfMap' in units and maptex:
        G.active(0x84C0 + units['rfMap'])
        gl.glBindTexture(0x0DE1, maptex)
    out = np.zeros((h, w, layers, 4))
    buf = (ctypes.c_float * (w * h * 4))()
    for k in range(layers):
        G.u1i(L('volLayer'), k)
        gl.glDrawArrays(5, 0, 4)
        gl.glReadPixels(0, 0, w, h, 0x1908, 0x1406, buf)
        out[:, :, k, :] = np.frombuffer(buf, np.float32).reshape(h, w, 4)
    return out


def trap(y, x):
    y, x = np.asarray(y, dtype=np.float64), np.asarray(x, dtype=np.float64)
    return float(((y[1:] + y[:-1]) * 0.5 * np.diff(x)).sum())


def smooth_random(n, blocks, seed):
    rng = np.random.default_rng(seed)
    b = rng.uniform(0.0, 1.0, (blocks, blocks))
    x = (np.arange(n) + 0.5) / n * blocks - 0.5
    i0 = np.clip(np.floor(x).astype(int), 0, blocks - 1)
    i1 = np.clip(i0 + 1, 0, blocks - 1)
    f = np.clip(x - np.floor(x), 0, 1)
    f = f * f * (3 - 2 * f)
    rowsI = b[i0] * (1 - f)[:, None] + b[i1] * f[:, None]
    return rowsI[:, i0] * (1 - f)[None, :] + rowsI[:, i1] * f[None, :]


# ---- the programs: the tree's, and today's (cell_volair.frag at BASE over a copy of the tree's shaders) --------------
NEED_GL = only & set('ZIPHSC')
if NEED_GL:
    P_NEW, U_NEW = program(SDIR)
    tdir = os.path.join(scratch, 'shaders_today')
    if os.path.isdir(tdir):
        shutil.rmtree(tdir)
    shutil.copytree(SDIR, tdir)
    old = subprocess.run(['git', '-C', TREE, 'show', BASE + ':res/shaders/cell_volair.frag'], capture_output=True, check=True).stdout
    open(os.path.join(tdir, 'cell_volair.frag'), 'wb').write(old)
    P_OLD, U_OLD = program(tdir)
    P_GATE, U_GATE = (P_OLD, U_OLD) if red == 'today' else (P_NEW, U_NEW)
    print('programs: new %d, today (%s) %d%s' % (P_NEW, BASE, P_OLD, '  [red today: the gates read today\'s]' if red == 'today' else ''))

# ---- Z: a weather that does not rain draws today's air bit for bit -----------------------------------------------------
if 'Z' in only:
    img = smooth_random(256, 6, 3)
    mt = tex_map(img)
    worst, n = 0.0, 0
    air = (1.0 / (3.0 * UPK), 0.0, 1.0 / (0.2 * UPK), 0.6 * UPK)
    for pos, yaw, pit in (((0, 0, 0.002), 10, 0), ((0, 0, 0.002), 200, 25), ((0, 0, 1.0), 70, -40)):
        sc = Scene(pos, yaw, pit, img)
        a = run(P_NEW, U_NEW, sc, rf_on=0, air=air, maptex=mt)
        b = run(P_OLD, U_OLD, sc, rf_on=0, air=air, maptex=mt)
        worst = max(worst, float(np.abs(a - b).max()))
        n += a.size
        tmin = float(a[..., 3].min())
    gate('Z clear', worst == 0.0, 'rfOn 0 vs today (%s), air on: max |diff| %.3g over %d values (3 cameras; min slice T %.4f)'
         % (BASE, worst, n, tmin))

# ---- I: the law integrated by the program vs a fine quadrature ---------------------------------------------------------
if 'I' in only:
    img = smooth_random(256, 6, 7)
    mt = tex_map(img)
    ws, wc = 0.0, 0.0
    for pos, yaw, pit in (((0, 0, 0.002), 30, 0), ((0, 0, 0.002), 120, 25), ((0, 0, 1.0), 250, -40)):
        sc = Scene(pos, yaw, pit, img)
        T = run(P_GATE, U_GATE, sc, maptex=mt)[..., 3]
        R = sc.ref_T()
        ws = max(ws, float(np.abs(T - R).max()))
        wc = max(wc, float(np.abs(T.prod(axis=2) - R.prod(axis=2)).max()))
        info = 'column T at 8 km: ref mean %.3f min %.3f' % (R.prod(axis=2).mean(), R.prod(axis=2).min())
    gate('I integral', ws <= 0.01 and wc <= 0.02, 'max |dT| a slice %.5f (<= 0.01), a column %.5f (<= 0.02); %s' % (ws, wc, info))

# ---- P: follows the coverage above -------------------------------------------------------------------------------------
if 'P' in only:
    n = 256
    x = ((np.arange(n) + 0.5) / n * 2 - 1) * 24.0   # km, the map's x (u east)
    row = np.clip(0.5 - x / 8.0, 0.0, 1.0)            # 1 west of -4 km, 0 east of +4 km
    img = np.tile(row, (n, 1))
    mt = tex_map(img)
    od = {}
    for side, yaw in (('covered', 180.0), ('clear', 0.0)):
        sc = Scene((0, 0, 0.002), yaw, 0.0, img, cols=8, rows=8, far_km=8.0)
        T = run(P_GATE, U_GATE, sc, maptex=mt)[..., 3]
        e = sc.edges()
        mid = 0.5 * (e[:-1] + e[1:]) * KU
        sel = (mid >= 6.0) & (mid <= 8.0)
        cc = (sc.rows // 2, sc.cols // 2)   # the centre column (2 rays: 3.75 deg off level at most: still under 0.6 km)
        od[side] = float(-np.log(np.maximum(T[cc[0], cc[1], sel], 1e-30)).sum())
        if side == 'covered':
            d = sc.dirs()[cc[0], cc[1]]
            ts = np.linspace(e[:-1][sel][0], e[1:][sel][-1], 20001)
            pv = d[None, :] * ts[:, None]
            hk = sc.M[2, :3] @ pv.T + sc.M[2, 3]
            want = trap(SIGMA_KM * KU * p_of(hk), ts)
    ratio = od['clear'] / max(od['covered'], 1e-30)
    rel = od['covered'] / want - 1.0
    gate('P place', od['covered'] > 0 and ratio <= 0.02 and abs(rel) <= 0.03,
         'od 6..8 km: covered %.4f (want %.4f, %+.2f %%), clear %.6f (ratio %.4f <= 0.02)' % (od['covered'], want, 100 * rel,
                                                                                         od['clear'], ratio))

# ---- S: random coverage seen from above --------------------------------------------------------------------------------
if 'S' in only:
    img = smooth_random(256, 4, 11)
    mt = tex_map(img)
    sc = Scene((0, 0, 1.0), 0.0, -90.0, img, cols=32, rows=32, far_km=1.6)
    T = run(P_GATE, U_GATE, sc, maptex=mt)[..., 3]
    d = sc.dirs()
    e = sc.edges()
    od_c, want = [], []
    for r in range(sc.rows):
        for c in range(sc.cols):
            dv = d[r, c]
            tg = sc.pos[2] / max(-(sc.R.T @ dv)[2], 1e-9)   # the ground's distance along the ray
            k = int(np.searchsorted(e, tg)) - 1          # slices wholly above the ground
            if k < 4:
                continue
            od_c.append(float(-np.log(np.maximum(T[r, c, :k], 1e-30)).sum()))
            ts = np.linspace(e[0], e[k], 4001)
            pv = dv[None, :] * ts[:, None]
            want.append(trap(sc.sigma_units(pv), ts))
    od_c, want = np.array(od_c), np.array(want)
    if od_c.std() > 0:
        corr = float(np.corrcoef(od_c, want)[0, 1])
        slope = float((od_c * want).sum() / (want * want).sum())
    else:
        corr, slope = 0.0, 0.0
    gate('S stripes', corr >= 0.99 and abs(slope - 1) <= 0.03,
         '%d columns: corr %.5f (>= 0.99), slope %.4f (1 +- 0.03); od range %.4f..%.4f' % (len(want), corr, slope, want.min(), want.max()))

# ---- H: the height falloff ---------------------------------------------------------------------------------------------
if 'H' in only:
    img = np.ones((64, 64))
    mt = tex_map(img)
    sc = Scene((0, 0, 0.002), 0.0, 90.0, img, cols=4, rows=4, far_km=3.0, cov_out=1.0)
    T = run(P_GATE, U_GATE, sc, maptex=mt)[..., 3]
    d = sc.dirs()[1, 1]
    e = sc.edges()
    zup = sc.R.T @ d
    worst, n, above_bad, nab, tiny, ntiny = 0.0, 0, 0.0, 0, 0.0, 0
    for k in range(64):
        t0, t1 = e[k], e[k + 1]
        h0 = sc.M[2, 3] - zup[2] * t0 * KU   # km below the base at the slice's ends
        h1 = sc.M[2, 3] - zup[2] * t1 * KU
        if h1 >= 0.0 and (t1 - t0) * KU < 0.12:
            s_eff = -math.log(max(T[1, 1, k], 1e-30)) / (t1 - t0)
            s_want = SIGMA_KM * KU * float(p_of(0.5 * (h0 + h1)))
            if s_want * (t1 - t0) >= 1e-4:   # amended: float32 T resolves -ln T to ~6e-8 (see the docstring)
                worst = max(worst, abs(s_eff / s_want - 1.0))
                n += 1
            else:
                tiny = max(tiny, abs(s_eff - s_want) * (t1 - t0))
                ntiny += 1
        if h0 <= -ABOVEKM:
            above_bad = max(above_bad, abs(1.0 - T[1, 1, k]))
            nab += 1
    gate('H height', n >= 20 and worst <= 0.02 and tiny <= 3e-7 and nab >= 1 and above_bad == 0.0,
         '%d slices below the base: max |sigma_eff / sigma p(h) - 1| %.4f (<= 0.02); %d slices of od < 1e-4: max |d od| %.2g '
         '(<= 3e-7, float32 T); %d slices above base + %.2f km: max |1 - T| %.3g'
         % (n, worst, ntiny, tiny, nab, ABOVEKM, above_bad))

# ---- M: the magnitude is the rain's own -------------------------------------------------------------------------------
if 'M' in only:
    Dmm, rate = DD['rain']
    v = min(max(9.65 - 10.3 * math.exp(-0.6 * Dmm), 0.1), 9.17)   # Atlas, Srivastava & Sekhon 1973, m/s
    m = math.pi / 6.0 * Dmm ** 3                                    # mg (water, 1 mg / mm^3)
    N = rate * 1.0e6 / 3600.0 / (m * v)                             # per m^3: the flux mg/m^2/s over (m v)
    sig = N * 2.0 * math.pi * (Dmm * 0.5e-3) ** 2 * 1000.0          # per km, Q 2
    rel = abs(SIGMA_KM / sig - 1.0)
    gate('M magnitude', rel <= 1e-5 and DD['sigmared'][0] == SIGMA_KM,
         'sigma %.6f / km (checker %.6f, rel %.2g; %.1f mm at %.1f mm/h, %.1f drops / m^3, visibility 3.912 / sigma %.1f km); '
         'the precipitation reds %s' % (SIGMA_KM, sig, rel, Dmm, rate, N, 3.912 / SIGMA_KM,
                                        'do not reach it' if DD['sigmared'][0] == SIGMA_KM else 'MOVE it'))

# ---- D: the rules ------------------------------------------------------------------------------------------------------
if 'D' in only:
    pe = max(abs(p - float(p_of(h))) for h, p in DD['profile'])
    se = 0.0
    for c, o, b, pct, v in DD['share']:
        want = (1.0 if c else 0.0) if not b else (pct if c else 0.0) + ((1 - pct) if o else 0.0)
        se = max(se, abs(v - want))
    sw = dict(DD['switch'])
    rw = dict(DD['red'])
    sw_ok = all(sw[k] == 1 for k in ('', '1', 'on', 'yes', 'true', '2')) and all(sw[k] == 0 for k in ('0', 'off', 'false', 'no'))
    rw_ok = all(rw[k] == 0 for k in ('', '0', 'off')) and all(rw[k] == 1 for k in ('1', 'on', 'old'))
    gate('D rules', pe <= 1e-12 and se <= 1e-12 and sw_ok and rw_ok,
         'profile max err %.2g (%d h), share max err %.2g (%d rows), switch words %s, red words %s; p(0) %.3f p(1 km) %.3f'
         % (pe, len(DD['profile']), se, len(DD['share']), sw_ok, rw_ok, float(p_of(0.0)), float(p_of(1.0))))

# ---- K: which weathers rain ---------------------------------------------------------------------------------------------
if 'K' in only:
    rows = [l.rstrip('\n').split('\t') for l in open(os.path.join(TREE, 'tests', 'rainfog', 'census_rain.tsv'))][1:]
    rule = (lambda r: int(r[2]) & 4 != 0) if red == 'flagrule' else (lambda r: r[6] != '')
    rains = [r[0] for r in rows if rule(r)]
    shaderless = {'DiamondWeather', 'DiamondWeatherPastel', 'TCommonwealthMarshOvercast', 'CommonwealthGSRadstormOld'}
    clear_ok = not (set(rains) & shaderless)
    must = {'CommonwealthRain', 'CommonwealthMistyRainy', 'DLC03_RainWeather', 'DLC04NukaWorldRain'}
    gate('K census', len(rows) == 107 and len(rains) == 10 and clear_ok and must <= set(rains),
         '%d weathers, %d rain under the rule%s (want 10); Rainy-flag weathers with no precipitation shader kept dry: %s'
         % (len(rows), len(rains), ' [red flagrule: DATA flag 4]' if red == 'flagrule' else ' (MNAM)', clear_ok))

# ---- W: the wiring -----------------------------------------------------------------------------------------------------
if 'W' in only:
    def src(rel):
        if red == 'wiring':
            r = subprocess.run(['git', '-C', TREE, 'show', BASE + ':' + rel], capture_output=True)
            return r.stdout.decode('latin-1') if r.returncode == 0 else ''
        p = os.path.join(TREE, rel)
        return open(p, 'rb').read().decode('latin-1') if os.path.exists(p) else ''
    es = src('src/esmweather.cpp')
    ld = src('src/gl/lookdevstage.cpp')
    cv = src('src/gl/cellvolair.cpp')
    va = src('res/shaders/cell_volair.frag')
    w1 = bool(re.search(r'f == "MNAM"[^\n]*\n\s*std::memcpy\( &out\.precipForm', es))
    w2 = bool(re.search(r'bool wwLookdevWeatherRains\(.*?cur = s\.w\.precipForm != 0;.*?old = s\.w\.last->precipForm != 0;', ld, re.S))
    i_rf = cv.find('g.rfNote = vaRainFog( scene, prog')
    i_key = cv.find('key.insert( key.end(), g.rfKey, g.rfKey + 5 )')
    w3 = 0 <= i_rf < i_key and 'why = QStringLiteral( "none(no rain)" )' in cv
    loop = re.search(r'tauQ\[q\] = airOd\(.*?scatters = scatters \|\| air1mE\( tauQ\[q\] \) > 0\.0;', va, re.S)
    w4 = '#include "cell_rainfog.glsl"' in va and bool(loop) and 'tauQ[q] += rfSigma(' in loop.group(0)
    gate('W wiring', w1 and w2 and w3 and w4, 'esmweather MNAM %s, wwLookdevWeatherRains %s, cellvolair uniforms before the far key %s, '
         'cell_volair sub-step add %s%s' % (w1, w2, w3, w4, ' [red wiring: %s sources]' % BASE if red == 'wiring' else ''))

# ---- C: cost -----------------------------------------------------------------------------------------------------------
if 'C' in only:
    img = smooth_random(256, 6, 5)
    mt = tex_map(img)
    air = (1.0 / (3.0 * UPK), 0.0, 1.0 / (0.2 * UPK), 0.6 * UPK)
    sc = Scene((0, 0, 0.002), 30, 5, img, cols=160, rows=90)
    q = (GLuint * 2)()
    f_genq(2, q)

    def timed(prog, units, on):
        run(prog, units, sc, rf_on=on, air=air, maptex=mt, layers=1)
        L = lambda n: G.loc(prog, n.encode())
        ms = []
        for rep in range(20):
            gl.glFinish()
            f_begq(0x88BF, q[0])
            for k in range(64):
                G.u1i(L('volLayer'), k)
                gl.glDrawArrays(5, 0, 4)
            f_endq(0x88BF)
            r = ctypes.c_uint64()
            f_getq(q[0], 0x8866, ctypes.byref(r))
            ms.append(r.value / 1e6)
        return float(np.median(ms))
    t_old = timed(P_OLD, U_OLD, 0)
    t_0 = timed(P_NEW, U_NEW, 0)
    t_1 = timed(P_NEW, U_NEW, 1)
    gate('C cost', t_0 <= t_old + 0.02 and t_1 <= t_old + 0.15,
         'inject 160x90x64, median of 20 (this machine\'s GPU, offline, lights off): today %.3f ms, rfOn 0 %.3f, rfOn 1 %.3f'
         % (t_old, t_0, t_1))

print('RAINFOG-CHECK %s (%d of %d)%s' % ('PASS' if all(results) else 'FAIL', sum(results), len(results),
                                        ' [red %s]' % red if red else ''))
if __name__ == '__main__':
    sys.exit(0 if results and all(results) else 1)
