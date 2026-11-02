#!/usr/bin/env python3
"""Lane FOGLEAK1 gate G2: the fog leak fix on the REAL res/shaders/cell_volair.prog, offline, through this machine's GL
driver in a hidden 4.1 core context (tests/spells/shader_link_check.py + atmo_lut_check.py, the scene of
tests/airsun/airsun_check.py). No NifSkope, no game data.

    python -I tests/fogleak/fogleak_check.py [--red dalcenv|nosungate|lightleak] [--only AEBLRC]

Programs: ON = the loader's WW_CELL_RTDDGI + WW_CELL_FOGLEAK (cell_volair.frag WW_AIR_LEAK), OFF = WW_CELL_RTDDGI alone
(today). The defines are pasted in after #version as the loader does (shader_link_check.inject). A red sets the on
program's volLeakRed bit as cellvolair.cpp does for WW_CELL_AIR_RED=<token> (1 dalcenv, 2 nosungate, 4 lightleak).
The scene: 160 x 90 froxels, 64 exponential slices 16 .. 30000 units, 60 deg fov, world = view (camera at 0 looking -z),
a uniform air (sigma0 1/50000). A synthetic ray-traced volume (one RGBA32F texture buffer in cell_ddgi.glsl's layout:
irradiance tiles, distance tiles, probe states, every probe usable). A froxel is classed by its centre at both readings
of the slice distance (along the ray / view depth) and both slice ends; a class holds a froxel only if all four agree.
Bars (written before the first run):
  A env     volume A (step 4000, x >= 0 inside, x <= -4000 outside), irradiance E(d) = E0 (1 + 0.5 d.z), E0 = 0.3 pi x the
            flat DALC ambient, the ambient term only (volTerms 8). Inside: on / off vs the law (E0 / pi A)(1 + 0.75 g dW.z)
            p99 |err| <= 0.03. Outside: on == off (|d| <= 1e-5 x off + 1e-7 x max).
  E env out the same run: inside the on / off median <= 0.5 (the flat ambient no longer lights the shadowed air).
  B sun     volume A, sky visibility 0.05 every texel (a roof), a clear shadow map, the sun only (volTerms 1). Past the
            cascades' reach (view depth > 16400) inside: on / off max <= 0.01. Near (|P| < 3000) inside: on / off
            min >= 0.995. Outside: on == off. Open ground (visibility 0.6): on == off on every froxel.
  L light   volume C (step 1000), a wall x = 2500 in the distance tiles, one unshadowed omni light at (4000, 0, -9000)
            radius 6000 (volTerms 2). Lit by the light (off > 1e-3 x max): near side (cell probes x <= 2000) on / off
            median <= 0.10 and p95 <= 0.25; far side (cell probes x >= 3000) median >= 0.95 and p5 >= 0.80.
  R restir  L again with WW_CELL_RESTIR, the light a shadow caster under ReSTIR (cellRestirOn): the same bars. And a
            cubed light (cellShadowOn, a slot, ReSTIR off, a lit cube map): on == off (its map stays the verdict).
  I int     lane INTGI1 (FOGLEAK1-INT): an unlit interior (cellInterior 1: no shadow map, the directional's air vis 1
            today), volume A, the directional only (volTerms 1). Sky visibility 0.05 (a closed room): inside, on / off
            max <= 0.01 at every depth (no cascade cover indoors); outside on == off. Visibility 0.6 (an opening): on == off
            on every froxel. Red nointsungate (16) fails the closed row only.
  S scale   lane FOGENV1 (the absolute gate: the DDGI env against the DALC it replaces, which the ratio rows cannot
            see): volume A open (sky vis 0.6), E(d) = K pi A (1 + 0.5 d.z) for K = 0.3, 3, 30, and the volume's open
            field (cellDdgiOpenE) the same E on the six axes, the ambient only (volTerms 8). Inside: on / off median in
            [0.95, 1.05], p1 >= 0.85, p99 <= 1.15 for every K. S occl: K = 3 with an open field 1 / 0.3 of the read
            (the probes see 0.3 of an open probe's light): median in [0.25, 0.36]. Red envscale (32, the read's own
            E / pi, FOGLEAK1 fix 1 as it was) fails both. A and E run with no open field (cellDdgiOpenE 0): the read's
            own scale, the law they always checked.
  N sun1    lane FOGENV1 (FOGLEAK1-COST): the sun's gate reads ONE tile (the walk's heaviest probe) instead of 8. An
            unlit interior (the gate owns the directional), volume A, the roof (sky vis 0.05) on probes x <= 12000 and
            open (0.6) on x >= 16000. Against the red sun8 (64, the 8-probe read): froxels with x <= 11000 or >= 17000
            (all 4 readings) equal (|d| <= 1e-5 x max); in the band between, the mean |on - red| / off <= 0.20 (a step at
            the probes' midpoint where the old read ramps). Its cost is fogleak_costbench.py --gate-sun.
  C cost    INFO: GPU ms of the 64 inject draws, every term (sun + light + ambient), on vs off (median of 9).
Amended after the first run, before any red run: A's reference reads the synthetic tile as the shader does (tile_E,
the octahedral interpolation of the 8 x 8 tile), not the analytic E(d): the analytic one read 0 - 8.8 % high, one-signed,
the same in every slice. Measured 11-02 (this GPU): two cellDdgiRead calls cost +2.3 ms on all three terms; the one
probe walk (airLeakEnv) +1.04 ms (off 0.601, on 1.645).
Reds (each must FAIL its rows): dalcenv (A, E), nosungate (B, I), lightleak (L, R), nointsungate (I closed only),
envscale (S).
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
args = sys.argv[1:]
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('AEBILRSNC')
REDBIT = {'': 0, 'dalcenv': 1, 'nosungate': 2, 'lightleak': 4, 'nointsungate': 16, 'envscale': 32, 'sun8': 64}[red]
SDIR = os.path.join(TREE, 'res', 'shaders')

import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
results = []


def gate(name, ok, msg):
    results.append(bool(ok))
    print('%-10s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


G = C.Gl()
f_u4f = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
f_u4i = S.fn('glUniform4i', None, GLint, GLint, GLint, GLint, GLint)
f_u3i = S.fn('glUniform3i', None, GLint, GLint, GLint, GLint)
f_um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
f_pget = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
f_plog = S.fn('glGetProgramInfoLog', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.c_char_p)
f_active = S.fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_int),
                ctypes.POINTER(GLint), ctypes.POINTER(GLenum), ctypes.c_char_p)
f_tex3d = S.fn('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum,
               ctypes.c_void_p)
f_genb = S.fn('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
f_bindb = S.fn('glBindBuffer', None, GLenum, GLuint)
f_bdata = S.fn('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
f_texbuf = S.fn('glTexBuffer', None, GLenum, GLenum, GLuint)
f_genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))
f_begq = S.fn('glBeginQuery', None, GLenum, GLuint)
f_endq = S.fn('glEndQuery', None, GLenum)
f_getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
SAMPLERS = set(range(0x8B5D, 0x8B65)) | set(range(0x8DC0, 0x8DDA)) | set(range(0x9108, 0x910E)) | set(range(0x900C, 0x9010))


def build(defines):
    prog = 'cell_volair.prog'
    files = [os.path.join(SDIR, f) for f in
             [l.split()[1:] for l in open(os.path.join(SDIR, prog)) if l.startswith('shaders')][0]]
    p = S.fn('glCreateProgram', GLuint)()
    for f in files:
        src = S.inject(S.expand(f), defines)   # the loader's define paste (glcontext.cpp Shader::load)
        s = S.fn('glCreateShader', GLuint, GLenum)(0x8B30 if f.endswith('frag') else 0x8B31)
        b = ctypes.c_char_p(src.encode('latin-1'))
        S.fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)(s, 1, ctypes.byref(b), None)
        S.fn('glCompileShader', None, GLuint)(s)
        ok = GLint()
        S.fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            raise SystemExit('COMPILE FAIL %s %s %s' % (prog, f, defines))
        S.fn('glAttachShader', None, GLuint, GLuint)(p, s)
    S.fn('glLinkProgram', None, GLuint)(p)
    ok = GLint()
    f_pget(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        buf = ctypes.create_string_buffer(4096)
        f_plog(p, 4096, None, buf)
        raise SystemExit('LINK FAIL %s %s\n%s' % (prog, defines, buf.value.decode('latin-1')[-1500:]))
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


# ---- the scene (airsun_check's) ----------------------------------------------------------------------------------------
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
MAPN = 256
GI_G = 0.5


def tex_buffer(data):
    b = GLuint()
    f_genb(1, ctypes.byref(b))
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    a = np.ascontiguousarray(data.astype(np.float32))
    f_bindb(0x8C2A, b)
    f_bdata(0x8C2A, a.nbytes, a.ctypes.data, 0x88E4)
    gl.glBindTexture(0x8C2A, t)
    f_texbuf(0x8C2A, 0x8814, b)
    return t.value


def clear_csm():   # a clear shadow map: the cascades say lit everywhere
    data = np.ones((3, MAPN, MAPN), np.float32)
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x8C1A, t)
    f_tex3d(0x8C1A, 0, 0x8CAC, MAPN, MAPN, 3, 0, 0x1902, 0x1406, data.ctypes.data)
    for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F), (0x884C, 0x884E), (0x884D, 0x0203)):
        gl.glTexParameteri(0x8C1A, k, v)
    return t.value


def lit_cube():   # a lit shadow cube array (every depth 1): a cubed light keeps its full light
    data = np.ones((6, 4, 4), np.float32)
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x9009, t)
    f_tex3d(0x9009, 0, 0x8CAC, 4, 4, 6, 0, 0x1902, 0x1406, data.ctypes.data)
    for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x884C, 0x884E), (0x884D, 0x0203)):
        gl.glTexParameteri(0x9009, k, v)
    return t.value


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


# ---- the synthetic ray-traced volume (cell_ddgi.glsl's layout) ---------------------------------------------------------
def tile_dirs(S_):
    out = np.zeros((S_ * S_, 3))
    n = S_ - 2
    for t in range(S_ * S_):
        x, y = t % S_, t // S_
        # the border texels as border.comp fills them: the octahedral wrap ((1 + e, v) is the direction of (1 - e, -v))
        u = ((x - 1 + 0.5) / n) * 2.0 - 1.0
        v = ((y - 1 + 0.5) / n) * 2.0 - 1.0
        if abs(u) > 1.0:
            u, v = math.copysign(2.0, u) - u, -v
        if abs(v) > 1.0:
            u, v = -u, math.copysign(2.0, v) - v
        dz = 1.0 - abs(u) - abs(v)
        if dz < 0.0:
            u, v = (1.0 - abs(v)) * (1.0 if u >= 0 else -1.0), (1.0 - abs(u)) * (1.0 if v >= 0 else -1.0)
        out[t] = np.array([u, v, dz]) / math.sqrt(u * u + v * v + dz * dz)
    return out


D8, D16 = tile_dirs(8), tile_dirs(16)


class Volume:
    def __init__(self, origin, step, dims, E_of_d, alpha, wall=None, far=3.0e4, alpha_x=None):
        self.origin, self.step, self.dims = np.array(origin, float), float(step), dims
        nx, ny, nz = dims
        nP = nx * ny * nz
        self.nP = nP
        E = np.maximum(E_of_d(D8), 0.0)   # irradiance at each texel's direction (decoded = enc^5 x 2 pi)
        enc = (E / (2.0 * math.pi)) ** 0.2
        irr = np.zeros((64, 4))
        irr[:, :3] = enc[:, None]
        irr[:, 3] = alpha
        irr = np.tile(irr, (nP, 1))
        q = np.stack(np.meshgrid(np.arange(nx), np.arange(ny), np.arange(nz), indexing='ij'), -1)
        q = q.transpose(2, 1, 0, 3).reshape(-1, 3)   # p = x + nx (y + ny z)
        pp = self.origin[None, :] + q * step
        if alpha_x is not None:   # lane FOGENV1 row N: the sky visibility per probe by its x
            irr = irr.reshape(nP, 64, 4)
            irr[:, :, 3] = alpha_x(pp[:, 0])[:, None]
            irr = irr.reshape(-1, 4)
        if wall is None:
            m = np.full((nP, 256), far)
        else:   # distance to the plane x = wall along each texel's direction (far past it)
            dx = D16[None, :, 0]
            with np.errstate(divide='ignore', invalid='ignore'):
                t = (wall - pp[:, 0:1]) / dx
            m = np.where((t > 0) & np.isfinite(t), np.minimum(t, far), far)
        dist = np.stack([m, m * m], -1).reshape(nP, 128, 4)
        probes = np.zeros((nP, 4))
        probes[:, 3] = 2.0   # usable (Vigilant, aged)
        self.data = np.concatenate([irr, dist.reshape(-1, 4), probes], 0)
        self.tex = tex_buffer(self.data)

    def tile_E(self, d):   # cellDdgiTile( p, d, 8 ) then the read's decode (every probe holds the same tile)
        enc = self.data[:64, 0]
        l1 = np.abs(d).sum(-1)
        u, v = d[..., 0] / l1, d[..., 1] / l1
        neg = d[..., 2] < 0.0
        sgn = lambda a: np.where(a >= 0.0, 1.0, -1.0)
        u, v = np.where(neg, (1.0 - np.abs(v)) * sgn(u), u), np.where(neg, (1.0 - np.abs(u)) * sgn(v), v)
        fx = 1.0 + (u * 0.5 + 0.5) * 6.0 - 0.5
        fy = 1.0 + (v * 0.5 + 0.5) * 6.0 - 0.5
        x0 = np.clip(np.floor(fx).astype(int), 0, 6)
        y0 = np.clip(np.floor(fy).astype(int), 0, 6)
        ax, ay = np.clip(fx - x0, 0.0, 1.0), np.clip(fy - y0, 0.0, 1.0)
        t = lambda x, y: enc[y * 8 + x]
        e = (t(x0, y0) * (1 - ax) + t(x0 + 1, y0) * ax) * (1 - ay) + (t(x0, y0 + 1) * (1 - ax) + t(x0 + 1, y0 + 1) * ax) * ay
        return np.maximum(e, 0.0) ** 5 * 2.0 * math.pi

    def lo_hi_fade1(self):   # the box where the read's fade is 1 (one step inside the faces)
        lo = self.origin + self.step
        hi = self.origin + self.step * (np.array(self.dims) - 2)
        return lo, hi


# ---- the froxels' centres -------------------------------------------------------------------------------------------
cx = ((np.arange(COLS) + 0.5) / COLS * 2 - 1) / P00
cy = ((np.arange(ROWS) + 0.5) / ROWS * 2 - 1) / P11
V = np.stack(np.broadcast_arrays(cx[None, :], cy[:, None], -np.ones((ROWS, COLS))), -1)
U = V / np.linalg.norm(V, axis=-1, keepdims=True)
lr = math.log(FAR / NEAR)
TS = [NEAR * np.exp(lr * (np.arange(SL) + f) / SL) for f in (0.0, 0.5, 1.0)]
# four readings a froxel: along the ray / view depth x the slice's two ends
PTS = [t[:, None, None, None] * B[None] for t in (TS[0], TS[2]) for B in (U, V)]
PMID = TS[1][:, None, None, None] * U[None]


def all_in(lo, hi, m=0.0):
    ok = np.ones((SL, ROWS, COLS), bool)
    for P in PTS:
        ok &= np.all((P >= lo + m) & (P <= hi - m), axis=-1)
    return ok


def all_where(f):
    ok = np.ones((SL, ROWS, COLS), bool)
    for P in PTS:
        ok &= f(P)
    return ok


# ---- uniforms ---------------------------------------------------------------------------------------------------------
def setu(p, name, v, ints=False):
    l = G.loc(p, name.encode())
    if l < 0:
        return
    if ints:
        f_u4i(l, *[int(x) for x in v])
    elif isinstance(v, bool) or isinstance(v, int):
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


DALC_M = 0.5
A_FLAT = DALC_M ** 2.2


def uniforms(p, units, vol, terms, leakred=0, light=None, shadow=False, restir=False, interior=False, openE=None):
    G.use(p)
    for k in range(6):   # lane FOGENV1: the volume's open field (none: 0, the read's own scale)
        setu(p, 'cellDdgiOpenE[%d]' % k, tuple(float(x) for x in openE[k]) if openE is not None else (0.0, 0.0, 0.0))
    setu(p, 'volStage', 1)
    f_u3i(G.loc(p, b'volDims'), COLS, ROWS, SL)
    setu(p, 'volProj', (P00, P11))
    setu(p, 'volSlices', (NEAR, lr, float(SL), 1.0))
    setu(p, 'fogView', (0.0, 1.0, 0.0, 1000.0))
    setu(p, 'fogDistScale', 1.0)
    setu(p, 'fogOn', 1)
    setu(p, 'volPhase[0]', (1.0, 1.0, 1.0, 0.0))
    setu(p, 'volPhase[1]', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'volPhase[2]', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'volGi', (1.0, 1.0, 1.0, GI_G))
    setu(p, 'airMed', (1.0 / 50000.0, 0.0, 0.0, 20000.0))
    for n in ('airLayer', 'airGround', 'volDrift', 'volMs'):
        setu(p, n, (0.0, 0.0, 0.0, 0.0))
    for n in ('airTerrOn', 'volTemporal', 'volDepthSkip', 'volFarMode', 'volLayerBase', 'rfOn', 'cellBulbCount',
              'ussTerrOn', 'ussContactOn', 'ussCloudOn', 'ussView', 'ussRed', 'csmRed', 'csmProbe', 'cellGiOn',
              'cellInterior', 'cellAmboCount', 'cellGoboOn', 'cellFogScaleOn', 'volSurfClusters', 'cellClusterOn',
              'airEnvFogHue', 'cellCtOn', 'cellDdgiRed', 'volSunRoom', 'cellRed', 'cellShadowFadeOn'):
        setu(p, n, 0)
    setu(p, 'volClusterDims', (0, 0, 0, 0), ints=True)
    setu(p, 'volLightShift', (0.0, 0.0, 0.0))
    setu(p, 'cellLightPhys', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'volRed', 256)   # physcol: the light raw
    setu(p, 'volTerms', terms)
    setu(p, 'volJitter', 0.0)
    setu(p, 'volLeakRed', leakred)
    setu(p, 'cellOn', 1)
    setu(p, 'cellInterior', int(interior))   # lane INTGI1: an unlit interior (after the zero list above)
    setu(p, 'cellHasDir', 1)
    setu(p, 'cellDirTo', tuple(Lv))
    setu(p, 'cellDirColor', (1.0, 1.0, 1.0))
    for k in range(3):
        row = [0.0, 0.0, 0.0, 0.0]
        row[k] = 1.0
        setu(p, 'cellRow[%d]' % k, tuple(row))
        setu(p, 'cellDalc[%d]' % k, (0.0, 0.0, 0.0, DALC_M))   # a flat ambient A = 0.5^2.2 on every axis
    setu(p, 'cellExtDalc', 1)
    setu(p, 'cellHasDalc', 1)
    # the cascades (clear)
    setu(p, 'volCsm', 1)
    setu(p, 'volSunAir', 1)
    setu(p, 'ussOn', 1)
    setu(p, 'ussCsmOn', 1)
    setu(p, 'csmForce', -1.0)
    for i in range(3):
        setu(p, 'csmMat[%d]' % i, csm_mat(i))
    setu(p, 'csmInvRange', tuple([1.0 / (2 * RANGE)] * 3))
    setu(p, 'csmParams', (1.0, 16000.0, float(MAPN), 400.0))
    setu(p, 'csmSplitOffset', (2000.0, 6500.0, 0.275, 1.0))
    setu(p, 'ussCsmFilter', (1.0, 3.0, 32.0, 0.0))
    setu(p, 'ussCsmBias', (12.0, 12.0, 12.0))
    setu(p, 'ussCsmWorld', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'ussCsmTexel', tuple(2 * h / MAPN for h in HALF))
    setu(p, 'ussCsmPhase', 0.0)
    setu(p, 'ussCsmStable', (1.0, 0.0, 0.0, 0.0))
    setu(p, 'ussCsmCellX', (0.0, 0.0, 0.0))
    setu(p, 'ussCsmCellY', (0.0, 0.0, 0.0))
    if 'csmMap' in units:
        G.active(0x84C0 + units['csmMap'])
        gl.glBindTexture(0x8C1A, CSM)
    # the volume
    setu(p, 'cellDdgiOn', 1)
    setu(p, 'cellGiLoop8', 8)
    o, s, (nx, ny, nz) = vol.origin, vol.step, vol.dims
    setu(p, 'cellDdgiOrigin', (o[0], o[1], o[2], s))
    setu(p, 'cellDdgiStep', (s, s, s, 0.3))
    setu(p, 'cellDdgiDims', (nx, ny, nz, vol.nP), ints=True)
    if 'cellDdgiAtlas' in units:
        G.active(0x84C0 + units['cellDdgiAtlas'])
        gl.glBindTexture(0x8C2A, vol.tex)
    # the light
    setu(p, 'cellLightCount', 1 if light is not None else 0)
    setu(p, 'cellShadowOn', int(shadow))
    setu(p, 'cellRestirOn', int(restir))
    setu(p, 'cellRestirSize', (0.0, 0.0))
    setu(p, 'cellShadowTexel', 0.0)
    if light is not None and 'cellLights' in units:
        G.active(0x84C0 + units['cellLights'])
        gl.glBindTexture(0x8C2A, light)
    if 'cellShadow' in units:
        G.active(0x84C0 + units['cellShadow'])
        gl.glBindTexture(0x9009, CUBE)


TG = {}


def target():
    if 'fb' not in TG:
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x0DE1, t)
        gl.glTexImage2D(0x0DE1, 0, 0x8814, COLS, ROWS, 0, 0x1908, 0x1406, None)
        for k, v in ((0x2801, 0x2600), (0x2800, 0x2600)):
            gl.glTexParameteri(0x0DE1, k, v)
        fb = GLuint()
        G.genfb(1, ctypes.byref(fb))
        G.bindfb(0x8D40, fb)
        G.fbtex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        TG['fb'] = fb.value
    return TG['fb']


def inject(p):
    G.bindfb(0x8D40, target())
    gl.glViewport(0, 0, COLS, ROWS)
    G.use(p)
    l = G.loc(p, b'volLayer')
    out = np.zeros((SL, ROWS, COLS, 4))
    buf = (ctypes.c_float * (COLS * ROWS * 4))()
    for k in range(SL):
        G.u1i(l, k)
        gl.glDrawArrays(5, 0, 4)
        gl.glReadPixels(0, 0, COLS, ROWS, 0x1908, 0x1406, buf)
        out[k] = np.frombuffer(buf, np.float32).reshape(ROWS, COLS, 4)
    return out


def lum(x):
    return x[..., 0] * 0.2126 + x[..., 1] * 0.7152 + x[..., 2] * 0.0722


def same(a, b, mask):
    la, lb = lum(a[..., :3]), lum(b[..., :3])
    m = max(float(lb.max()), 1e-30)
    d = np.abs(la - lb)
    bad = (d > 1e-5 * np.abs(lb) + 1e-7 * m) & mask
    return int(bad.sum()), float((d[mask] / m).max()) if mask.any() else 0.0


def light_buffer(pos, radius, slot=-1.0, flag=0.0):
    t = np.zeros((8, 4))
    t[0] = [pos[0], pos[1], pos[2], radius]
    t[1] = [1.0, 1.0, 1.0, -2.0]   # colour, omni
    t[3] = [0.0, 1.0, 1.0, 0.0]    # radial: 1 - d / r, ^2.2
    t[4] = [slot, flag, 0.0, 0.0]
    return tex_buffer(t)


CSM = clear_csm()
CUBE = lit_cube()
P_ON, U_ON = build(['WW_CELL_RTDDGI', 'WW_CELL_FOGLEAK'])
P_OFF, U_OFF = build(['WW_CELL_RTDDGI'])
tag = ' [red %s]' % red if red else ''

# ---- A / E / B: volume A (step 4000; x >= 0 inside) -------------------------------------------------------------------
if only & set('AEB'):
    E0 = 0.3 * math.pi * A_FLAT
    open_ = None
    for alpha in (0.05, 0.6):
        if alpha == 0.6 and 'B' not in only:
            break
        VA = Volume((-4000.0, -24000.0, -36000.0), 4000.0, (19, 13, 12), lambda d: E0 * (1.0 + 0.5 * d[:, 2]), alpha)
        lo, hi = VA.lo_hi_fade1()
        inside = all_in(lo, hi, 1100.0)
        outside = all_where(lambda P: P[..., 0] <= -5100.0)
        if alpha == 0.05 and only & set('AE'):
            uniforms(P_OFF, U_OFF, VA, 8)
            off = inject(P_OFF)
            uniforms(P_ON, U_ON, VA, 8, REDBIT)
            on = inject(P_ON)
            r = lum(on[..., :3]) / np.maximum(lum(off[..., :3]), 1e-30)
            # the law on the tile read's own values (cellDdgiTile mirrored: bilinear on the encoded texels): the first run
            # against the analytic E(+-dW) read 0 .. 8.8 % high, one-signed, the same in every slice = the 6 x 6 tile's
            # octahedral interpolation of the synthetic field, not the fix (amended after that run, before any red run)
            Ep, Em = VA.tile_E(U), VA.tile_E(-U)
            law = 0.25 * np.maximum(2.0 * (Ep + Em) + 3.0 * GI_G * (Ep - Em), 0.0) / math.pi / A_FLAT
            law = np.broadcast_to(law[None], r.shape)
            err = np.abs(r / law - 1.0)[inside]
            nbad, dmax = same(on, off, outside)
            if '--diag' in args:
                q = (r / law)
                dzs = np.broadcast_to(U[None, ..., 2], r.shape)
                for k in range(0, SL, 8):
                    m_ = inside[k]
                    if m_.any():
                        print('diag slice %2d t %.0f: r/law median %.4f min %.4f max %.4f' % (
                            k, TS[1][k], float(np.median(q[k][m_])), float(q[k][m_].min()), float(q[k][m_].max())))
                for a_, b_ in ((-1.0, -0.95), (-0.95, -0.85), (-0.85, -0.75), (-0.75, -0.6)):
                    m_ = inside & (dzs >= a_) & (dzs < b_)
                    if m_.any():
                        print('diag dz %.2f..%.2f: r/law median %.4f p1 %.4f p99 %.4f' % (
                            a_, b_, float(np.median(q[m_])), float(np.percentile(q[m_], 1)), float(np.percentile(q[m_], 99))))
            if 'A' in only:
                gate('A env', len(err) > 100 and float(np.percentile(err, 99)) <= 0.03 and nbad == 0 and outside.sum() > 100,
                     'inside %d froxels: on/off vs the law p99 |err| %.4f (median ratio %.3f, law %.3f..%.3f); outside %d '
                     'froxels: %d differ (max |d| %.2e of max)%s' % (inside.sum(), float(np.percentile(err, 99)) if len(err) else -1,
                                                                      float(np.median(r[inside])), float(law[inside].min()),
                                                                      float(law[inside].max()), outside.sum(), nbad, dmax, tag))
            if 'E' in only:
                med = float(np.median(r[inside]))
                gate('E env', med <= 0.5, 'inside: the shadowed air\'s env on/off median %.3f (bar <= 0.5; the volume holds '
                     '0.3 x the flat ambient)%s' % (med, tag))
        if 'B' in only:
            uniforms(P_OFF, U_OFF, VA, 1)
            off = inject(P_OFF)
            uniforms(P_ON, U_ON, VA, 1, REDBIT)
            on = inject(P_ON)
            r = lum(on[..., :3]) / np.maximum(lum(off[..., :3]), 1e-30)
            if alpha == 0.05:
                far = inside & all_where(lambda P: -P[..., 2] > 16400.0)
                near = inside & all_where(lambda P: np.linalg.norm(P, axis=-1) < 3000.0)
                nbad, dmax = same(on, off, outside)
                ok = far.sum() > 100 and near.sum() > 10 and float(r[far].max()) <= 0.01 and float(r[near].min()) >= 0.995 \
                    and nbad == 0
                gate('B sun', ok, 'roofed (sky vis 0.05): past the cascades %d froxels on/off max %.4f (bar <= 0.01); near %d '
                     'froxels min %.4f (bar >= 0.995); outside %d differ (max |d| %.2e)%s'
                     % (far.sum(), float(r[far].max()) if far.any() else -1, near.sum(),
                        float(r[near].min()) if near.any() else -1, nbad, dmax, tag))
            else:
                every = np.ones(r.shape, bool)
                nbad, dmax = same(on, off, every)
                gate('B open', nbad == 0, 'open ground (sky vis 0.6): %d of %d froxels differ (max |d| %.2e of max)%s'
                     % (nbad, every.sum(), dmax, tag))

# ---- I: an unlit interior (lane INTGI1, FOGLEAK1-INT), volume A, the directional only ----------------------------------
if 'I' in only:
    for alpha in (0.05, 0.6):
        VI = Volume((-4000.0, -24000.0, -36000.0), 4000.0, (19, 13, 12), lambda d: 0.1 + 0.0 * d[:, 2], alpha)
        lo, hi = VI.lo_hi_fade1()
        inside = all_in(lo, hi, 1100.0)
        outside = all_where(lambda P: P[..., 0] <= -5100.0)
        uniforms(P_OFF, U_OFF, VI, 1, 0, interior=True)
        off = inject(P_OFF)
        uniforms(P_ON, U_ON, VI, 1, REDBIT, interior=True)
        on = inject(P_ON)
        lo_ = lum(off[..., :3])
        r = lum(on[..., :3]) / np.maximum(lo_, 1e-30)
        if alpha == 0.05:
            near = inside & all_where(lambda P: np.linalg.norm(P, axis=-1) < 3000.0)
            nbad, dmax = same(on, off, outside)
            ok = inside.sum() > 100 and near.sum() > 10 and float(lo_[inside].min()) > 0.0 \
                and float(r[inside].max()) <= 0.01 and nbad == 0 and outside.sum() > 100
            gate('I closed', ok, 'unlit interior, closed (sky vis 0.05): inside %d froxels on/off max %.4f (bar <= 0.01; '
                 'near %d froxels max %.4f); outside %d froxels: %d differ (max |d| %.2e)%s'
                 % (inside.sum(), float(r[inside].max()) if inside.any() else -1, near.sum(),
                    float(r[near].max()) if near.any() else -1, outside.sum(), nbad, dmax, tag))
        else:
            every = np.ones(r.shape, bool)
            nbad, dmax = same(on, off, every)
            gate('I open', nbad == 0 and float(lo_.max()) > 0.0, 'unlit interior, an opening (sky vis 0.6): %d of %d '
                 'froxels differ (max |d| %.2e of max)%s' % (nbad, every.sum(), dmax, tag))

# ---- L / R: volume C (step 1000), a wall x = 2500 between the light and the near air ----------------------------------
if only & set('LR'):
    VC = Volume((-4000.0, -6000.0, -17000.0), 1000.0, (17, 13, 17), lambda d: 0.1 + 0.0 * d[:, 2], 1.0, wall=2500.0)
    lo, hi = VC.lo_hi_fade1()
    inside = all_in(lo, hi, 300.0)
    nearS = inside & all_where(lambda P: P[..., 0] < 1900.0)
    farS = inside & all_where(lambda P: P[..., 0] > 3100.0)

    def light_rows(name, defs, shadow, restir, slot, flag):
        pon, uon = (P_ON, U_ON) if not defs else build(['WW_CELL_RTDDGI', 'WW_CELL_FOGLEAK'] + defs)
        poff, uoff = (P_OFF, U_OFF) if not defs else build(['WW_CELL_RTDDGI'] + defs)
        lb = light_buffer((4000.0, 0.0, -9000.0), 6000.0, slot, flag)
        uniforms(poff, uoff, VC, 2, 0, lb, shadow, restir)
        off = inject(poff)
        uniforms(pon, uon, VC, 2, REDBIT, lb, shadow, restir)
        on = inject(pon)
        lo_ = lum(off[..., :3])
        lit = lo_ > 1e-3 * lo_.max()
        r = lum(on[..., :3]) / np.maximum(lo_, 1e-30)
        n_, f_ = nearS & lit, farS & lit
        if not (n_.sum() > 50 and f_.sum() > 50):
            gate(name, False, 'too few lit froxels: near %d far %d' % (n_.sum(), f_.sum()))
            return pon, uon, poff, uoff, lb
        mn, pn = float(np.median(r[n_])), float(np.percentile(r[n_], 95))
        mf, pf = float(np.median(r[f_])), float(np.percentile(r[f_], 5))
        gate(name, mn <= 0.10 and pn <= 0.25 and mf >= 0.95 and pf >= 0.80,
             'behind the wall %d froxels on/off median %.3f p95 %.3f (bars 0.10 / 0.25); the light\'s side %d froxels median '
             '%.3f p5 %.3f (bars 0.95 / 0.80)%s' % (n_.sum(), mn, pn, f_.sum(), mf, pf, tag))
        return pon, uon, poff, uoff, lb

    if 'L' in only:
        light_rows('L light', [], False, False, -1.0, 0.0)
    if 'R' in only:
        pon, uon, poff, uoff, lb = light_rows('R restir', ['WW_CELL_RESTIR'], True, True, 0.0, 1.0)
        # a cubed light (ReSTIR off): its map stays the verdict, the volume is not read
        lb = light_buffer((4000.0, 0.0, -9000.0), 6000.0, 0.0, 1.0)
        uniforms(P_OFF, U_OFF, VC, 2, 0, lb, True, False)
        off = inject(P_OFF)
        uniforms(P_ON, U_ON, VC, 2, REDBIT, lb, True, False)
        on = inject(P_ON)
        every = np.ones(off.shape[:3], bool)
        nbad, dmax = same(on, off, every)
        gate('R cubed', nbad == 0 and float(lum(off[..., :3]).max()) > 0.0,
             'a light with a drawn cube: %d of %d froxels differ (max |d| %.2e of max; lit max %.3g)%s'
             % (nbad, every.sum(), dmax, float(lum(off[..., :3]).max()), tag))

# ---- S: the absolute scale on open ground (lane FOGENV1) ----------------------------------------------------------------
if 'S' in only:
    def open_axes(E0):   # E(d) = E0 (1 + 0.5 d.z) on the six axes +X -X +Y -Y +Z -Z
        return [(E0,) * 3, (E0,) * 3, (E0,) * 3, (E0,) * 3, (1.5 * E0,) * 3, (0.5 * E0,) * 3]
    for K, share in ((0.3, 1.0), (3.0, 1.0), (30.0, 1.0), (3.0, 0.3)):
        E0 = K * math.pi * A_FLAT
        VS = Volume((-4000.0, -24000.0, -36000.0), 4000.0, (19, 13, 12), lambda d: E0 * (1.0 + 0.5 * d[:, 2]), 0.6)
        lo, hi = VS.lo_hi_fade1()
        inside = all_in(lo, hi, 1100.0)
        uniforms(P_OFF, U_OFF, VS, 8)
        off = inject(P_OFF)
        uniforms(P_ON, U_ON, VS, 8, REDBIT, openE=open_axes(E0 / share))
        on = inject(P_ON)
        r = (lum(on[..., :3]) / np.maximum(lum(off[..., :3]), 1e-30))[inside]
        med = float(np.median(r)) if len(r) else -1.0
        p1, p99 = (float(np.percentile(r, 1)), float(np.percentile(r, 99))) if len(r) else (-1.0, -1.0)
        if share == 1.0:
            gate('S open K=%g' % K, len(r) > 100 and 0.95 <= med <= 1.05 and p1 >= 0.85 and p99 <= 1.15,
                 'open ground, the read %g x pi A: inside %d froxels on/off median %.4f p1 %.4f p99 %.4f (bar median '
                 '0.95..1.05, p1 >= 0.85, p99 <= 1.15)%s' % (K, len(r), med, p1, p99, tag))
        else:
            gate('S occl', len(r) > 100 and 0.25 <= med <= 0.36,
                 'the probes see %.1f of an open probe: inside %d froxels on/off median %.4f (bar 0.25..0.36)%s'
                 % (share, len(r), med, tag))

# ---- N: the sun's gate from one tile (lane FOGENV1, FOGLEAK1-COST) ------------------------------------------------------
if 'N' in only:
    VN = Volume((-4000.0, -24000.0, -36000.0), 4000.0, (19, 13, 12), lambda d: 0.1 + 0.0 * d[:, 2], 0.05,
                alpha_x=lambda x: np.where(x <= 12000.0, 0.05, 0.6))
    lo, hi = VN.lo_hi_fade1()
    inside = all_in(lo, hi, 1100.0)
    awayL = inside & all_where(lambda P: P[..., 0] <= 11000.0)
    awayR = inside & all_where(lambda P: P[..., 0] >= 17000.0)
    band = inside & all_where(lambda P: (P[..., 0] >= 12000.0) & (P[..., 0] <= 16000.0))
    uniforms(P_OFF, U_OFF, VN, 1, 0, interior=True)
    off = inject(P_OFF)
    uniforms(P_ON, U_ON, VN, 1, REDBIT, interior=True)
    on = inject(P_ON)
    uniforms(P_ON, U_ON, VN, 1, 64, interior=True)
    r8 = inject(P_ON)
    nbad, dmax = same(on, r8, awayL | awayR)
    lo_ = np.maximum(lum(off[..., :3]), 1e-30)
    dband = float(np.mean(np.abs(lum(on[..., :3]) - lum(r8[..., :3]))[band] / lo_[band])) if band.any() else -1.0
    shut = float((lum(on[..., :3]) / lo_)[awayL].max()) if awayL.any() else -1.0
    ok = awayL.sum() > 100 and awayR.sum() > 100 and band.sum() > 10 and nbad == 0 and 0.0 <= dband <= 0.20 and shut <= 0.01
    gate('N sun1', ok, 'one tile vs the 8-probe red: away %d + %d froxels, %d differ (max |d| %.2e); band %d froxels mean '
         '|on - red| / off %.3f (bar <= 0.20); roofed on/off max %.4f (<= 0.01)%s'
         % (awayL.sum(), awayR.sum(), nbad, dmax, band.sum(), dband, shut, tag))

# ---- C: cost (INFO) ---------------------------------------------------------------------------------------------------
if 'C' in only:
    VC = Volume((-4000.0, -6000.0, -17000.0), 1000.0, (17, 13, 17), lambda d: 0.1 + 0.0 * d[:, 2], 0.6, wall=2500.0)
    lb = light_buffer((4000.0, 0.0, -9000.0), 6000.0)
    q = (GLuint * 1)()
    f_genq(1, q)

    def timed(p, units, terms=1 | 2 | 8, leak=0):
        uniforms(p, units, VC, terms, leak, lb)
        G.bindfb(0x8D40, target())
        gl.glViewport(0, 0, COLS, ROWS)
        l = G.loc(p, b'volLayer')
        ms = []
        for rep in range(9):
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
    toff, ton = timed(P_OFF, U_OFF), timed(P_ON, U_ON)
    gate('C cost', True, 'INFO inject 160x90x64, sun + one unshadowed light + ambient, median of 9 (this GPU, offline): off %.3f ms, '
         'on %.3f ms (+%.3f)' % (toff, ton, ton - toff))
    parts = []
    for nm, terms in (('sun', 1), ('light', 2), ('amb', 8)):
        parts.append('%s off %.3f on %.3f' % (nm, timed(P_OFF, U_OFF, terms), timed(P_ON, U_ON, terms)))
    parts.append('all, on with every red bit %.3f' % timed(P_ON, U_ON, 1 | 2 | 8, 7))
    gate('C parts', True, 'INFO ' + '; '.join(parts))

print('FOGLEAK-CHECK %s (%d of %d)%s' % ('PASS' if results and all(results) else 'FAIL', sum(results), len(results), tag))
sys.exit(0 if results and all(results) else 1)
