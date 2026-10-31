"""Lane TREELEAF4 (FIX1031A, 2026-10-31), FXCOVRESID1 + TAABLENDMOVER1: does the TAA object-vector pass keep exactly the
texels the effect's colour pass keeps? Offline, the REAL programs through the real driver in a hidden GL 4.1 context
(tests/spells/shader_link_check.py's context and #include expansion): the colour pass = fo4_effectcell.prog
(fo4_default.vert + fo4_effectcell.frag, the cell-lit effect shader), the side pass = taa_objvel.prog. No NifSkope.

The draw: a 48x48 curved grid (|N.V| from 0.2 to 1) in view space, a random normal map, a base map whose alpha ramps
with noise, vertex alphas, a palette, and an opaque-pass depth plane that cuts through the grid (the soft fade runs
0..1 across it). Each case sets the colour program's uniforms as setupProgramCE1 does and the side program's as
taainputs.cpp does (Shape::wwFxSideAlpha: fxMode 4 + 1 vertex alpha + 2 palette + 8 falloff + 16 soft).

Checks per case (`kept` = the colour pass wrote the texel; its alpha = the composed alpha it blends with):
  test   an alpha-tested effect: the side pass (covMode 0) keeps exactly the colour pass's kept texels
  cov    a blended effect: covMode 1 keeps exactly the kept texels with composed alpha >= 0.5; covMode 2 (the reactive
         mark) exactly the rest of the triangles' texels
Model views: identity (the two programs' positions are then bit-identical) and a rotated, moved one (the colour program
multiplies P x (MV x v), the side program (P x MV) x v: rasterisation can differ by rounding at the boundary).
Gate: identity view 0 texels differ in every case; moved view <= 0.05 % of the covered texels. Reds: the lane base's
taa_objvel shaders (git show <base>) and the new shaders with TREELEAF3's fxMode (no 8 / 16 = WW_FXCOVRESID_RED) must
FAIL (texels differ in the falloff / soft / coverage cases).

usage: python -I tests/treeleaf4/fxcov_gate.py [--red base|resid] [--base <commit>] [--out <txt>]
Prints `FXCOV RESULT PASS|FAIL`; rc 0 on PASS, 1 on FAIL."""
import ctypes
import math
import os
import subprocess
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
args = sys.argv[1:]
RED = args[args.index('--red') + 1] if '--red' in args else ''
BASE = args[args.index('--base') + 1] if '--base' in args else '563dcf97'
OUT = args[args.index('--out') + 1] if '--out' in args else ''
W = H = 256
NG = 48

S.context()
F = {}


def fn(name, res, *a):
    if name not in F:
        F[name] = S.fn(name, res, *a)
    return F[name]


