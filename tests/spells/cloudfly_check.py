"""lane CLOUD4: offline gate of the fly-through clouds (src/gl/cloudfly.cpp, res/shaders/cloud_flybake.frag,
cloud_flysdf.frag, cloud_fly.frag = cloud_march.frag under WW_CLOUDFLY), through the real driver, no NifSkope.

  python -I tests/spells/cloudfly_check.py <scratch> [--red sdfshrink|nobox|eyeheights|stale|nobound] [--only BDFIWT]

The presets, placement, age and the Chebyshev reference come from src/gl/cloudflymath.h through
tests/cloudfly/cloudfly_dump.cpp; the noise, calibration and GL helpers from cloudvol_check (the exe's bytes).
Fixture: the Low band 1.0 .. 2.5 km above sea level (CLOUD4: bands start at 600 m and up), coverage 0.7 in blobs inside
a 3 km disc round the camera, nothing outside it; the camera INSIDE the band at 1.6 km.
  B  bound: every cell where the density is above 0 at any of 6^3 points (cfProbe) is occupied (no cloud the grid misses);
     the empty share is INFO (what the skip can win)
  D  distance field: the GPU's three separable passes (R8, the exe's layer draws) == cloudflymath.h chebyshev, every cell
  F  continuity: the march jumping the free cells == the same march stepping finely through every cell (red skip off as
     the reference), camera inside the band; max |dT| <= 0.01 and mean |dL| / mean L <= 0.01. Red sdfshrink (cfSafe -1:
     a jump may pass an occupied cell) must fail
  I  inside look: the fly march at the game's settings (64 plain steps over 80 km after the box) == a fine plain march
     (1536 steps over 6 km, all the cloud): mean |d alpha| <= 0.03, mean |dL| / mean L <= 0.1, the cloud depth (where the
     fog sits) within 0.15. Reds nobox (the plain march alone, 64 steps over the ray) and eyeheights (the bands back above
     the eye, the camera under them) fail
  W  age: a grid baked at wind 0 and marched 31.25 s later (0.25 km of wind) after a camera move (the map
     follows the camera, 0.75 km) with the age safety (kSafe + ageCells) == the fine reference then; red stale (kSafe alone) fails
  T  cost: GL_TIME_ELAPSED at 1080p (High volume history 480x270, the march 1/16 a frame): the fly march minus the plain
     one, the full grid build, the rolling share a frame (this machine's GPU, offline)
"""
import ctypes, math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudfly_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('BDFIWT')
os.makedirs(scratch, exist_ok=True)

sys.path.insert(0, HERE)
sys.argv = [sys.argv[0], scratch, '--only', '-']   # cloudvol_check: its helpers and dump, none of its gates
import cloudvol_check as K   # noqa: E402
import numpy as np            # noqa: E402

S, C, g, gl = K.S, K.C, K.g, K.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = os.path.join(TREE, 'res', 'shaders')
LOW = (1.0, 2.5, 1.0)     # base km, top km, density
MID = (5.0, 6.5, 0.0)
CAM = (0.03, 0.02, 1.6)
HALF = 8.0                # the map square, km
f_fbl = S.fn('glFramebufferTextureLayer', None, GLenum, GLenum, GLuint, GLint, GLint)
f_linkst = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
f_drawbufs = S.fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))

ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
DUMP = os.path.join(scratch, 'cloudfly_dump.exe')
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(TREE, 'tests', 'cloudfly', 'cloudfly_dump.cpp'), '-o', DUMP], env=ENV,
                   capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])


def dump(*a):
    return subprocess.run([DUMP] + [str(x) for x in a], env=ENV, capture_output=True, text=True, check=True).stdout


PRE = {}
for l in dump('presets').split('\n'):
    w = l.split()
    if w and w[0] == 'preset':
        PRE[int(w[1])] = dict(nxy=int(w[2]), nz=int(w[3]), cell=float(w[4]), step=float(w[5]), maxSteps=int(w[6]),
                              mem=float(w[7]))
    elif w and w[0] == 'const':
        CAP, SAFE, NEAR, MARGIN, SLACK, FRAMES = int(w[1]), int(w[2]), float(w[3]), float(w[4]), float(w[5]), int(w[6])
NAMES = {1: 'Low', 2: 'Medium', 3: 'High', 4: 'Ultra'}


