"""lane FIX1101A (bungo 10-31 RULED: "we need ssr for water"): the water's screen-space reflection, OFFLINE through the
real driver: the tree's cell_ssrwater.prog (stages 1-4: ray, march, blur across, blur down) over a synthetic lake, then
the tree's water_refl.glsl (wwWaterSsrBend, wwWaterReflEnv, wwWaterFresnel) composing what the water shows.
Scene (view space, x right, y up, -z forward; world z up): the eye 300 u over a lake, level; a far bank wall 6000 u
ahead, up to 900 u over the eye, its colour known; nothing (sky) above it; the water's normals a sum of gentle waves
(slopes ~0.04).
  share    on CALM water, the wall's reflected share in the water's reflection (env - sky) / (wall - sky) where the calm mirror sees the
           wall, away from the screen edges: >= 0.85; and in the final (body x (1 - F) + env x F) the object's share vs
           Schlick F (n 1.333) 0.85 .. 1.10.   red --red off: the water draws no SSR (today's verdict under WW_CELL_SSR=0,
           the shot runners' pin; cellssrwater.h's red) -> share 0; --red old: WW_CELL_SSR_WATER_RED=1's march
           (the game's 50 u refusal: a far bank's rays are refused)
  bend     the flat ray + the bent read (the default; the ray pass skips the normal stack) vs the full-normal ray
           (WW_CELL_SSR_WATER_RED=1, the old pass) on a striped wall: mean |difference| / stripe contrast must be under
           0.75 x the flat ray read unbent (no ripple in the reflection) and under 0.12
  seam     a wall taller than the screen: down each column of the near water that mirrors it at the screen's top
           edge, the reflected share must fade to the sky over >= 12 px per column (the march's edge fade, no hard cut);
           a mutant with the fade cut to 1 must break it (the measure can fail)
  cost     1920 x 1080 (half-size march), GPU timer: stages 1-4, cell_ssrwater vs cell_ssr; printed, not gated
usage: python -I ssr_gate.py [--red off] [--shaders <dir>]"""
import ctypes
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SD = os.path.join(TREE, 'res', 'shaders')
RED = ''
a = sys.argv[1:]
while a:
    if a[0] == '--red':
        RED = a[1]
        a = a[2:]
    elif a[0] == '--shaders':
        SD = a[1]
        a = a[2:]
    else:
        sys.exit('unknown ' + a[0])
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as slc  # noqa: E402

gl = slc.gl
slc.context()
U, I, P, E, F = ctypes.c_uint, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint, ctypes.c_float


def f(name, res, *args):
    ad = gl.wglGetProcAddress(name.encode())
    if ad:
        return ctypes.WINFUNCTYPE(res, *args)(ad)
    g = getattr(gl, name)
    g.restype = res
    g.argtypes = list(args)
    return g


