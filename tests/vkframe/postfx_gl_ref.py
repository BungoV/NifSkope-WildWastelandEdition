"""lane VKFRAME1 (P7): the GL depth of field, motion blur and object blur chains (the tree's res/shaders/features/post/game_dof*,
game_doflens*, game_mblur, game_dofmblur, cell_objblur* .frag) on a dumped frame, in a hidden GL 4.1 context
(tests/spells/shader_link_check.py), no NifSkope. Writes one job folder per chain for tests/vkframe/postfxmatch.cpp, which
runs the HLSL kernels (res/shaders/hlsl/post/dof_*, mblur, doflens_*, objblur*) on Vulkan and compares every stage's halves.

    python -I tests/vkframe/postfx_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr>]

Sources: `real` = the .hdr dump tone mapped into [0, 1) (the post passes run on the tone-mapped frame) with its .depth;
`syn` = 640x360 random HDR with NaN, +-inf and > 65504 texels (the down / prep sanitize). Motion vectors 0 .. 55 px a
frame (still on the left third), object velocity squares for the object blur. Every pass's uniforms as cellpost.cpp sets
them. Job folder: chain.txt (images, passes, compares; postfxmatch.cpp reads it), pc_<n>.bin (the kernel's 128-byte push
block), gl_<image>.bin (int32 W, H, then the halves), gl_ms.txt (each pass's GPU ms, GL_TIME_ELAPSED, median of 32 after
0.5 s of warm-up chains) and red_<kernel>.txt (the chain with that kernel's PFX_RED build, which must FAIL).
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
u4fv = _fn('glUniform4fv', None, I, I, VP); uM4 = _fn('glUniformMatrix4fv', None, I, I, ctypes.c_ubyte, VP)
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
NEAREST, LINEAR = 0x2600, 0x2601
# name: GL internal format, pixel format, channels, type, numpy dtype as stored
FMT = {'rgba16f': (0x881A, RGBA, 4, HALF, np.uint16), 'rg16f': (0x822F, RG, 2, HALF, np.uint16),
       'r16f': (0x822D, RED, 1, HALF, np.uint16), 'r32f': (0x822E, RED, 1, FLOAT, np.float32),
       'rg32f': (0x8230, RG, 2, FLOAT, np.float32)}
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
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('POSTFX-GL-REF FAIL (compile)'); sys.exit(2)
    return s


_progs = {}


def program(frag):
    if frag not in _progs:
        p = createProgram()
        attach(p, compile_(0x8B31, VERT)); attach(p, compile_(0x8B30, S.expand(os.path.join(POST, frag)))); link(p)
        ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
        if not ok.value:
            print('POSTFX-GL-REF FAIL (link %s)' % frag); sys.exit(2)
        _progs[frag] = p
    return _progs[frag]


def put(path, a, w, h):
    with open(path, 'wb') as f:
        f.write(np.array([w, h], np.int32).tobytes())
        f.write(np.ascontiguousarray(a).tobytes())


def half(a):
    return a.astype(np.float16).view(np.uint16)


fbo = U(); genFB(1, ctypes.byref(fbo))


class Img:
    def __init__(self, name, fmt, w, h, filt, data=None, src=None):
        self.name, self.fmt, self.w, self.h, self.filt, self.src = name, fmt, w, h, filt, src
        internal, pf, ch, typ, dt = FMT[fmt]
        t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
        d = None if data is None else np.ascontiguousarray(data)
        texImage(T2D, 0, internal, w, h, 0, pf, typ, None if d is None else d.ctypes.data)
        texParam(T2D, 0x2801, filt); texParam(T2D, 0x2800, filt)
        texParam(T2D, 0x2802, 0x812F); texParam(T2D, 0x2803, 0x812F); texParam(T2D, 0x813D, 0)
        self.tex = t.value

    def read(self):
        internal, pf, ch, typ, dt = FMT[self.fmt]
        a = np.zeros((self.h, self.w, ch), dt)
        readPixels(0, 0, self.w, self.h, pf, typ, a.ctypes.data)
        return a


def pc_bytes(c, i):
    cc = np.zeros((7, 4), np.float32)
    for k, v in enumerate(c):
        cc[k] = np.asarray(v, np.float32)
    return cc.tobytes() + np.asarray(list(i) + [0] * (4 - len(i)), np.int32).tobytes()


class Pass:
    """one GL program draw and its Vulkan kernel: outs = image names; units = [(sampler uniform, image)] for GL;
    vk = image names for T0..T4; uni = [(name, kind, value)]; c / i = the kernel's push block"""
    def __init__(self, label, kernel, frag, outs, units, vk, uni, c=(), i=()):
        self.label, self.kernel, self.frag, self.outs, self.units, self.vk, self.uni = label, kernel, frag, outs, units, vk, uni
        self.pc = pc_bytes(c, i)


