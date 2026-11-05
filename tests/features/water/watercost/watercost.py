"""lane FIX1101A (bungo 10-31 RULED: "water that looks good and is not expensive to render"): the water look's GPU cost,
OFFLINE through the real driver (no NifSkope): the tree's fo4_waterlook.prog (and fo4_water.prog, the shipped water)
drawn over a 1920 x 1080 frame that is ALL water (an upper bound: a frame's water share scales it), GPU timer, the
median of N draws after a warm-up, rounds interleaved. Inputs: WATERDATA1's water info dump (a real baked body at
the chosen spot, so the shore / depth / flow branches run as in the cell view), seeded noise, a synthetic frame and
depth behind the water (a bed 150 u down), every sampler of the program on its own unit (ocean arrays and the SSR
targets filled with seeded data), the uniforms cellwater.cpp / cellocean.cpp set (their engineering constants).
  rows: shipped (fo4_water.prog) | all off | the default (the rows that ship ON) | the default minus each term
        (its cost = default - that) | the default plus each OFF row (what turning it on would cost)
        | the SSR water ray pass (waterSslr 1: the water program's front half, full size)
usage: python -I watercost.py <tree> <water info dump dir> [sea|river] [W H] [N]
Prints one `COST <name> <ms>` line per row and a table."""
import ctypes
import os
import re
import sys

import numpy as np

T, CPP = sys.argv[1], sys.argv[2]
SPOT = sys.argv[3] if len(sys.argv) > 3 else 'sea'
FW = int(sys.argv[4]) if len(sys.argv) > 4 else 1920
FH = int(sys.argv[5]) if len(sys.argv) > 5 else 1080
NREP = int(sys.argv[6]) if len(sys.argv) > 6 else 20
sys.path.insert(0, os.path.join(T, 'tests', 'spells'))
import shader_link_check as slc  # noqa: E402

gl = slc.gl
slc.context()
print('GL', gl.glGetString(0x1F02).decode(), '|', gl.glGetString(0x1F01).decode())
U, I, P, E, F = ctypes.c_uint, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint, ctypes.c_float


def f(name, res, *args):
    a = gl.wglGetProcAddress(name.encode())
    if a:
        return ctypes.WINFUNCTYPE(res, *args)(a)
    g = getattr(gl, name)
    g.restype = res
    g.argtypes = list(args)
    return g


NAMES = {
    'glGenTextures': (None, I, P), 'glBindTexture': (None, E, U), 'glTexImage2D': (None, E, I, I, I, I, I, E, E, P),
    'glTexImage3D': (None, E, I, I, I, I, I, I, E, E, P), 'glGenerateMipmap': (None, E),
    'glTexParameteri': (None, E, E, I), 'glPixelStorei': (None, E, I), 'glActiveTexture': (None, E),
    'glCreateShader': (U, E), 'glShaderSource': (None, U, I, ctypes.POINTER(ctypes.c_char_p), P),
    'glCompileShader': (None, U), 'glGetShaderiv': (None, U, E, P), 'glGetShaderInfoLog': (None, U, I, P, ctypes.c_char_p),
    'glCreateProgram': (U,), 'glAttachShader': (None, U, U), 'glLinkProgram': (None, U), 'glGetProgramiv': (None, U, E, P),
    'glGetProgramInfoLog': (None, U, I, P, ctypes.c_char_p), 'glUseProgram': (None, U),
    'glGetUniformLocation': (I, U, ctypes.c_char_p), 'glGetActiveUniform': (None, U, U, I, P, P, P, ctypes.c_char_p),
    'glUniform4fv': (None, I, I, P), 'glUniform3f': (None, I, F, F, F), 'glUniform4f': (None, I, F, F, F, F),
    'glUniform1f': (None, I, F), 'glUniform1i': (None, I, I), 'glUniform2f': (None, I, F, F),
    'glUniformMatrix4fv': (None, I, I, ctypes.c_ubyte, P), 'glGetUniformBlockIndex': (U, U, ctypes.c_char_p),
    'glUniformBlockBinding': (None, U, U, U), 'glGetUniformIndices': (None, U, I, ctypes.POINTER(ctypes.c_char_p), P),
    'glGetActiveUniformsiv': (None, U, I, P, E, P), 'glGenBuffers': (None, I, P), 'glBindBuffer': (None, E, U),
    'glBufferData': (None, E, ctypes.c_ssize_t, P, E), 'glBindBufferBase': (None, E, U, U),
    'glGenVertexArrays': (None, I, P), 'glBindVertexArray': (None, U), 'glEnableVertexAttribArray': (None, U),
    'glVertexAttribPointer': (None, U, I, E, ctypes.c_ubyte, I, P), 'glGenFramebuffers': (None, I, P),
    'glBindFramebuffer': (None, E, U), 'glFramebufferTexture2D': (None, E, E, E, U, I),
    'glCheckFramebufferStatus': (E, E), 'glViewport': (None, I, I, I, I), 'glClearColor': (None, F, F, F, F),
    'glClear': (None, U), 'glDisable': (None, E), 'glDrawArrays': (None, E, I, I), 'glFinish': (None,),
    'glGenQueries': (None, I, P), 'glBeginQuery': (None, E, U), 'glEndQuery': (None, E),
    'glGetQueryObjectui64v': (None, U, E, P), 'glGetError': (U,), 'glReadPixels': (None, I, I, I, I, E, E, P)}