NAMES = {
    'glGenTextures': (None, I, P), 'glBindTexture': (None, E, U), 'glTexImage2D': (None, E, I, I, I, I, I, E, E, P),
    'glTexParameteri': (None, E, E, I), 'glActiveTexture': (None, E), 'glCreateShader': (U, E),
    'glShaderSource': (None, U, I, ctypes.POINTER(ctypes.c_char_p), P), 'glCompileShader': (None, U),
    'glGetShaderiv': (None, U, E, P), 'glGetShaderInfoLog': (None, U, I, P, ctypes.c_char_p), 'glCreateProgram': (U,),
    'glAttachShader': (None, U, U), 'glLinkProgram': (None, U), 'glGetProgramiv': (None, U, E, P),
    'glUseProgram': (None, U), 'glGetUniformLocation': (I, U, ctypes.c_char_p), 'glUniform1i': (None, I, I),
    'glUniform2i': (None, I, I, I), 'glUniform2iv': (None, I, I, P), 'glUniform2f': (None, I, F, F),
    'glUniform1f': (None, I, F), 'glUniform4f': (None, I, F, F, F, F), 'glUniform4fv': (None, I, I, P),
    'glUniformMatrix3fv': (None, I, I, ctypes.c_ubyte, P), 'glUniformMatrix4fv': (None, I, I, ctypes.c_ubyte, P),
    'glGenBuffers': (None, I, P), 'glBindBuffer': (None, E, U), 'glBufferData': (None, E, ctypes.c_ssize_t, P, E),
    'glGenVertexArrays': (None, I, P), 'glBindVertexArray': (None, U), 'glEnableVertexAttribArray': (None, U),
    'glVertexAttribPointer': (None, U, I, E, ctypes.c_ubyte, I, P), 'glGenFramebuffers': (None, I, P),
    'glBindFramebuffer': (None, E, U), 'glFramebufferTexture2D': (None, E, E, E, U, I), 'glPixelStorei': (None, E, I),
    'glViewport': (None, I, I, I, I), 'glDisable': (None, E), 'glDrawArrays': (None, E, I, I),
    'glReadPixels': (None, I, I, I, I, E, E, P), 'glGenQueries': (None, I, P), 'glBeginQuery': (None, E, U),
    'glEndQuery': (None, E), 'glGetQueryObjectui64v': (None, U, E, P), 'glGetError': (U,),
    'glClearColor': (None, F, F, F, F), 'glClear': (None, U)}
globals().update({k: f(k, *v) for k, v in NAMES.items()})
glPixelStorei(0x0CF5, 1)
glPixelStorei(0x0D05, 1)
KEEP = []


def gen(g):
    v = U(0)
    g(1, ctypes.byref(v))
    return v.value


def tex(unit, ifmt, w, h, fmt, typ, data, linear=False):
    t = gen(glGenTextures)
    glActiveTexture(0x84C0 + unit)
    glBindTexture(0x0DE1, t)
    if data is not None:
        data = np.ascontiguousarray(data)
        KEEP.append(data)
    glTexImage2D(0x0DE1, 0, ifmt, w, h, 0, fmt, typ, data.ctypes.data if data is not None else None)
    flt = 0x2601 if linear else 0x2600
    for p_, v in ((0x2801, flt), (0x2800, flt), (0x2802, 0x812F), (0x2803, 0x812F), (0x813D, 0)):
        glTexParameteri(0x0DE1, p_, v)
    return t


def comp(kind, src):
    s = glCreateShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    glShaderSource(s, 1, ctypes.byref(b), None)
    glCompileShader(s)
    ok = I(0)
    glGetShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 14)
        glGetShaderInfoLog(s, 1 << 14, None, log)
        print('COMPILE FAIL ' + log.value.decode(errors='replace')[:2000])
        print('VERDICT FAIL (compile)')
        sys.exit(2)
    return s


VS = comp(0x8B31, slc.expand(os.path.join(SD, 'cell_ssr.vert')))


