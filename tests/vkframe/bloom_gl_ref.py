"""lane VKFRAME1 (item 3): the GL bloom chain (cell_bloom_down / cell_bloom_hold / cell_bloom_up, the tree's .frag files) on
dumped inputs, in a hidden GL 4.1 context (tests/spells/shader_link_check.py), no NifSkope. Writes one job folder per
input x arm for tests/vkframe/bloommatch.cpp, which runs the Fusion Shaders HLSL (res/shaders/hlsl/post) on Vulkan and
compares every level's half-float bits.

    python -I tests/vkframe/bloom_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr> [exposure]]

Inputs: `real` = the .hdr dump (int32 W, H, then RGBA float32 rows bottom-up) times the exposure (default: the one that puts
the 99th luminance percentile at 2, so the threshold 1 cuts through it), as half floats; `syn` = 640x360 random HDR with
NaN, +-inf and > 65504 texels (the sanitize edges). Arms:
  A  hold on (ring of 8 random layers, k 5, n 8; level 0 goes to layer k, the hold mean to mip 0), Karis (red 0),
     radius 1, dispersion 0.15
  B  hold off (level 0 straight to mip 0), plain box (red 1), radius 1.6, dispersion 0
Up chain weights: level l srcW = 0.55 + 0.05 l, accW = 1 (the deepest accumulated: 0.7), as bandWeights shapes them.
Each job folder: job.txt, src.bin, ring.bin (A), gl_mip<i>.bin, gl_up<i>.bin (int32 W, H, then uint16 RGBA), gl_ms.txt
(the chain's GPU ms, GL_TIME_ELAPSED, median of 32 after 2 warm-ups).
"""
import ctypes, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
POST = os.path.join(TREE, 'res', 'shaders', 'features', 'post')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = sys.argv[1]
HDR = sys.argv[2] if len(sys.argv) > 2 else 'X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr'
EXPO = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
MIPS = 6

S.context()
gl = S.gl


def _fn(name, res, *a):
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *a)(addr)
    f = getattr(gl, name)
    f.restype, f.argtypes = res, list(a)
    return f


U, I, E, F = ctypes.c_uint, ctypes.c_int, ctypes.c_uint, ctypes.c_float
VP = ctypes.c_void_p
createShader = _fn('glCreateShader', U, E); shaderSource = _fn('glShaderSource', None, U, I, ctypes.POINTER(ctypes.c_char_p), VP)
compileShader = _fn('glCompileShader', None, U); getShaderiv = _fn('glGetShaderiv', None, U, E, ctypes.POINTER(I))
getShaderInfoLog = _fn('glGetShaderInfoLog', None, U, I, VP, ctypes.c_char_p)
createProgram = _fn('glCreateProgram', U); attach = _fn('glAttachShader', None, U, U); link = _fn('glLinkProgram', None, U)
getProgramiv = _fn('glGetProgramiv', None, U, E, ctypes.POINTER(I)); useProgram = _fn('glUseProgram', None, U)
uloc = _fn('glGetUniformLocation', I, U, ctypes.c_char_p)
u1i = _fn('glUniform1i', None, I, I); u2f = _fn('glUniform2f', None, I, F, F); u4f = _fn('glUniform4f', None, I, F, F, F, F)
genVA = _fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bindVA = _fn('glBindVertexArray', None, U)
genTex = _fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = _fn('glBindTexture', None, E, U)
texImage = _fn('glTexImage2D', None, E, I, I, I, I, I, E, E, VP)
texImage3 = _fn('glTexImage3D', None, E, I, I, I, I, I, I, E, E, VP)
texParam = _fn('glTexParameteri', None, E, E, I); activeTex = _fn('glActiveTexture', None, E)
genFB = _fn('glGenFramebuffers', None, I, ctypes.POINTER(U)); bindFB = _fn('glBindFramebuffer', None, E, U)
fbTex = _fn('glFramebufferTexture2D', None, E, E, E, U, I); fbLayer = _fn('glFramebufferTextureLayer', None, E, E, U, I, I)
checkFB = _fn('glCheckFramebufferStatus', E, E)
viewport = _fn('glViewport', None, I, I, I, I); drawArrays = _fn('glDrawArrays', None, E, I, I)
readPixels = _fn('glReadPixels', None, I, I, I, I, E, E, VP)
pixelStore = _fn('glPixelStorei', None, E, I); finish = _fn('glFinish', None)
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
getError = _fn('glGetError', E)

T2D, T2DA, FBO, HALF, RGBA, RGBA16F = 0x0DE1, 0x8C1A, 0x8D40, 0x140B, 0x1908, 0x881A
va = U(); genVA(1, ctypes.byref(va)); bindVA(va)
pixelStore(0x0CF5, 1); pixelStore(0x0D05, 1)
VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""