fp = ctypes.POINTER(ctypes.c_float)
glGetUniformLocation = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
glUseProgram = fn('glUseProgram', None, GLuint)
glUniform1i = fn('glUniform1i', None, GLint, GLint)
glUniform1f = fn('glUniform1f', None, GLint, ctypes.c_float)
glUniform2f = fn('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
glUniform2i = fn('glUniform2i', None, GLint, GLint, GLint)
glUniform3f = fn('glUniform3f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float)
glUniform4f = fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
glUniformMatrix3fv = fn('glUniformMatrix3fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, fp)
glUniformMatrix4fv = fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, fp)
glActiveTexture = fn('glActiveTexture', None, GLenum)
glGenBuffers = fn('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
glBindBuffer = fn('glBindBuffer', None, GLenum, GLuint)
glBufferData = fn('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
glBindBufferBase = fn('glBindBufferBase', None, GLenum, GLuint, GLuint)
glGetUniformBlockIndex = fn('glGetUniformBlockIndex', GLuint, GLuint, ctypes.c_char_p)
glUniformBlockBinding = fn('glUniformBlockBinding', None, GLuint, GLuint, GLuint)
glGenVertexArrays = fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
glBindVertexArray = fn('glBindVertexArray', None, GLuint)
glVertexAttribPointer = fn('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, ctypes.c_void_p)
glEnableVertexAttribArray = fn('glEnableVertexAttribArray', None, GLuint)
glDisableVertexAttribArray = fn('glDisableVertexAttribArray', None, GLuint)
glGenFramebuffers = fn('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
glBindFramebuffer = fn('glBindFramebuffer', None, GLenum, GLuint)
glFramebufferTexture2D = fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
glDrawBuffers = fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
glClearBufferfv = fn('glClearBufferfv', None, GLenum, GLint, fp)
glColorMaski = fn('glColorMaski', None, GLuint, ctypes.c_ubyte, ctypes.c_ubyte, ctypes.c_ubyte, ctypes.c_ubyte)
glDrawElements = fn('glDrawElements', None, GLenum, ctypes.c_int, GLenum, ctypes.c_void_p)
gl.glTexImage2D.argtypes = [GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p]
gl.glReadPixels.argtypes = [GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p]


def build(files, defines=()):
    p = fn('glCreateProgram', GLuint)()
    for f in files:
        src = S.inject(S.expand(f), list(defines) + (['NUM_TEXTURE_UNITS=30'] if f.endswith('frag') else []))
        sh = fn('glCreateShader', GLuint, GLenum)(0x8B30 if f.endswith('frag') else 0x8B31)
        b = ctypes.c_char_p(src.encode('latin-1'))
        fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)(sh, 1, ctypes.byref(b), None)
        fn('glCompileShader', None, GLuint)(sh)
        ok = GLint()
        fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(sh, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(8192)
            fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)(sh, 8192, None, log)
            raise SystemExit('COMPILE FAIL %s\n%s' % (f, log.value.decode(errors='replace')[:2000]))
        fn('glAttachShader', None, GLuint, GLuint)(p, sh)
    fn('glLinkProgram', None, GLuint)(p)
    ok = GLint()
    fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL %s' % files)
    return p


def tex2d(data, internal, fmt, typ, filt=0x2601):
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x0DE1, t)
    h, w = data.shape[:2]
    gl.glTexImage2D(0x0DE1, 0, internal, w, h, 0, fmt, typ, np.ascontiguousarray(data).ctypes.data_as(ctypes.c_void_p))
    for k, v in ((0x2801, filt), (0x2800, filt), (0x2802, 0x812F), (0x2803, 0x812F), (0x813D, 0)):
        gl.glTexParameteri(0x0DE1, k, v)
    return t.value


def target():
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x0DE1, t)
    gl.glTexImage2D(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, None)
    for k, v in ((0x2801, 0x2600), (0x2800, 0x2600)):
        gl.glTexParameteri(0x0DE1, k, v)
    return t.value


# ---- the scene -------------------------------------------------------------------------------------------------
rng = np.random.default_rng(31)
n_near, n_far = 1.0, 1000.0
fy = 1.0 / math.tan(math.radians(30))
P = np.zeros((4, 4))
P[0, 0], P[1, 1] = fy, fy
P[2, 2] = -(n_far + n_near) / (n_far - n_near)
P[2, 3] = -2 * n_far * n_near / (n_far - n_near)
P[3, 2] = -1.0


def grid():
    u = np.linspace(0, 1, NG)
    U, V = np.meshgrid(u, u)
    x, y = (U - 0.5) * 90, (V - 0.5) * 90
    z = -100 + 28 * np.cos(np.hypot(x, y) / 22.0)	# a dome: |N.V| falls toward its rim
    dzdx = -28 * np.sin(np.hypot(x, y) / 22.0) / 22.0 * x / np.maximum(np.hypot(x, y), 1e-6)
    dzdy = -28 * np.sin(np.hypot(x, y) / 22.0) / 22.0 * y / np.maximum(np.hypot(x, y), 1e-6)
    pos = np.dstack([x, y, z]).reshape(-1, 3)
    t = np.dstack([np.ones_like(x), 0 * x, dzdx]).reshape(-1, 3)
    b = np.dstack([0 * x, np.ones_like(x), dzdy]).reshape(-1, 3)
    t /= np.linalg.norm(t, axis=1, keepdims=True)
    b /= np.linalg.norm(b, axis=1, keepdims=True)
    nrm = np.cross(t, b)
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
    uv = np.dstack([U, V]).reshape(-1, 2)
    col = np.ones((NG * NG, 4))
    col[:, 3] = 0.35 + 0.65 * (0.5 + 0.5 * np.sin(U * 9.1 + V * 4.3)).reshape(-1)
    idx = []
    for j in range(NG - 1):
        for i in range(NG - 1):
            a = j * NG + i
            idx += [a, a + 1, a + NG, a + NG, a + 1, a + NG + 1]
    return [x.astype(np.float32) for x in (pos, col, nrm, t, b, uv)], np.array(idx, np.uint16)


ATTR, IDX = grid()
vao = GLuint()
glGenVertexArrays(1, ctypes.byref(vao))
glBindVertexArray(vao)
for loc, arr in zip((0, 1, 2, 3, 4, 7, 8), (ATTR[0], ATTR[1], ATTR[2], ATTR[3], ATTR[4], ATTR[5], ATTR[0])):
    bo = GLuint()
    glGenBuffers(1, ctypes.byref(bo))
    glBindBuffer(0x8892, bo)
    a = np.ascontiguousarray(arr, np.float32)
    glBufferData(0x8892, a.nbytes, a.ctypes.data_as(ctypes.c_void_p), 0x88E4)
    glVertexAttribPointer(loc, a.shape[1], 0x1406, 0, 0, None)
    glEnableVertexAttribArray(loc)
ebo = GLuint()
glGenBuffers(1, ctypes.byref(ebo))
glBindBuffer(0x8893, ebo)
glBufferData(0x8893, IDX.nbytes, IDX.ctypes.data_as(ctypes.c_void_p), 0x88E4)

# textures: base (alpha ramp + noise), normal map (random bumps), palette, the opaque-pass depth plane
yy, xx = np.mgrid[0:128, 0:128] / 128.0
base = np.zeros((128, 128, 4), np.uint8)
base[..., :3] = 200
base[..., 3] = np.clip((0.15 + 0.9 * xx + 0.25 * (rng.random((128, 128)) - 0.5)) * 255, 0, 255).astype(np.uint8)
nm = np.zeros((128, 128, 4), np.uint8)
g = rng.normal(0, 0.45, (128, 128, 2))
from PIL import Image  # noqa: E402
for c in range(2):
    g[..., c] = np.asarray(Image.fromarray(((g[..., c] * 0.5 + 0.5).clip(0, 1) * 255).astype(np.uint8)).resize((16, 16), Image.BILINEAR).resize((128, 128), Image.BICUBIC), float) / 255 * 2 - 1
nm[..., 0] = ((g[..., 0] * 0.5 + 0.5) * 255).astype(np.uint8)
nm[..., 1] = ((g[..., 1] * 0.5 + 0.5) * 255).astype(np.uint8)
nm[..., 2] = 255
nm[..., 3] = 255
pal = np.zeros((64, 64, 4), np.uint8)
pu, pv = np.mgrid[0:64, 0:64] / 63.0
pal[..., 3] = (np.clip(0.2 + 0.9 * pv * (0.5 + pu) - 0.15, 0, 1) * 255).astype(np.uint8)	# a = f(v rows, u cols)
pal[..., :3] = 128
T_BASE = tex2d(base, 0x8058, 0x1908, 0x1401)
T_NORM = tex2d(nm, 0x8058, 0x1908, 0x1401)
T_PAL = tex2d(pal, 0x8058, 0x1908, 0x1401)
# the depth plane: view distance w(x) from 70 (in front of the dome's top at ~72) to 130 across the frame
wpl = (70.0 + 60.0 * (np.arange(W) + 0.5) / W)[None, :].repeat(H, 0)
zndc = -P[2, 2] + P[2, 3] / wpl
T_DEPTH = tex2d(((zndc + 1) * 0.5).astype(np.float32), 0x8CAC, 0x1902, 0x1406, 0x2600)	# DEPTH_COMPONENT32F

# the frame UBO (uniforms.glsl, std140) and an empty skinning block
ubo = np.zeros(336 // 4, np.float32)
ubo[0:12] = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0]
ubo[12:24] = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0]
ubo[24:40] = P.T.reshape(-1)	# column-major
ubo[40:44] = [0, 0, 1, 0]
ubo[64:68] = [0.3, 0.3, 0.3, 1]
ubo[68:72] = [1, 1, 1, 1]
vpd = np.array([0, 0, W, H], np.int32)
ubo[72:76] = vpd.view(np.float32)
for binding, data in ((0, ubo), (1, np.zeros(256 * 12, np.float32))):
    bo = GLuint()
    glGenBuffers(1, ctypes.byref(bo))
    glBindBuffer(0x8A11, bo)
    glBufferData(0x8A11, data.nbytes, data.ctypes.data_as(ctypes.c_void_p), 0x88E4)
    glBindBufferBase(0x8A11, binding, bo)

SD = os.path.join(TREE, 'res', 'shaders')
COLOUR = build([os.path.join(SD, 'fo4_default.vert'), os.path.join(SD, 'fo4_effectcell.frag')])
for blk, b in (('globalUniforms', 0), ('skinningUniforms', 1)):
    i = glGetUniformBlockIndex(COLOUR, blk.encode())
    if i != 0xFFFFFFFF:
        glUniformBlockBinding(COLOUR, i, b)
SAMPLER_TYPES = {0x8B5D, 0x8B5E, 0x8B5F, 0x8B60, 0x8B61, 0x8B62, 0x8DC1, 0x8DC4, 0x8DC5, 0x8DC2, 0x8DD0, 0x8DD8, 0x8DCA,
                 0x8DD2, 0x900C, 0x8DCB, 0x8DD3, 0x9108, 0x8DCC, 0x8DD4, 0x8DCF, 0x8DD7}


def spread_samplers(prog, first=4):
    """every sampler its own unit from `first` (all default to 0: two types on one unit refuse the draw)"""
    n = GLint()
    fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(prog, 0x8B86, ctypes.byref(n))
    gau = fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(GLint),
             ctypes.POINTER(GLenum), ctypes.c_char_p)
    glUniform1iv = fn('glUniform1iv', None, GLint, ctypes.c_int, ctypes.POINTER(GLint))
    glUseProgram(prog)
    u = first
    for i in range(n.value):
        size, typ, name = GLint(), GLenum(), ctypes.create_string_buffer(256)
        gau(prog, i, 256, None, ctypes.byref(size), ctypes.byref(typ), name)
        if typ.value not in SAMPLER_TYPES:
            continue
        nm_ = name.value.decode()
        loc = glGetUniformLocation(prog, nm_.encode())
        glUniform1iv(loc, size.value, (GLint * size.value)(*range(u, u + size.value)))
        u += size.value
    return u


NUNITS = spread_samplers(COLOUR)


def side_program(red):
    if red == 'base':
        d = os.path.join(HERE, '_red_shaders')
        os.makedirs(d, exist_ok=True)
        for f in ('taa_objvel.vert', 'taa_objvel.frag'):
            r = subprocess.run(['git', '-C', TREE, 'show', '%s:res/shaders/%s' % (BASE, f)], capture_output=True)
            if r.returncode != 0:
                raise SystemExit('cannot read %s at %s' % (f, BASE))
            open(os.path.join(d, f), 'wb').write(r.stdout)
        return build([os.path.join(d, 'taa_objvel.vert'), os.path.join(d, 'taa_objvel.frag')])
    return build([os.path.join(SD, 'taa_objvel.vert'), os.path.join(SD, 'taa_objvel.frag')])


SIDE = side_program(RED)
T0, T1 = target(), target()
fbo = GLuint()
glGenFramebuffers(1, ctypes.byref(fbo))
glBindFramebuffer(0x8D40, fbo)
glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, T0, 0)
glFramebufferTexture2D(0x8D40, 0x8CE1, 0x0DE1, T1, 0)