def prog_src(src):
    pr = glCreateProgram()
    glAttachShader(pr, VS)
    glAttachShader(pr, comp(0x8B30, src))
    glLinkProgram(pr)
    ok = I(0)
    glGetProgramiv(pr, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('VERDICT FAIL (link)')
        sys.exit(2)
    return pr


SSRW = slc.expand(os.path.join(SD, 'cell_ssrwater.frag'))
SSRG = slc.expand(os.path.join(SD, 'cell_ssr.frag'))
FADE = 'c = smoothstep( 0.0, 0.1, e.x ) * smoothstep( 0.0, 0.1, e.y );'
assert SSRW.count(FADE) == 1, 'the water march edge fade line moved'
P_W = prog_src(SSRW)
P_MUT = prog_src(SSRW.replace(FADE, 'c = 1.0;'))
P_G = prog_src(SSRG)

vao = gen(glGenVertexArrays)
glBindVertexArray(vao)
vbo = gen(glGenBuffers)
glBindBuffer(0x8892, vbo)
quad = np.array([-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0], np.float32)
glBufferData(0x8892, quad.nbytes, quad.ctypes.data, 0x88E4)
glEnableVertexAttribArray(0)
glVertexAttribPointer(0, 3, 0x1406, 0, 0, None)
glDisable(0x0B71)
glDisable(0x0BE2)
glDisable(0x0B90)
QID = gen(glGenQueries)

FOVY = np.radians(60.0)
EYEH, WALL, WALLTOP, FAR = 300.0, 6000.0, 900.0, 20000.0
SKY = np.array([0.30, 0.50, 0.80], np.float32)
WALLC = np.array([0.90, 0.20, 0.10], np.float32)
BODY = np.array([0.05, 0.08, 0.07], np.float32)
M = np.array([[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]], np.float32)   # view -> world (columns: x, y -> z, z -> -y)


class Scene:
    def __init__(self, FW, FH, stripes, flat, calm=False, walltop=WALLTOP, pitch=0.0):
        # pitch (rad, + = down): camera -> level frame C (columns: the camera's x, y, z in the level frame)
        cp, sp = np.cos(pitch), np.sin(pitch)
        C = np.array([[1.0, 0.0, 0.0], [0.0, cp, sp], [0.0, -sp, cp]])
        self.C = C
        self.M = (M.astype(np.float64) @ C).astype(np.float32)   # camera view -> world
        self.FW, self.FH = FW, FH
        self.HW, self.HH = FW // 2, FH // 2
        P11 = 1.0 / np.tan(FOVY / 2)
        P00 = P11 * FH / FW
        self.P00, self.P11 = P00, P11
        ys, xs = np.mgrid[0:FH, 0:FW]
        nx, ny = (xs + 0.5) / FW * 2 - 1, 1 - (ys + 0.5) / FH * 2   # top row first
        dv = np.stack([nx / P00, ny / P11, -np.ones_like(nx)], -1)
        dv /= np.linalg.norm(dv, axis=-1, keepdims=True)
        dL = dv @ C.T   # the same rays in the level frame
        t = np.where(dL[..., 1] < -1e-4, -EYEH / np.minimum(dL[..., 1], -1e-4), np.inf)
        zw = t * -dv[..., 2]
        water = np.isfinite(zw) & (zw < WALL)   # the lake ends at the wall (the wall hides what is behind it)
        PwL = dL * np.where(water, t, 0.0)[..., None]
        Pw = dv * np.where(water, t, 0.0)[..., None]
        # the water's normal (level frame, y up): gentle waves over the plane's (x, z)
        x, z = PwL[..., 0], PwL[..., 2]
        sx = 0.030 * np.cos(x / 61.0 + z / 173.0) + 0.020 * np.cos(x / 23.0 - z / 41.0)
        sz = 0.030 * np.cos(z / 97.0 - x / 211.0) + 0.020 * np.sin(z / 29.0 + x / 37.0)
        if calm:   # the share and seam views: a calm surface, so the wall's mirror image sits where the mask says
            sx, sz = sx * 0.0, sz * 0.0
        NL = np.stack([-sx, np.ones_like(sx), -sz], -1)
        NL /= np.linalg.norm(NL, axis=-1, keepdims=True)
        RfL = dL * np.array([1.0, -1.0, 1.0])
        RnL = dL - 2.0 * np.sum(dL * NL, -1, keepdims=True) * NL
        N, Rf, Rn = NL @ C, RfL @ C, RnL @ C   # back to the camera frame
        self.PwL, self.RfL = PwL, RfL
        R = Rf if flat else Rn
        keep = water & (-R[..., 2] > 0.2)
        wr = np.zeros((FH, FW, 4), np.float32)
        wr[..., :3] = np.where(keep[..., None], R, 0.0)
        wr[..., 3] = np.where(water, zw, 0.0)
        self.water, self.dv, self.N, self.Rn, self.Rf, self.Pw = water, dv, N, Rn, Rf, Pw
        tw = WALL / np.maximum(-dL[..., 2], 1e-6)
        wall_y = tw * dL[..., 1]
        wallpix = (~water) & (wall_y < walltop) & (-dL[..., 2] > 1e-6)
        zs = np.where(water, zw * 1.05, np.where(wallpix, tw * -dv[..., 2], 1e9)).astype(np.float32)
        mips = [zs]
        for _ in range(4):
            m_ = mips[-1]
            h2, w2 = m_.shape[0] // 2, m_.shape[1] // 2
            mips.append(m_[:h2 * 2, :w2 * 2].reshape(h2, 2, w2, 2).min((1, 3)))
        self.mips = mips
        wx = tw * dL[..., 0]
        col = np.zeros((FH, FW, 4), np.float32)
        wc = WALLC[None, None, :] * (0.55 + 0.45 * np.sin(wx / 90.0))[..., None] if stripes else WALLC[None, None, :]
        col[..., :3] = np.where(wallpix[..., None], wc, np.where(water[..., None], BODY, 0.0))
        self.wallcol = wc
        self.u = {}
        self.u['ray'] = tex(1, 0x8814, FW, FH, 0x1908, 0x1406, wr[::-1])            # bottom row first
        self.u['scn'] = tex(2, 0x8814, FW, FH, 0x1908, 0x1406, col[::-1], True)     # bottom row first, bilinear
        for m in range(1, 5):
            tex(2 + m, 0x822E, mips[m].shape[1], mips[m].shape[0], 0x1903, 0x1406, mips[m])   # top row first
        self.t = {k: tex(8, 0x8814, self.HW, self.HH, 0x1908, 0x1406, None, k != 'ray') for k in ('ray', 'raw', 'acr', 'fin')}
        self.fb = {}
        for k, tt in self.t.items():
            fb = gen(glGenFramebuffers)
            glBindFramebuffer(0x8D40, fb)
            glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, tt, 0)
            self.fb[k] = fb

    def setup(self, pr, stage, src):
        glUseProgram(pr)
        L = lambda n: glGetUniformLocation(pr, n.encode())  # noqa: E731
        glUniform1i(L('ssrStage'), stage)
        glUniform1i(L('waterRay'), 1)
        glUniform1i(L('scene'), 2)
        glUniform1i(L('prevScene'), 2)
        glUniform1i(L('gbuf'), 2)
        for m in range(1, 5):
            glUniform1i(L('zMip%d' % m), 2 + m)
        glUniform1i(L('src'), 9)
        glActiveTexture(0x84C9)
        glBindTexture(0x0DE1, self.t[src] if src else 0)
        glUniform2i(L('fullSize'), self.FW, self.FH)
        glUniform2i(L('halfSize'), self.HW, self.HH)
        ms = np.array([[self.FW, self.FH]] + [[self.mips[m].shape[1], self.mips[m].shape[0]] for m in range(1, 5)], np.int32)
        glUniform2iv(L('mipSize'), 5, ms.ctypes.data)
        glUniform2f(L('proj'), self.P00, self.P11)
        glUniformMatrix3fv(L('viewToWorld'), 1, 0, np.eye(3, dtype=np.float32).ctypes.data)
        glUniform4f(L('ssrK'), 1.0, 0.2, 2.0, 1.0)
        glUniform2f(L('ssrClip'), 15.0, FAR)
        glUniform1i(L('ssrRed'), 8 if RED == 'old' else 0)   # 8: the water rays' old end (50 u thickness, no step fade)
        glUniform1i(L('haveWaterRay'), 1)
        glUniform1i(L('opaqueRays'), 0)
        glUniform1i(L('ssrPrev'), 0)

    def march(self, pr, timed=0):
        ms = 0.0
        for stage, out, src in ((1, 'ray', None), (2, 'raw', 'ray'), (3, 'acr', 'raw'), (4, 'fin', 'acr')):
            glBindFramebuffer(0x8D40, self.fb[out])
            glViewport(0, 0, self.HW, self.HH)
            glClearColor(0, 0, 0, 0)
            glClear(0x4000)
            self.setup(pr, stage, src)
            if timed:
                ts = []
                for _ in range(timed):
                    glBeginQuery(0x88BF, QID)
                    glDrawArrays(0x0005, 0, 4)
                    glEndQuery(0x88BF)
                    v = ctypes.c_uint64(0)
                    glGetQueryObjectui64v(QID, 0x8866, ctypes.byref(v))
                    ts.append(v.value / 1e6)
                ms += float(np.median(ts))
            else:
                glDrawArrays(0x0005, 0, 4)
        e = glGetError()
        if e:
            sys.exit('GL error 0x%x' % e)
        return ms