def draw(p, imgs):
    bindFB(FBO, fbo)
    for k, n in enumerate(p.outs):
        fbTex(FBO, 0x8CE0 + k, T2D, imgs[n].tex, 0)
    fbTex(FBO, 0x8CE0 + len(p.outs), T2D, 0, 0)
    bufs = (ctypes.c_uint * 2)(0x8CE0, 0x8CE1)
    drawBuffers(len(p.outs), bufs)
    o = imgs[p.outs[0]]
    viewport(0, 0, o.w, o.h)
    prog = program(p.frag)
    useProgram(prog)
    for u, (n, im) in enumerate(p.units):
        activeTex(0x84C0 + u); bindTex(T2D, imgs[im].tex)
        loc = uloc(prog, n.encode())
        if loc >= 0:
            u1i(loc, u)
    for n, kind, v in p.uni:
        loc = uloc(prog, n.encode())
        if loc < 0:
            continue
        if kind == 'i':
            u1i(loc, int(v))
        elif kind == 'f':
            u1f(loc, float(v))
        elif kind == '2f':
            u2f(loc, float(v[0]), float(v[1]))
        elif kind == '4fv':
            a = np.ascontiguousarray(np.asarray(v, np.float32).reshape(-1, 4))
            u4fv(loc, a.shape[0], a.ctypes.data)
        else:
            m = np.ascontiguousarray(v, np.float32)
            uM4(loc, 1, 0, m.ctypes.data)
    drawArrays(4, 0, 3)


def job(name, imgs, passes, cmps, reds, time_it):
    """imgs: Img list (inputs carry src = their file); cmps: [(image, rule)]; reds: kernels whose red runs on this job"""
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    im = {x.name: x for x in imgs}
    for p in passes:
        draw(p, im)
    finish()
    if checkFB(FBO) != 0x8CD5 or getError() != 0:
        print('POSTFX-GL-REF FAIL (gl error in %s)' % name); sys.exit(2)
    for n, rule in cmps:
        bindFB(FBO, fbo); fbTex(FBO, 0x8CE0, T2D, im[n].tex, 0); fbTex(FBO, 0x8CE1, T2D, 0, 0); readBuffer(0x8CE0)
        put(os.path.join(d, 'gl_%s.bin' % n), im[n].read(), im[n].w, im[n].h)

    def chain(path, red):
        with open(path, 'w') as f:
            for x in imgs:
                f.write('img %s %s %d %d %s\n' % (x.name, x.fmt, x.w, x.h, x.src or '-'))
            for k, p in enumerate(passes):
                spv = p.kernel + ('_red' if p.kernel == red else '') + '.spv'
                outs = list(p.outs) + ['-'] * (2 - len(p.outs))
                vk = list(p.vk) + ['-'] * (5 - len(p.vk))
                f.write('pass %s %s pc_%d.bin %s %s\n' % (p.label, spv, k, ' '.join(outs), ' '.join(vk)))
            for n, rule in cmps:
                f.write('cmp %s gl_%s.bin %s\n' % (n, n, rule))
    for k, p in enumerate(passes):
        with open(os.path.join(d, 'pc_%d.bin' % k), 'wb') as f:
            f.write(p.pc)
    chain(os.path.join(d, 'chain.txt'), None)
    for r in reds:
        chain(os.path.join(d, 'red_%s.txt' % r), r)
    ms = None
    if time_it:
        qs = (U * len(passes))(); genQ(len(passes), qs)
        t = [[] for _ in passes]
        t0 = time.perf_counter()
        while time.perf_counter() - t0 < 0.5:   # warm the clocks (postfxmatch does the same)
            for p in passes:
                draw(p, im)
            finish()
        for r in range(34):
            finish()
            for k, p in enumerate(passes):
                beginQ(0x88BF, qs[k]); draw(p, im); endQ(0x88BF)
            for k in range(len(passes)):
                v = ctypes.c_uint64(); getQ64(qs[k], 0x8866, ctypes.byref(v))
                if r >= 2:
                    t[k].append(v.value / 1e6)
        ms = [float(np.median(x)) for x in t]
        with open(os.path.join(d, 'gl_ms.txt'), 'w') as f:
            for p, m in zip(passes, ms):
                f.write('%s %.5f\n' % (p.label, m))
    print('job %s: %s%s' % (name, ' '.join(p.label for p in passes),
                            '' if ms is None else ' | GL ms ' + ' '.join('%s %.4f' % (p.label, m) for p, m in zip(passes, ms))
                            + ' total %.4f' % sum(ms)))