def U(prog, name):
    return glGetUniformLocation(prog, name.encode())


def mat(m):
    a = np.ascontiguousarray(np.asarray(m, np.float32).T.reshape(-1))	# column-major
    return a.ctypes.data_as(fp), a


def read(att):
    gl.glReadBuffer(0x8CE0 + att)
    buf = (ctypes.c_float * (W * H * 4))()
    gl.glReadPixels(0, 0, W, H, 0x1908, 0x1406, buf)
    return np.frombuffer(buf, np.float32).reshape(H, W, 4).copy()


def clear():
    glBindFramebuffer(0x8D40, fbo)
    gl.glViewport(0, 0, W, H)
    glDrawBuffers(2, (GLenum * 2)(0x8CE0, 0x8CE1))
    glColorMaski(0, 1, 1, 1, 1)
    glColorMaski(1, 1, 1, 1, 1)
    s = (ctypes.c_float * 4)(-7, -7, -7, -7)
    glClearBufferfv(0x1800, 0, s)
    glClearBufferfv(0x1800, 1, s)


def bind(unit, t, prog, name):
    glActiveTexture(0x84C0 + unit)
    gl.glBindTexture(0x0DE1, t)
    glUniform1i(U(prog, name), unit)


def model_view(moved):
    if not moved:
        return np.eye(4)
    a = math.radians(9.0)
    R = np.array([[math.cos(a), 0, math.sin(a), 0], [0, 1, 0, 0], [-math.sin(a), 0, math.cos(a), 0], [0, 0, 0, 1]])
    Rx = np.array([[1, 0, 0, 0], [0, math.cos(a * 0.6), -math.sin(a * 0.6), 0], [0, math.sin(a * 0.6), math.cos(a * 0.6), 0], [0, 0, 0, 1]])
    T = np.eye(4)
    T[:3, 3] = [3.3, -2.1, 1.7]
    return T @ Rx @ R