REFL = (open(slc.shader_path(os.path.join(SD, 'water_refl.glsl')), encoding='latin-1').read() + chr(10)   # MODULES1: features/water
        + open(slc.shader_path(os.path.join(SD, 'water_ssrbend.glsl')), encoding='latin-1').read())
COMPOSE = '''#version 410 core
uniform mat4 projectionMatrix;
uniform mat3 v2w;
uniform vec4 waterSky[3];
uniform sampler2D raw;
uniform sampler2D fin;
uniform sampler2D rw;
uniform sampler2D vw;
uniform sampler2D nw;
uniform bool haveSsr;
uniform bool bendOn;
uniform vec2 full;
uniform vec3 skyC;
uniform float alphaR;
vec3 cellWorldDir( vec3 v ) { return v2w * v; }
''' + REFL + '''
out vec4 fragColor;
void main()
{
	ivec2 q = ivec2( gl_FragCoord.xy );
	vec2 suv = ( vec2( q ) + 0.5 ) / full;
	vec2 st = vec2( suv.x, 1.0 - suv.y );
	vec3 R = texelFetch( rw, q, 0 ).xyz, V = texelFetch( vw, q, 0 ).xyz, N = texelFetch( nw, q, 0 ).xyz;
	if ( bendOn && waterSsrFlat )
		st += wwWaterSsrBend( R, vec3( V.xy, -V.z ) );
	vec4 r = textureLod( raw, st, 0.0 ), fi = textureLod( fin, st, 0.0 );
	vec3 env = wwWaterReflEnv( skyC, r, fi, haveSsr, alphaR );
	fragColor = vec4( env, wwWaterFresnel( dot( -V, N ), alphaR, 0.0204 ) );
}
'''
VSQ = comp(0x8B31, slc.expand(os.path.join(SD, 'cell_ssr.vert')))
P_C = prog_src(COMPOSE)