def linked(p, name):
    ok = GLint()
    f_linkst(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL ' + name)
    return p


P_BAKE = linked(C.build(SDIR, 'cloud_flybake.prog'), 'cloud_flybake.prog')
P_SDF = linked(C.build(SDIR, 'cloud_flysdf.prog'), 'cloud_flysdf.prog')
P_FLY = linked(C.build(SDIR, 'cloud_fly.prog'), 'cloud_fly.prog')
P_MARCH = K.P_MARCH


def vol(nx, ny, nz):
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x806F, t)
    K.f_tex3d(0x806F, 0, 0x8229, nx, ny, nz, 0, 0x1903, 0x1401, None)
    for k, v in ((0x2801, 0x2600), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F), (0x8072, 0x812F)):
        gl.glTexParameteri(0x806F, k, v)
    return t.value


def vol_read(t, nx, ny, nz):
    gl.glBindTexture(0x806F, t)
    gl.glPixelStorei(0x0D05, 1)
    buf = (ctypes.c_ubyte * (nx * ny * nz))()
    gl.glGetTexImage(0x806F, 0, 0x1903, 0x1401, buf)
    gl.glPixelStorei(0x0D05, 4)
    return np.frombuffer(buf, np.uint8).reshape(nz, ny, nx).copy()


FB = GLuint()
g.genfb(1, ctypes.byref(FB))
ONE = (GLenum * 1)(0x8CE0)


def layers(prog, tex, n, nz, uni, texs, time_q=None):
    """one draw a layer into the R8 volume, as cloudfly.cpp runPass"""
    g.bindfb(0x8D40, FB.value)
    f_drawbufs(1, ONE)
    gl.glViewport(0, 0, n, n)
    g.use(prog)
    K.setuni(prog, uni)
    K.bind(prog, texs)
    if time_q:
        time_q[0]()
    for k in range(nz):
        f_fbl(0x8D40, 0x8CE0, tex, 0, k)
        g.u1i(g.loc(prog, b'cfLayer'), k)
        gl.glDrawArrays(5, 0, 4)
    if time_q:
        time_q[1]()


def blobs_map(n=256, seed=3, disc=3.0, cov=0.7):
    """R (Low) = cov in round blobs (0.5 .. 0.9 km) inside a disc round the camera, 0 elsewhere: empty space to skip"""
    rng = np.random.default_rng(abs(seed))
    x = (np.arange(n) + 0.5) / n * 2 * HALF - HALF
    X, Y = np.meshgrid(x, x)
    m = np.zeros((n, n), bool)
    m |= np.hypot(X - CAM[0], Y - CAM[1]) < (disc if seed < 0 else 0.7)   # one round the camera: it starts inside cloud
    for _ in range(14 if seed >= 0 else 0):
        a, rr = rng.uniform(0, 2 * math.pi), rng.uniform(0.5, disc - 0.6)
        m |= np.hypot(X - CAM[0] - rr * math.cos(a), Y - CAM[1] - rr * math.sin(a)) < rng.uniform(0.5, 0.9)
    m &= np.hypot(X - CAM[0], Y - CAM[1]) < disc
    img = np.zeros((n, n, 4))
    img[..., 0] = np.where(m, cov, 0.0)
    return img


MAPIMG = blobs_map()
MAPTEX = K.tex2d(MAPIMG)
K.MAP = MAPTEX
SH, DE = K.textures()


def bands(low=LOW, wind=0.0):
    return K.bands_uni(low, MID, K.cal, cov=(0.0, 0.0, 0.0), rect=(-HALF, -HALF, 1.0 / (2 * HALF), 1.0),
                       wind=(wind, 0.0, 0.0))


def org_of(tier, low=LOW):
    w = dump('place', tier, CAM[0], CAM[1], CAM[2], low[0], low[1]).split()
    return (float(w[1]), float(w[2]), float(w[3]))