def karis(c, expose):
    c = np.maximum(np.nan_to_num(c[..., :3], nan=0.0, posinf=6e4, neginf=0.0), 0) * expose
    return c / (1.0 + c.max(-1, keepdims=True))


# kBlurWeights (cellpost.cpp), row radius - 1
BW = np.array([
    [0, 0, 0, 0, 0, 0, 0.106506713, 0.786985755, 0.106506713, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0.0544885658, 0.244201481, 0.402619928, 0.244201481, 0.0544885658, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0.0366327688, 0.111280687, 0.216745228, 0.270682156, 0.216745228, 0.111280687, 0.0366327688, 0, 0, 0, 0],
    [0, 0, 0, 0.0276304893, 0.0662821382, 0.1238316, 0.180173829, 0.204163671, 0.180173829, 0.1238316, 0.0662821382,
     0.0276304893, 0, 0, 0],
    [0, 0, 0.0221905038, 0.0455889553, 0.0798113719, 0.119064637, 0.151360765, 0.163967222, 0.151360765, 0.119064637,
     0.0798113719, 0.0455889553, 0.0221905038, 0, 0],
    [0, 0.018543981, 0.0341669098, 0.0563317239, 0.0831085816, 0.109719239, 0.129617959, 0.137022808, 0.129617959,
     0.109719239, 0.0831085816, 0.0563317239, 0.0341669098, 0.018543981, 0],
    [0.0159283616, 0.0270778015, 0.0424232148, 0.0612547807, 0.0815124661, 0.0999668092, 0.112988554, 0.117695801,
     0.112988554, 0.0999668092, 0.0815124661, 0.0612547807, 0.0424232148, 0.0270778015, 0.0159283616]], np.float32)


def dof_passes(W, H, radius, qa, qb, src='cur'):
    """the game's down + two blur passes (cellpost.cpp step 2) into q0d -> q1 -> q0b"""
    taps = [(-1, -1, 0.25, 0), (1, -1, 0.25, 0), (-1, 1, 0.25, 0), (1, 1, 0.25, 0)]
    c4 = (1.0, 1.0, np.float32(2.0) / np.float32(W), np.float32(2.0) / np.float32(H))
    c7 = (np.float32(1.0) / np.float32(W), np.float32(1.0) / np.float32(H), 0.0, 0.0)
    ps = [Pass('down', 'dof_down', 'game_dof_down.frag', ['q0d'], [('t0', src)], [src],
               [('c4', '4fv', c4), ('c7', '4fv', c7), ('taps', '4fv', taps), ('dstSize', '2f', qa)], [c4, c7] + taps)]
    n = 2 * radius + 1
    row = radius - 1
    for pas, (s, dst, sw, sh) in enumerate([('q0d', 'q1', qa[0], qa[1]), ('q1', 'q0b', qb[0], qb[1])]):
        o = np.zeros((15, 4), np.float32)
        offs = np.zeros(16, np.float32)
        for i in range(n):
            k = 7 - radius + i
            off = np.float32(k - 7) / np.float32(sw if pas == 1 else sh)
            o[i] = (off if pas == 1 else 0.0, off if pas == 0 else 0.0, BW[row][k], 0.0)
            offs[i] = off
        dsz = qb if pas == 0 else qa
        ps.append(Pass('blur' + 'yx'[pas], 'dof_blur', 'game_dof_blur.frag', [dst], [('t0', s)], [s],
                       [('o', '4fv', o[:n]), ('taps', 'i', n), ('dstSize', '2f', dsz)], list(offs.reshape(4, 4)),
                       (n, row, 7 - radius, pas)))
    return ps


