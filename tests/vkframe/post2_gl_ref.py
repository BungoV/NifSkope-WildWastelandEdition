"""lane VKFRAME1 (round 8): the GL reference for the six pieces after the tone map and around the temporal AA: the TAA
copy-back (game_taa_copy), the motion vectors (game_taa_mv), the MS resolve (taa2_msresolve), RCAS (taa2_rcas), the clouds'
front composite (cloud_composite, cvFront 1 / 2) and the lens-flare sprites (cell_lensflare .vert/.frag), drawn from the tree's
own .frag/.vert on a dumped frame in a hidden GL 4.1 context (tests/spells/shader_link_check.py), no NifSkope. Writes one
job folder per check for tests/vkframe/postfxmatch.cpp (the format postfx_gl_ref.py writes; that file runs at import, so its
GL helpers are repeated here).

    python -I tests/vkframe/post2_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr>] [time]

`time` also writes gl_ms.txt (GPU ms a pass, median of 32 after 0.5 s warm) and the tm_* jobs (the MS4 RGBA8 frame targets,
timed only). A multisampled input goes in flat (W x S wide, sample s of pixel x at column x * S + s) and is drawn into its
MS texture per sample on both sides. Jobs marked raster_only (blended draws, the flare's own vertex shader) run only as
fragment passes in postfxmatch.
"""
import ctypes, os, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = sys.argv[1]
HDR = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != 'time' else 'X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr'
TIME = 'time' in sys.argv[2:]

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
bindAttrib = _fn('glBindAttribLocation', None, U, U, ctypes.c_char_p)
uloc = _fn('glGetUniformLocation', I, U, ctypes.c_char_p)
u1i = _fn('glUniform1i', None, I, I); u1f = _fn('glUniform1f', None, I, F); u2f = _fn('glUniform2f', None, I, F, F)
u3f = _fn('glUniform3f', None, I, F, F, F)
u4fv = _fn('glUniform4fv', None, I, I, VP); uM4 = _fn('glUniformMatrix4fv', None, I, I, ctypes.c_ubyte, VP)
uM3 = _fn('glUniformMatrix3fv', None, I, I, ctypes.c_ubyte, VP)
genVA = _fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bindVA = _fn('glBindVertexArray', None, U)
genBuf = _fn('glGenBuffers', None, I, ctypes.POINTER(U)); bindBuf = _fn('glBindBuffer', None, E, U)
bufData = _fn('glBufferData', None, E, ctypes.c_ssize_t, VP, E)
attribPtr = _fn('glVertexAttribPointer', None, U, I, E, ctypes.c_ubyte, I, VP); enableAttrib = _fn('glEnableVertexAttribArray', None, U)
genTex = _fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = _fn('glBindTexture', None, E, U)
texImage = _fn('glTexImage2D', None, E, I, I, I, I, I, E, E, VP)
texImageMS = _fn('glTexImage2DMultisample', None, E, I, E, I, I, ctypes.c_ubyte)
texParam = _fn('glTexParameteri', None, E, E, I); activeTex = _fn('glActiveTexture', None, E)
genFB = _fn('glGenFramebuffers', None, I, ctypes.POINTER(U)); bindFB = _fn('glBindFramebuffer', None, E, U)
fbTex = _fn('glFramebufferTexture2D', None, E, E, E, U, I)
drawBuffers = _fn('glDrawBuffers', None, I, VP); readBuffer = _fn('glReadBuffer', None, E)
checkFB = _fn('glCheckFramebufferStatus', E, E)
viewport = _fn('glViewport', None, I, I, I, I); drawArrays = _fn('glDrawArrays', None, E, I, I)
readPixels = _fn('glReadPixels', None, I, I, I, I, E, E, VP)
pixelStore = _fn('glPixelStorei', None, E, I); finish = _fn('glFinish', None)
enable = _fn('glEnable', None, E); disable = _fn('glDisable', None, E)
blendFunc = _fn('glBlendFunc', None, E, E); blendFuncSep = _fn('glBlendFuncSeparate', None, E, E, E, E)
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
getError = _fn('glGetError', E)