def build(tier, wind=0.0, probe=False, time_q=None):
    """the grid as cloudfly.cpp builds it: bake (or the probe census), then the x, y, z distance passes"""
    p = PRE[tier]
    n, nz = p['nxy'], p['nz']
    org = org_of(tier)
    v = [vol(n, n, nz) for _ in range(3)]
    texs = [('cvShape', 0x806F, SH), ('cvDetail', 0x806F, DE), ('cvMap', 0x0DE1, MAPTEX)]
    uni = dict(bands(wind=wind), cfOrgKm=org, cfCellKm=(p['cell'],), cfMargin=(MARGIN,), cfSlackKm=(SLACK,), cfProbe=0)
    out = {'org': org}
    if probe:   # the census: the densest of 6^3 points a cell, float
        tgt = K.Mrt(n, n)
        dens = np.zeros((nz, n, n))
        for k in range(nz):
            K.draw(P_BAKE, tgt, dict(uni, cfProbe=1, cfLayer=k), texs)
            dens[k] = tgt.read()[..., 0]
        out['dens'] = dens
    tq = [None] * 4
    if time_q:
        tq = time_q
    layers(P_BAKE, v[0], n, nz, uni, texs, tq[0])
    for ax, (src, dst) in enumerate(((v[0], v[1]), (v[1], v[2]), (v[2], v[1]))):
        layers(P_SDF, dst, n, nz, {'cfAxis': ax, 'cfDim': (float(n), float(n), float(nz))}, [('cfSrc', 0x806F, src)],
               tq[1 + ax])
    out['occ'], out['sdftex'] = v[0], v[1]
    out['n'], out['nz'] = n, nz
    return out


def march(prog, W, H, fly=None, low=LOW, wind=0.0, steps=64, maxKm=80.0, extra=None, hist=None, stride=1,
          phase=(0.0, 0.0), tgt=None):
    cam = K.camera(25, 5)
    u = dict(bands(low, wind), **K.march_uni(cam, CAM, hist or (W, H), stride, phase, 30.0, sun_az=40, steps=steps,
                                             drop=K.drop, maxKm=maxKm))
    texs = K.march_texs(K.trans, K.trans)
    if fly is not None:
        p = PRE[fly['tier']]
        u.update({'cfOrgKm': fly['org'], 'cfDim': (float(fly['n']), float(fly['n']), float(fly['nz'])),
                  'cfCellKm': (p['cell'],), 'cfStepKm': (p['step'],), 'cfMaxSteps': fly.get('maxSteps', p['maxSteps']),
                  'cfSafe': (float(fly.get('safe', SAFE)),), 'cfOn': (fly.get('on', 1.0),), 'cfSkip': (fly.get('skip', 1.0),)})
        texs = texs + [('cfSdf', 0x806F, fly['sdftex'])]
    if extra:
        u.update(extra)
    tgt = tgt or K.Mrt(W, H)
    K.draw(prog, tgt, u, texs)
    return np.concatenate([tgt.read()[..., :4], tgt.read(1)[..., :1]], axis=-1)   # L, T, the mean cloud distance km


results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg))


def cmp(a, b):
    dT = np.abs(a[..., 3] - b[..., 3])
    dL = np.abs(a[..., :3] - b[..., :3]).mean() / max(np.abs(b[..., :3]).mean(), 1e-9)
    return float(dT.max()), float(dT.mean()), float(dL)


GRID = {}

# ---- B + D: the bound holds the density; the GPU distance field == the CPU one ---------------------------------------
if 'B' in only or 'D' in only:
    worst_b, worst_d, lines = 0, 0, []
    for tier in (1, 2, 3, 4):
        G = build(tier, probe=True)
        GRID[tier] = G
        n, nz = G['n'], G['nz']
        occ = vol_read(G['occ'], n, n, nz)
        if red == 'nobound':   # the red: the bound without the margin and the slack (a plain sample instead)
            occ = (G['dens'] > 0.05).astype(np.uint8)
        miss = int(((G['dens'] > 0.0) & (occ == 0)).sum())
        worst_b = max(worst_b, miss)
        o = os.path.join(scratch, 'occ_%d.bin' % tier)
        s = os.path.join(scratch, 'sdf_%d.bin' % tier)
        (occ > 0).astype(np.uint8).tofile(o)
        dump('sdf', o, n, n, nz, s)
        ref = np.fromfile(s, np.uint8).reshape(nz, n, n)
        gpu = vol_read(G['sdftex'], n, n, nz)
        bad = int((gpu != ref).sum())
        worst_d = max(worst_d, bad)
        lines.append('%s %dx%dx%d: dense cells %d, occupied %d, missed %d, empty %.1f%%, sdf diff %d of %d'
                     % (NAMES[tier], n, n, nz, int((G['dens'] > 0).sum()), int((occ > 0).sum()), miss,
                        100.0 * float((occ == 0).mean()), bad, gpu.size))
    if 'B' in only:
        gate('B bound', worst_b == 0, 'cells with density > 0 the grid calls empty: %d (bar 0) | %s' % (worst_b, '; '.join(lines)))
    if 'D' in only:
        gate('D sdf', worst_d == 0, 'GPU x, y, z passes vs cloudflymath.h chebyshev: %d cells differ (bar 0)' % worst_d)