def compile_(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = I(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('BLOOM-GL-REF FAIL (compile)'); sys.exit(2)
    return s


def program(frag_file):
    p = createProgram()
    attach(p, compile_(0x8B31, VERT)); attach(p, compile_(0x8B30, S.expand(os.path.join(POST, frag_file)))); link(p)
    ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('BLOOM-GL-REF FAIL (link)'); sys.exit(2)
    return p


P_DOWN, P_HOLD, P_UP = program('cell_bloom_down.frag'), program('cell_bloom_hold.frag'), program('cell_bloom_up.frag')


def tex(w, h, half=None):
    """cellbloom.cpp makeTex: RGBA16F, linear, clamped, one level"""
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
    d = None if half is None else np.ascontiguousarray(half, np.uint16)
    texImage(T2D, 0, RGBA16F, w, h, 0, RGBA, HALF, None if d is None else d.ctypes.data)
    texParam(T2D, 0x2801, 0x2601); texParam(T2D, 0x2800, 0x2601)
    texParam(T2D, 0x2802, 0x812F); texParam(T2D, 0x2803, 0x812F); texParam(T2D, 0x813D, 0)
    return t.value


def ring_tex(w, h, half):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2DA, t)
    d = np.ascontiguousarray(half, np.uint16)
    texImage3(T2DA, 0, RGBA16F, w, h, 8, 0, RGBA, HALF, d.ctypes.data)
    texParam(T2DA, 0x2801, 0x2600); texParam(T2DA, 0x2800, 0x2600)
    texParam(T2DA, 0x2802, 0x812F); texParam(T2DA, 0x2803, 0x812F)
    return t.value


fbo = U(); genFB(1, ctypes.byref(fbo)); bindFB(FBO, fbo)


def target(t, w, h, layer=-1):
    if layer >= 0:
        fbLayer(FBO, 0x8CE0, t, 0, layer)
    else:
        fbTex(FBO, 0x8CE0, T2D, t, 0)
    viewport(0, 0, w, h)


def setu(p, name, *v):
    loc = uloc(p, name.encode())
    if loc < 0:
        return
    if len(v) == 1:
        u1i(loc, int(v[0]))
    elif len(v) == 2:
        u2f(loc, *v)
    else:
        u4f(loc, *v)


def bind(unit, kind, t):
    activeTex(0x84C0 + unit); bindTex(kind, t)


def read(t, w, h, layer=-1):
    target(t, w, h, layer)
    a = np.zeros((h, w, 4), np.uint16)
    readPixels(0, 0, w, h, RGBA, HALF, a.ctypes.data)
    return a


def plan(w, h):
    dims = []
    for _ in range(MIPS):
        if w <= 1 and h <= 1:
            break
        w, h = max((w + 1) // 2, 1), max((h + 1) // 2, 1)
        dims.append((w, h))
    return dims


def weights(level, n):
    src = 0.55 + 0.05 * level
    acc = 0.7 if level + 2 == n else 1.0     # accumulated level + 1 == mipCount: the deepest band's own weight
    return src, acc


class Chain:
    def __init__(self, src_half, ring_half, arm):
        self.H, self.W = src_half.shape[:2]
        self.arm = arm
        self.dims = plan(self.W, self.H)
        self.src = tex(self.W, self.H, src_half)
        self.mip = [tex(w, h) for w, h in self.dims]
        self.up = [tex(w, h) for w, h in self.dims[:-1]]
        self.ring = ring_tex(self.dims[0][0], self.dims[0][1], ring_half) if arm['hold'] else 0

    def run(self):
        a, n = self.arm, len(self.dims)
        for i, (w, h) in enumerate(self.dims):
            if i == 0 and a['hold']:
                target(self.ring, w, h, a['k'])
            else:
                target(self.mip[i], w, h)
            useProgram(P_DOWN)
            bind(8, T2D, self.src if i == 0 else self.mip[i - 1])
            setu(P_DOWN, 'bloomSrc', 8); setu(P_DOWN, 'bloomDst', float(w), float(h))
            setu(P_DOWN, 'bloomParams', a['thr'], a['knee'], 1.0 if i == 0 else 0.0, 1.0)
            setu(P_DOWN, 'bloomRed', a['red'])
            drawArrays(4, 0, 3)
            if i == 0 and a['hold']:
                target(self.mip[0], w, h)
                useProgram(P_HOLD)
                bind(8, T2DA, self.ring)
                setu(P_HOLD, 'bloomRing', 8); setu(P_HOLD, 'bloomHoldK', a['k']); setu(P_HOLD, 'bloomHoldN', a['n'])
                drawArrays(4, 0, 3)
                bind(8, T2DA, 0)
        acc = self.mip[n - 1]
        for off in range(n - 1, 0, -1):
            lv = off - 1
            w, h = self.dims[lv]
            sw, aw = weights(lv, n)
            target(self.up[lv], w, h)
            useProgram(P_UP)
            bind(8, T2D, self.mip[lv]); bind(9, T2D, acc)
            setu(P_UP, 'bloomSrc', 8); setu(P_UP, 'bloomMip', 9); setu(P_UP, 'bloomDst', float(w), float(h))
            setu(P_UP, 'bloomParams', a['radius'], sw, aw, a['disp'])
            drawArrays(4, 0, 3)
            acc = self.up[lv]


def put(path, a):
    with open(path, 'wb') as f:
        f.write(np.array(a.shape[:2][::-1], np.int32).tobytes())
        f.write(np.ascontiguousarray(a, np.uint16).tobytes())


def job(name, src_half, arm, rng, time_it):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    dims = plan(src_half.shape[1], src_half.shape[0])
    ring = None
    if arm['hold']:
        w0, h0 = dims[0]
        ring = np.exp(rng.normal(-1.0, 1.5, (8, h0, w0, 4))).astype(np.float16).view(np.uint16)
    c = Chain(src_half, ring, arm)
    c.run(); finish()
    if getError() != 0:
        print('BLOOM-GL-REF FAIL (gl error)'); sys.exit(2)
    put(os.path.join(d, 'src.bin'), src_half)
    if ring is not None:
        with open(os.path.join(d, 'ring.bin'), 'wb') as f:
            f.write(np.array([dims[0][0], dims[0][1], 8], np.int32).tobytes()); f.write(ring.tobytes())
    for i, (w, h) in enumerate(dims):
        put(os.path.join(d, 'gl_mip%d.bin' % i), read(c.mip[i], w, h))
        if i + 1 < len(dims):
            put(os.path.join(d, 'gl_up%d.bin' % i), read(c.up[i], w, h))
    with open(os.path.join(d, 'job.txt'), 'w') as f:
        f.write('hold %d\nk %d\nn %d\nred %d\nthr %.9g\nknee %.9g\nradius %.9g\ndisp %.9g\nmips %d\n'
                % (arm['hold'], arm['k'], arm['n'], arm['red'], arm['thr'], arm['knee'], arm['radius'], arm['disp'], len(dims)))
        for lv in range(len(dims) - 1):
            sw, aw = weights(lv, len(dims))
            f.write('w%d %.9g %.9g\n' % (lv, sw, aw))
    ms = None
    if time_it:
        q = U(); genQ(1, ctypes.byref(q))
        t = []
        for r in range(34):
            finish(); beginQ(0x88BF, q); c.run(); endQ(0x88BF)
            v = ctypes.c_uint64(); getQ64(q, 0x8866, ctypes.byref(v))
            if r >= 2:
                t.append(v.value / 1e6)
        ms = float(np.median(t))
        with open(os.path.join(d, 'gl_ms.txt'), 'w') as f:
            f.write('%.5f\n' % ms)
    print('job %s %dx%d mips %s%s' % (name, src_half.shape[1], src_half.shape[0], ' '.join('%dx%d' % x for x in dims),
                                       '' if ms is None else ' GL chain %.4f ms (median of 32)' % ms))


def main():
    rng = np.random.default_rng(11)
    with open(HDR, 'rb') as f:
        w, h = np.frombuffer(f.read(8), np.int32)
        px = np.frombuffer(f.read(), np.float32).reshape(h, w, 4).copy()
    lum = px[..., 0] * 0.2126 + px[..., 1] * 0.7152 + px[..., 2] * 0.0722
    lum = lum[np.isfinite(lum)]
    expo = EXPO if EXPO > 0 else 2.0 / max(float(np.percentile(lum, 99)), 1e-6)
    real = (px * np.float32(expo)).astype(np.float16).view(np.uint16)
    print('real input %s %dx%d, exposure %.6g (99th luminance -> 2), max %.4g' % (HDR, w, h, expo, float(np.nanmax(px)) * expo))
    syn = np.exp(rng.normal(-0.5, 2.0, (360, 640, 4))).astype(np.float32)
    m = rng.random((360, 640))
    syn[m < 0.002] = np.nan
    syn[(m >= 0.002) & (m < 0.004)] = np.inf
    syn[(m >= 0.004) & (m < 0.005)] = -np.inf
    syn[(m >= 0.005) & (m < 0.006)] = 1e6     # > 65504: inf as half
    syn = syn.astype(np.float16).view(np.uint16)
    A = dict(hold=1, k=5, n=8, red=0, thr=1.0, knee=0.5, radius=1.0, disp=0.15)
    B = dict(hold=0, k=0, n=1, red=1, thr=1.0, knee=0.5, radius=1.6, disp=0.0)
    job('real_A', real, A, rng, True)
    job('real_B', real, B, rng, True)
    job('syn_A', syn, A, rng, False)
    job('syn_B', syn, B, rng, False)
    print('BLOOM-GL-REF OK')


main()