T2D, T2DMS, FBO, HALF, FLOAT, UBYTE, RGBA, RG, RED = 0x0DE1, 0x9100, 0x8D40, 0x140B, 0x1406, 0x1401, 0x1908, 0x8227, 0x1903
NEAREST, LINEAR, BLEND = 0x2600, 0x2601, 0x0BE2
# name: GL internal format, pixel format, channels, type, numpy dtype as stored
FMT = {'rgba16f': (0x881A, RGBA, 4, HALF, np.uint16), 'rg16f': (0x822F, RG, 2, HALF, np.uint16),
       'r16f': (0x822D, RED, 1, HALF, np.uint16), 'r32f': (0x822E, RED, 1, FLOAT, np.float32),
       'rg32f': (0x8230, RG, 2, FLOAT, np.float32), 'rgba8': (0x8058, RGBA, 4, UBYTE, np.uint8),
       'rgba32f': (0x8814, RGBA, 4, FLOAT, np.float32)}
va = U(); genVA(1, ctypes.byref(va)); bindVA(va)
pixelStore(0x0CF5, 1); pixelStore(0x0D05, 1)
VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""
EXPAND = """#version 410 core
uniform sampler2D flatTex;
uniform int msS;
out vec4 o;
void main() { o = texelFetch( flatTex, ivec2( int( gl_FragCoord.x ) * msS + gl_SampleID, int( gl_FragCoord.y ) ), 0 ); }
"""


def fail(why):
    print('POST2-GL-REF FAIL (%s)' % why)
    sys.exit(2)


