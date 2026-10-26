"""Offline gate of the far ground pages (lane FARPAGES1) through the real GL driver, no NifSkope.

    python -I tests/spells/fgpages_gpu.py <tree> [--red bc1|decalonly|nodecals]

A synthetic 3 x 3 cell ring (3 LTEX layers, weights and vertex colour varying over the grid, 3 decals, one over a
cell border) runs the tree's own shaders the way src/gl/cellfarground.cpp does: cell_fgpage paints the centre cell's
page, mips, cell_fgbc1 encodes each level into RG32UI, glReadPixels into a pixel buffer, glCompressedTexSubImage3D
into a BC1 array layer. Then, at the same texels:
  A  the live blend + decals (cell_farground.glsl, WW_FARGROUND 1, the lookup twin's fgGround)
  P  the painted page before compression (must equal A)
  B  the draw's fgGround (WW_FARGROUND 3) through the cell table and the BC1 page (A within BC1 error)
  and every level's BC1 blocks decoded against the painted level.
Reds: bc1 (fgBc1Red=1, endpoints swapped), decalonly (FARDRAW1's decal-only page, cell_fgstamp), nodecals (the page
without decals). Each must FAIL. Prints `N checks, M failures` and PASS/FAIL.
"""
import ctypes, math, os, shutil, sys, tempfile
import numpy as np

TREE = sys.argv[1]
RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S   # noqa: E402  (the hidden 4.1 core context, #include expansion)

GLuint, GLint, GLenum, GLf = ctypes.c_uint, ctypes.c_int, ctypes.c_uint, ctypes.c_float
gl = S.gl
S.context()
print('driver:', gl.glGetString(0x1F01).decode(), '|', gl.glGetString(0x1F02).decode())

def f(name, res, *args):
    p = gl.wglGetProcAddress(name.encode())
    if p:
        return ctypes.WINFUNCTYPE(res, *args)(p)
    g = getattr(gl, name)   # GL 1.1 entries come from opengl32 itself
    g.restype = res
    g.argtypes = list(args)
    return g