def compose(sc, have, bend, flatflag):
    FW, FH = sc.FW, sc.FH
    Rw = sc.Rn @ sc.M.T
    Vw = sc.dv @ sc.M.T
    Nw = sc.N @ sc.M.T
    tR = tex(20, 0x8814, FW, FH, 0x1907, 0x1406, Rw.astype(np.float32)[::-1])
    tV = tex(21, 0x8814, FW, FH, 0x1907, 0x1406, Vw.astype(np.float32)[::-1])
    tN = tex(22, 0x8814, FW, FH, 0x1907, 0x1406, Nw.astype(np.float32)[::-1])
    outT = tex(23, 0x8814, FW, FH, 0x1908, 0x1406, None)
    fb = gen(glGenFramebuffers)
    glBindFramebuffer(0x8D40, fb)
    glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, outT, 0)
    glViewport(0, 0, FW, FH)
    glUseProgram(P_C)
    L = lambda n: glGetUniformLocation(P_C, n.encode())  # noqa: E731
    pm = np.zeros((4, 4), np.float32)
    pm[0, 0], pm[1, 1] = sc.P00, sc.P11
    glUniformMatrix4fv(L('projectionMatrix'), 1, 1, pm.ctypes.data)
    glUniformMatrix3fv(L('v2w'), 1, 1, np.ascontiguousarray(sc.M).ctypes.data)
    glActiveTexture(0x84C0 + 24)
    glBindTexture(0x0DE1, sc.t['raw'])
    glActiveTexture(0x84C0 + 25)
    glBindTexture(0x0DE1, sc.t['fin'])
    for n_, u_ in (('raw', 24), ('fin', 25), ('rw', 20), ('vw', 21), ('nw', 22)):
        glUniform1i(L(n_), u_)
    glUniform1i(L('haveSsr'), 1 if have else 0)
    glUniform1i(L('bendOn'), 1 if bend else 0)
    glUniform1i(L('waterSsrFlat'), 1 if flatflag else 0)
    glUniform1i(L('waterReflRed'), 0)
    glUniform4f(L('waterGlintK'), 16.0, 0.0204, 0.18, 0.015)
    glUniform2f(L('full'), float(FW), float(FH))
    glUniform3f = f('glUniform3f', None, I, F, F, F)
    glUniform3f(L('skyC'), *[float(x) for x in SKY])
    glUniform1f(L('alphaR'), 0.01)
    glDrawArrays(0x0005, 0, 4)
    out = np.empty((FH, FW, 4), np.float32)
    glReadPixels(0, 0, FW, FH, 0x1908, 0x1406, out.ctypes.data)
    e = glGetError()
    if e:
        sys.exit('GL error 0x%x compose' % e)
    return out[::-1].copy()   # top row first