def colour_pass(c, MV):
    """fo4_effectcell as setupProgramCE1 sets it: alphaFlags 4 (`>`) or 6 (`>=`), + 8 to keep the composed alpha"""
    clear()
    glDrawBuffers(1, (GLenum * 1)(0x8CE0))
    p = COLOUR
    glUseProgram(p)
    m4, k4 = mat(MV)
    glUniformMatrix4fv(U(p, 'modelViewMatrix'), 1, 0, m4)
    m3, k3 = mat(MV[:3, :3].T)	# the normal matrix: `n * normalMatrix` = R n
    glUniformMatrix3fv(U(p, 'normalMatrix'), 1, 0, m3)
    glUniform4f(U(p, 'vertexColorOverride'), 0, 0, 0, 1.0 if not c['vtx'] else 0.0)
    glUniform2f(U(p, 'uvScale'), 1, 1)
    glUniform2f(U(p, 'uvOffset'), 0, 0)
    glUniform1i(U(p, 'greyscaleAlpha'), int(c['pal']))
    glUniform1i(U(p, 'useFalloff'), int(c['fall']))
    glUniform4f(U(p, 'falloffParams'), *c['fp'])
    glUniform4f(U(p, 'glowColor'), 1, 1, 1, c['bca'])
    glUniform1f(U(p, 'glowMult'), 1.0)
    glUniform1i(U(p, 'alphaFlags'), 8 | c['func'] if c['kind'] == 'test' else 8)
    glUniform1f(U(p, 'alphaThreshold'), c['ref'] / 255.0)
    glUniform1i(U(p, 'cellOn'), 1)
    glUniform1i(U(p, 'fxSoft'), int(c['soft']))
    glUniform1f(U(p, 'fxSoftDepth'), c['sd'])
    glUniform1f(U(p, 'fxDistScale'), 1.0)
    glUniform1i(U(p, 'fxRed'), 0)
    glUniform1i(U(p, 'volPart'), 0)
    bind(0, T_BASE, p, 'BaseMap')
    bind(1, T_PAL, p, 'GreyscaleMap')
    bind(2, T_NORM, p, 'NormalMap')
    bind(3, T_DEPTH, p, 'fxDepth')
    glDrawElements(4, IDX.size, 0x1403, None)
    o = read(0)
    kept = o[..., 0] != -7
    return kept, np.where(kept, o[..., 3], np.nan)