VP = ctypes.c_void_p
glGenTextures = f('glGenTextures', None, ctypes.c_int, ctypes.POINTER(GLuint))
glBindTexture = f('glBindTexture', None, GLenum, GLuint)
glTexImage2D = f('glTexImage2D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, VP)
glTexImage3D = f('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, VP)
glCompressedTexImage3D = f('glCompressedTexImage3D', None, GLenum, GLint, GLenum, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, ctypes.c_int, VP)
glCompressedTexSubImage3D = f('glCompressedTexSubImage3D', None, GLenum, GLint, GLint, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLenum, ctypes.c_int, VP)
glGetCompressedTexImage = f('glGetCompressedTexImage', None, GLenum, GLint, VP)
glGetTexImage = f('glGetTexImage', None, GLenum, GLint, GLenum, GLenum, VP)
glTexParameteri = f('glTexParameteri', None, GLenum, GLenum, GLint)
glPixelStorei = f('glPixelStorei', None, GLenum, GLint)
glGenerateMipmap = f('glGenerateMipmap', None, GLenum)
glActiveTexture = f('glActiveTexture', None, GLenum)
glGenBuffers = f('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
glBindBuffer = f('glBindBuffer', None, GLenum, GLuint)
glBufferData = f('glBufferData', None, GLenum, ctypes.c_ssize_t, VP, GLenum)
glTexBuffer = f('glTexBuffer', None, GLenum, GLenum, GLuint)
glGenFramebuffers = f('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
glBindFramebuffer = f('glBindFramebuffer', None, GLenum, GLuint)
glFramebufferTexture2D = f('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
glCheckFramebufferStatus = f('glCheckFramebufferStatus', GLenum, GLenum)
glDrawBuffers = f('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
glReadBuffer = f('glReadBuffer', None, GLenum)
glReadPixels = f('glReadPixels', None, GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, VP)
glViewport = f('glViewport', None, GLint, GLint, ctypes.c_int, ctypes.c_int)
glGenVertexArrays = f('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
glBindVertexArray = f('glBindVertexArray', None, GLuint)
glEnableVertexAttribArray = f('glEnableVertexAttribArray', None, GLuint)
glVertexAttribPointer = f('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, VP)
glDrawArrays = f('glDrawArrays', None, GLenum, GLint, ctypes.c_int)
glUseProgram = f('glUseProgram', None, GLuint)
glGetUniformLocation = f('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
glUniform1i = f('glUniform1i', None, GLint, GLint)
glUniform1f = f('glUniform1f', None, GLint, GLf)
glUniform2f = f('glUniform2f', None, GLint, GLf, GLf)
glUniform4f = f('glUniform4f', None, GLint, GLf, GLf, GLf, GLf)
glGetError = f('glGetError', GLenum)
glTexParameterf = f('glTexParameterf', None, GLenum, GLenum, GLf)

TEX2D, ARR, BUF = 0x0DE1, 0x8C1A, 0x8C2A
FB, DRAW_FB, READ_FB, COL0 = 0x8D40, 0x8CA9, 0x8CA8, 0x8CE0
PACK, UNPACK = 0x88EB, 0x88EC
BC1 = 0x83F0

def tex(target):
    t = GLuint(); glGenTextures(1, ctypes.byref(t)); glBindTexture(target, t.value); return t.value

def params(target, minf, magf, wrap=0x812F):
    glTexParameteri(target, 0x2801, minf); glTexParameteri(target, 0x2800, magf)
    glTexParameteri(target, 0x2802, wrap); glTexParameteri(target, 0x2803, wrap)

# ---- the tree's shaders, plus two test wrappers beside them (a copy: nothing is written into the tree)
SD = tempfile.mkdtemp(prefix='fgpages_', dir=os.environ.get('FG_SCRATCH') or None)
for n in os.listdir(os.path.join(TREE, 'res', 'shaders')):
    shutil.copy(os.path.join(TREE, 'res', 'shaders', n), SD)
WRAP = """#version 410 core
#define WW_FARGROUND %d
#include "cell_farground.glsl"
uniform vec2 tCell;
uniform float tSide;
out vec4 fragColor;
void main()
{
	float texel = 4096.0 / tSide;
	vec2 w = tCell + gl_FragCoord.xy * texel;
	fragColor = vec4( fgGround( vec3( w, 0.0 ), vec3( 1.0, 0.0, 1.0 ) ), 1.0 );
}
"""
for mode in (1, 3):
    open(os.path.join(SD, 't_fg%d.frag' % mode), 'w').write(WRAP % mode)
    open(os.path.join(SD, 't_fg%d.prog' % mode), 'w').write('shaders cell_ao.vert t_fg%d.frag\n' % mode)

createShader = f('glCreateShader', GLuint, GLenum)
shaderSource = f('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), VP)
compileShader = f('glCompileShader', None, GLuint)
getShaderiv = f('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
getShaderInfoLog = f('glGetShaderInfoLog', None, GLuint, ctypes.c_int, VP, ctypes.c_char_p)
createProgram = f('glCreateProgram', GLuint)
attach = f('glAttachShader', None, GLuint, GLuint)
link = f('glLinkProgram', None, GLuint)
getProgramiv = f('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))

def program(prog):
    files = [l.split()[1:] for l in open(os.path.join(SD, prog)) if l.startswith('shaders')][0]
    p = createProgram()
    for fn_ in files:
        src = S.expand(os.path.join(SD, fn_))
        s = createShader(0x8B30 if fn_.endswith('frag') else 0x8B31)
        b = ctypes.c_char_p(src.encode('latin-1')); shaderSource(s, 1, ctypes.byref(b), None); compileShader(s)
        ok = GLint(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(1 << 14); getShaderInfoLog(s, 1 << 14, None, log)
            sys.exit('COMPILE FAIL %s %s\n%s' % (prog, fn_, log.value.decode(errors='replace')[:1200]))
        attach(p, s)
    link(p)
    ok = GLint(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        sys.exit('LINK FAIL ' + prog)
    return p

class U:
    def __init__(self, p): self.p = p; glUseProgram(p)
    def loc(self, n): return glGetUniformLocation(self.p, n.encode())
    def i(self, n, v): glUniform1i(self.loc(n), v)
    def f1(self, n, v): glUniform1f(self.loc(n), v)
    def f2(self, n, a, b): glUniform2f(self.loc(n), a, b)
    def f4(self, n, a, b, c, d): glUniform4f(self.loc(n), a, b, c, d)

# ---- the synthetic ring: 3 x 3 cells from world (0, 0), the page cell is the centre (1, 1)
CELL, SPC, SPACING, TILING = 4096.0, 32, 128.0, 1024.0
NX = 3 * SPC + 1
rng = np.random.default_rng(7)
yy, xx = np.mgrid[0:NX, 0:NX].astype(np.float32)
w0 = 0.5 + 0.5 * np.sin(xx * 0.21) * np.cos(yy * 0.13)
w1 = (1.0 - w0) * (0.5 + 0.5 * np.sin(yy * 0.31 + 1.0))
w2 = np.clip(1.0 - w0 - w1, 0, 1)
W = np.stack([w0, w1, w2, np.zeros_like(w0)], -1)
Wq = np.round(np.clip(W, 0, 1) * 255).astype(np.uint32)
grid = np.zeros((NX, NX, 4), np.uint32)
grid[..., 0] = 0 | (1 << 16)
grid[..., 1] = 2 | (65535 << 16)
grid[..., 2] = Wq[..., 0] | (Wq[..., 1] << 8) | (Wq[..., 2] << 16) | (Wq[..., 3] << 24)
vc = np.round((0.75 + 0.25 * np.stack([np.sin(xx * 0.05), np.cos(yy * 0.07), np.sin((xx + yy) * 0.03)], -1)) * 255).astype(np.uint32)
grid[..., 3] = vc[..., 0] | (vc[..., 1] << 8) | (vc[..., 2] << 16) | (255 << 24)

LS = 512
v = np.mgrid[0:LS, 0:LS].astype(np.float32)
layers = []
for k, (a, b) in enumerate(((0.11, 0.07), (0.05, 0.19), (0.29, 0.023))):
    n = rng.random((LS, LS)).astype(np.float32) * 0.25
    base = 0.5 + 0.25 * np.sin(v[1] * a) * np.cos(v[0] * b) + n
    col = np.stack([base * (0.6 + 0.2 * k), base * (0.8 - 0.15 * k), base * (0.4 + 0.25 * k), np.ones_like(base)], -1)
    layers.append(np.clip(col * 255, 0, 255).astype(np.uint8))
ltex = np.ascontiguousarray(np.stack(layers))

AS = 256
av = np.mgrid[0:AS, 0:AS].astype(np.float32)
r_ = np.hypot(av[0] - 128, av[1] - 128) / 128
atlas = np.zeros((AS, AS, 4), np.float32)
atlas[..., 0] = 0.2 + 0.6 * (np.sin(av[1] * 0.2) > 0)
atlas[..., 1] = 0.15 + 0.3 * r_
atlas[..., 2] = 0.1
atlas[..., 3] = np.clip(1.2 - r_, 0, 1)
atlas = np.clip(atlas * 255, 0, 255).astype(np.uint8)

decals = [  # pos x, y, z, scale, yaw deg, box x0 y0 x1 y1
    (CELL + 1000, CELL + 1500, 0, 1.0, 30, -300, -200, 300, 200),
    (CELL + 2200, CELL + 2100, 0, 1.5, -20, -250, -250, 250, 250),
    (CELL + 3950, CELL + 800, 0, 1.0, 75, -400, -120, 400, 120),     # crosses into cell (2, 1)
]
recs = []
for (px, py, pz, sc, yaw, x0, y0, x1, y1) in decals:
    h = math.radians(yaw) * 0.5
    recs += [px, py, pz, sc, 0, 0, math.sin(h), math.cos(h), x0, y0, x1, y1, -50, 50, (x1 - x0) / AS, 0, 0, 0, 1, 1]
BIN = 512.0
BN = int(3 * CELL / BIN)
lists = [[] for _ in range(BN * BN)]
for i, (px, py, pz, sc, yaw, x0, y0, x1, y1) in enumerate(decals):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    pts = [(px + sc * (c * x - s * y), py + sc * (s * x + c * y)) for x in (x0, x1) for y in (y0, y1)]
    bx0, by0 = int(min(p[0] for p in pts) // BIN), int(min(p[1] for p in pts) // BIN)
    bx1, by1 = int(max(p[0] for p in pts) // BIN), int(max(p[1] for p in pts) // BIN)
    for by in range(max(0, by0), min(BN - 1, by1) + 1):
        for bx in range(max(0, bx0), min(BN - 1, bx1) + 1):
            lists[by * BN + bx].append(i)
binTex, ids = [], []
for l in lists:
    binTex += [len(ids), len(l), 0, 0]
    ids += l
while len(ids) % 4:
    ids.append(0)
buf = np.array(recs + binTex + ids, np.float32)
binOff = len(recs) // 4
idOff = binOff + BN * BN
maxLod = math.log2(AS)

# ---- upload (the units cellunits.h names)
UG, UL, UD, UA, UP, UH, UPF = 50, 51, 52, 53, 54, 55, 56
def unit(u, target, t): glActiveTexture(0x84C0 + u); glBindTexture(target, t)
glActiveTexture(0x84C0 + UG); tGrid = tex(TEX2D); glPixelStorei(0x0CF5, 1)
glTexImage2D(TEX2D, 0, 0x8D70, NX, NX, 0, 0x8D99, 0x1405, grid.ctypes.data); params(TEX2D, 0x2600, 0x2600)  # RGBA32UI
glActiveTexture(0x84C0 + UL); tL = tex(ARR)
glTexImage3D(ARR, 0, 0x8058, LS, LS, 3, 0, 0x1908, 0x1401, ltex.ctypes.data); glGenerateMipmap(ARR)
params(ARR, 0x2703, 0x2601, 0x2901); glTexParameterf(ARR, 0x84FE, 8.0)
glActiveTexture(0x84C0 + UD); b = GLuint(); glGenBuffers(1, ctypes.byref(b)); glBindBuffer(BUF, b.value)
glBufferData(BUF, buf.nbytes, buf.ctypes.data, 0x88E4); tD = tex(BUF); glTexBuffer(BUF, 0x8814, b.value)
glActiveTexture(0x84C0 + UA); tA = tex(TEX2D)
glTexImage2D(TEX2D, 0, 0x8058, AS, AS, 0, 0x1908, 0x1401, atlas.ctypes.data); glGenerateMipmap(TEX2D); params(TEX2D, 0x2703, 0x2601)
glActiveTexture(0x84C0 + UH); tH = tex(TEX2D); hz = np.zeros((NX, NX), np.float32)
glTexImage2D(TEX2D, 0, 0x822E, NX, NX, 0, 0x1903, 0x1406, hz.ctypes.data); params(TEX2D, 0x2601, 0x2601)

vao = GLuint(); glGenVertexArrays(1, ctypes.byref(vao)); glBindVertexArray(vao.value)
quad = np.array([-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0], np.float32)
vb = GLuint(); glGenBuffers(1, ctypes.byref(vb)); glBindBuffer(0x8892, vb.value)
glBufferData(0x8892, quad.nbytes, quad.ctypes.data, 0x88E4); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, 0x1406, 0, 0, None)
fbo = GLuint(); glGenFramebuffers(1, ctypes.byref(fbo))
one = (GLenum * 1)(COL0)

def target(t, level=0):
    glBindFramebuffer(FB, fbo.value); glFramebufferTexture2D(FB, COL0, TEX2D, t, level); glDrawBuffers(1, one)
    st = glCheckFramebufferStatus(FB)
    if st != 0x8CD5:
        sys.exit('incomplete target 0x%x' % st)

def base(u):
    u.i('fgGrid', UG); u.i('fgLtex', UL); u.f4('fgGridMap', 0, 0, 1 / SPACING, TILING); u.f2('fgGridN', NX, NX)
def decalU(u):
    u.i('fgDecal', UD); u.i('fgAtlas', UA); u.f4('fgBinMap', 0, 0, 1 / BIN, maxLod); u.f4('fgBinN', BN, BN, binOff, idOff)

SIDE = 512
LEVELS = int(math.log2(SIDE)) - 1   # down to 4x4
def read_rgba(t, side, level=0):
    glActiveTexture(0x84C0); glBindTexture(TEX2D, t)
    out = np.zeros((side, side, 4), np.uint8); glPixelStorei(0x0D05, 1); glGetTexImage(TEX2D, level, 0x1908, 0x1401, out.ctypes.data)
    return out

# ---- 1. paint the page (cell_fgpage, or the reds' decal-only / no-decal page)
glActiveTexture(0x84C0); scr = tex(TEX2D)
glTexImage2D(TEX2D, 0, 0x8058, SIDE, SIDE, 0, 0x1908, 0x1401, None); glGenerateMipmap(TEX2D); params(TEX2D, 0x2703, 0x2601)
glTexParameteri(TEX2D, 0x813D, LEVELS - 1); glBindTexture(TEX2D, 0)
for uu, t_, tg in ((UG, tGrid, TEX2D), (UL, tL, ARR), (UD, tD, BUF), (UA, tA, TEX2D), (UH, tH, TEX2D)):
    unit(uu, tg, t_)
u = U(program('cell_fgstamp.prog' if RED == 'decalonly' else 'cell_fgpage.prog'))
base(u); decalU(u); u.i('fgHeight', UH); u.f1('fgStampSide', SIDE); u.f2('fgStampCell', CELL, CELL)
u.i('fgPageDecals', 0 if RED == 'nodecals' else 1)
target(scr); glViewport(0, 0, SIDE, SIDE); glDrawArrays(5, 0, 4)
glFramebufferTexture2D(FB, COL0, TEX2D, 0, 0)
glActiveTexture(0x84C0); glBindTexture(TEX2D, scr); glGenerateMipmap(TEX2D)
painted = [read_rgba(scr, SIDE >> L, L) for L in range(LEVELS)]

# ---- 2. BC1 on the GPU into layer 1 of a 2-layer pool, through a pixel buffer, as paintJobs does
glActiveTexture(0x84C0 + UP); pool = tex(ARR)
for L in range(LEVELS):
    s = SIDE >> L
    glCompressedTexImage3D(ARR, L, BC1, s, s, 2, 0, (s // 4) * (s // 4) * 8 * 2, None)
glTexParameteri(ARR, 0x813C, 0); glTexParameteri(ARR, 0x813D, LEVELS - 1); params(ARR, 0x2703, 0x2601)
bw0 = SIDE // 4
glActiveTexture(0x84C0); blocks = tex(TEX2D)
glTexImage2D(TEX2D, 0, 0x823C, bw0, bw0, 0, 0x8228, 0x1405, None); params(TEX2D, 0x2600, 0x2600)  # RG32UI
pbo = GLuint(); glGenBuffers(1, ctypes.byref(pbo)); glBindBuffer(PACK, pbo.value); glBufferData(PACK, bw0 * bw0 * 8, None, 0x88E6)
glBindBuffer(PACK, 0)
e = U(program('cell_fgbc1.prog'))
glActiveTexture(0x84C0); glBindTexture(TEX2D, scr)
e.i('fgSrc', 0); e.i('fgBc1Red', 1 if RED == 'bc1' else 0)
target(blocks); glBindFramebuffer(READ_FB, fbo.value); glReadBuffer(COL0)
for L in range(LEVELS):
    s = SIDE >> L; bw = s // 4
    e.i('fgSrcLevel', L); glViewport(0, 0, bw, bw); glDrawArrays(5, 0, 4)
    glBindBuffer(PACK, pbo.value); glReadPixels(0, 0, bw, bw, 0x8228, 0x1405, None); glBindBuffer(PACK, 0)
    glBindBuffer(UNPACK, pbo.value); glActiveTexture(0x84C0 + UP); glBindTexture(ARR, pool)
    glCompressedTexSubImage3D(ARR, L, 0, 0, 1, s, s, 1, BC1, bw * bw * 8, None); glBindBuffer(UNPACK, 0)
err = glGetError()

def bc1_decode(raw, s):
    blk = np.frombuffer(raw, np.uint32).reshape(-1, 2)
    c0, c1, bits = blk[:, 0] & 0xFFFF, blk[:, 0] >> 16, blk[:, 1]
    def rgb(c): return np.stack([((c >> 11) & 31) / 31.0, ((c >> 5) & 63) / 63.0, (c & 31) / 31.0], -1)
    p0, p1 = rgb(c0), rgb(c1)
    four = (c0 > c1)[:, None]
    pal = np.stack([p0, p1, np.where(four, (2 * p0 + p1) / 3, (p0 + p1) / 2), np.where(four, (p0 + 2 * p1) / 3, 0)], 1)
    idx = (bits[:, None] >> (2 * np.arange(16, dtype=np.uint32))) & 3
    px = np.take_along_axis(pal, idx[..., None].astype(np.int64).repeat(3, -1), 1)   # blocks x 16 x 3
    bw = s // 4
    return px.reshape(bw, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(s, s, 3)

def bc1_ref(img):
    # independent reference: endpoints = the block's extremes along its principal axis, 565, nearest of 4 colours
    s = img.shape[0]; bw = s // 4
    blk = img.reshape(bw, 4, bw, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3)
    mu = blk.mean(1, keepdims=True); d = blk - mu
    cov = np.einsum('bki,bkj->bij', d, d)
    ax = np.ones((len(blk), 3)) / math.sqrt(3)
    for _ in range(8):
        ax = np.einsum('bij,bj->bi', cov, ax); ax /= np.maximum(np.linalg.norm(ax, axis=1, keepdims=True), 1e-12)
    t = np.einsum('bki,bi->bk', d, ax)
    e0 = mu[:, 0] + ax * t.max(1, keepdims=True); e1 = mu[:, 0] + ax * t.min(1, keepdims=True)
    q = lambda e: np.round(np.clip(e, 0, 1) * [31, 63, 31]) / [31, 63, 31]
    e0, e1 = q(e0), q(e1)
    pal = np.stack([e0, e1, (2 * e0 + e1) / 3, (e0 + 2 * e1) / 3], 1)
    dd = ((blk[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)
    out = np.take_along_axis(pal, dd.argmin(2)[..., None].repeat(3, -1), 1)
    return out.reshape(bw, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(s, s, 3)

# every level read back as blocks (layer 1) and decoded
dec0 = None
ref_rmse = []
glActiveTexture(0x84C0 + UP); glBindTexture(ARR, pool)
level_rmse = []
for L in range(LEVELS):
    s = SIDE >> L
    raw = ctypes.create_string_buffer((s // 4) * (s // 4) * 8 * 2)
    glGetCompressedTexImage(ARR, L, raw)
    dec = bc1_decode(raw.raw[(s // 4) * (s // 4) * 8:], s)
    ref = painted[L][..., :3].astype(np.float32) / 255
    level_rmse.append(float(np.sqrt(np.mean((dec - ref) ** 2)) * 255))
    ref_rmse.append(float(np.sqrt(np.mean((bc1_ref(ref) - ref) ** 2)) * 255))
    if L == 0:
        dec0 = dec * 255

# ---- 3. A (live) and B (the draw through the cell table) at the page's texels
glActiveTexture(0x84C0); outA = tex(TEX2D)
glTexImage2D(TEX2D, 0, 0x8058, SIDE, SIDE, 0, 0x1908, 0x1401, None); params(TEX2D, 0x2600, 0x2600)
outB = tex(TEX2D); glTexImage2D(TEX2D, 0, 0x8058, SIDE, SIDE, 0, 0x1908, 0x1401, None); params(TEX2D, 0x2600, 0x2600)
glBindTexture(TEX2D, 0)
for uu, t_, tg in ((UG, tGrid, TEX2D), (UL, tL, ARR), (UD, tD, BUF), (UA, tA, TEX2D)):
    unit(uu, tg, t_)
a = U(program('t_fg1.prog')); base(a); decalU(a); a.i('fgMode', 1); a.i('fgDecalsOn', 1)
a.f2('tCell', CELL, CELL); a.f1('tSide', SIDE)
target(outA); glViewport(0, 0, SIDE, SIDE); glDrawArrays(5, 0, 4)
glActiveTexture(0x84C0); outA0 = tex(TEX2D)
glTexImage2D(TEX2D, 0, 0x8058, SIDE, SIDE, 0, 0x1908, 0x1401, None); params(TEX2D, 0x2600, 0x2600); glBindTexture(TEX2D, 0)
a.i('fgDecalsOn', 0); target(outA0); glDrawArrays(5, 0, 4)
cells = np.zeros((3, 3, 4), np.uint16); cells[1, 1] = (1, 1, 255, 0)
glActiveTexture(0x84C0 + UG); tCells = tex(TEX2D)
glTexImage2D(TEX2D, 0, 0x8D76, 3, 3, 0, 0x8D99, 0x1403, cells.ctypes.data); params(TEX2D, 0x2600, 0x2600)  # RGBA16UI
unit(UP, ARR, pool); unit(UPF, ARR, pool)
bq = U(program('t_fg3.prog')); bq.i('fgMode', 3); bq.i('fgCells', UG); bq.i('fgPagesN', UP); bq.i('fgPagesF', UPF)
bq.f4('fgGridMap', 0, 0, 1 / SPACING, TILING); bq.f2('tCell', CELL, CELL); bq.f1('tSide', SIDE)
target(outB); glDrawArrays(5, 0, 4)
err = err or glGetError()
A = read_rgba(outA, SIDE)[..., :3].astype(np.float32)
A0 = read_rgba(outA0, SIDE)[..., :3].astype(np.float32)
B = read_rgba(outB, SIDE)[..., :3].astype(np.float32)
P = painted[0][..., :3].astype(np.float32)
dec_mask = None

# ---- verdicts
checks = fails = 0
def check(ok, what):
    global checks, fails
    checks += 1
    if not ok:
        fails += 1
        print('FAIL', what)

rmse = lambda x, y: float(np.sqrt(np.mean((x - y) ** 2)))
pa, ab = np.abs(P - A), np.abs(A - B)
decalpx = int(np.count_nonzero(np.abs(A - A0).max(-1) > 8))
print('red: %s' % (RED or 'none'))
print('GL error after the encode + draws: 0x%x' % err)
print('painted page vs live blend + decals: max %.1f, rmse %.2f (8-bit levels)' % (pa.max(), rmse(P, A)))
print('drawn BC1 page vs live: max %.1f, rmse %.2f, mean %.2f' % (ab.max(), rmse(A, B), float(ab.mean())))
print('BC1 levels decoded vs painted levels, rmse: ' + ' '.join('%d:%.2f' % (L, r) for L, r in enumerate(level_rmse)))
print('reference CPU encoder (principal axis), rmse:  ' + ' '.join('%d:%.2f' % (L, r) for L, r in enumerate(ref_rmse)))
print('drawn page vs its own level 0 decoded here: max %.1f (the draw samples the texel it was given)' % float(np.abs(B - dec0).max()))
print('live texels the decals change over the bare ground: %d of %d' % (decalpx, SIDE * SIDE))
check(err == 0, 'no GL error')
check(float(A.std()) > 10, 'the live ground is not flat (a real picture)')
check(pa.max() <= 2, 'the page = the live blend + decals at its texels (max <= 2 levels)')
check(decalpx > 2000, 'the decals are in the live picture (> 2000 texels changed)')
check(float(np.abs(B - dec0).max()) <= 2, 'the draw reads the page texel for texel (max <= 2 levels vs the decoded level 0)')
check(rmse(A, B) <= 1.15 * ref_rmse[0] + 1.0, 'the drawn BC1 page within BC1 error of the live ground (rmse <= 1.15 x the reference encoder + 1)')
check(all(r <= 1.15 * q + 1.0 for r, q in zip(level_rmse, ref_rmse)), 'every BC1 level within 1.15 x the reference encoder + 1')
print('%d checks, %d failures' % (checks, fails))
print('FAIL' if fails else 'PASS')
shutil.rmtree(SD, ignore_errors=True)
sys.exit(1 if fails else 0)
