"""lane VKFRAME1 (P5): the GL Modern temporal AA resolve (the tree's res/shaders/features/post/taa2.frag) on a dumped frame,
in a hidden GL 4.1 context (tests/spells/shader_link_check.py), no NifSkope. Writes the inputs once per source and one job
folder per arm for tests/vkframe/taamatch.cpp, which runs res/shaders/hlsl/post/taa_resolve.hlsl on Vulkan and compares
the three outputs' half-float bits.

    python -I tests/vkframe/taa_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr>]

Sources: `real` = the .hdr dump (int32 W, H, then RGBA float32 rows); `syn` = 640x360 random HDR with NaN, +-inf and
> 65504 texels. Every source gets a history (the frame one pixel over, in the resolve's own space, +-5 % noise, a = 1 -
a depth near the frame's), camera vectors (still on the left third: the lock paths; 0.5 .. 3 px a frame elsewhere),
TAAINPUTS1 velocity (+ an own-motion band) and mask (sparse reactive / composition / own-motion squares), a lock (flip
rate, sign, recent range) and depth (smooth with steps), and a reproj matrix (a slight push forward).
Arms: A Ultra linear inputs, H High linear inputs, M Medium display (dithered), L Low linear, N no history, X / Y the
taa2Red bits (1|4|64 and 2|8|32|128) at Ultra. Input files: int32 W, H, then cur / hist / lock / mask as uint16 RGBA,
mv / vel as float32 RG, depth as float32. Job folders: job.txt, gl_o0.bin, gl_o1.bin, gl_o2.bin (uint16 RGBA), gl_ms.txt
(A and H: the resolve's GPU ms, GL_TIME_ELAPSED, median of 32 after 0.5 s of warm-up resolves).
"""
import ctypes, os, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
POST = os.path.join(TREE, 'res', 'shaders', 'features', 'post')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = sys.argv[1]
HDR = sys.argv[2] if len(sys.argv) > 2 else 'X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr'

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
u1i = _fn('glUniform1i', None, I, I); u1f = _fn('glUniform1f', None, I, F); u2f = _fn('glUniform2f', None, I, F, F)
uM4 = _fn('glUniformMatrix4fv', None, I, I, ctypes.c_ubyte, VP)
genVA = _fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bindVA = _fn('glBindVertexArray', None, U)
genTex = _fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = _fn('glBindTexture', None, E, U)
texImage = _fn('glTexImage2D', None, E, I, I, I, I, I, E, E, VP)
texParam = _fn('glTexParameteri', None, E, E, I); activeTex = _fn('glActiveTexture', None, E)
genFB = _fn('glGenFramebuffers', None, I, ctypes.POINTER(U)); bindFB = _fn('glBindFramebuffer', None, E, U)
fbTex = _fn('glFramebufferTexture2D', None, E, E, E, U, I)
drawBuffers = _fn('glDrawBuffers', None, I, VP); readBuffer = _fn('glReadBuffer', None, E)
checkFB = _fn('glCheckFramebufferStatus', E, E)
viewport = _fn('glViewport', None, I, I, I, I); drawArrays = _fn('glDrawArrays', None, E, I, I)
readPixels = _fn('glReadPixels', None, I, I, I, I, E, E, VP)
pixelStore = _fn('glPixelStorei', None, E, I); finish = _fn('glFinish', None)
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
getError = _fn('glGetError', E)

T2D, FBO, HALF, FLOAT, RGBA, RG, RED = 0x0DE1, 0x8D40, 0x140B, 0x1406, 0x1908, 0x8227, 0x1903
RGBA16F, RG32F, R32F = 0x881A, 0x8230, 0x822E
NEAREST, LINEAR = 0x2600, 0x2601
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
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('TAA-GL-REF FAIL (compile)'); sys.exit(2)
    return s


P = createProgram()
attach(P, compile_(0x8B31, VERT)); attach(P, compile_(0x8B30, S.expand(os.path.join(POST, 'taa2.frag')))); link(P)
_ok = I(); getProgramiv(P, 0x8B82, ctypes.byref(_ok))
if not _ok.value:
    print('TAA-GL-REF FAIL (link)'); sys.exit(2)