def main():
    rng = np.random.default_rng(29)
    with open(HDR, 'rb') as f:
        w, h = np.frombuffer(f.read(8), np.int32)
        px = np.frombuffer(f.read(), np.float32).reshape(h, w, 4).copy()
    with open(HDR + '.depth', 'rb') as f:
        dw, dh = np.frombuffer(f.read(8), np.int32)
        proj = np.frombuffer(f.read(64), np.float32).reshape(4, 4)   # the frame's projection (GL columns), then the depths
        depth = np.frombuffer(f.read(), np.float32).reshape(dh, dw).copy()
    W, H = int(w), int(h)
    pa, pb = float(proj[2][2]), float(proj[3][2])
    lum = px[..., 0] * 0.2126 + px[..., 1] * 0.7152 + px[..., 2] * 0.0722
    lum = lum[np.isfinite(lum)]
    expo = 1.0 / max(float(np.percentile(lum, 99)), 1e-6)
    disp = np.sqrt(karis(px, expo))
    cur = np.concatenate([disp, np.ones((H, W, 1), np.float32)], -1)
    srcd = os.path.join(OUT, 'in_real')
    os.makedirs(srcd, exist_ok=True)
    zn, zf = pb / (pa - 1.0), pb / (pa + 1.0)   # the frame's own near / far
    x = 1.0 - depth
    z = zf * zn / (x * (zf - zn) + zn)
    zs = z[x > 0.00001]
    dist = float(np.median(zs)) if zs.size else 100.0
    print('real input %s %dx%d, expose %.6g, view distance median %.6g' % (HDR, W, H, expo, dist))
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # the game's vectors, previous minus current, texture units: 0 .. 55 px a frame, still on the left third
    mpx = np.stack([28.0 + 25.0 * np.sin(yy / 53.0), -8.0 + 20.0 * np.cos(xx / 61.0)], -1)
    mpx[:, : W // 3] *= 0.02
    mv = half(mpx / np.array([W, H], np.float32))
    # the object blur's velocity: the camera's reprojection plus object squares (mask .b)
    R = np.array([[1.01, 0, 0, 0], [0, 1.01, 0, 0], [0, 0, 1.0, 0.004], [0, 0, 0, 1.0]], np.float32)  # rows = GL columns
    jit = (np.float32(0.6) / np.float32(W), np.float32(-0.4) / np.float32(H))
    uv = np.stack([(xx + 0.5) / W, (yy + 0.5) / H], -1)
    ndc = np.concatenate([uv * 2 - 1 - np.array(jit, np.float32), (depth * 2 - 1)[..., None], np.ones((H, W, 1), np.float32)], -1)
    q = ndc @ R
    cam = (q[..., :2] / q[..., 3:] - ndc[..., :2]) * 0.5
    vel = cam.astype(np.float32).copy()
    mask = np.zeros((H, W, 4), np.float32)
    for _ in range(50):
        x0, y0 = rng.integers(0, W - 60), rng.integers(0, H - 60)
        sw_, sh_ = rng.integers(10, 120), rng.integers(10, 90)
        m = rng.uniform(-60, 60, 2).astype(np.float32) / np.array([W, H], np.float32)
        vel[y0:y0 + sh_, x0:x0 + sw_] += m
        mask[y0:y0 + sh_, x0:x0 + sw_, 2] = 1.0
    files = dict(cur=(half(cur), 'rgba16f'), depth=(depth, 'r32f'), mv=(mv, 'rg16f'), vel=(vel, 'rg32f'), mask=(half(mask), 'rgba16f'))
    real = {}
    for k, (a, fmt) in files.items():
        put(os.path.join(srcd, k + '.bin'), a, W, H)
        filt = LINEAR if k in ('cur', 'mv') else NEAREST
        real[k] = (fmt, a, filt, 'in_real/%s.bin' % k)
    # syn: random HDR with NaN / inf / > 65504 texels, a smooth depth with a sky band
    sh2, sw2 = 360, 640
    syn = np.exp(rng.normal(-0.5, 1.5, (sh2, sw2, 4))).astype(np.float32)
    r = rng.random((sh2, sw2))
    syn[r < 0.003] = np.nan
    syn[(r >= 0.003) & (r < 0.006)] = np.inf
    syn[(r >= 0.006) & (r < 0.008)] = -np.inf
    syn[(r >= 0.008) & (r < 0.010)] = 1e6
    sy, sx = np.mgrid[0:sh2, 0:sw2].astype(np.float32)
    sdepth = (0.9990 + 0.0009 * np.sin(sx / 37.0) * np.cos(sy / 23.0)).astype(np.float32)
    sdepth[: sh2 // 6] = 1.0
    sd = os.path.join(OUT, 'in_syn')
    os.makedirs(sd, exist_ok=True)
    put(os.path.join(sd, 'cur.bin'), half(syn), sw2, sh2)
    put(os.path.join(sd, 'depth.bin'), sdepth, sw2, sh2)
    synd = dict(cur=('rgba16f', half(syn), LINEAR, 'in_syn/cur.bin'), depth=('r32f', sdepth, NEAREST, 'in_syn/depth.bin'))

    def inputs(src, names):
        return [Img(n, src[n][0], src[n][1].shape[1], src[n][1].shape[0], src[n][2], src[n][1], src[n][3]) for n in names]

    def quarter(w_, h_):
        return ((w_ + 3) // 4, (h_ + 3) // 4), (max(w_ >> 2, 1), max(h_ >> 2, 1))

    def dof_job(name, src, w_, h_, radius, far_only, sky_sharp, dz, time_it, reds):
        qa, qb = quarter(w_, h_)
        imgs = inputs(src, ['cur', 'depth']) + [Img('q0d', 'rgba16f', qa[0], qa[1], NEAREST), Img('q1', 'rgba16f', qb[0], qb[1], NEAREST),
                                               Img('q0b', 'rgba16f', qa[0], qa[1], LINEAR), Img('out', 'rgba16f', w_, h_, NEAREST)]
        dc0 = (0.6 * dz, 0.6 * dz, dz, 0.0)
        dc1 = (1.0, 1.0, 1.0, 1.0 if sky_sharp else 0.0)
        dc2 = (-1e8, zn, zf - zn, zf * zn)
        dc3 = (0.0, 0.0, 1.0, 1.0)
        ps = dof_passes(w_, h_, radius, qa, qb)
        ps.append(Pass('comp', 'dof_comp', 'game_dof.frag', ['out'], [('t0', 'cur'), ('t1', 'q0b'), ('t2', 'depth')],
                       ['cur', 'q0b', 'depth'], [('c0', '4fv', dc0), ('c1', '4fv', dc1), ('c2', '4fv', dc2), ('c3', '4fv', dc3),
                                                 ('farOnly', 'i', far_only), ('dofRed', 'i', 0)],
                       [dc0, dc1, dc2, dc3], (far_only, 0)))
        job(name, imgs, ps, [('q0d', 'ulp1'), ('q1', 'ulp1'), ('q0b', 'ulp1'), ('out', 'ulp1')], reds, time_it)

    dof_job('dof_A', real, W, H, 7, 0, False, dist, True, ['dof_down', 'dof_blur', 'dof_comp'])
    dof_job('dof_F', real, W, H, 3, 1, True, dist, False, [])
    sz_ = sdepth[sdepth < 1.0]
    sdist = float(np.median(zf * zn / ((1.0 - sz_) * (zf - zn) + zn)))
    dof_job('dof_S', synd, sw2, sh2, 5, 0, False, sdist, False, [])

    mc0 = (np.float32(50.0) * np.float32(0.001) / np.float32(1.0 / 60.0), 0.01, 70.0, 0.0)
    mc1 = (1.0, 1.0, np.float32(W - 1) / np.float32(W), np.float32(H - 1) / np.float32(H))
    imgs = inputs(real, ['cur', 'mv']) + [Img('out', 'rgba16f', W, H, NEAREST)]
    job('mb_A', imgs, [Pass('mblur', 'mblur', 'game_mblur.frag', ['out'], [('t0', 'cur'), ('t1', 'mv')], ['cur', 'mv'],
                            [('c0', '4fv', mc0), ('c1', '4fv', mc1)], [(0, 0, 0, 0)] * 4 + [mc0, mc1])],
        [('out', 'ulp1')], ['mblur'], True)

    # the fused depth of field + motion blur (POSTFX2)
    qa, qb = quarter(W, H)
    dc0 = (0.6 * dist, 0.6 * dist, dist, 0.0)
    dc1 = (1.0, 1.0, 1.0, 0.0)
    dc2 = (-1e8, zn, zf - zn, zf * zn)
    dc3 = (0.0, 0.0, 1.0, 1.0)
    imgs = inputs(real, ['cur', 'depth', 'mv']) + [Img('q0d', 'rgba16f', qa[0], qa[1], NEAREST), Img('q1', 'rgba16f', qb[0], qb[1], NEAREST),
                                                  Img('q0b', 'rgba16f', qa[0], qa[1], LINEAR), Img('out', 'rgba16f', W, H, NEAREST)]
    ps = dof_passes(W, H, 7, qa, qb)
    ps.append(Pass('dofmblur', 'dofmblur', 'game_dofmblur.frag', ['out'], [('t0', 'cur'), ('t1', 'mv'), ('t2', 'q0b'), ('t3', 'depth')],
                   ['cur', 'mv', 'q0b', 'depth'], [('dc0', '4fv', dc0), ('dc1', '4fv', dc1), ('dc2', '4fv', dc2), ('dc3', '4fv', dc3),
                                                   ('mc0', '4fv', mc0), ('mc1', '4fv', mc1), ('farOnly', 'i', 0), ('dofRed', 'i', 0)],
                   [dc0, dc1, dc2, dc3, mc0, mc1], (0, 0)))
    job('dmb_A', imgs, ps, [('q0b', 'ulp1'), ('out', 'ulp1')], ['dofmblur'], True)

    # the thin lens (DOFLENS1): focus at the median distance, f 20, far circle at infinity 32 px
    def lens_job(name, src, w_, h_, s1, near_on, near_tile, fuse, time_it, reds):
        qa, _ = quarter(w_, h_)
        tw, th = (qa[0] + 7) // 8, (qa[1] + 7) // 8
        f = 20.0
        sfoc = s1 if s1 > 0 else dist
        A = 32.0 * (sfoc - f) / f
        lz = (zn, zf - zn, zf * zn, 0.0)
        lensP = (s1, A, f, 0.0)
        lensQ = (64.0, 1.0 if near_on else 0.0, 1.0, 1.0 if near_tile else 0.0)
        lu = [('lz', '4fv', lz), ('lensP', '4fv', lensP), ('lensQ', '4fv', lensQ)]
        names = ['cur', 'depth'] + (['mv'] if fuse else [])
        imgs = inputs(src, names) + [Img('lp', 'rgba16f', qa[0], qa[1], LINEAR), Img('far', 'rgba16f', qa[0], qa[1], LINEAR),
                                     Img('near', 'rgba16f', qa[0], qa[1], LINEAR), Img('out', 'rgba16f', w_, h_, NEAREST)]
        ps = [Pass('prep', 'lens_prep', 'game_doflens_prep.frag', ['lp'], [('t0', 'cur'), ('t1', 'depth')], ['cur', 'depth'], lu,
                   [lz, lensP, lensQ])]
        cmps = [('lp', 'ulp1')]
        if near_tile:
            imgs += [Img('lt0', 'r16f', tw, th, NEAREST), Img('lt1', 'r16f', tw, th, NEAREST)]
            for k, (s, d) in enumerate([('lp', 'lt0'), ('lt0', 'lt1')]):
                ps.append(Pass('tile%d' % k, 'lens_tile', 'game_doflens_tile.frag', [d], [('t0', s)], [s], [('dilate', 'i', k)], [], (k,)))
            cmps += [('lt0', 'ulp1'), ('lt1', 'ulp1')]
        t1 = 'lt1' if near_tile else 'lp'
        ps.append(Pass('gather', 'lens_gather', 'game_doflens_gather.frag', ['far', 'near'], [('t0', 'lp'), ('t1', t1)], ['lp', t1],
                       [('lensQ', '4fv', lensQ)], [lz, lensP, lensQ]))
        cmps += [('far', 'step'), ('near', 'step')]
        if fuse:
            ps.append(Pass('lensmblur', 'lensmblur', 'game_doflensmblur.frag', ['out'],
                           [('t0', 'cur'), ('t1', 'mv'), ('t2', 'far'), ('t3', 'depth'), ('lensNear', 'near')],
                           ['cur', 'mv', 'far', 'depth', 'near'], lu + [('mc0', '4fv', mc0), ('mc1', '4fv', mc1)],
                           [lz, lensP, lensQ, (0, 0, 0, 0), mc0, mc1]))
        else:
            ps.append(Pass('comp', 'lens_comp', 'game_doflens.frag', ['out'],
                           [('t0', 'cur'), ('t1', 'far'), ('t2', 'depth'), ('lensNear', 'near')], ['cur', 'far', 'depth', 'near'], lu,
                           [lz, lensP, lensQ]))
        cmps.append(('out', 'step'))
        job(name, imgs, ps, cmps, reds, time_it)

    lens_job('lens_A', real, W, H, dist, True, True, False, True, ['lens_prep', 'lens_tile', 'lens_gather', 'lens_comp'])
    lens_job('lens_F', real, W, H, 0.0, False, False, False, False, [])
    lens_job('lens_S', synd, sw2, sh2, sdist, True, True, False, False, [])
    lens_job('lmb_A', real, W, H, dist, True, True, True, True, ['lensmblur'])

    # the object blur (POSTFX1)
    tile, samples = 16, 15
    tw, th = (W + tile - 1) // tile, (H + tile - 1) // tile
    inv = (np.float32(1.0) / np.float32(W), np.float32(1.0) / np.float32(H))
    imgs = inputs(real, ['cur', 'depth', 'vel', 'mask']) + [Img('obv', 'rg16f', W, H, NEAREST), Img('ot0', 'rg16f', tw, th, NEAREST),
                                                           Img('ot1', 'rg16f', tw, th, NEAREST), Img('out', 'rgba16f', W, H, NEAREST)]
    ps = [Pass('obvec', 'ob_vec', 'cell_objblur_vec.frag', ['obv'], [('velTex', 'vel'), ('maskTex', 'mask'), ('depthTex', 'depth')],
               ['vel', 'mask', 'depth'], [('reproj', 'm', R), ('jitterNdc', '2f', jit), ('invSize', '2f', inv), ('shutter', 'f', 0.5),
                                          ('maxPx', 'f', 32.0), ('objRed', 'i', 0)],
               [R[0], R[1], R[2], R[3], (jit[0], jit[1], inv[0], inv[1]), (0.5, 32.0, 0.0, 0.0)], (0,))]
    for k, (s, d) in enumerate([('obv', 'ot0'), ('ot0', 'ot1')]):
        ps.append(Pass('obtile%d' % k, 'ob_tile', 'cell_objblur_tile.frag', [d], [('srcTex', s)], [s],
                       [('tile', 'i', tile), ('tileMode', 'i', k)], [], (tile, k)))
    ps.append(Pass('obgather', 'ob', 'cell_objblur.frag', ['out'], [('colTex', 'cur'), ('blurTex', 'obv'), ('neighTex', 'ot1'),
                                                                   ('depthTex', 'depth')], ['cur', 'obv', 'ot1', 'depth'],
                   [('tile', 'i', tile), ('samples', 'i', samples), ('zNear', 'f', zn), ('zFar', 'f', zf), ('noiseSeed', 'f', 0.375)],
                   [(zn, zf, 0.375, 0.0)], (tile, samples)))
    job('ob_A', imgs, ps, [('obv', 'ulp1'), ('ot0', 'ulp1'), ('ot1', 'ulp1'), ('out', 'gather')], ['ob_vec', 'ob_tile', 'ob'], True)
    print('POSTFX-GL-REF OK')


main()
