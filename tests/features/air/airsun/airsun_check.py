#!/usr/bin/env python3
"""Lane PERFFIX3 (PERFRES1101 gap: the air's sun shadow read) and the 16F integrated volume, gated OFFLINE on the REAL
res/shaders/features/air/cell_volair.prog through this machine's GL driver in a hidden 4.1 core context (tests/spells/
shader_link_check.py + atmo_lut_check.py). No NifSkope, no game data.

    python -I tests/features/air/airsun/airsun_check.py <scratch> [--red noshadow|inject16] [--base <sha>] [--only ZAFC]

The scene (bars written before the first run): a 160 x 90 froxel grid (1080p at 12 px), 64 exponential slices from 16 to
30000 units, 60 deg vertical fov; a uniform air (sigma0 1/50000 per unit below a top 19000 units over the camera); the
sun from view direction (0.30, 0.80, 0.52); three cascades (half extents 1000 / 4000 / 16000 units, 1024^2 D32F,
splits 2000 / 6500, reach 16000, band 400) holding a field of smooth blobs of occluders 3000 units toward the sun: shafts
through every slice the cascades reach. Only the directional light is on (volTerms 1).
  Z off      volSunAir 0 (red WW_CELL_AIR_RED=sunpcf) == today's program (cell_volair.frag at --base), every froxel's RGBA
             bit for bit. Bar: max |diff| == 0.
  A accuracy the integrated light L(k) (what every surface reads) against the converged disc filter (today's Vogel disc,
             48 taps x 32 screen rotations averaged), relative to the frame's largest L:
               new (volSunAir 1)  p99 |err| <= 0.02, mean |err| <= 0.004, and mean |err| <= 2 x today's
             (today = the surfaces' read as shipped: world-locked rotation, the preset's taps). The integrated T is
             untouched by the sun read: |dT| == 0.
  F format   the integrated volume rendered into RGBA16F (the new default) vs RGBA32F (today), the integrate stage of the
             real program: max |dT| <= 5e-4 (half a 16F step below 1), |dL| <= 1e-3 x L + 1e-6 x Lmax on every froxel.
  C cost     GPU ms of the 64 inject draws at 160 x 90 (GL_TIME_ELAPSED, median of 15): today with 32 and 44 Vogel taps vs
             the new read. Bar: new <= 0.6 x today(32 taps). INFO otherwise.
Reds: noshadow (the air's sun unshadowed: A must FAIL), inject16 (the per-slice transmittance also in 16F: F must FAIL;
why the injected volume stays RGBA32F).
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, shutil, subprocess, sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'airsun_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ZAFC')
BASE = args[args.index('--base') + 1] if '--base' in args else '4c9ea88e'
WORLD = '--world' in args   # FO4CS-level shadows on: the world-unit radius (2.4 / 9 / 36), 8 taps a cascade
os.makedirs(scratch, exist_ok=True)

import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = args[args.index('--sdir') + 1] if '--sdir' in args else os.path.join(TREE, 'res', 'shaders')   # variants
results = []


def gate(name, ok, msg):
    results.append(bool(ok))
    print('%-10s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


# today's shaders: a scratch copy of res/shaders with cell_volair.frag from --base
BDIR = os.path.join(scratch, 'base_shaders')
if os.path.isdir(BDIR):
    shutil.rmtree(BDIR)
shutil.copytree(SDIR, BDIR)
GIT = shutil.which('git') or 'E:/Tools/Git/cmd/git.exe'
r = subprocess.run([GIT, '-C', TREE, 'show', BASE + ':res/shaders/cell_volair.frag'], capture_output=True)
if r.returncode != 0:
    raise SystemExit('git show failed: ' + r.stderr.decode('latin-1'))
open(os.path.join(BDIR, 'cell_volair.frag'), 'wb').write(r.stdout)

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
f_tex3d = S.fn('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum,
               ctypes.c_void_p)
f_fbtl = S.fn('glFramebufferTextureLayer', None, GLenum, GLenum, GLuint, GLint, GLint)
SAMPLERS = set(range(0x8B5D, 0x8B65)) | set(range(0x8DC0, 0x8DDA)) | set(range(0x9108, 0x910E)) | set(range(0x900C, 0x9010))


def program(sdir):
    p = C.build(sdir, 'cell_volair.prog')
    ok = GLint()
    f_pget(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL cell_volair.prog in ' + sdir)
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


# ---- the scene ----------------------------------------------------------------------------------------------------------
COLS, ROWS, SL = 160, 90, 64
NEAR, FAR = 16.0, 30000.0
P11 = 1.0 / math.tan(math.radians(30.0))
P00 = P11 * ROWS / COLS
Lv = np.array([0.30, 0.80, 0.52])
Lv /= np.linalg.norm(Lv)
E1 = np.cross(Lv, [0.0, 0.0, 1.0])
E1 /= np.linalg.norm(E1)
E2 = np.cross(Lv, E1)
HALF = [1000.0, 4000.0, 16000.0]
RANGE = 40000.0
MAPN = 1024
OCC = 3000.0   # the occluders' distance toward the sun (view units)


def occluded(a, b):   # the blob field in light-plane coordinates (view units)
    return (np.sin(a / 173.0) * np.sin(b / 251.0) + 0.6 * np.sin((a + b) / 97.0)) > 0.35


def shadow_map():
    data = np.ones((3, MAPN, MAPN), np.float32)
    c = (np.arange(MAPN) + 0.5) / MAPN - 0.5
    for i, h in enumerate(HALF):
        A, B = np.meshgrid(c * 2 * h, c * 2 * h)   # rows = v, cols = u
        data[i][occluded(A, B)] = 0.5 - OCC / (2 * RANGE)
    data = np.ascontiguousarray(data)
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x8C1A, t)
    f_tex3d(0x8C1A, 0, 0x8CAC, MAPN, MAPN, 3, 0, 0x1902, 0x1406, data.ctypes.data)
    for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F), (0x884C, 0x884E), (0x884D, 0x0203)):
        gl.glTexParameteri(0x8C1A, k, v)
    return t.value, data


def csm_mat(i):
    M = np.zeros((4, 4))
    M[0, :3] = E1 / (2 * HALF[i])
    M[0, 3] = 0.5
    M[1, :3] = E2 / (2 * HALF[i])
    M[1, 3] = 0.5
    M[2, :3] = -Lv / (2 * RANGE)
    M[2, 3] = 0.5
    M[3, 3] = 1.0
    return M


def setu(p, name, v):
    l = G.loc(p, name.encode())
    if l < 0:
        return
    if isinstance(v, bool) or isinstance(v, int):
        G.u1i(l, int(v))
    elif isinstance(v, float):
        G.u1f(l, v)
    elif isinstance(v, np.ndarray) and v.shape == (4, 4):
        m = np.ascontiguousarray(v.T.astype(np.float32))
        f_um4(l, 1, 0, m.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
    elif len(v) == 2:
        G.u2f(l, *[float(x) for x in v])
    elif len(v) == 3:
        G.u3f(l, *[float(x) for x in v])
    else:
        f_u4f(l, *[float(x) for x in v])


def uniforms(p, units, csmtex, taps=32, radius=3.0, stable=1, phase=0.0, sunair=1, noshadow=False, world=WORLD):
    G.use(p)
    setu(p, 'volStage', 1)
    l = G.loc(p, b'volDims')
    f_u3i(l, COLS, ROWS, SL)
    setu(p, 'volProj', (P00, P11))
    setu(p, 'volSlices', (NEAR, math.log(FAR / NEAR), float(SL), 1.0))
    setu(p, 'fogView', (0.0, 1.0, 0.0, 1000.0))
    setu(p, 'fogDistScale', 1.0)
    setu(p, 'fogOn', 1)   # off zeroes the in-scatter (cell_volair.frag's tail)
    setu(p, 'volPhase[0]', (1.0, 1.0, 1.0, 0.0))   # the air lobe alone (Rayleigh shape)
    setu(p, 'volPhase[1]', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'volPhase[2]', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'airMed', (1.0 / 50000.0, 0.0, 0.0, 20000.0))
    for n in ('airLayer', 'airGround', 'volDrift', 'volMs'):
        setu(p, n, (0.0, 0.0, 0.0, 0.0))
    for n in ('airTerrOn', 'volTemporal', 'volDepthSkip', 'volFarMode', 'volLayerBase', 'rfOn', 'cellLightCount',
              'cellBulbCount', 'ussTerrOn', 'ussContactOn', 'ussCloudOn', 'ussView', 'ussRed', 'csmRed', 'csmProbe'):
        setu(p, n, 0)
    setu(p, 'volRed', 256 | (8 if noshadow else 0))   # 256 physcol: the light raw (no record colour: no fogK needed)
    setu(p, 'volTerms', 1)
    setu(p, 'volJitter', 0.0)
    setu(p, 'cellOn', 1)
    setu(p, 'cellHasDir', 1)
    setu(p, 'cellInterior', 0)
    setu(p, 'cellDirTo', tuple(Lv))
    setu(p, 'cellDirColor', (1.0, 1.0, 1.0))
    for k in range(3):
        row = [0.0, 0.0, 0.0, 0.0]
        row[k] = 1.0
        setu(p, 'cellRow[%d]' % k, tuple(row))
    setu(p, 'volCsm', 1)
    setu(p, 'volSunAir', int(sunair))
    setu(p, 'volSunRoom', 0)
    setu(p, 'ussOn', 1)
    setu(p, 'ussCsmOn', 1)
    setu(p, 'csmForce', -1.0)
    for i in range(3):
        setu(p, 'csmMat[%d]' % i, csm_mat(i))
    setu(p, 'csmInvRange', tuple([1.0 / (2 * RANGE)] * 3))
    setu(p, 'csmParams', (1.0, 16000.0, float(MAPN), 400.0))
    setu(p, 'csmSplitOffset', (2000.0, 6500.0, 0.275, 1.0))
    setu(p, 'ussCsmFilter', (1.0, radius, float(taps), 0.0))   # the air gets no receiver plane (shadowset.cpp: !volume)
    setu(p, 'ussCsmBias', (12.0, 12.0, 12.0))
    setu(p, 'ussCsmWorld', (1.0, 2.4, 9.0, 36.0) if world else (0.0, 0.0, 0.0, 0.0))   # FO4CS-level shadows: world radius, 8 taps
    setu(p, 'ussCsmTexel', tuple(2 * h / MAPN for h in HALF))
    setu(p, 'ussCsmPhase', phase)
    setu(p, 'ussCsmStable', (float(stable), 0.0, 0.0, 0.0))
    setu(p, 'ussCsmCellX', (0.0, 0.0, 0.0))
    setu(p, 'ussCsmCellY', (0.0, 0.0, 0.0))
    G.active(0x84C0 + units['csmMap'])
    gl.glBindTexture(0x8C1A, csmtex)


TG = {}


def target(fmt):
    if fmt not in TG:
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x0DE1, t)
        gl.glTexImage2D(0x0DE1, 0, fmt, COLS, ROWS, 0, 0x1908, 0x1406, None)
        for k, v in ((0x2801, 0x2600), (0x2800, 0x2600)):
            gl.glTexParameteri(0x0DE1, k, v)
        fb = GLuint()
        G.genfb(1, ctypes.byref(fb))
        G.bindfb(0x8D40, fb)
        G.fbtex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        TG[fmt] = fb.value
    return TG[fmt]


def inject(p, layers=SL):
    G.bindfb(0x8D40, target(0x8814))
    gl.glViewport(0, 0, COLS, ROWS)
    G.use(p)
    l = G.loc(p, b'volLayer')
    out = np.zeros((layers, ROWS, COLS, 4))
    buf = (ctypes.c_float * (COLS * ROWS * 4))()
    for k in range(layers):
        G.u1i(l, k)
        gl.glDrawArrays(5, 0, 4)
        gl.glReadPixels(0, 0, COLS, ROWS, 0x1908, 0x1406, buf)
        out[k] = np.frombuffer(buf, np.float32).reshape(ROWS, COLS, 4)
    return out


def integrate(inj):   # front to back, float64 (the stage's law)
    L = np.zeros(inj.shape[1:3] + (3,))
    T = np.ones(inj.shape[1:3])
    outL = np.zeros(inj.shape[:3] + (3,))
    outT = np.zeros(inj.shape[:3])
    for k in range(inj.shape[0]):
        L = L + T[..., None] * inj[k, ..., :3]
        T = T * inj[k, ..., 3]
        outL[k], outT[k] = L, T
    return outL, outT


CSM, MAPDATA = shadow_map()
P_NEW, U_NEW = program(SDIR)
P_OLD, U_OLD = program(BDIR)

# ---- Z: off == today --------------------------------------------------------------------------------------------------
if 'Z' in only:
    uniforms(P_OLD, U_OLD, CSM)
    a = inject(P_OLD)
    uniforms(P_NEW, U_NEW, CSM, sunair=0)
    b = inject(P_NEW)
    d = float(np.abs(a - b).max())
    lit = float(a[..., :3].max())
    gate('Z off', d == 0.0 and lit > 0.0, 'volSunAir 0 vs today\'s program (%s): max |diff| %.3g over %d values (the frame lit: '
         'max in-scatter %.3g, min T %.4f)' % (BASE, d, a.size, lit, float(a[..., 3].min())))

# ---- A: accuracy against the converged disc --------------------------------------------------------------------------
if 'A' in only:
    acc = None
    NPH = 32
    for ph in range(NPH):
        uniforms(P_OLD, U_OLD, CSM, taps=48, stable=0, phase=float(ph * 1.7))
        x = inject(P_OLD)
        acc = x if acc is None else acc + x
    ref = acc / NPH
    uniforms(P_OLD, U_OLD, CSM, taps=32)
    today = inject(P_OLD)
    uniforms(P_NEW, U_NEW, CSM, sunair=1, noshadow=(red == 'noshadow'))
    new = inject(P_NEW)
    Lr, Tr = integrate(ref)
    Lt, Tt = integrate(today)
    Ln, Tn = integrate(new)
    m = Lr.max()
    et = np.abs(Lt - Lr).max(axis=-1) / m
    en = np.abs(Ln - Lr).max(axis=-1) / m
    lit = Lr.max(axis=-1) > 1e-4 * m
    p99n, meann, meant = float(np.percentile(en[lit], 99)), float(en[lit].mean()), float(et[lit].mean())
    dT = float(np.abs(Tn - Tr).max())
    # the shafts are there: the reference's light in the deepest shadowed vs lit columns differs
    shaft = float((Lr[-1].max(axis=-1).max() - Lr[-1].max(axis=-1).min()) / m)
    ok = p99n <= 0.02 and meann <= 0.004 and meann <= 2.0 * max(meant, 1e-9) and dT == 0.0
    gate('A accuracy', ok, 'integrated L vs the converged disc (48 taps x %d rotations): new p99 %.4f mean %.5f, today (32 taps, '
         'world-locked) p99 %.4f mean %.5f; |dT| %.3g; shaft contrast %.3f%s'
         % (NPH, p99n, meann, float(np.percentile(et[lit], 99)), meant, dT, shaft, ' [red noshadow]' if red == 'noshadow' else ''))

# ---- F: the integrated volume in RGBA16F -------------------------------------------------------------------------------
if 'F' in only:
    uniforms(P_NEW, U_NEW, CSM, sunair=1)
    inj = inject(P_NEW)
    if red == 'inject16':
        inj = inj.astype(np.float16).astype(np.float64)
    # the injected volume as a 3D texture (32F), then the real integrate stage (2) into a 32F and a 16F target per layer
    tex = GLuint()
    gl.glGenTextures(1, ctypes.byref(tex))
    gl.glBindTexture(0x806F, tex)
    a32 = np.ascontiguousarray(inj.astype(np.float32))
    f_tex3d(0x806F, 0, 0x8814, COLS, ROWS, SL, 0, 0x1908, 0x1406, a32.ctypes.data)
    for k, v in ((0x2801, 0x2600), (0x2800, 0x2600)):
        gl.glTexParameteri(0x806F, k, v)
    outs = {}
    for fmt in (0x8814, 0x881A):   # RGBA32F, RGBA16F
        G.bindfb(0x8D40, target(fmt))
        gl.glViewport(0, 0, COLS, ROWS)
        G.use(P_NEW)
        setu(P_NEW, 'volStage', 2)
        setu(P_NEW, 'volLayerBase', 0)
        G.active(0x84C0 + U_NEW['volInject'])
        gl.glBindTexture(0x806F, tex)
        l = G.loc(P_NEW, b'volLayer')
        o = np.zeros((SL, ROWS, COLS, 4))
        buf = (ctypes.c_float * (COLS * ROWS * 4))()
        for k in range(SL):
            G.u1i(l, k)
            gl.glDrawArrays(5, 0, 4)
            gl.glReadPixels(0, 0, COLS, ROWS, 0x1908, 0x1406, buf)
            o[k] = np.frombuffer(buf, np.float32).reshape(ROWS, COLS, 4)
        outs[fmt] = o
    if red == 'inject16':   # judge the 16F-inject path against the 32F inject's integrate
        uniforms(P_NEW, U_NEW, CSM, sunair=1)
        ref32 = integrate(inject(P_NEW))
        o16 = outs[0x8814]
        dT = float(np.abs(o16[..., 3] - ref32[1]).max())
        Lm = ref32[0].max()
        dL = np.abs(o16[..., :3] - ref32[0])
        okL = bool((dL <= 1e-3 * ref32[0] + 1e-6 * Lm).all())
    else:
        o32, o16 = outs[0x8814], outs[0x881A]
        dT = float(np.abs(o16[..., 3] - o32[..., 3]).max())
        Lm = o32[..., :3].max()
        dL = np.abs(o16[..., :3] - o32[..., :3])
        okL = bool((dL <= 1e-3 * np.abs(o32[..., :3]) + 1e-6 * Lm).all())
    gate('F 16F', dT <= 5e-4 and okL, 'integrated volume 16F vs 32F (the real integrate stage): max |dT| %.2e (bar 5e-4), every '
         '|dL| <= 1e-3 L + 1e-6 Lmax: %s%s' % (dT, okL, ' [red inject16: the per-slice T in 16F]' if red == 'inject16' else ''))
    gate('F bytes', True, 'INFO the integrated volume 160x90x64: %.1f MB 32F -> %.1f MB 16F' % (COLS * ROWS * SL * 16 / 2**20,
                                                                                            COLS * ROWS * SL * 8 / 2**20))

# ---- C: cost ----------------------------------------------------------------------------------------------------------
if 'C' in only:
    q = (GLuint * 1)()
    f_genq(1, q)

    def timed(p, **kw):
        uniforms(p, U_NEW if p == P_NEW else U_OLD, CSM, **kw)
        G.bindfb(0x8D40, target(0x8814))
        gl.glViewport(0, 0, COLS, ROWS)
        l = G.loc(p, b'volLayer')
        ms = []
        for rep in range(15):
            gl.glFinish()
            f_begq(0x88BF, q[0])
            for k in range(SL):
                G.u1i(l, k)
                gl.glDrawArrays(5, 0, 4)
            f_endq(0x88BF)
            r = ctypes.c_uint64()
            f_getq(q[0], 0x8866, ctypes.byref(r))
            ms.append(r.value / 1e6)
        return float(np.median(ms))
    t32 = timed(P_OLD, taps=32)
    t44 = timed(P_OLD, taps=44, radius=3.5)
    tn = timed(P_NEW, sunair=1)
    tn0 = timed(P_NEW, sunair=0, taps=32)
    # amended after the first --world run (before any red run): with FO4CS-level shadows the preset disc is already 8 taps,
    # so the bar there is new <= today (the 0.6 x bar is the preset regime's)
    gate('C cost', tn <= (t32 if WORLD else 0.6 * t32), 'inject 160x90x64 sun only, median of 15 (this machine\'s GPU, offline): today 32 taps %.3f ms, '
         '44 taps %.3f ms; new read %.3f ms; new program red sunpcf (32 taps) %.3f ms' % (t32, t44, tn, tn0))

print('AIRSUN-CHECK %s (%d of %d)%s' % ('PASS' if results and all(results) else 'FAIL', sum(results), len(results),
                                      ' [red %s]' % red if red else ''))
if __name__ == '__main__':
    sys.exit(0 if results and all(results) else 1)