W0, H0 = 192, 108


def grid(tier, wind=0.0):
    if wind == 0.0 and tier in GRID:
        return GRID[tier]
    G = build(tier, wind=wind)
    if wind == 0.0:
        GRID[tier] = G
    return G

# ---- F: jumping the free cells == stepping through every cell --------------------------------------------------------
if 'F' in only:
    lines, ok = [], True
    for tier in (1, 3, 4):
        G = dict(grid(tier), tier=tier, maxSteps=200000)
        ref = march(P_FLY, W0, H0, fly=dict(G, skip=0.0))
        got = march(P_FLY, W0, H0, fly=dict(G, safe=(-1 if red == 'sdfshrink' else SAFE)))
        mx, mn, dl = cmp(got, ref)
        ok &= mx <= 0.01 and dl <= 0.01
        lines.append('%s max|dT| %.5f mean %.6f |dL|/L %.5f (alpha in frame %.3f)' % (NAMES[tier], mx, mn, dl,
                                                                                     float(1 - ref[..., 3].mean())))
    gate('F skip', ok, 'camera inside the band at %.1f km; bars max|dT| <= 0.01, |dL|/L <= 0.01 | %s' % (CAM[2], '; '.join(lines)))

# ---- I: the inside look == a fine plain march ------------------------------------------------------------------------
if 'I' in only:
    ref = march(P_MARCH, W0, H0, steps=1536, maxKm=6.0)
    lines, ok = [], True
    for tier in (1, 3, 4):
        G = dict(grid(tier), tier=tier)
        if red == 'nobox':
            got = march(P_FLY, W0, H0, fly=dict(G, on=0.0))
        elif red == 'eyeheights':   # the bands above the eye again: the camera under them, no grid (not near a band)
            got = march(P_FLY, W0, H0, fly=dict(G, on=0.0), low=(LOW[0] + CAM[2], LOW[1] + CAM[2], LOW[2]))
        else:
            got = march(P_FLY, W0, H0, fly=G)
        da = float(np.abs(got[..., 3] - ref[..., 3]).mean())
        dl = float(np.abs(got[..., :3] - ref[..., :3]).mean() / max(ref[..., :3].mean(), 1e-9))
        # where the fog sits: the transmittance-weighted cloud distance (the aerial perspective and the scene test read it)
        m = (ref[..., 3] < 0.5) & (got[..., 3] < 0.5)
        dz = float(np.abs(got[..., 4] - ref[..., 4])[m].mean() / max(ref[..., 4][m].mean(), 1e-9)) if m.any() else 1.0
        ok &= da <= 0.03 and dl <= 0.1 and dz <= 0.15
        # INFO: the rows within 10 deg of the horizon (camera pitch 5, vfov 60), where the plain march's rays are longest
        ang = 5.0 + np.degrees(np.arctan(((np.arange(H0) + 0.5) / H0 * 2 - 1) * math.tan(math.radians(30))))
        mh = m & (np.abs(ang) < 10.0)[:, None]
        dzh = float(np.abs(got[..., 4] - ref[..., 4])[mh].mean() / max(ref[..., 4][mh].mean(), 1e-9)) if mh.any() else 1.0
        lines.append('%s mean|d alpha| %.4f |dL|/L %.4f |d depth|/depth %.4f (INFO near the horizon %.4f)'
                     % (NAMES[tier], da, dl, dz, dzh))
    gate('I inside', ok, 'reference alpha %.3f, cloud depth %.3f km (fine plain march); bars mean|d alpha| <= 0.03, '
         '|dL|/L <= 0.1, |d depth|/depth <= 0.15 (the depth bar added after red nobox passed the first two: in a thick '
         'cloud the colour and alpha saturate, where the fog sits does not) | %s'
         % (float(1 - ref[..., 3].mean()), float(ref[..., 4][ref[..., 3] < 0.5].mean()), '; '.join(lines)))