def side_pass(c, MV, cov, red):
    """taa_objvel as taainputs.cpp sets it (rigid mode 0; clipJit = P x MV computed on the host)"""
    clear()
    p = SIDE
    glUseProgram(p)
    mode = 4 | (1 if c['vtx'] else 0) | (2 if c['pal'] else 0)
    if red != 'resid':
        mode |= (8 if c['fall'] else 0) | (16 if c['soft'] else 0)
    keep = []
    PM = (P.astype(np.float32).astype(np.float64) @ MV.astype(np.float32).astype(np.float64))
    for k, v in (('clipJit', PM), ('clipCur', PM), ('clipPrev', PM), ('modelViewMatrix', MV)):
        m4, k4 = mat(v)
        keep.append(k4)
        glUniformMatrix4fv(U(p, k), 1, 0, m4)
    m3, k3 = mat(MV[:3, :3].T)
    glUniformMatrix3fv(U(p, 'normalMatrix'), 1, 0, m3)
    glUniform1i(U(p, 'velMode'), 0)
    glUniform3f(U(p, 'wwGrassFade'), 0, 0, 0)
    glUniform2f(U(p, 'uvScale'), 1, 1)
    glUniform2f(U(p, 'uvOffset'), 0, 0)
    test = c['kind'] == 'test'
    glUniform1i(U(p, 'alphaTest'), int(test))
    glUniform1i(U(p, 'fxMode'), mode)
    glUniform1f(U(p, 'alpha'), np.float32(c['bca']) * np.float32(c['bca']))
    t = np.float32(c['ref'] / 255.0)
    glUniform1f(U(p, 'alphaThreshold'), np.nextafter(t, np.float32(2.0)) if c['func'] == 4 else t)
    glUniform4f(U(p, 'falloffParams'), *c['fp'])
    glUniform1i(U(p, 'fxOrtho'), 0)
    glUniform2i(U(p, 'fxDepthOrigin'), 0, 0)
    glUniform2f(U(p, 'fxProj'), P[2, 2], P[2, 3])	# projectionMatrix[2][2], [3][2] (column, row)
    glUniform1f(U(p, 'fxDistScale'), 1.0)
    glUniform1f(U(p, 'fxSoftDepth'), c['sd'])
    glUniform1f(U(p, 'covThreshold'), 0.5)
    bind(0, T_BASE, p, 'BaseMap')
    bind(1, T_PAL, p, 'GreyscaleMap')
    bind(2, T_NORM, p, 'NormalMap')
    bind(3, T_DEPTH, p, 'fxDepth')
    if not c['vtx']:
        glDisableVertexAttribArray(1)
        fn('glVertexAttrib4f', None, GLuint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)(1, 1, 1, 1, 1)
    glDrawBuffers(2, (GLenum * 2)(0x8CE0, 0x8CE1))
    glColorMaski(1, 0, 0, 1, 0)
    glUniform1i(U(p, 'covMode'), cov)
    if cov == 2:
        glColorMaski(0, 0, 0, 0, 0)
        glColorMaski(1, 1, 0, 0, 0)
    glDrawElements(4, IDX.size, 0x1403, None)
    glEnableVertexAttribArray(1)
    glColorMaski(0, 1, 1, 1, 1)
    glColorMaski(1, 1, 1, 1, 1)
    f1 = read(1)
    return (f1[..., 0] == 1.0) if cov == 2 else (f1[..., 2] == 1.0)