def tex(w, h, internal, fmt, typ, data, filt):
    """gametaa.cpp makeTex: one level, clamped, the given filter"""
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
    d = None if data is None else np.ascontiguousarray(data)
    texImage(T2D, 0, internal, w, h, 0, fmt, typ, None if d is None else d.ctypes.data)
    texParam(T2D, 0x2801, filt); texParam(T2D, 0x2800, filt)
    texParam(T2D, 0x2802, 0x812F); texParam(T2D, 0x2803, 0x812F); texParam(T2D, 0x813D, 0)
    return t.value


fbo = U(); genFB(1, ctypes.byref(fbo))


def setu(name, kind, *v):
    loc = uloc(P, name.encode())
    if loc < 0:
        return
    if kind == 'i':
        u1i(loc, int(v[0]))
    elif kind == 'f':
        u1f(loc, float(v[0]))
    elif kind == '2f':
        u2f(loc, float(v[0]), float(v[1]))
    else:
        m = np.ascontiguousarray(v[0], np.float32)
        uM4(loc, 1, 0, m.ctypes.data)


def put(path, a, header=None):
    with open(path, 'wb') as f:
        f.write(np.array(header if header else a.shape[:2][::-1], np.int32).tobytes())
        f.write(np.ascontiguousarray(a).tobytes())


def half(a):
    return a.astype(np.float16).view(np.uint16)


def karis(c, expose):
    c = np.maximum(np.nan_to_num(c[..., :3], nan=0.0, posinf=6e4, neginf=0.0), 0) * expose
    return c / (1.0 + c.max(-1, keepdims=True))