globals().update({k: f(k, *v) for k, v in NAMES.items()})


def gen(g):
    v = U(0)
    g(1, ctypes.byref(v))
    return v.value


KEEP = []


def tex2d(unit, ifmt, w, h, fmt, typ, data, filt=0x2601, wrap=0x2901, mips=False):
    t = gen(glGenTextures)
    glActiveTexture(0x84C0 + unit)
    glBindTexture(0x0DE1, t)
    glPixelStorei(0x0CF5, 1)
    if data is not None:
        KEEP.append(data)
    glTexImage2D(0x0DE1, 0, ifmt, w, h, 0, fmt, typ, data.ctypes.data if data is not None else None)
    glTexParameteri(0x0DE1, 0x2801, 0x2703 if mips else filt)
    glTexParameteri(0x0DE1, 0x2800, filt)
    glTexParameteri(0x0DE1, 0x2802, wrap)
    glTexParameteri(0x0DE1, 0x2803, wrap)
    if mips:
        glGenerateMipmap(0x0DE1)
    else:
        glTexParameteri(0x0DE1, 0x813D, 0)
    return t


def tex2darray(unit, w, h, layers, data):
    t = gen(glGenTextures)
    glActiveTexture(0x84C0 + unit)
    glBindTexture(0x8C1A, t)
    KEEP.append(data)
    glTexImage3D(0x8C1A, 0, 0x881A, w, h, layers, 0, 0x1908, 0x1406, data.ctypes.data)   # RGBA16F from float
    glTexParameteri(0x8C1A, 0x2801, 0x2703)
    glTexParameteri(0x8C1A, 0x2800, 0x2601)
    glTexParameteri(0x8C1A, 0x2802, 0x2901)
    glTexParameteri(0x8C1A, 0x2803, 0x2901)
    glGenerateMipmap(0x8C1A)
    return t


# ---- the water info tables
cen = open(os.path.join(CPP, 'census.txt')).read()
m = re.search(r'grid origin (\S+) (\S+) texel (\S+) shore quantum (\S+) tiles (\d+) (\d+)', cen)
ox, oy, texel, q16, tX, tY = float(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4)), int(m.group(5)), int(m.group(6))
EDGE, PER = 16, 64
idx = np.fromfile(os.path.join(CPP, 'index.u32'), '<u4')
atl = np.fromfile(os.path.join(CPP, 'atlas.u32'), '<u4')
bod = np.fromfile(os.path.join(CPP, 'bodies.f32'), '<f4')
aW, aH = PER * EDGE, atl.size // (PER * EDGE)
bRows = bod.size // (256 * 4)
rIdx = idx.reshape(tY, tX, 2)
rAtl = atl.reshape(-1, aW)
bodT = bod.reshape(bRows * 256, 4)