def compile_(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = I(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); fail('compile')
    return s


_progs = {}


def program(vert, frag):
    """vert / frag: a file under res/shaders/features, or None (vert: the full-screen triangle) / raw source"""
    key = (vert, frag)
    if key not in _progs:
        p = createProgram()
        vs = VERT if vert is None else S.expand(os.path.join(FEAT, vert))
        fs = frag if frag.startswith('#version') else S.expand(os.path.join(FEAT, frag))
        attach(p, compile_(0x8B31, vs)); attach(p, compile_(0x8B30, fs))
        for k, n in enumerate(('vertexPosition', 'vertexColor', 'vertexUV', 'vertexLight')):
            bindAttrib(p, k, n.encode())
        link(p)
        ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
        if not ok.value:
            fail('link %s %s' % (vert, frag[:40]))
        _progs[key] = p
    return _progs[key]


def put(path, a, w, h):
    with open(path, 'wb') as f:
        f.write(np.array([w, h], np.int32).tobytes())
        f.write(np.ascontiguousarray(a).tobytes())


def half(a):
    return a.astype(np.float16).view(np.uint16)


fbo = U(); genFB(1, ctypes.byref(fbo))


def tex2d(fmt, w, h, filt, data):
    internal, pf, ch, typ, dt = FMT[fmt]
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
    d = None if data is None else np.ascontiguousarray(data)
    texImage(T2D, 0, internal, w, h, 0, pf, typ, None if d is None else d.ctypes.data)
    texParam(T2D, 0x2801, filt); texParam(T2D, 0x2800, filt)
    texParam(T2D, 0x2802, 0x812F); texParam(T2D, 0x2803, 0x812F); texParam(T2D, 0x813D, 0)
    return t.value


class Img:
    """samples > 1: a multisampled texture; its data (if any) is the flat W x S upload, drawn in per sample"""
    def __init__(self, name, fmt, w, h, filt, data=None, src=None, samples=1):
        self.name, self.fmt, self.w, self.h, self.filt, self.src, self.samples = name, fmt, w, h, filt, src, samples
        if samples == 1:
            self.target, self.tex = T2D, tex2d(fmt, w, h, filt, data)
            return
        internal = FMT[fmt][0]
        t = U(); genTex(1, ctypes.byref(t)); bindTex(T2DMS, t)
        texImageMS(T2DMS, samples, internal, w, h, 1)
        self.target, self.tex = T2DMS, t.value
        if data is None:
            return
        flat = tex2d(fmt, w * samples, h, NEAREST, data)
        bindFB(FBO, fbo)
        fbTex(FBO, 0x8CE0, T2DMS, self.tex, 0); fbTex(FBO, 0x8CE1, T2D, 0, 0)
        drawBuffers(1, (ctypes.c_uint * 1)(0x8CE0))
        viewport(0, 0, w, h)
        prog = program(None, EXPAND)
        useProgram(prog)
        activeTex(0x84C0); bindTex(T2D, flat)
        u1i(uloc(prog, b'flatTex'), 0); u1i(uloc(prog, b'msS'), samples)
        disable(BLEND)
        drawArrays(4, 0, 3)
        finish()
        if checkFB(FBO) != 0x8CD5 or getError() != 0:
            fail('ms expand %s' % name)

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
    """one GL draw and its Vulkan kernel: frag / vert under res/shaders/features; outs = image names; units = [(sampler
    uniform, image)]; vk = image names for T0..T4; uni = [(name, kind, value)]; c / i = the kernel's push block; vbo = the
    flare's expanded vertices (n x 16 floats, the four vec4 attributes) drawn with vert and the Vulkan vs_spv; blend 0 / 1
    add (ONE, ONE) / 2 alpha (SRC_ALPHA, ONE_MINUS_SRC_ALPHA; alpha ZERO, ONE)"""
    def __init__(self, label, kernel, frag, outs, units, vk, uni, c=(), i=(), vert=None, vbo=None, blend=0, vs_spv=None):
        self.label, self.kernel, self.frag, self.outs, self.units, self.vk, self.uni = label, kernel, frag, outs, units, vk, uni
        self.pc = pc_bytes(c, i)
        self.vert, self.blend, self.vs_spv = vert, blend, vs_spv
        self.draw = 3
        self.vao = None
        if vbo is not None:
            v = np.ascontiguousarray(vbo, np.float32)
            self.draw = v.shape[0]
            a = U(); genVA(1, ctypes.byref(a)); bindVA(a)
            b = U(); genBuf(1, ctypes.byref(b)); bindBuf(0x8892, b)
            bufData(0x8892, v.nbytes, v.ctypes.data, 0x88E4)
            for k in range(4):
                attribPtr(k, 4, FLOAT, 0, 64, VP(16 * k)); enableAttrib(k)
            bindVA(va)
            self.vao, self.vbuf = a, b


def draw(p, imgs):
    bindFB(FBO, fbo)
    for k, n in enumerate(p.outs):
        fbTex(FBO, 0x8CE0 + k, imgs[n].target, imgs[n].tex, 0)
    fbTex(FBO, 0x8CE0 + len(p.outs), T2D, 0, 0)
    drawBuffers(len(p.outs), (ctypes.c_uint * 2)(0x8CE0, 0x8CE1))
    o = imgs[p.outs[0]]
    viewport(0, 0, o.w, o.h)
    prog = program(p.vert, p.frag)
    useProgram(prog)
    for u, (n, im) in enumerate(p.units):
        activeTex(0x84C0 + u); bindTex(imgs[im].target, imgs[im].tex)
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
        elif kind == '3f':
            u3f(loc, float(v[0]), float(v[1]), float(v[2]))
        elif kind == '4fv':
            a = np.ascontiguousarray(np.asarray(v, np.float32).reshape(-1, 4))
            u4fv(loc, a.shape[0], a.ctypes.data)
        elif kind == 'm3':
            m = np.ascontiguousarray(v, np.float32)
            uM3(loc, 1, 0, m.ctypes.data)
        else:
            m = np.ascontiguousarray(v, np.float32)
            uM4(loc, 1, 0, m.ctypes.data)
    if p.blend == 1:
        enable(BLEND); blendFunc(1, 1)
    elif p.blend == 2:
        enable(BLEND); blendFuncSep(0x0302, 0x0303, 0, 1)
    else:
        disable(BLEND)
    bindVA(p.vao if p.vao is not None else va)
    drawArrays(4, 0, p.draw)
    bindVA(va)
    disable(BLEND)


def job(name, imgs, passes, cmps, reds, raster_only=False, timed_only=False):
    """imgs: Img list (inputs and prefilled targets carry src = their file); cmps: [(image, rule)]; reds: kernels whose
    red runs on this job"""
    if timed_only and not TIME:
        return
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    im = {x.name: x for x in imgs}
    for p in passes:
        draw(p, im)
    finish()
    if checkFB(FBO) != 0x8CD5:
        fail('framebuffer in %s' % name)
    e = getError()
    if e != 0:
        fail('gl error 0x%x in %s' % (e, name))
    for n, rule in cmps:
        bindFB(FBO, fbo); fbTex(FBO, 0x8CE0, T2D, im[n].tex, 0); fbTex(FBO, 0x8CE1, T2D, 0, 0); readBuffer(0x8CE0)
        put(os.path.join(d, 'gl_%s.bin' % n), im[n].read(), im[n].w, im[n].h)

    def chain(path, red):
        with open(path, 'w') as f:
            for x in imgs:
                f.write('img %s %s %d %d %s%s\n' % (x.name, x.fmt, x.w, x.h, x.src or '-',
                                                    ' ms%d' % x.samples if x.samples > 1 else ''))
            for k, p in enumerate(passes):
                r = '_red' if p.kernel == red else ''
                outs = list(p.outs) + ['-'] * (2 - len(p.outs))
                vk = list(p.vk) + ['-'] * (5 - len(p.vk))
                opt = ''
                if p.draw != 3:
                    opt += ' draw=%d' % p.draw
                if p.blend:
                    opt += ' blend=' + ('add' if p.blend == 1 else 'alpha')
                if p.vs_spv:
                    opt += ' vs=%s%s_vs.spv' % (p.vs_spv, r)
                f.write('pass %s %s%s.spv pc_%d.bin %s %s%s\n' % (p.label, p.kernel, r, k, ' '.join(outs), ' '.join(vk), opt))
            for n, rule in cmps:
                f.write('cmp %s gl_%s.bin %s\n' % (n, n, rule))
    for k, p in enumerate(passes):
        with open(os.path.join(d, 'pc_%d.bin' % k), 'wb') as f:
            f.write(p.pc)
    chain(os.path.join(d, 'chain.txt'), None)
    for r in reds:
        chain(os.path.join(d, 'red_%s.txt' % r), r)
    if raster_only or timed_only:
        open(os.path.join(d, 'raster_only'), 'w').close()
    ms = None
    if TIME:
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


def main():
    rng = np.random.default_rng(31)
    with open(HDR, 'rb') as f:
        w, h = np.frombuffer(f.read(8), np.int32)
        px = np.frombuffer(f.read(), np.float32).reshape(h, w, 4).copy()
    with open(HDR + '.depth', 'rb') as f:
        dw, dh = np.frombuffer(f.read(8), np.int32)
        proj = np.frombuffer(f.read(64), np.float32).reshape(4, 4).copy()   # GL columns
        depth = np.frombuffer(f.read(), np.float32).reshape(dh, dw).copy()
    W, H = int(w), int(h)
    pa, pb = float(proj[2][2]), float(proj[3][2])
    lum = px[..., 0] * 0.2126 + px[..., 1] * 0.7152 + px[..., 2] * 0.0722
    lum = lum[np.isfinite(lum)]
    expo = float(np.float32(1.0 / max(float(np.percentile(lum, 99)), 1e-6)))
    disp = np.sqrt(karis(px, expo))
    disp4 = np.concatenate([disp, np.ones((H, W, 1), np.float32)], -1)
    disp8 = np.clip(np.round(disp4 * 255.0), 0, 255).astype(np.uint8)
    zn, zf = pb / (pa - 1.0), pb / (pa + 1.0)
    x_ = 1.0 - depth
    zs = (zf * zn / (x_ * (zf - zn) + zn))[x_ > 0.00001]
    dist = float(np.median(zs)) if zs.size else 100.0
    print('real input %s %dx%d, expose %.6g, view distance median %.6g, sky %.1f %%'
          % (HDR, W, H, expo, dist, 100.0 * float((depth >= 1.0).mean())))
    sd = os.path.join(OUT, 'in_real')
    os.makedirs(sd, exist_ok=True)

    def inp(name, fmt, a, filt, samples=1):
        hh, ww = a.shape[0], a.shape[1] // samples
        put(os.path.join(sd, name + '.bin'), a, a.shape[1], hh)
        return lambda nm=None: Img(nm or name, fmt, ww, hh, filt, a, 'in_real/%s.bin' % name, samples)

    cur16 = inp('cur16', 'rgba16f', half(disp4), NEAREST)
    cur8 = inp('cur8', 'rgba8', disp8, NEAREST)
    hdr16 = inp('hdr16', 'rgba16f', half(np.nan_to_num(px, nan=0.0, posinf=6e4, neginf=-6e4)), NEAREST)
    dep = inp('depth', 'r32f', depth, NEAREST)

    # the TAA copy-back: the picture into the frame, RGBA16F (TAA2 linear) and RGBA8 (the LDR frame)
    job('copy_A', [cur16('src16'), cur8('src8'), Img('f16', 'rgba16f', W, H, NEAREST), Img('f8', 'rgba8', W, H, NEAREST)],
        [Pass('copy16', 'taa_copy', 'post/game_taa_copy.frag', ['f16'], [('srcTex', 'src16')], ['src16'], []),
         Pass('copy8', 'taa_copy8', 'post/game_taa_copy.frag', ['f8'], [('srcTex', 'src8')], ['src8'], [])],
        [('f16', 'ulp1'), ('f8', 'ulp1')], ['taa_copy', 'taa_copy8'])

    # the motion vectors: a reprojection close to the camera's step, the jitter as the game's
    R = np.array([[1.01, 0.002, 0, 0], [-0.003, 1.01, 0, 0], [0.01, -0.006, 1.0, 0.004], [0.002, 0.001, 0, 1.0]], np.float32)
    jit = (np.float32(0.6) / np.float32(W), np.float32(-0.4) / np.float32(H))
    inv = (np.float32(1.0) / np.float32(W), np.float32(1.0) / np.float32(H))
    job('mv_A', [dep(), Img('mv', 'rg32f', W, H, NEAREST)],
        [Pass('mv', 'taa_mv', 'post/game_taa_mv.frag', ['mv'], [('depthTex', 'depth')], ['depth'],
              [('reproj', 'm', R), ('jitterNdc', '2f', jit), ('invSize', '2f', inv)],
              [R[0], R[1], R[2], R[3], (jit[0], jit[1], inv[0], inv[1])])],
        [('mv', 'ndc')], ['taa_mv'])

    # the MS resolve: four samples a pixel from the frame and its neighbours (edges), the real HDR, then a synthetic one
    def msflat(a):
        hh, ww = a.shape[:2]
        out = np.zeros((hh, ww * 4, a.shape[2]), a.dtype)
        for s, (ox, oy) in enumerate([(0, 0), (1, 0), (0, 1), (-1, -1)]):
            yy = np.clip(np.arange(hh) + oy, 0, hh - 1)
            xx = np.clip(np.arange(ww) + ox, 0, ww - 1)
            out[:, s::4] = a[yy][:, xx]
        return out
    ms_real = inp('ms_real', 'rgba16f', half(msflat(np.nan_to_num(px, nan=0.0, posinf=6e4, neginf=-6e4))), NEAREST, 4)
    sh2, sw2 = 360, 640
    syn = np.exp(rng.normal(-0.5, 1.5, (sh2, sw2, 4))).astype(np.float32)
    r = rng.random((sh2, sw2))
    syn[r < 0.003] = np.nan
    syn[(r >= 0.003) & (r < 0.006)] = np.inf
    syn[(r >= 0.006) & (r < 0.008)] = -np.inf
    syn[(r >= 0.008) & (r < 0.010)] = 1e6
    ms_syn = inp('ms_syn', 'rgba16f', half(msflat(syn)), NEAREST, 4)
    for nm, src, ww, hh in (('msr_A', ms_real, W, H), ('msr_S', ms_syn, sw2, sh2)):
        job(nm, [src('ms'), Img('res', 'rgba16f', ww, hh, NEAREST)],
            [Pass('msresolve', 'taa_msr', 'post/taa2_msresolve.frag', ['res'], [('msTex', 'ms')], ['ms'],
                  [('msSamples', 'i', 4), ('msExpose', 'f', expo)], [(expo, 0, 0, 0)], (4,))],
            [('res', 'ulp1')], ['taa_msr'] if nm == 'msr_A' else [])

    # RCAS: the LDR frame (RGBA8, the strongest), the linear frame (RGBA16F, one stop down)
    job('rcas_L', [cur8('src8'), Img('o8', 'rgba8', W, H, NEAREST)],
        [Pass('rcas8', 'taa_rcas8', 'post/taa2_rcas.frag', ['o8'], [('srcTex', 'src8')], ['src8'],
              [('rcasSharp', 'f', 1.0), ('rcasLinear', 'i', 0), ('rcasExpose', 'f', expo)], [(1.0, expo, 0, 0)], (0,))],
        [('o8', 'ulp1')], ['taa_rcas8'])
    job('rcas_H', [hdr16('src16'), Img('o16', 'rgba16f', W, H, NEAREST)],
        [Pass('rcas16', 'taa_rcas', 'post/taa2_rcas.frag', ['o16'], [('srcTex', 'src16')], ['src16'],
              [('rcasSharp', 'f', 0.5), ('rcasLinear', 'i', 1), ('rcasExpose', 'f', expo)], [(0.5, expo, 0, 0)], (1,))],
        [('o16', 'ulp1')], ['taa_rcas'])

    # the clouds' front composite: a half-size history over the real depth; mean depths around the scene's (both sides of
    # the test), a cloud-free fifth, a lightning strike near the camera
    hw, hh = W // 2, H // 2
    yy, xx = np.mgrid[0:hh, 0:hw].astype(np.float32)
    T = np.clip(0.5 + 0.6 * np.sin(xx / 23.0) * np.cos(yy / 17.0) + 0.3 * np.sin((xx + yy) / 11.0), 0.0, 1.0)
    T[:, hw * 4 // 5:] = 1.0
    a = 1.0 - T
    lite = (0.6 + 0.4 * np.sin(xx / 7.0 + yy / 13.0))[..., None] * np.array([2.0, 1.9, 1.8], np.float32)
    hist = np.concatenate([a[..., None] * lite, T[..., None]], -1).astype(np.float32)
    zk = 1e-3
    proj4 = (float(proj[0][0]), float(proj[1][1]), float(proj[2][0]), float(proj[2][1]))
    cy = np.clip(((yy + 0.5) * 2).astype(int), 0, H - 1)
    cx = np.clip(((xx + 0.5) * 2).astype(int), 0, W - 1)
    zc = depth[cy, cx]
    ndx = (xx + 0.5) / hw * 2 - 1
    ndy = (yy + 0.5) / hh * 2 - 1
    vlen = np.sqrt(((ndx + proj4[2]) / proj4[0]) ** 2 + ((ndy + proj4[3]) / proj4[1]) ** 2 + 1.0)
    with np.errstate(divide='ignore', invalid='ignore'):
        ts = vlen * pb / (2.0 * zc - 1.0 + pa) * zk
    fac = 0.55 + 0.9 * (0.5 + 0.5 * np.sin(xx / 31.0 + yy / 19.0))
    hdep = np.where(zc >= 1.0, 8.0, ts * fac)
    hdep = np.where(a > 1e-4, hdep, 1e4).astype(np.float32)
    hdep = np.stack([hdep, np.zeros_like(hdep)], -1).astype(np.float32)
    ca, cb_ = np.radians(30.0), np.radians(10.0)
    right = np.array([np.cos(ca), -np.sin(ca), 0.0], np.float32)
    fwd = np.array([np.sin(ca) * np.cos(cb_), np.cos(ca) * np.cos(cb_), np.sin(cb_)], np.float32)
    up = np.cross(right, fwd).astype(np.float32)
    cam = np.stack([right, up, -fwd]).astype(np.float32)        # rows = GL columns
    cam4 = np.concatenate([cam, np.zeros((3, 1), np.float32)], -1)[None]
    ltSrc = (0.5, 1.0, 1.2, 1.0 / 0.4)
    ltPrm = (0.01, 1.8, 5.2, 0.3)
    ltCol = (40.0, 45.0, 60.0)
    camKm = (0.0, 0.0, 0.03)
    rect = (0.0, 0.0, float(inv[0]), float(inv[1]))
    sceneZ = (pa, pb, zk, 1.0)
    hist_i = inp('cv_hist', 'rgba16f', half(hist), LINEAR)
    hdep_i = inp('cv_hdep', 'rg32f', hdep, NEAREST)
    cam_i = inp('cv_cam', 'rgba32f', cam4, NEAREST)
    fr16 = inp('frame16', 'rgba16f', half(disp4), NEAREST)

    def cloud_pass(vt, front, lt, exposure, blend=0):
        uni = [('cvRect', '4fv', rect), ('sceneExposure', 'f', exposure), ('viewTransform', 'i', vt), ('cvFront', 'i', front),
               ('cvSceneZ', '4fv', sceneZ), ('cvProj', '4fv', proj4), ('cvLtOn', 'i', lt), ('cvLtSrc', '4fv', ltSrc),
               ('cvLtPrm', '4fv', ltPrm), ('cvLtCol', '3f', ltCol), ('cvCam', 'm3', cam), ('cvCamKm', '3f', camKm)]
        c = [rect, sceneZ, proj4, ltSrc, ltPrm, ltCol + (exposure,), camKm + (0.0,)]
        return Pass('cloud', 'cloud_front', 'clouds/cloud_composite.frag', ['fr'],
                    [('cvHist', 'hist'), ('cvHistDepth', 'hdep'), ('cvScene', 'depth')], ['hist', 'hdep', 'depth', 'cam'],
                    uni, c, (vt, front, lt), blend=blend)

    def cimgs():
        return [hist_i('hist'), hdep_i('hdep'), dep('depth'), cam_i('cam'), fr16('fr')]
    # cloud_f2 writes the light unmapped (cvFront 2): at the hard strike (r0 0.1 km, 3gD 0.3) GL's own float altitude off
    # the 6360 km radius moves in 0.5 m steps and phi + 3gD grad cancels, so GL, Vulkan and float64 disagree by up to 2x
    # there (VK 0.266, GL 0.370, float64 0.341 at one pixel). v0 / v2 / B keep that strike under the tone map (BOUND);
    # unmapped, the strike's core still differs by up to 15 % (0.05 % of halves > 2 ulp) with 3gD 0 or r0 0.5 km, so f2
    # gates the premultiplied branch with the flash off
    for nm, vt, front, lt, ex, reds in (('cloud_v0', 0, 1, 1, 1.0, ['cloud_front']), ('cloud_v1', 1, 1, 0, 0.8, []),
                                         ('cloud_v2', 2, 1, 1, 1.3, []), ('cloud_f2', 0, 2, 0, 1.0, [])):
        job(nm, cimgs(), [cloud_pass(vt, front, lt, ex)], [('fr', 'step')], reds)
    job('cloud_B', cimgs(), [cloud_pass(0, 1, 1, 1.0, blend=2)], [('fr', 'step')], ['cloud_front'], raster_only=True)

    # the lens-flare sprites over the LDR frame: lights in front (seen), behind the scene (only the sky lets them show),
    # at the scene's distance (part seen); half shrink when occluded; ghosts mirrored through the centre
    spr = np.zeros((64, 64, 4), np.float32)
    gy, gx = np.mgrid[0:64, 0:64].astype(np.float32)
    rr = np.hypot(gx - 31.5, gy - 31.5) / 32.0
    spr[..., 0] = np.clip(1.0 - rr, 0, 1) ** 2
    spr[..., 1] = np.clip(1.0 - rr, 0, 1) ** 1.5 * 0.9
    spr[..., 2] = np.exp(-((rr - 0.6) / 0.08) ** 2)
    spr[..., 3] = 1.0
    spr8 = np.clip(np.round(spr * 255.0), 0, 255).astype(np.uint8)
    nspr = 16
    tab = np.zeros((nspr * 4, 4, 4), np.float32)
    vbo = []
    corners = [(-1, -1, 0, 0), (1, -1, 1, 0), (1, 1, 1, 1), (-1, 1, 0, 1)]
    for s in range(nspr):
        lx, ly = rng.uniform(-0.9, 0.9, 2)
        vz = [5.0, dist, 1e5, dist * 0.5, dist * 2.0][s % 5]
        k = 1.0 if s % 3 else -0.6
        cxs, cys = lx * k, ly * k
        sx, sy = rng.uniform(0.03, 0.2), rng.uniform(0.03, 0.2)
        col = rng.uniform(0.2, 1.0, 3)
        for cn, (ox, oy, u, v) in enumerate(corners):
            tab[s * 4 + cn] = [(cxs, cys, ox * sx, oy * sy), (col[0], col[1], col[2], float(s % 2)), (u, v, 0, 0), (lx, ly, vz, 0)]
        for cn in (0, 1, 2, 0, 2, 3):
            vbo.append(tab[s * 4 + cn].reshape(-1))
    vbo = np.array(vbo, np.float32)
    spr_i = inp('flare_tex', 'rgba8', spr8, LINEAR)
    tab_i = inp('flare_vtx', 'rgba32f', tab, NEAREST)
    fr8 = inp('frame8', 'rgba8', disp8, NEAREST)
    slack = 10.0
    fl_c = [proj[0], proj[1], proj[2], proj[3], (float(W), float(H), slack, 0.0)]

    def flare_pass():
        return Pass('flare', 'lens_flare', 'post/cell_lensflare.frag', ['fr'], [('flareTex', 'spr'), ('flareDepth', 'depth')],
                    ['spr', 'tab', 'depth'], [('flareProj', 'm', proj), ('flareViewport', '2f', (W, H)), ('flareSlack', 'f', slack),
                                              ('flareRed', 'i', 0)], fl_c, (0,), vert='post/cell_lensflare.vert', vbo=vbo,
                    blend=1, vs_spv='lens_flare')
    job('flare_A', [spr_i('spr'), tab_i('tab'), dep('depth'), fr8('fr')], [flare_pass()], [('fr', 'step')], ['lens_flare'],
        raster_only=True)

    # timed only (time): the LDR frame as it is, MS4 RGBA8 (copy-back, RCAS and the flares write into it)
    job('tm_frame', [cur8('src8'), spr_i('spr'), tab_i('tab'), dep('depth'), Img('fr', 'rgba8', W, H, NEAREST, samples=4)],
        [Pass('copy8', 'taa_copy8', 'post/game_taa_copy.frag', ['fr'], [('srcTex', 'src8')], ['src8'], []),
         Pass('rcas8', 'taa_rcas8', 'post/taa2_rcas.frag', ['fr'], [('srcTex', 'src8')], ['src8'],
              [('rcasSharp', 'f', 1.0), ('rcasLinear', 'i', 0), ('rcasExpose', 'f', expo)], [(1.0, expo, 0, 0)], (0,)),
         flare_pass()], [], [], timed_only=True)
    print('POST2-GL-REF OK')


main()