def mirror_mask(sc, margin=0.15):
    """water pixels whose CALM mirror ray meets the wall on the screen, away from the edges"""
    Rf, Pw = sc.RfL, sc.PwL
    t = (-WALL - Pw[..., 2]) / np.minimum(Rf[..., 2], -1e-6)
    H = Pw + Rf * t[..., None]
    Hc = H @ sc.C
    zh = -Hc[..., 2]
    ux = 0.5 + 0.5 * sc.P00 * Hc[..., 0] / zh
    uy = 0.5 - 0.5 * sc.P11 * Hc[..., 1] / zh
    ys, xs = np.mgrid[0:sc.FH, 0:sc.FW]
    sx = (xs + 0.5) / sc.FW
    return (sc.water & (t > 0) & (H[..., 1] > -EYEH + 80) & (H[..., 1] < WALLTOP - 120) & (ux > margin) & (ux < 1 - margin)
            & (uy > margin) & (sx > margin) & (sx < 1 - margin) & (-sc.Rf[..., 2] > 0.25))


fails = 0
HAVE = RED != 'off'
print('== share (960 x 540, uniform wall %s, sky %s)%s' % (WALLC, SKY, '  RED off: the water draws no SSR' if not HAVE
      else '  RED old: WW_CELL_SSR_WATER_RED=1 (the 50 u refusal, no bend, no hole fill)' if RED == 'old' else ''))
sc = Scene(960, 540, False, True, True)
sc.march(P_W)
out = compose(sc, HAVE, True, RED != 'old')
mk = mirror_mask(sc)
share = (out[..., 0] - SKY[0]) / (WALLC[0] - SKY[0])
sh = float(share[mk].mean()) if mk.any() else 0.0
NoV = np.clip(-np.sum(sc.dv * sc.N, -1), 0, 1)
schlick = 0.0201 + (1 - 0.0201) * (1 - NoV) ** 5
Fout = out[..., 3]
fin_share = sh * Fout   # the object's share in body x (1 - F) + env x F (body and sky take the rest)
ratio = float((fin_share[mk] / schlick[mk]).mean()) if mk.any() else 0.0
ok = sh >= 0.85 and 0.85 <= ratio <= 1.10
fails += not ok
print('SHARE pixels %d, env share of the wall mean %.3f (p10 %.3f), final share / Schlick %.3f (F mean %.4f)  %s' % (
    int(mk.sum()), sh, float(np.percentile(share[mk], 10)) if mk.any() else 0.0, ratio,
    float(Fout[mk].mean()) if mk.any() else 0.0, 'PASS' if ok else 'FAIL'))