# ---- W: an aged grid with the age safety == the fine march at the new wind and the moved map ------------------------
if 'W' in only:
    lines, ok = [], True
    # 31.25 s of wind (0.25 km of noise) and a 0.75 km camera move (the cloud map follows the camera: the coverage moves
    # with it, cloudmap.cpp) = 1 km. Smaller ages cannot fail the red: the bake's map slack (0.25 km) and the shape bound
    # over mips 0..2 already hold 0.25 km of move + 0.25 km of wind (measured 10-26: no dense cell outside the old grid).
    age, wind_kms, move = 31.25, 0.008, 0.75
    w1 = age * wind_kms
    rect1 = (-HALF + move, -HALF, 1.0 / (2 * HALF), 1.0)
    for tier in (1, 2, 3, 4):
        cells = int(dump('age', tier, age, wind_kms, move).split()[1])
        G = dict(grid(tier), tier=tier, maxSteps=200000)   # baked at wind 0, the map at rect0
        ref = march(P_FLY, W0, H0, fly=dict(G, skip=0.0), wind=w1, extra={'cvBands.mapRect': rect1})
        safe = SAFE if red == 'stale' else SAFE + cells
        got = march(P_FLY, W0, H0, fly=dict(G, safe=safe), wind=w1, extra={'cvBands.mapRect': rect1})
        mx, mn, dl = cmp(got, ref)
        ok &= mx <= 0.01 and dl <= 0.01
        lines.append('%s age %d cells safe %d: max|dT| %.5f |dL|/L %.5f' % (NAMES[tier], cells, safe, mx, dl))
    gate('W age', ok, 'baked at wind 0, marched %.2f s later (%.2f km of wind) after a %.2f km camera move; bars as F | %s'
         % (age, w1, move, '; '.join(lines)))

# ---- T: cost at 1080p ------------------------------------------------------------------------------------------------
if 'T' in only:
    q = GLuint()
    S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(q))
    begq = S.fn('glBeginQuery', None, GLenum, GLuint)
    endq = S.fn('glEndQuery', None, GLenum)
    getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))

    def elapsed(fn, reps):
        gl.glFinish()
        begq(0x88BF, q)
        for _ in range(reps):
            fn()
        endq(0x88BF)
        ns = ctypes.c_uint64()
        getq(q, 0x8866, ctypes.byref(ns))
        return ns.value / 1e6 / reps

    HW, HH = 480, 270             # the High volume history at 1080p; the march draws 1/16 of it a frame
    MW, MH = (HW + 3) // 4, (HH + 3) // 4
    tgt = K.Mrt(MW, MH)
    common = dict(hist=(HW, HH), stride=4, phase=(1.0, 2.0), tgt=tgt)
    march(P_MARCH, MW, MH, **common)
    plain = elapsed(lambda: gl.glDrawArrays(5, 0, 4), 20)
    lines = []
    for tier in (1, 2, 3, 4):
        G = dict(grid(tier), tier=tier)
        march(P_FLY, MW, MH, fly=G, **common)
        flyms = elapsed(lambda: gl.glDrawArrays(5, 0, 4), 20)
        p = PRE[tier]
        n, nz = p['nxy'], p['nz']
        stamps = []
        tq = [((lambda: (gl.glFinish(), begq(0x88BF, q))), (lambda: (endq(0x88BF), stamps.append(q_ms())))) for _ in range(4)]

        def q_ms():
            ns = ctypes.c_uint64()
            getq(q, 0x8866, ctypes.byref(ns))
            return ns.value / 1e6
        build(tier, time_q=tq)
        full = sum(stamps)
        lpf = max(1, (4 * nz + FRAMES - 1) // FRAMES)
        roll = max(stamps) / nz * lpf   # the dearest pass's layers a frame
        lines.append('%s march +%.3f ms (fly %.3f vs plain %.3f), grid full build %.3f ms (bake %.3f, sdf %.3f/%.3f/%.3f), '
                     'rolling %.3f ms a frame (%d layers), memory %.2f MB'
                     % (NAMES[tier], flyms - plain, flyms, plain, full, stamps[0], stamps[1], stamps[2], stamps[3], roll,
                        lpf, p['mem'] / 1048576))
    print('T cost       INFO  1080p, the High volume preset (history %dx%d, march %dx%d a frame), camera inside the band '
          '(this machine GPU, offline): %s' % (HW, HH, MW, MH, ' | '.join(lines)))

print('CLOUDFLY-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