def tris_mask(MV):
    """every texel the grid's triangles cover (the colour program with no test, alpha forced kept)"""
    c = dict(kind='blend', vtx=False, pal=False, fall=False, soft=False, fp=(0, 1, 1, 1), bca=1.0, func=4, ref=0, sd=1.0)
    k, _ = colour_pass(c, MV)
    return k


CASES = []
for kind in ('test', 'cov'):
    for fall in (False, True):
        for soft in (False, True):
            for pal in (False, True):
                CASES.append(dict(kind='test' if kind == 'test' else 'blend', vtx=(fall != pal), pal=pal, fall=fall,
                                  soft=soft, fp=(0.15, 0.85, 0.05, 1.0), bca=0.8, func=4 if not (fall and pal) else 6,
                                  ref=60 if kind == 'test' else 0, sd=25.0))

lines = []
say = lambda s: (print(s), lines.append(s), sys.stdout.flush())
say('TREELEAF4 fxcov gate: fo4_effectcell.prog vs taa_objvel.prog (%s), %dx%d, grid %dx%d%s' % (
    'base %s' % BASE if RED == 'base' else ('new shaders, TREELEAF3 fxMode' if RED == 'resid' else 'this tree'), W, H, NG, NG,
    ' RED ' + RED if RED else ''))
bad = 0
worst_moved = 0.0
for moved in (False, True):
    MV = model_view(moved)
    tri = tris_mask(MV)
    say('\n== model view %s: triangles cover %d texels ==' % ('moved' if moved else 'identity', int(tri.sum())))
    for c in CASES:
        kept, a = colour_pass(c, MV)
        name = '%-5s fall %d soft %d pal %d vtx %d func %d' % (c['kind'], c['fall'], c['soft'], c['pal'], c['vtx'], c['func'])
        if c['kind'] == 'test':
            s = side_pass(c, MV, 0, RED)
            diff = int((s != kept).sum())
            extra = ''
        else:
            want1 = kept & (a >= 0.5)
            want2 = tri & ~want1
            s1 = side_pass(c, MV, 1, RED)
            s2 = side_pass(c, MV, 2, RED) if RED != 'base' else np.zeros_like(s1)
            diff = int((s1 != want1).sum() + (s2 != want2).sum())
            extra = ' (vector %d want %d; reactive %d want %d)' % (int(s1.sum()), int(want1.sum()), int(s2.sum()), int(want2.sum()))
        frac = diff / max(int(tri.sum()), 1)
        ok = diff == 0 if not moved else frac <= 0.0005
        if moved:
            worst_moved = max(worst_moved, frac)
        bad += not ok
        say('%s  kept %6d  differ %6d (%.4f %%) %s%s' % (name, int(kept.sum()), diff, 100 * frac, 'ok' if ok else 'FAIL', extra))
res = 'PASS' if bad == 0 else 'FAIL'
say('\n%d of %d checks fail; moved view worst %.4f %% of the covered texels' % (bad, 2 * len(CASES), 100 * worst_moved))
say('FXCOV RESULT %s' % res)
if OUT:
    open(OUT, 'w').write('\n'.join(lines) + '\n')
sys.exit(0 if res == 'PASS' else 1)