def word(tx, ty):
    slot = int(rIdx[ty // EDGE, tx // EDGE, 0])
    if slot == 0:
        return int(rIdx[ty // EDGE, tx // EDGE, 1])
    s1 = slot - 1
    return int(rAtl[(s1 // PER) * EDGE + ty % EDGE, (s1 % PER) * EDGE + tx % EDGE])


# spot 5 = the Boston sea shore (cells 14..16, -2..3, class 0); spot 6 = Cutler Bend's river (-9..-7, -19..-17, class 1)
CELLS = {'sea': ((14, 16), (-2, 3), 0), 'river': ((-9, -7), (-19, -17), 1)}
(cx0, cx1), (cy0, cy1), want = CELLS[SPOT]
best = None
for cy in range(cy0, cy1 + 1):
    for cx in range(cx0, cx1 + 1):
        tx0, ty0 = int((cx * 4096 - ox) / texel), int((cy * 4096 - oy) / texel)
        for ty in range(ty0, ty0 + 32):
            for tx in range(tx0, tx0 + 32):
                w_ = word(tx, ty)
                bid = w_ & 0xFFFF
                if not bid:
                    continue
                steps, code = (w_ >> 16) & 0xFF, w_ >> 24
                row = bodT[bid]
                if int(row[1]) == want and 2 <= steps <= 6:
                    sc = code
                    if best is None or sc > best[0]:
                        best = (sc, tx, ty, bid, row.copy())
if best is None:
    sys.exit('no %s texel found' % SPOT)
_, stx, sty, sbid, srow = best
X0, Y0, H0 = ox + (stx + 0.5) * texel, oy + (sty + 0.5) * texel, float(srow[0])
print('spot %s texel %d %d world %.0f %.0f body %d height %.1f class %d' % (SPOT, stx, sty, X0, Y0, sbid, H0, int(srow[1])))

U_INFO = 10
tex2d(U_INFO + 0, 0x823C, tX, tY, 0x8228, 0x1405, idx, 0x2600, 0x812F)        # RG32UI
tex2d(U_INFO + 1, 0x8236, aW, aH, 0x8D94, 0x1405, atl, 0x2600, 0x812F)        # R32UI
tex2d(U_INFO + 2, 0x8814, 256, bRows, 0x1908, 0x1406, bod, 0x2600, 0x812F)    # RGBA32F
rng = np.random.default_rng(1101)
for k in range(3):   # the WATR noise layers: 256 x 256 RG8, mipmapped, wrapped
    tex2d(k, 0x822B, 256, 256, 0x8227, 0x1401, rng.integers(0, 256, (256, 256, 2), dtype=np.uint8), mips=True)
GENERIC = tex2d(30, 0x881A, 256, 256, 0x1908, 0x1406, rng.random((256, 256, 4), dtype=np.float32), mips=True)
UGENERIC = tex2d(31, 0x8D70, 4, 4, 0x8D99, 0x1405, np.zeros((4, 4, 4), np.uint32), 0x2600, 0x812F)   # RGBA32UI
ARR = tex2darray(32, 128, 128, 6, (rng.random((6, 128, 128, 4), dtype=np.float32) * 0.2 - 0.1).astype(np.float32))

WMAT = np.array([
    [45 / 255., 62 / 255., 62 / 255., 60.0], [46 / 255., 61 / 255., 57 / 255., 1.0], [51 / 255., 68 / 255., 70 / 255., 0.5],
    [0.05, 0.10, 0.10, 0.0], [0.30, 0.30, 0.25, 0.3], [0.10, 0.10, 0.08, 0.0], [200.0, 0.8, 0.0, 0.0],
    [0.0, 0.0, 0.5, 3007.0], [0.9, 1.0, 0.6, 0.2], [0.93, 0.0, 0.0, 1.0], [1.0, 0.0, 0.058, 0.0], [0.5, 0.3, 0.2, 0.0],
    [1000.0, 500.0, 250.0, 0.0]], np.float32)
WSKY = np.array([[0.5, 0.55, 0.6, 0], [0.3, 0.35, 0.4, 0], [0.2, 0.3, 0.5, 0]], np.float32)

VERT = r'''#version 410 core
layout( location = 0 ) in vec2 corner;
uniform mat4 hView;
uniform mat4 hProj;
uniform vec3 hCentre;
uniform float hHalf;
uniform vec3 hLightView;
out vec3 LightDir;
out vec3 ViewDir;
out vec2 texCoord;
out mat3 btnMatrix;
flat out vec4 A;
out vec4 C;
flat out vec4 D;
out float rawVertexAlpha;
flat out mat3 reflMatrix;
out vec2 wwOcBase;
void main()
{
	vec3 w = hCentre + vec3( corner * hHalf, 0.0 );
	vec4 v = hView * vec4( w, 1.0 );
	ViewDir = -v.xyz;
	LightDir = hLightView;
	texCoord = corner;
	btnMatrix = mat3( 1.0 );
	A = vec4( 0.2, 0.2, 0.2, 1.0 );
	C = vec4( 1.0 );
	D = vec4( 1.0, 0.95, 0.85, 1.0 );
	rawVertexAlpha = 1.0;
	reflMatrix = mat3( 1.0 );
	wwOcBase = w.xy;
	gl_Position = hProj * v;
}
'''


def compile_one(kind, src):
    s = glCreateShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    glShaderSource(s, 1, ctypes.byref(b), None)
    glCompileShader(s)
    ok = I(0)
    glGetShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 15)
        glGetShaderInfoLog(s, 1 << 15, None, log)
        sys.exit('COMPILE FAIL %s' % log.value.decode(errors='replace')[:1500])
    return s


VS = compile_one(0x8B31, VERT)
SAMPLER_2D = {0x8B5E: 'f', 0x8DCA: 'i', 0x8DD2: 'u'}   # sampler2D, isampler2D, usampler2D
SAMPLER_2DARRAY = {0x8DC1}
FIXED = {'waterNoise0': 0, 'waterNoise1': 1, 'waterNoise2': 2, 'waterScene': 3, 'waterDepth': 4,
         'waterSsrRaw': 6, 'waterSsrFin': 7, 'waterInfoIndex': U_INFO, 'waterInfoAtlas': U_INFO + 1,
         'waterInfoBodies': U_INFO + 2}


def program(sdir, prog):
    frag = None
    for line in open(slc.shader_path(os.path.join(sdir, prog))):
        if line.startswith('shaders'):
            frag = line.split()[2]
    src = slc.expand(os.path.join(sdir, frag))
    if 'NUM_TEXTURE_UNITS' in src:
        src = src.replace('NUM_TEXTURE_UNITS', '30')
    pr = glCreateProgram()
    glAttachShader(pr, VS)
    glAttachShader(pr, compile_one(0x8B30, src))
    glLinkProgram(pr)
    ok = I(0)
    glGetProgramiv(pr, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 15)
        glGetProgramInfoLog(pr, 1 << 15, None, log)
        sys.exit('LINK FAIL %s %s' % (prog, log.value.decode(errors='replace')[:800]))
    glUseProgram(pr)
    n = I(0)
    glGetProgramiv(pr, 0x8B86, ctypes.byref(n))
    binds = []
    spare = 40
    for i in range(n.value):
        nm = ctypes.create_string_buffer(256)
        sz, ty = I(0), U(0)
        glGetActiveUniform(pr, i, 256, None, ctypes.byref(sz), ctypes.byref(ty), nm)
        name = nm.value.decode().split('[')[0]
        tv = ty.value
        if tv in SAMPLER_2D or tv in SAMPLER_2DARRAY or tv in (0x8B5F, 0x8B60, 0x8DC4, 0x8B62, 0x8DD3, 0x8DD4, 0x8DCF,
                                                                0x9108, 0x8DD7, 0x8DD8, 0x8DC5, 0x8B5D, 0x900C, 0x900D,
                                                                0x900F, 0x8DCC, 0x8DCB, 0x8DCE):
            u = FIXED.get(name)
            if u is None:
                u = spare
                spare += 1
                if tv == 0x8B5E:
                    binds.append((u, 0x0DE1, GENERIC))
                elif tv == 0x8DD2:
                    binds.append((u, 0x0DE1, UGENERIC))
                elif tv in SAMPLER_2DARRAY:
                    binds.append((u, 0x8C1A, ARR))
                # other kinds (cubes, 3D, shadow) stay unbound on their own unit: incomplete, read 0
            glUniform1i(glGetUniformLocation(pr, name.encode()), u)
    bi = glGetUniformBlockIndex(pr, b'globalUniforms')
    if bi != 0xFFFFFFFF:
        glUniformBlockBinding(pr, bi, 0)
    return pr, binds


def look(eye, at):
    fwd = (at - eye) / np.linalg.norm(at - eye)
    s = np.cross(fwd, [0.0, 0.0, 1.0])
    s /= np.linalg.norm(s)
    u = np.cross(s, fwd)
    R = np.array([s, u, -fwd])
    V = np.eye(4)
    V[:3, :3] = R
    V[:3, 3] = -R @ eye
    return V, R


# the eye 450 u over the water, looking 35 deg down (the frame is all water: the horizon above its top edge)
EYE = np.array([X0, Y0 - 640.0, H0 + 450.0])
AT = np.array([X0, Y0, H0])
VIEW, ROT = look(EYE, AT)
NEAR, FAR, FOVY = 5.0, 200000.0, np.radians(55.0)
SUN = np.array([0.35, 0.45, 0.82])
SUN /= np.linalg.norm(SUN)
BED = 150.0
fy = 1.0 / np.tan(FOVY / 2)
PM = np.zeros((4, 4))
PM[0, 0], PM[1, 1] = fy * FH / FW, fy
PM[2, 2], PM[2, 3], PM[3, 2] = -(FAR + NEAR) / (FAR - NEAR), -2 * FAR * NEAR / (FAR - NEAR), -1.0
ys, xs = np.mgrid[0:FH, 0:FW]
nx, ny = (xs + 0.5) / FW * 2 - 1, (ys + 0.5) / FH * 2 - 1
dvw = np.stack([nx / PM[0, 0], ny / PM[1, 1], -np.ones_like(nx)], -1) @ ROT
tb = (H0 - BED - EYE[2]) / np.where(np.abs(dvw[..., 2]) > 1e-9, dvw[..., 2], -1e-9)
ndcz = (PM[2, 2] * -tb + PM[2, 3]) / tb
DZ = np.where(tb > 0, 0.5 * (ndcz + 1), 1.0).astype(np.float32)
COL = np.stack([0.25 + 0.4 * xs / FW, 0.2 + 0.3 * ys / FH, 0.15 + 0.1 * np.sin(xs * 0.05), np.ones_like(nx)], -1).astype(np.float32)
tex2d(3, 0x8814, FW, FH, 0x1908, 0x1406, COL, 0x2600, 0x812F)
tex2d(4, 0x822E, FW, FH, 0x1903, 0x1406, DZ, 0x2600, 0x812F)
GB = np.zeros((FH, FW, 4), np.float32)   # the obscurance target for the ray pass: .a the opaque view depth
GB[..., 3] = np.where(tb > 0, tb, 0.0)
GBT = tex2d(5, 0x8814, FW, FH, 0x1908, 0x1406, GB, 0x2600, 0x812F)
SSRD = rng.random((FH // 2, FW // 2, 4), dtype=np.float32)
tex2d(6, 0x8814, FW // 2, FH // 2, 0x1908, 0x1406, SSRD)
tex2d(7, 0x8814, FW // 2, FH // 2, 0x1908, 0x1406, SSRD)
OUT = tex2d(8, 0x881A, FW, FH, 0x1908, 0x1406, None, 0x2600, 0x812F)
FBO = gen(glGenFramebuffers)
glBindFramebuffer(0x8D40, FBO)
glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, OUT, 0)
assert glCheckFramebufferStatus(0x8D40) == 0x8CD5
UBO = gen(glGenBuffers)
VAO = gen(glGenVertexArrays)
glBindVertexArray(VAO)
VBO = gen(glGenBuffers)
glBindBuffer(0x8892, VBO)
QUAD = np.array([-1, -1, 1, -1, -1, 1, 1, 1], np.float32)
glBufferData(0x8892, QUAD.nbytes, QUAD.ctypes.data, 0x88E4)
glEnableVertexAttribArray(0)
glVertexAttribPointer(0, 2, 0x1406, 0, 0, None)
QID = gen(glGenQueries)


def block(pr):
    names = ['viewMatrix', 'projectionMatrix', 'viewportDimensions', 'toneMapScale', 'brightnessScale']
    arr = (ctypes.c_char_p * len(names))(*[n.encode() for n in names])
    ids = (U * len(names))()
    glGetUniformIndices(pr, len(names), arr, ids)
    offs = (I * len(names))()
    glGetActiveUniformsiv(pr, len(names), ids, 0x8A3B, offs)
    buf = bytearray(512)

    def put(name, data):
        o = offs[names.index(name)]
        if 0 <= o < 512:
            b = data.tobytes()
            buf[o:o + len(b)] = b
    m3 = np.zeros((3, 4), np.float32)
    m3[:, :3] = ROT.T
    put('viewMatrix', m3)
    put('projectionMatrix', PM.T.astype(np.float32))
    put('viewportDimensions', np.array([0, 0, FW, FH], np.int32))
    put('toneMapScale', np.array([1.0], np.float32))
    put('brightnessScale', np.array([1.0], np.float32))
    glBindBuffer(0x8A11, UBO)
    b = (ctypes.c_char * len(buf)).from_buffer(buf)
    glBufferData(0x8A11, len(buf), ctypes.addressof(b), 0x88E8)
    glBindBufferBase(0x8A11, 0, UBO)


# the rows' uniforms: (name, value when on); the default ships ON, OFF rows are what turning them on would cost
ROWS_ON = ['clear', 'refl', 'glint', 'ocean', 'edge', 'foam', 'contact', 'riverfoam']
ROWS_OFF = ['depth', 'caustics', 'swell', 'skyvis', 'surf']
ROWS_U = {'clear': 'waterClearOn', 'refl': 'waterReflOn', 'glint': 'waterGlintOn', 'ocean': 'waterOceanOn',
          'contact': 'waterContactOn', 'riverfoam': 'waterRiverFoamOn', 'depth': 'waterDepthOn', 'caustics': 'waterCauOn',
          'swell': 'waterWavesOn', 'skyvis': 'waterSkyVisOn', 'surf': 'waterSurfOn'}


def setup(pr, rows, sslr=0, ssr=0):
    glUseProgram(pr)
    block(pr)
    L = lambda n: glGetUniformLocation(pr, n.encode())  # noqa: E731
    glUniformMatrix4fv(L('hView'), 1, 1, VIEW.astype(np.float32).ctypes.data)
    glUniformMatrix4fv(L('hProj'), 1, 1, PM.astype(np.float32).ctypes.data)
    glUniform3f(L('hCentre'), X0, Y0, H0)
    glUniform1f(L('hHalf'), 60000.0)
    glUniform3f(L('hLightView'), *(ROT @ SUN))
    crow = np.zeros((3, 4), np.float32)
    crow[:, :3] = ROT.T
    crow[:, 3] = EYE
    glUniform4fv(L('cellRow'), 3, crow.ctypes.data)
    glUniform4fv(L('waterMat'), 13, WMAT.ctypes.data)
    glUniform4fv(L('waterSky'), 3, WSKY.ctypes.data)
    glUniform4f(L('waterScroll0'), 0.1, 0.2, 0.3, 0.4)
    glUniform4f(L('waterScroll1'), 0.5, 0.6, 0, 0)
    glUniform1i(L('waterHaveDepth'), 1)
    glUniform1i(L('waterSslr'), sslr)
    glUniform1i(L('waterSsrOn'), ssr & 1)
    glUniform1i(L('waterSsrFlat'), (ssr >> 1) & 1)   # FIX1101A: the flat ray + the bent read
    glUniform1i(L('waterInfoOn'), 1)
    glUniform4f(L('waterInfoGrid'), ox, oy, texel, q16)
    glUniform4f(L('waterInfoLayout'), float(tX), float(tY), float(EDGE), float(PER))
    if SPOT == 'sea':   # the sea does not flow: a valid flow grid of zero words (one wave read); the river: two reads
        glUniform4f(L('waterFlowGrid'), ox, oy, texel, 1.0)
        glUniform4f(L('waterFlowLayout'), 4.0, 4.0, 1.0, 4.0)
    for k, un in ROWS_U.items():
        glUniform1i(L(un), 1 if k in rows else 0)
    glUniform1i(L('waterShoreOn'), (1 if 'edge' in rows else 0) | (2 if 'foam' in rows else 0))
    glUniform1i(L('waterOceanRed'), 2 if 'oceantile' in rows else 0)   # a preset step: one copy a cascade
    glUniform4f(L('waterShoreK0'), 64.0, 256.0, 192.0, 0.25)
    glUniform4f(L('waterShoreK1'), 96.0, 6.0, 0.3, 0.85)
    glUniform4f(L('waterShoreK2'), 9.94, 0.26, 40.0, 0.3)
    glUniform4f(L('waterShoreK3'), 0.25, 0.78, 0.35, 0.05)
    glUniform4f(L('waterShoreK4'), 35.0, 105.0, 64.0, 0.0)
    glUniform1i(L('waterShoreLaw'), 1)
    glUniform1i(L('waterShoreEdgeLaw'), 1)
    glUniform1f(L('waterShoreTime'), 7.5)
    glUniform4f(L('waterSurfK'), 8.0, 0.78, 7.0, 0.9)
    glUniform4f(L('waterContactK'), 4.0, 2.0, 0.04, 0.9)
    glUniform4f(L('waterRiverFoamK'), 8.0, 0.12, 0.4, 0.85)
    glUniform4f(L('waterRiverFoamK2'), 24.0, 8.0, 0.3, 0.8)
    glUniform4f(L('waterGlintK'), 16.0, 0.0204, 0.18, 0.015)
    glUniform1f(L('waterCauTime'), 2.0)
    cls = np.array([[1.0, 1680.0, 1680.0, 0.045], [16.0, 70.0, 280.0, 0.015], [12.0, 105.0, 560.0, 0.03]], np.float32)
    glUniform4fv(L('waterWavesClass'), 3, cls.ctypes.data)
    glUniform4f(L('waterWavesShore'), 64.0, 0.5, 4.0, 64.0)
    glUniform4f(L('waterWavesK'), 0.6, 0.8, 2.0, 1.0)
    invL = np.array([[1 / 1187.0, 1 / 97.1, 1 / 15.3, 1.0], [1 / 61.9, 1 / 9.53, 1 / 1.47, 0.6]], np.float32)
    glUniform4fv(L('waterOceanInvL'), 2, invL.ctypes.data)
    st = np.array([[0.6, 0.55, 0.26, 0.01], [0.05, 0.03, 0.012, 0.005]], np.float32)
    glUniform4fv(L('waterOceanSet'), 2, st.ctypes.data)
    sb = np.array([[8495.0, 0.5, 8400.0, 0], [100.0, 2.0, 1400.0, 0]], np.float32)
    glUniform4fv(L('waterOceanSetB'), 2, sb.ctypes.data)
    glUniform4f(L('waterOceanWind'), 0.0, 1.0, 9.94, 3.61)
    glUniform4f(L('waterOceanPatch'), 10.0, 20.0, 30.0, 40.0)
    glUniform4f(L('waterOceanRip'), 1.0, 0.4, 0.5, 0.6)
    glUniform4f(L('waterOceanFlow'), 1.0, 3.0, 0.5, 0.5)
    glUniform4f(L('waterOceanDisp'), 4096.0, 8192.0, 8.0, 64.0)
    corr = np.full((16, 4), 0.5, np.float32)
    glUniform4fv(L('waterOceanFlowCorr'), 16, corr.ctypes.data)
    glUniform4f(L('waterCloudGeo'), X0, Y0, 60000.0, 1.0)
    glUniform4f(L('waterCloudZ'), 8000.0, 16000.0, 30000.0, 0.0)
    glUniform4f(L('waterCloudCov'), 0.3, 0.3, 0.2, 0.0)
    glUniform3f(L('waterCloudTint'), 1.0, 1.0, 1.0)


def time_draw(pr, binds, rows, sslr=0, ssr=0, n=NREP):
    glBindFramebuffer(0x8D40, FBO)
    glViewport(0, 0, FW, FH)
    glDisable(0x0B71)
    glDisable(0x0BE2)
    for u, tgt, t in binds:
        glActiveTexture(0x84C0 + u)
        glBindTexture(tgt, t)
    if sslr:
        glActiveTexture(0x84C0 + 4)
        glBindTexture(0x0DE1, GBT)
    setup(pr, rows, sslr, ssr)
    times = []
    for _ in range(n):
        glBeginQuery(0x88BF, QID)
        glDrawArrays(0x0005, 0, 4)
        glEndQuery(0x88BF)
        v = ctypes.c_uint64(0)
        glGetQueryObjectui64v(QID, 0x8866, ctypes.byref(v))
        times.append(v.value / 1e6)
    if sslr:   # back to the frame's depth
        glActiveTexture(0x84C0 + 4)
        glBindTexture(0x0DE1, 0)
    e = glGetError()
    if e:
        sys.exit('GL error 0x%x' % e)
    return float(np.median(times))


SD = os.path.join(T, 'res', 'shaders')
SHIP = program(SD, 'fo4_water.prog')
LOOKP = program(SD, 'fo4_waterlook.prog')
DEF = set(ROWS_ON)
CASES = [('shipped fo4_water', SHIP, set(), 0, 0), ('look, all off', LOOKP, set(), 0, 0), ('default', LOOKP, DEF, 0, 0),
         ('default + ssr read', LOOKP, DEF, 0, 1)]
CASES += [('default - %s' % r, LOOKP, DEF - {r}, 0, 0) for r in ROWS_ON]
CASES += [('default + %s' % r, LOOKP, DEF | {r}, 0, 0) for r in ROWS_OFF]
# the water preset's steps (src/features/water/waterpreset.h): Low = the shipped program, Ultra = the default rows
P_MED = {'clear', 'refl', 'edge', 'riverfoam'}
MEDP = program(SD, 'fo4_waterlookm.prog') if os.path.exists(slc.shader_path(os.path.join(SD, 'fo4_waterlookm.prog'))) else LOOKP
HIGHP = LOOKP   # High draws with the full twin (a High-only twin measured slower: 1.098 vs 0.998 ms at the sea)
CASES += [('preset medium', MEDP, P_MED, 0, 3), ('preset high', HIGHP, P_MED | {'ocean', 'foam'}, 0, 3),
          ('preset ultra', LOOKP, DEF, 0, 3), ('preset low', SHIP, set(), 0, 1),
          ('ocean one copy (default rows)', LOOKP, DEF | {'oceantile'}, 0, 0),
          ('ssr ray pass shipped', SHIP, set(), 1, 0), ('ssr ray pass flat medium', MEDP, P_MED, 1, 2),
          ('ssr ray pass flat high', MEDP, P_MED, 1, 2),
          ('ssr ray pass flat ultra', MEDP, P_MED, 1, 2)]   # the ray pass always takes the Medium twin (cellwater.cpp)
CASES += [('ssr ray pass (default rows)', LOOKP, DEF, 1, 0), ('ssr ray pass flat', LOOKP, DEF, 1, 2),
          ('default + ssr read bent', LOOKP, DEF, 0, 3)]
for _ in range(2):   # warm-up
    for c in CASES:
        time_draw(c[1][0], c[1][1], c[2], c[3], c[4], 4)
res = {c[0]: [] for c in CASES}
for _ in range(5):
    for c in CASES:
        res[c[0]].append(time_draw(c[1][0], c[1][1], c[2], c[3], c[4]))
best = {k: min(v) for k, v in res.items()}
print('frame %d x %d, all water, spot %s; GPU ms (min of 5 rounds of the median of %d draws)' % (FW, FH, SPOT, NREP))
for k, v in res.items():
    print('COST %-32s %.3f  (rounds %s)' % (k, best[k], ' '.join('%.3f' % x for x in v)))
d = best['default']
print('TABLE term | ms at full-frame water (default - term, or + term for an OFF row)')
for r in ROWS_ON:
    print('TABLE %-10s %.3f (on)' % (r, d - best['default - %s' % r]))
for r in ROWS_OFF:
    print('TABLE %-10s %.3f (off; cost if turned on)' % (r, best['default + %s' % r] - d))
print('TABLE ssr-read   %.3f' % (best['default + ssr read'] - d))
print('TABLE ssr-rays   %.3f (the water ray pass, full size, the full normal: WW_CELL_SSR_WATER_RED=1)' % best['ssr ray pass (default rows)'])
print('TABLE ssr-rays-flat %.3f (the water ray pass, full size, the flat ray: the default)' % best['ssr ray pass flat'])
print('TABLE ssr-read-bent %.3f' % (best['default + ssr read bent'] - d))
for k in ('low', 'medium', 'high', 'ultra'):   # the colour pass with its SSR read; + the ray pass + the march (ssr_gate.py)
    print('PRESET %-7s colour pass %.3f ray pass %.3f' % (k, best['preset ' + k],
          best['ssr ray pass shipped'] if k == 'low' else best['ssr ray pass flat %s' % k] if ('ssr ray pass flat %s' % k) in best else best['ssr ray pass flat']))