if HAVE:
    print('== bend (striped wall): flat ray + bent read vs the full-normal ray (the old pass)')
    scS = Scene(960, 540, True, True)
    scS.march(P_W)
    bent = compose(scS, True, True, True)
    flatnb = compose(scS, True, False, True)
    scN = Scene(960, 540, True, False)
    scN.march(P_W)
    full = compose(scN, True, False, False)
    mk2 = mirror_mask(scS, 0.2)
    con = float(WALLC[0]) * 0.9
    e_b = float(np.abs(bent[..., 0] - full[..., 0])[mk2].mean()) / con
    e_f = float(np.abs(flatnb[..., 0] - full[..., 0])[mk2].mean()) / con
    ok = e_b <= 0.75 * e_f and e_b < 0.12
    fails += not ok
    print('BEND pixels %d, error bent %.4f vs unbent %.4f (of the stripe contrast)  %s' % (int(mk2.sum()), e_b, e_f, 'PASS' if ok else 'FAIL'))

    print('== seam (a wall taller than the screen: the near water mirrors it up to the screen\'s top edge)')
    scT = Scene(960, 540, False, True, True, 1e7, np.radians(15.0))   # 15 deg down: the mirror runs past the top

    def jumps(o):
        # down each column, over the near water whose calm mirror ray meets the wall near or past the screen's top
        s_ = (o[..., 0] - SKY[0]) / (WALLC[0] - SKY[0])
        Rf, Pw = scT.RfL, scT.PwL
        t = (-WALL - Pw[..., 2]) / np.minimum(Rf[..., 2], -1e-6)
        Hh = (Pw + Rf * t[..., None]) @ scT.C
        uy = 0.5 - 0.5 * scT.P11 * Hh[..., 1] / -Hh[..., 2]
        xs = np.arange(scT.FW)[None, :].repeat(scT.FH, 0)
        band = scT.water & (t > 0) & (uy < 0.2) & (uy > -0.2) & (xs > 0.2 * scT.FW) & (xs < 0.8 * scT.FW)
        # the fade's width per column: the pixels the share spends between 0.1 and 0.9 (a hard cut: a pixel or two)
        mid = band & (s_ > 0.1) & (s_ < 0.9)
        cols = band.any(0)
        wid = mid.sum(0)[cols]
        return float(np.median(wid)) if cols.any() else 0.0, float(s_[band].mean()) if band.any() else 0.0
    scT.march(P_W)
    jg, mg = jumps(compose(scT, True, True, True))
    scT.march(P_MUT)
    jm, _ = jumps(compose(scT, True, True, True))
    ok = jg >= 12.0 and jm < 12.0
    fails += not ok
    print('SEAM the fade to the sky at the top edge, median width per column: green %.1f px (mean share in the band %.3f), mutant (no edge fade) %.1f px  %s' % (
        jg, mg, jm, 'PASS' if ok else 'FAIL'))

    print('== cost at 1920 x 1080 (half-size targets 960 x 540), GPU ms of stages 1-4, min of 3 rounds of the median of 20')
    scC = Scene(1920, 1080, False, True)
    rw_, rg_ = [], []
    for _ in range(2):
        scC.march(P_W, 5)
        scC.march(P_G, 5)
    for _ in range(3):
        rw_.append(scC.march(P_W, 20))
        rg_.append(scC.march(P_G, 20))
    print('COST cell_ssrwater stages 1-4 %.3f ms (rounds %s); cell_ssr (the game\'s 32 steps) %.3f ms' % (
        min(rw_), ' '.join('%.3f' % x for x in rw_), min(rg_)))

print('VERDICT %s' % ('PASS' if fails == 0 else 'FAIL (%d)' % fails))