def make_inputs(name, cur, expose, rng):
    """cur: float32 (H, W, 4) linear. Writes in_<name>/ and returns the arrays and GL textures."""
    h, w = cur.shape[:2]
    d = os.path.join(OUT, 'in_' + name)
    os.makedirs(d, exist_ok=True)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    # depth: smooth with steps (objects in front)
    depth = (0.6 + 0.3 * np.sin(xx / 97.0) * np.cos(yy / 71.0)).astype(np.float32)
    for _ in range(40):
        x0, y0 = rng.integers(0, w), rng.integers(0, h)
        depth[y0:y0 + rng.integers(4, 80), x0:x0 + rng.integers(4, 80)] = rng.uniform(0.1, 0.5)
    # camera vectors, uv units (previous minus current): still on the left third, 0.5 .. 3 px a frame elsewhere
    mpx = np.stack([1.2 + 1.5 * np.sin(yy / 53.0), -0.4 + 1.1 * np.cos(xx / 61.0)], -1)
    mpx[:, : w // 3] *= 0.05
    mv = (mpx / np.array([w, h], np.float32)).astype(np.float32)
    vel = mv.copy()
    band = (yy > h * 0.6) & (yy < h * 0.7)
    vel[band] += np.array([4.0 / w, 0.0], np.float32)
    mask = np.zeros((h, w, 4), np.float32)
    for _ in range(60):
        x0, y0 = rng.integers(0, w), rng.integers(0, h)
        ch = rng.integers(0, 3)
        mask[y0:y0 + rng.integers(4, 40), x0:x0 + rng.integers(4, 40), ch] = rng.choice([0.3, 1.0])
    mask[band, 2] = 1.0
    # history: the frame one pixel over in the resolve's space (display values on a display arm are the same shape)
    t = karis(cur, expose)
    hist = np.empty((h, w, 4), np.float32)
    hist[..., :3] = np.roll(t, 1, axis=1) * rng.uniform(0.95, 1.05, (h, w, 1))
    hist[..., 3] = 1.0 - depth * rng.uniform(0.97, 1.06, (h, w))
    lock = np.empty((h, w, 4), np.float32)
    lock[..., 0] = rng.random((h, w))
    lock[..., 1] = rng.choice([-1.0, 0.0, 1.0], (h, w))
    lock[..., 2:] = rng.uniform(0.0, 0.05, (h, w, 2))
    arrs = dict(cur=half(cur), hist=half(hist), lock=half(lock), mask=half(mask), mv=mv, vel=vel, depth=depth)
    disp = np.clip(t, 0, 1) ** (1 / 2.2)
    curd = np.concatenate([disp, np.ones((h, w, 1), np.float32)], -1)
    arrs['curd'] = half(curd)
    histd = hist.copy()
    histd[..., :3] = np.roll(np.clip(disp, 0, 1), 1, axis=1)
    arrs['histd'] = half(histd)
    for k in ('cur', 'hist', 'lock', 'mask', 'curd', 'histd'):
        put(os.path.join(d, k + '.bin'), arrs[k])
    put(os.path.join(d, 'mv.bin'), mv, [w, h])
    put(os.path.join(d, 'vel.bin'), vel, [w, h])
    put(os.path.join(d, 'depth.bin'), depth, [w, h])
    texs = dict(
        cur=tex(w, h, RGBA16F, RGBA, HALF, arrs['cur'], NEAREST), curd=tex(w, h, RGBA16F, RGBA, HALF, arrs['curd'], NEAREST),
        hist=tex(w, h, RGBA16F, RGBA, HALF, arrs['hist'], LINEAR), histd=tex(w, h, RGBA16F, RGBA, HALF, arrs['histd'], LINEAR),
        lock=tex(w, h, RGBA16F, RGBA, HALF, arrs['lock'], NEAREST), mask=tex(w, h, RGBA16F, RGBA, HALF, arrs['mask'], NEAREST),
        mv=tex(w, h, RG32F, RG, FLOAT, mv, NEAREST), vel=tex(w, h, RG32F, RG, FLOAT, vel, NEAREST),
        depth=tex(w, h, R32F, RED, FLOAT, depth, NEAREST))
    return w, h, texs


# a slight push forward: clip x, y grow by 1 %, depth moves a little (current unjittered clip -> previous)
REPROJ = np.array([[1.01, 0, 0, 0], [0, 1.01, 0, 0], [0, 0, 1.0, 0.004], [0, 0, 0, 1.0]], np.float32)  # rows = GL columns


def run(w, h, texs, arm, outs):
    bindFB(FBO, fbo)
    for i, t in enumerate(outs):
        fbTex(FBO, 0x8CE0 + i, T2D, t, 0)
    bufs = (ctypes.c_uint * 3)(0x8CE0, 0x8CE1, 0x8CE2)
    drawBuffers(3, bufs)
    viewport(0, 0, w, h)
    useProgram(P)
    disp = arm['linear'] == 0
    units = [('curTex', 'curd' if disp else 'cur'), ('histTex', 'histd' if disp else 'hist'), ('mvTex', 'mv'),
             ('depthTex', 'depth'), ('lockTex', 'lock'), ('velTex', 'vel'), ('maskTex', 'mask')]
    for u, (n, k) in enumerate(units):
        activeTex(0x84C0 + u); bindTex(T2D, texs[k]); setu(n, 'i', u)
    setu('invSize', '2f', 1.0 / w, 1.0 / h)
    setu('jitterPx', '2f', arm['jx'], arm['jy'])
    setu('taa2Tier', 'i', arm['tier']); setu('taa2Red', 'i', arm['red']); setu('taa2NoHist', 'i', arm['nohist'])
    setu('taa2Inputs', 'i', arm['inputs']); setu('taa2Linear', 'i', arm['linear']); setu('taa2Expose', 'f', arm['expose'])
    setu('wwDitherOn', 'i', 1 if arm['seed'] >= 0 else 0); setu('wwDitherSeed', 'i', max(arm['seed'], 0))
    setu('reproj', 'm', REPROJ)
    drawArrays(4, 0, 3)


def read(i, w, h):
    readBuffer(0x8CE0 + i)
    a = np.zeros((h, w, 4), np.uint16)
    readPixels(0, 0, w, h, RGBA, HALF, a.ctypes.data)
    return a


def job(name, src, w, h, texs, arm, time_it):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    outs = [tex(w, h, RGBA16F, RGBA, HALF, None, NEAREST) for _ in range(3)]
    run(w, h, texs, arm, outs); finish()
    if checkFB(FBO) != 0x8CD5 or getError() != 0:
        print('TAA-GL-REF FAIL (gl error)'); sys.exit(2)
    for i in range(3):
        put(os.path.join(d, 'gl_o%d.bin' % i), read(i, w, h))
    with open(os.path.join(d, 'job.txt'), 'w') as f:
        f.write('input %s\n' % ('in_' + src))
        for k in ('tier', 'red', 'nohist', 'inputs', 'linear', 'seed'):
            f.write('%s %d\n' % (k, arm[k]))
        f.write('expose %.9g\njx %.9g\njy %.9g\n' % (arm['expose'], arm['jx'], arm['jy']))
        f.write('reproj %s\n' % ' '.join('%.9g' % v for v in REPROJ.reshape(-1)))
    ms = None
    if time_it:
        q = U(); genQ(1, ctypes.byref(q))
        t = []
        t0 = time.perf_counter()
        while time.perf_counter() - t0 < 0.5:   # warm the clocks (taamatch does the same)
            run(w, h, texs, arm, outs); finish()
        for r in range(34):
            finish(); beginQ(0x88BF, q); run(w, h, texs, arm, outs); endQ(0x88BF)
            v = ctypes.c_uint64(); getQ64(q, 0x8866, ctypes.byref(v))
            if r >= 2:
                t.append(v.value / 1e6)
        ms = float(np.median(t))
        with open(os.path.join(d, 'gl_ms.txt'), 'w') as f:
            f.write('%.5f\n' % ms)
    print('job %s %dx%d tier %d red %d linear %d%s' % (name, w, h, arm['tier'], arm['red'], arm['linear'],
                                                       '' if ms is None else ' GL resolve %.4f ms (median of 32)' % ms))


def main():
    rng = np.random.default_rng(23)
    with open(HDR, 'rb') as f:
        w, h = np.frombuffer(f.read(8), np.int32)
        px = np.frombuffer(f.read(), np.float32).reshape(h, w, 4).copy()
    lum = px[..., 0] * 0.2126 + px[..., 1] * 0.7152 + px[..., 2] * 0.0722
    lum = lum[np.isfinite(lum)]
    expo = 1.0 / max(float(np.percentile(lum, 99)), 1e-6)
    print('real input %s %dx%d, expose %.6g (99th luminance -> 1)' % (HDR, w, h, expo))
    rw, rh, rt = make_inputs('real', px.astype(np.float16).astype(np.float32), expo, rng)
    syn = np.exp(rng.normal(-0.5, 2.0, (360, 640, 4))).astype(np.float32)
    m = rng.random((360, 640))
    syn[m < 0.002] = np.nan
    syn[(m >= 0.002) & (m < 0.004)] = np.inf
    syn[(m >= 0.004) & (m < 0.005)] = -np.inf
    syn[(m >= 0.005) & (m < 0.006)] = 1e6
    sw, sh, st = make_inputs('syn', syn.astype(np.float16).astype(np.float32), 1.0, rng)
    base = dict(tier=4, red=0, nohist=0, inputs=3, linear=1, seed=-1, expose=expo, jx=0.3125, jy=-0.1875)
    arms = dict(A=dict(base), H=dict(base, tier=3), M=dict(base, tier=2, inputs=0, linear=0, seed=7, expose=1.0),
                L=dict(base, tier=1, inputs=0), N=dict(base, tier=3, nohist=1), X=dict(base, red=1 | 4 | 64),
                Y=dict(base, red=2 | 8 | 32 | 128))
    for k, a in arms.items():
        job('real_' + k, 'real', rw, rh, rt, a, k in ('A', 'H'))
    job('syn_A', 'syn', sw, sh, st, dict(arms['A'], expose=1.0), False)
    job('syn_M', 'syn', sw, sh, st, dict(arms['M']), False)
    print('TAA-GL-REF OK')


main()
