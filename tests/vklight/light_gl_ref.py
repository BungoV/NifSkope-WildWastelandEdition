"""lane VKLIGHT1: the GL lighting passes (the tree's res/shaders/features/<module>/*.frag) on a dumped frame, in a hidden GL 4.1
context (tests/spells/shader_link_check.py), no NifSkope. Writes one job folder per chain for tests/vklight/lightmatch.cpp,
which runs the HLSL kernels (res/shaders/hlsl/vklight/*.hlsl) on Vulkan and compares every stage. GL plumbing copied from
tests/vkframe/postfx_gl_ref.py (VKFRAME1 P7), plus rgba32f images and ivec2 / vec3 uniforms.

    python -I tests/vklight/light_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr>] [<pass> ...]

Passes: ao. Job folder: chain.txt, pc_<n>.bin (128-byte push block), gl_<image>.bin (int32 W, H, then the values), gl_ms.txt
(each pass's GPU ms, GL_TIME_ELAPSED, median of 32 after 0.5 s of warm-up chains), red_<kernel>.txt (must FAIL).
"""
import ctypes, os, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = sys.argv[1]
HDR = sys.argv[2] if len(sys.argv) > 2 else 'X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr'
WANT = set(sys.argv[3:]) or {'ao'}

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
uM3 = _fn('glUniformMatrix3fv', None, I, I, ctypes.c_ubyte, VP)
u1i = _fn('glUniform1i', None, I, I); u1f = _fn('glUniform1f', None, I, F); u2f = _fn('glUniform2f', None, I, F, F)
u2i = _fn('glUniform2i', None, I, I, I); u3f = _fn('glUniform3f', None, I, F, F, F)
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
# compute (ww_contact.comp): SSBO, image store, dispatch
u4i = _fn('glUniform4i', None, I, I, I, I, I)
genBuf = _fn('glGenBuffers', None, I, ctypes.POINTER(U)); bindBuf = _fn('glBindBuffer', None, E, U)
bufData = _fn('glBufferData', None, E, ctypes.c_ssize_t, VP, E); bindBufBase = _fn('glBindBufferBase', None, E, U, U)
bindImage = _fn('glBindImageTexture', None, U, U, I, ctypes.c_ubyte, I, E, E)
dispatch = _fn('glDispatchCompute', None, U, U, U); memBarrier = _fn('glMemoryBarrier', None, U)
texSub = _fn('glTexSubImage2D', None, E, I, I, I, I, I, E, E, VP)

T2D, FBO, HALF, FLOAT, RGBA, RG, RED = 0x0DE1, 0x8D40, 0x140B, 0x1406, 0x1908, 0x8227, 0x1903
NEAREST, LINEAR = 0x2600, 0x2601
# name: GL internal format, pixel format, channels, type, numpy dtype as stored
FMT = {'rgba16f': (0x881A, RGBA, 4, HALF, np.uint16), 'rg16f': (0x822F, RG, 2, HALF, np.uint16),
       'r16f': (0x822D, RED, 1, HALF, np.uint16), 'r32f': (0x822E, RED, 1, FLOAT, np.float32),
       'rg32f': (0x8230, RG, 2, FLOAT, np.float32), 'rgba32f': (0x8814, RGBA, 4, FLOAT, np.float32),
       'r8': (0x8229, RED, 1, 0x1401, np.uint8)}
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
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('LIGHT-GL-REF FAIL (compile)'); sys.exit(2)
    return s


_progs = {}


def program(frag):
    """frag: path under res/shaders/features (module/name.frag)"""
    if frag not in _progs:
        p = createProgram()
        attach(p, compile_(0x8B31, VERT)); attach(p, compile_(0x8B30, S.expand(os.path.join(FEAT, frag)))); link(p)
        ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
        if not ok.value:
            print('LIGHT-GL-REF FAIL (link %s)' % frag); sys.exit(2)
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
        if v is not None:
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
        elif kind == '2i':
            u2i(loc, int(v[0]), int(v[1]))
        elif kind == '3f':
            u3f(loc, float(v[0]), float(v[1]), float(v[2]))
        elif kind == 'm3':   # GL columns, row k of v = column k
            m = np.ascontiguousarray(v, np.float32)
            uM3(loc, 1, 0, m.ctypes.data)
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
        print('LIGHT-GL-REF FAIL (gl error in %s)' % name); sys.exit(2)
    for n, rule in cmps:
        bindFB(FBO, fbo); fbTex(FBO, 0x8CE0, T2D, im[n].tex, 0); fbTex(FBO, 0x8CE1, T2D, 0, 0); readBuffer(0x8CE0)
        put(os.path.join(d, 'gl_%s.bin' % n), im[n].read(), im[n].w, im[n].h)

    def chain(path, red):
        with open(path, 'w') as f:
            for x in imgs:
                lv = getattr(x, 'lv', 1)
                f.write('img %s %s %d %d %s%s\n' % (x.name, x.fmt, x.w, x.h, x.src or '-', ' %d' % lv if lv > 1 else ''))
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
    if time_it and not os.environ.get('VKLIGHT_NOTIME'):   # VKLIGHT_NOTIME=1: gate only (the GPU is busy elsewhere)
        qs =(U * len(passes))(); genQ(len(passes), qs)
        t = [[] for _ in passes]
        t0 = time.perf_counter()
        while time.perf_counter() - t0 < 0.5:   # warm the clocks (lightmatch does the same)
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


def load_dump():
    with open(HDR, 'rb') as f:
        w, h = np.frombuffer(f.read(8), np.int32)
        px = np.frombuffer(f.read(), np.float32).reshape(h, w, 4).copy()
    with open(HDR + '.depth', 'rb') as f:
        dw, dh = np.frombuffer(f.read(8), np.int32)
        proj = np.frombuffer(f.read(64), np.float32).reshape(4, 4).copy()   # GL columns, then the depths
        depth = np.frombuffer(f.read(), np.float32).reshape(dh, dw).copy()
    return int(w), int(h), px, proj, depth


# ---------------------------------------------------------------------------------------------------------------- AO
AO = os.environ.get('VKLIGHT_AO_SRC', 'ao/cell_ao.frag')   # a stage probe's copy (absolute path) overrides


def ao_chain(name, srcd, W, H, depth, proj, unit, zlin_route, angles, no_reset, time_it, reds):
    """src/features/ao/ao.cpp: stage 5 (gbuf), 1 x 4 (mips), 2 (raw), 3 (across), 4 (fin)"""
    hw, hh = (W + 1) // 2, (H + 1) // 2
    ms = [(W, H)]
    for m in range(1, 5):
        ms.append((max(ms[m - 1][0] // 2, 1), max(ms[m - 1][1] // 2, 1)))
    M = proj.T.astype(np.float64)                      # math matrix (rows)
    inv = np.linalg.inv(M).astype(np.float32)          # clip -> view
    invc = inv.T.copy()                                # GL columns (row k = column k)
    pa, pb = float(proj[2][2]), float(proj[3][2])
    zn, zf = pb / (pa - 1.0), pb / (pa + 1.0)
    x = 1.0 - depth.astype(np.float64)
    zl = np.where(depth < 1.0, zf * zn / (x * (zf - zn) + zn) * unit, 0.0).astype(np.float32)
    put(os.path.join(srcd, 'zlin.bin'), zl, W, H)
    imgs = [Img('zres', 'r32f', W, H, NEAREST, depth, os.path.basename(srcd) + '/depth.bin'),
            Img('zlin', 'r32f', W, H, NEAREST, zl, os.path.basename(srcd) + '/zlin.bin'),
            Img('gbuf', 'rgba32f', W, H, NEAREST)]
    for m in range(1, 5):
        imgs.append(Img('mip%d' % m, 'r32f', ms[m][0], ms[m][1], NEAREST))
    imgs += [Img('raw', 'rg32f', hw, hh, NEAREST), Img('across', 'rg32f', hw, hh, NEAREST), Img('fin', 'r32f', hw, hh, NEAREST)]
    sizes = [('fullSize', '2i', (W, H)), ('halfSize', '2i', (hw, hh))] + [('mipSize[%d]' % m, '2i', ms[m]) for m in range(5)]
    c4 = (W, H, hw, hh)
    c2 = (ms[1][0], ms[1][1], ms[2][0], ms[2][1])
    c3 = (ms[3][0], ms[3][1], ms[4][0], ms[4][1])
    P00, P11 = float(M[0][0]), float(M[1][1])
    radius, bias, inten = 108.2, 0.6, 7.1
    zl_on = 1 if zlin_route else 0
    ps = [Pass('gbuf', 'ao_gbuf', AO, ['gbuf'], [('zres', 'zres'), ('zlin', 'zlin')], ['zres', 'zlin'],
               [('aoStage', 'i', 5), ('aoInvProj', 'm', invc), ('aoUnit', 'f', unit), ('aoZLin', 'i', zl_on)] + sizes,
               [invc[0], invc[1], invc[2], invc[3], c4, (unit, 0, 0, 0)], (zl_on,))]
    for m in range(1, 5):
        src = 'gbuf' if m == 1 else 'mip%d' % (m - 1)
        units = [('gbuf', 'gbuf')] if m == 1 else [('src', src)]
        ps.append(Pass('mip%d' % m, 'ao_mip', AO, ['mip%d' % m], units, [src],
                       [('aoStage', 'i', 1), ('mipLevel', 'i', m)] + sizes, [None, None, c2, c3, c4], (m,)))
    ps.append(Pass('raw', 'ao_raw', AO, ['raw'], [('gbuf', 'gbuf'), ('zMip1', 'mip1'), ('zMip2', 'mip2'), ('zMip3', 'mip3'), ('zMip4', 'mip4')],
                   ['gbuf', 'mip1', 'mip2', 'mip3', 'mip4'],
                   [('aoStage', 'i', 2), ('proj', '2f', (P00, P11)), ('aoParams', '3f', (radius, bias, inten)), ('aoAngles', 'i', angles),
                    ('aoNoReset', 'i', no_reset), ('aoNoBlur', 'i', 0)] + sizes,
                   [(P00, P11, radius, bias), (inten, 0, 0, 0), c2, c3, c4], (angles, no_reset)))
    for st, s_, d_, k_ in ((3, 'raw', 'across', 'ao_blur'), (4, 'across', 'fin', 'ao_blurf')):
        ps.append(Pass('blur%d' % st, k_, AO, [d_], [('src', s_)], [s_], [('aoStage', 'i', st), ('aoNoBlur', 'i', 0)] + sizes,
                       [None, None, None, None, c4], (1 if st == 4 else 0, 0)))
    # gbuf and mips are bit-exact; the AO sum's float order differs from NVIDIA's GL lowering on ~0.8 % of real values
    # (max 2.4e-6 measured on AO in [0, 1]): bound 1e-5 absolute (lightmatch absX)
    cmps = [('gbuf', 'ulp1'), ('mip1', 'ulp1'), ('mip2', 'ulp1'), ('mip3', 'ulp1'), ('mip4', 'ulp1'), ('raw', 'abs1e-5'),
            ('across', 'abs1e-5'), ('fin', 'abs1e-5')]
    job(name, imgs, ps, cmps, reds, time_it)
    print('  %s: %dx%d half %dx%d near %.6g far %.6g unit %g zlin %d angles %d' % (name, W, H, hw, hh, zn, zf, unit, zl_on, angles))


def ao_main():
    W, H, px, proj, depth = load_dump()
    srcd = os.path.join(OUT, 'in_real_ao')
    os.makedirs(srcd, exist_ok=True)
    put(os.path.join(srcd, 'depth.bin'), depth, W, H)
    print('real input %s %dx%d, sky %.3f' % (HDR, W, H, float((depth >= 1.0).mean())))
    # the app's default (Ultra: 8 angles), the window-depth route; the zlin route at 3 angles; no-reset at 1 angle
    ao_chain('ao_A', srcd, W, H, depth, proj, 1.0, False, 8, 0, True, ['ao_gbuf', 'ao_mip', 'ao_raw', 'ao_blur', 'ao_blurf'])
    ao_chain('ao_L', srcd, W, H, depth, proj, 1.0, True, 3, 0, True, [])
    ao_chain('ao_N', srcd, W, H, depth, proj, 1.0, False, 1, 1, False, [])
    # syn: 640x360, an odd size, a sky band, steps and a ramp (silhouettes, the sky cross, mip clamps)
    sw, sh = 641, 361
    rng = np.random.default_rng(31)
    sy, sx = np.mgrid[0:sh, 0:sw].astype(np.float32)
    sd = (0.9990 + 0.0008 * np.sin(sx / 29.0) * np.cos(sy / 17.0)).astype(np.float32)
    for _ in range(40):
        x0, y0 = rng.integers(0, sw - 40), rng.integers(0, sh - 40)
        sd[y0:y0 + rng.integers(5, 60), x0:x0 + rng.integers(5, 80)] -= np.float32(rng.uniform(0.0002, 0.0008))
    sd[: sh // 7] = 1.0
    sd[:, : 3] = 1.0
    sdir = os.path.join(OUT, 'in_syn_ao')
    os.makedirs(sdir, exist_ok=True)
    put(os.path.join(sdir, 'depth.bin'), sd, sw, sh)
    ao_chain('ao_S', sdir, sw, sh, sd, proj, 1.0, False, 6, 0, False, [])
    ao_chain('ao_SL', sdir, sw, sh, sd, proj, 1.0, True, 8, 0, False, [])


# ------------------------------------------------------------------------------------------------------- contact shadows
CONTACT = os.environ.get('VKLIGHT_CONTACT_SRC') or os.path.join(FEAT, 'shadows', 'ww_contact.comp')   # override: a probe copy
_cprog = []


def contact_program():
    if not _cprog:
        p = createProgram()
        attach(p, compile_(0x91B9, S.expand(CONTACT))); link(p)
        ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
        if not ok.value:
            print('LIGHT-GL-REF FAIL (link ww_contact.comp)'); sys.exit(2)
        _cprog.append(p)
    return _cprog[0]


def gl_gbuf(srcd, W, H, depth, proj):
    """the AO stage 5 g-buffer (view normal, linear depth) the contact march reads, written to <srcd>/gbuf.bin"""
    inv = np.linalg.inv(proj.T.astype(np.float64)).astype(np.float32).T.copy()
    im = {'zres': Img('zres', 'r32f', W, H, NEAREST, depth), 'zlin': Img('zlin', 'r32f', W, H, NEAREST, np.zeros_like(depth)),
          'gbuf': Img('gbuf', 'rgba32f', W, H, NEAREST)}
    draw(Pass('gbuf', 'ao_gbuf', AO, ['gbuf'], [('zres', 'zres'), ('zlin', 'zlin')], [],
              [('aoStage', 'i', 5), ('aoInvProj', 'm', inv), ('aoUnit', 'f', 1.0), ('aoZLin', 'i', 0), ('fullSize', '2i', (W, H))]), im)
    finish()
    bindFB(FBO, fbo); fbTex(FBO, 0x8CE0, T2D, im['gbuf'].tex, 0); fbTex(FBO, 0x8CE1, T2D, 0, 0); readBuffer(0x8CE0)
    g = im['gbuf'].read()
    put(os.path.join(srcd, 'gbuf.bin'), g, W, H)
    return g


def contact_job(name, srcd, gb, W, H, proj, lv, scale, samples, reach_d, time_it, reds):
    """src/features/shadows/shadowset.cpp's line route (contactLineRun): the light lv (view space) projected as the app does"""
    import subprocess
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    mw, mh = (W + scale - 1) // scale, (H + scale - 1) // scale
    pm = proj.astype(np.float64)                     # pm[column][row], as the app's pm
    lv = np.asarray(lv, np.float64); lv /= np.linalg.norm(lv)
    cx, cy, cz, cw = (sum(pm[k][r] * lv[k] for k in range(3)) for r in range(4))
    if abs(cw) < 1e-6:
        cw = -1e-6 if cw < 0 else 1e-6
    lx, ly, lz = (cx / cw * 0.5 + 0.5) * mw, (cy / cw * 0.5 + 0.5) * mh, cz / cw * 0.5 + 0.5
    light = np.array([lx, ly, lz, 1.0 if cw > 0 else -1.0], np.float32)
    out = subprocess.run([os.environ['VKLIGHT_BANDS'], '%.9g' % light[0], '%.9g' % light[1], str(mw), str(mh),
                          os.path.join(d, 'bands.bin')], capture_output=True, text=True).stdout.split()
    nb, waves, first = int(out[1]), int(out[3]), [int(x) for x in out[5:9]]
    bands = np.fromfile(os.path.join(d, 'bands.bin'), np.int32)
    params = np.array([0.02, 0.02, 4.0, samples], np.float32)
    reach = np.array([48.0 if reach_d else 0.0, 0.0, reach_d, 0.0], np.float32)   # kContactReach, cascade preset D
    tan = np.array([1.0 / proj[0][0] if proj[0][0] != 0 else 1.0, 1.0 / proj[1][1] if proj[1][1] != 0 else 1.0], np.float32)
    gx, gy = min(waves, 32768), (waves + 32767) // 32768
    # GL: the app's calls in order (mask cleared to lit, SSBO at 1, image unit 0 R8, gbuf on unit 0)
    gtex = Img('gbuf', 'rgba32f', W, H, NEAREST, gb)
    ones = np.full((mh, mw), 255, np.uint8)
    mask = Img('mask', 'r8', mw, mh, NEAREST, ones)
    ssbo = U(); genBuf(1, ctypes.byref(ssbo)); bindBuf(0x90D2, ssbo)
    bufData(0x90D2, bands.nbytes, bands.ctypes.data, 0x88E0)
    prog = contact_program()

    def run():
        bindBufBase(0x90D2, 1, ssbo)
        useProgram(prog)
        L = lambda n: uloc(prog, n.encode())
        u1i(L('gbuf'), 0); u2i(L('fullSize'), W, H); u2i(L('maskSize'), mw, mh); u1f(L('maskScale'), float(scale))
        u4fv(L('lightCoord'), 1, light.ctypes.data); u4fv(L('sssParams'), 1, params.ctypes.data); u1f(L('sssFloor'), 1.5)
        u4fv(L('sssReach'), 1, reach.ctypes.data); u2f(L('sssTan'), float(tan[0]), float(tan[1]))
        u1i(L('bandCount'), nb); u1i(L('waveCount'), waves); u4i(L('sideFirst'), *first)
        activeTex(0x84C0); bindTex(T2D, gtex.tex)
        bindImage(0, mask.tex, 0, 0, 0, 0x88B9, 0x8229)
        dispatch(gx, gy, 1)
        memBarrier(0xFFFFFFFF)
    run()
    finish()
    if getError() != 0:
        print('LIGHT-GL-REF FAIL (gl error in %s)' % name); sys.exit(2)
    bindFB(FBO, fbo); fbTex(FBO, 0x8CE0, T2D, mask.tex, 0); fbTex(FBO, 0x8CE1, T2D, 0, 0); readBuffer(0x8CE0)
    m = mask.read()
    put(os.path.join(d, 'gl_mask.bin'), m, mw, mh)
    put(os.path.join(srcd, 'ones_%dx%d.bin' % (mw, mh)), ones, mw, mh)
    with open(os.path.join(d, 'pc_0.bin'), 'wb') as f:
        f.write(pc_bytes([light, params, reach, (scale, 1.5, tan[0], tan[1]), (W, H, mw, mh), (nb, waves, 0, 0)], first))
    sd = os.path.basename(srcd)

    def chain(path, red):
        with open(path, 'w') as f:
            f.write('img gbuf rgba32f %d %d %s/gbuf.bin\n' % (W, H, sd))
            f.write('img mask r8 %d %d %s/ones_%dx%d.bin\n' % (mw, mh, sd, mw, mh))
            f.write('buf bands.bin\n')
            f.write('pass contact %s pc_0.bin mask - gbuf - - - - %d %d\n' % ('contact' + ('_red' if red else '') + '.spv', gx, gy))
            f.write('cmp mask gl_mask.bin ulp1\n')
    chain(os.path.join(d, 'chain.txt'), False)
    for r in reds:
        chain(os.path.join(d, 'red_%s.txt' % r), True)
    ms = None
    if time_it and not os.environ.get('VKLIGHT_NOTIME'):
        q = U(); genQ(1, ctypes.byref(q))
        t = []
        t0 = time.perf_counter()
        while time.perf_counter() - t0 < 0.5:
            run(); finish()
        for r in range(34):
            finish()
            beginQ(0x88BF, q); run(); endQ(0x88BF)
            v = ctypes.c_uint64(); getQ64(q, 0x8866, ctypes.byref(v))
            if r >= 2:
                t.append(v.value / 1e6)
        ms = float(np.median(t))
        with open(os.path.join(d, 'gl_ms.txt'), 'w') as f:
            f.write('contact %.5f\n' % ms)
    shade = float((m < 255).mean())
    print('job %s: contact | %s  mask %dx%d light %.1f %.1f %.4f w%+d bands %d waves %d samples %d reach %g shaded %.3f'
          % (name, '' if ms is None else 'GL ms contact %.4f total %.4f' % (ms, ms), mw, mh, light[0], light[1], light[2],
             int(light[3]), nb, waves, samples, reach_d, shade))


def contact_main():
    W, H, px, proj, depth = load_dump()
    srcd = os.path.join(OUT, 'in_real_ct')
    os.makedirs(srcd, exist_ok=True)
    gb = gl_gbuf(srcd, W, H, depth, proj)
    # the default tier (96 samples, full mask) with the cascades' reach, a sun up-right in front of the camera
    contact_job('ct_A', srcd, gb, W, H, proj, (0.35, 0.55, -0.75), 1, 96, 3000.0, True, ['contact'])
    # a light behind the camera (w < 0: the march runs away from the light), half mask, 60 samples, no reach
    contact_job('ct_B', srcd, gb, W, H, proj, (-0.3, 0.4, 0.85), 2, 60, 0.0, True, [])
    # nearly across the view (the light far off screen), half mask, 32 samples, the far preset's reach
    contact_job('ct_C', srcd, gb, W, H, proj, (0.98, 0.15, -0.05), 2, 32, 6000.0, False, [])
    # synthetic: the AO job's odd-size steps and sky, full mask, light on screen
    sw, sh = 641, 361
    rng = np.random.default_rng(31)
    sy, sx = np.mgrid[0:sh, 0:sw].astype(np.float32)
    sdp = (0.9990 + 0.0008 * np.sin(sx / 29.0) * np.cos(sy / 17.0)).astype(np.float32)
    for _ in range(40):
        x0, y0 = rng.integers(0, sw - 40), rng.integers(0, sh - 40)
        sdp[y0:y0 + rng.integers(5, 60), x0:x0 + rng.integers(5, 80)] -= np.float32(rng.uniform(0.0002, 0.0008))
    sdp[: sh // 7] = 1.0
    sdp[:, : 3] = 1.0
    sdir = os.path.join(OUT, 'in_syn_ct')
    os.makedirs(sdir, exist_ok=True)
    sg = gl_gbuf(sdir, sw, sh, sdp, proj)
    contact_job('ct_S', sdir, sg, sw, sh, proj, (-0.2, 0.3, -0.93), 1, 96, 3000.0, False, [])


# -------------------------------------------------------------------------------------------------------------- SSGI
SSGI = os.environ.get('VKLIGHT_SSGI_SRC', 'gi/cell_ssgi.frag')   # a stage probe's copy (absolute path) overrides
genMip = _fn('glGenerateMipmap', None, E)
getTexImage = _fn('glGetTexImage', None, E, I, E, E, VP)
SCENE = {}
NOISE = None


def mip_img(name, srcd, W, H, rgba, levels):
    """an RGBA16F input with GL's own mip chain (glGenerateMipmap, LINEAR_MIPMAP_NEAREST, as cellssgi.cpp makes the scene's);
    every level read back into <srcd>/<name>.bin so Vulkan samples the same texels"""
    x = Img(name, 'rgba16f', W, H, LINEAR, half(rgba), os.path.basename(srcd) + '/%s.bin' % name)
    bindTex(T2D, x.tex)
    texParam(T2D, 0x813D, levels - 1); genMip(T2D); texParam(T2D, 0x2801, 0x2701)
    with open(os.path.join(srcd, name + '.bin'), 'wb') as f:
        for m in range(levels):
            mw, mh = max(W >> m, 1), max(H >> m, 1)
            a = np.zeros((mh, mw, 4), np.uint16)
            getTexImage(T2D, m, RGBA, HALF, a.ctypes.data)
            f.write(np.array([mw, mh], np.int32).tobytes()); f.write(a.tobytes())
    x.lv = levels
    return x


def ssgi_job(name, srcd, W, H, proj, div, slices, steps, blue, frame, radius, time_it, reds):
    """src/features/gi/cellssgi.cpp stage 1 (the cast) at a tier's work size; the interval pyramid off (its default),
    the full-mask early out on, no emissive subtraction"""
    ww = (W + div - 1) // div if div > 1 else W
    wh = (H + div - 1) // div if div > 1 else H
    P00, P11 = float(proj[0][0]), float(proj[1][1])
    gbf = os.path.basename(srcd) + '/gbuf.bin'
    with open(os.path.join(srcd, 'gbuf.bin'), 'rb') as f:
        f.read(8); gb = np.frombuffer(f.read(), np.float32).reshape(H, W, 4)
    imgs = [Img('gbuf', 'rgba32f', W, H, NEAREST, gb, gbf), SCENE[srcd],
            Img('noise', 'rgba16f', 128, 64 * 128, NEAREST, NOISE, 'noise.bin'), Img('cast', 'rgba16f', ww, wh, NEAREST)]
    thick, srcb, sat = 32.0, 1.0, 0.8
    flags = (1 if blue else 0) | 2
    uni = [('ssgiStage', 'i', 1), ('fullSize', '2i', (W, H)), ('workSize', '2i', (ww, wh)), ('proj', '2f', (P00, P11)),
           ('slices', 'i', slices), ('steps', 'i', steps), ('radius', 'f', radius), ('thickness', 'f', thick),
           ('sourceBrightness', 'f', srcb), ('saturation', 'f', sat), ('frameIndex', 'i', frame), ('blueNoise', 'i', int(blue)),
           ('hizLevels', 'i', 0), ('fullBreak', 'i', 1), ('emitSub', 'i', 0), ('oldLaw', 'i', 0)]
    ps = [Pass('cast', 'ssgi', SSGI, ['cast'], [('gbuf', 'gbuf'), ('scene', 'scene'), ('noiseTex', 'noise')],
               ['gbuf', 'scene', 'noise'], uni, [(W, H, ww, wh), (P00, P11, radius, thick), (srcb, sat, 0, 0)],
               (slices, steps, frame, flags))]
    job(name, imgs, ps, [('cast', os.environ.get('VKLIGHT_SSGI_RULE', 'sector'))], reds, time_it)


def ssgi_main():
    global NOISE
    W, H, px, proj, depth = load_dump()
    rng = np.random.default_rng(57)
    nz = np.zeros((64 * 128, 128, 4), np.float32)
    nz[..., :2] = rng.integers(0, 256, (64 * 128, 128, 2)) / 255.0
    NOISE = half(nz)
    os.makedirs(OUT, exist_ok=True)
    put(os.path.join(OUT, 'noise.bin'), NOISE, 128, 64 * 128)
    srcd = os.path.join(OUT, 'in_real_sg')
    os.makedirs(srcd, exist_ok=True)
    gl_gbuf(srcd, W, H, depth, proj)
    SCENE[srcd] = mip_img('scene', srcd, W, H, np.nan_to_num(px, posinf=6.0e4, neginf=0.0).astype(np.float32), 5)
    # quality 1 (the default: quarter work size, 3 x 12), blue noise
    ssgi_job('sg_A', srcd, W, H, proj, 4, 3, 12, True, 5, 256.0, True, ['ssgi'])
    # quality 3 (half work size, 4 x 12), the closed-form noise
    ssgi_job('sg_B', srcd, W, H, proj, 2, 4, 12, False, 41, 256.0, True, [])
    # synthetic: odd sizes, sky rows, a bright scene with steps; full work size, a short radius (the interior's spacing)
    sw, sh = 641, 361
    sy, sx = np.mgrid[0:sh, 0:sw].astype(np.float32)
    sdp = (0.9990 + 0.0008 * np.sin(sx / 29.0) * np.cos(sy / 17.0)).astype(np.float32)
    for _ in range(40):
        x0, y0 = rng.integers(0, sw - 40), rng.integers(0, sh - 40)
        sdp[y0:y0 + rng.integers(5, 60), x0:x0 + rng.integers(5, 80)] -= np.float32(rng.uniform(0.0002, 0.0008))
    sdp[: sh // 7] = 1.0
    sdir = os.path.join(OUT, 'in_syn_sg')
    os.makedirs(sdir, exist_ok=True)
    gl_gbuf(sdir, sw, sh, sdp, proj)
    col = np.zeros((sh, sw, 4), np.float32)
    col[..., 0] = 0.5 + 0.5 * np.sin(sx / 7.0)
    col[..., 1] = (sy / sh) * 4.0
    col[..., 2] = ((sx // 16 + sy // 16) % 2) * 20.0
    col[..., 3] = 1.0
    SCENE[sdir] = mip_img('scene', sdir, sw, sh, col, 5)
    ssgi_job('sg_S', sdir, sw, sh, proj, 1, 2, 12, True, 63, 128.0, False, [])


# --------------------------------------------------------------------------------------------------------------- SSR
SSR = os.environ.get('VKLIGHT_SSR_SRC', 'reflections/cell_ssr.frag')   # a stage probe's copy (absolute path) overrides


def ssr_inputs(srcd, W, H, depth, proj, rot, water):
    """the AO pass's g-buffer and depth mips 1..4 (GL's own, cell_ao.frag stages 5 and 1), the scene (alpha = the reflection
    flag where the world normal points up) and, with water, a water ray band; all written under srcd"""
    g = gl_gbuf(srcd, W, H, depth, proj)
    ms = [(W, H)]
    for m in range(1, 5):
        ms.append((max(ms[m - 1][0] // 2, 1), max(ms[m - 1][1] // 2, 1)))
    im = {'gbuf': Img('gbuf', 'rgba32f', W, H, NEAREST, g)}
    sizes = [('fullSize', '2i', (W, H))] + [('mipSize[%d]' % m, '2i', ms[m]) for m in range(5)]
    with open(os.path.join(srcd, 'zmips.bin'), 'wb') as f:
        for m in range(1, 5):
            im['mip%d' % m] = Img('mip%d' % m, 'r32f', ms[m][0], ms[m][1], NEAREST)
            units = [('gbuf', 'gbuf')] if m == 1 else [('src', 'mip%d' % (m - 1))]
            draw(Pass('mip', 'ao_mip', AO, ['mip%d' % m], units, [], [('aoStage', 'i', 1), ('mipLevel', 'i', m)] + sizes), im)
            finish()
            bindFB(FBO, fbo); fbTex(FBO, 0x8CE0, T2D, im['mip%d' % m].tex, 0); fbTex(FBO, 0x8CE1, T2D, 0, 0); readBuffer(0x8CE0)
            a = im['mip%d' % m].read()
            f.write(np.array(ms[m], np.int32).tobytes()); f.write(a.tobytes())
            put(os.path.join(srcd, 'mip%d.bin' % m), a, ms[m][0], ms[m][1])
    return g, ms


def ssr_job(name, srcd, W, H, g, ms, scene, proj, rot, water, red, reds):
    """src/features/reflections/cellssr.cpp: stages 1 (ray), 2 (march), 3 (across), 4 (down), no previous frame, base program"""
    hw, hh = (W + 1) // 2, (H + 1) // 2
    b = os.path.basename(srcd)
    P00, P11 = float(proj[0][0]), float(proj[1][1])
    pa, pb = float(proj[2][2]), float(proj[3][2])
    far = min(pb / (pa + 1.0), 353840.0)
    near = 15.0
    def mipimg(m):
        with open(os.path.join(srcd, 'mip%d.bin' % m), 'rb') as f:
            f.read(8); a = np.frombuffer(f.read(), np.float32).reshape(ms[m][1], ms[m][0]).copy()
        return Img('mip%d' % m, 'r32f', ms[m][0], ms[m][1], NEAREST, a)
    zm = Img('zmips', 'r32f', ms[1][0], ms[1][1], NEAREST, None, b + '/zmips.bin'); zm.lv = 4
    wr = water if water is not None else np.zeros((H, W, 4), np.float32)
    put(os.path.join(srcd, 'water_%s.bin' % name), wr, W, H)
    put(os.path.join(srcd, 'scene_%s.bin' % name), scene, W, H)
    imgs = [Img('gbuf', 'rgba32f', W, H, NEAREST, g, b + '/gbuf.bin'), zm] + [mipimg(m) for m in range(1, 5)] + [
        Img('scene', 'rgba32f', W, H, LINEAR, scene, b + '/scene_%s.bin' % name),
        Img('water', 'rgba32f', W, H, NEAREST, wr, b + '/water_%s.bin' % name),
        Img('ray', 'rgba32f', hw, hh, NEAREST), Img('raw', 'rgba32f', hw, hh, LINEAR),
        Img('across', 'rgba32f', hw, hh, LINEAR), Img('fin', 'rgba32f', hw, hh, LINEAR)]
    have = 1 if water is not None else 0
    uni = [('fullSize', '2i', (W, H)), ('halfSize', '2i', (hw, hh))] + [('mipSize[%d]' % m, '2i', ms[m]) for m in range(5)] + [
        ('proj', '2f', (P00, P11)), ('viewToWorld', 'm3', rot), ('ssrK', '4fv', (1.0, 0.2, 2.0, 1.0)), ('ssrClip', '2f', (near, far)),
        ('ssrRed', 'i', red), ('haveWaterRay', 'i', have), ('opaqueRays', 'i', 1), ('ssrPrev', 'i', 0)]
    c = [(W, H, hw, hh), (P00, P11, near, far), (1.0, 0.2, 2.0, 1.0), tuple(rot[0]) + (0,), tuple(rot[1]) + (0,), tuple(rot[2]) + (0,)]
    gu = [('gbuf', 'gbuf'), ('zMip1', 'mip1'), ('zMip2', 'mip2'), ('zMip3', 'mip3'), ('zMip4', 'mip4'), ('scene', 'scene')]
    ps = []
    for st, k, o, s in ((1, 'ssr_ray', 'ray', 'fin'), (2, 'ssr_march', 'raw', 'ray'), (3, 'ssr_across', 'across', 'raw'),
                        (4, 'ssr_down', 'fin', 'across')):
        ps.append(Pass('s%d' % st, k, SSR, [o], gu + [('src', s if st > 1 else 'gbuf'), ('waterRay', 'water')],
                       ['gbuf', 'zmips', 'scene', s, 'water'], [('ssrStage', 'i', st)] + uni, c, (red, have, 1)))
    rule = os.environ.get('VKLIGHT_SSR_RULE', 'ulp1')
    job(name, imgs, ps, [('ray', 'ulp1'), ('raw', rule), ('across', rule), ('fin', rule)], reds, True)
    print('  %s: %dx%d half %dx%d near %g far %g water %d red %d' % (name, W, H, hw, hh, near, far, have, red))


def ssr_rot(yaw, pitch):
    """viewToWorld's GL columns (row k = view axis k in the world, z up): a camera yawed and pitched"""
    cy, sy, cp, sp_ = np.cos(yaw), np.sin(yaw), np.cos(pitch), np.sin(pitch)
    fwd = np.array([cy * cp, sy * cp, sp_]); right = np.array([sy, -cy, 0.0]); up = np.cross(right, fwd)
    return np.array([right, up, -fwd], np.float32)   # view x, y, z (the view looks down -z)


def ssr_scene(px, g, rot, rng):
    col = np.nan_to_num(px, posinf=6.0e4, neginf=0.0).astype(np.float32).copy()
    nw = g[..., 0:1] * rot[0] + g[..., 1:2] * rot[1] + g[..., 2:3] * rot[2]
    col[..., 3] = np.where((nw[..., 2] > 0.5) & (g[..., 3] > 0.0), 1.0, 0.0)
    return col


def ssr_main():
    W, H, px, proj, depth = load_dump()
    rng = np.random.default_rng(83)
    rot = ssr_rot(0.6, -0.15)
    srcd = os.path.join(OUT, 'in_real')
    os.makedirs(srcd, exist_ok=True)
    g, ms = ssr_inputs(srcd, W, H, depth, proj, rot, False)
    scene = ssr_scene(px, g, rot, rng)
    print('real input %s %dx%d, flagged %.3f' % (HDR, W, H, float(scene[..., 3].mean())))
    ssr_job('ss_A', srcd, W, H, g, ms, scene, proj, rot, None, 0, ['ssr_ray', 'ssr_march', 'ssr_across', 'ssr_down'])
    # a water band (lower third): its rays from the water pass, some zero (the water's own refusal)
    wr = np.zeros((H, W, 4), np.float32)
    band = slice(0, H // 3)   # bottom row first: the lower third of the picture
    d = rng.normal(0.0, 0.25, (H // 3, W, 3)).astype(np.float32) + np.array([0.0, 0.7, -0.6], np.float32)
    d /= np.linalg.norm(d, axis=-1, keepdims=True)
    d[rng.random((H // 3, W)) < 0.05] = 0.0
    wr[band, :, :3] = d
    wr[band, :, 3] = np.where(g[band, :, 3] > 0.0, g[band, :, 3], 2000.0)
    ssr_job('ss_W', srcd, W, H, g, ms, scene, proj, rot, wr, 0, [])
    # the reds' bits on the green (no gap refusal, no fade): another branch set must match too
    ssr_job('ss_R', srcd, W, H, g, ms, scene, proj, rot, None, 6, [])
    # synthetic: odd size, steps and a ramp floor, a bright checker scene
    sw, sh = 641, 361
    sy, sx = np.mgrid[0:sh, 0:sw].astype(np.float32)
    sdp = (0.9990 + 0.0008 * np.sin(sx / 29.0) * np.cos(sy / 17.0)).astype(np.float32)
    for _ in range(40):
        x0, y0 = rng.integers(0, sw - 40), rng.integers(0, sh - 40)
        sdp[y0:y0 + rng.integers(5, 60), x0:x0 + rng.integers(5, 80)] -= np.float32(rng.uniform(0.0002, 0.0008))
    sdp[sh - sh // 7:] = 1.0
    sdir = os.path.join(OUT, 'in_syn')
    os.makedirs(sdir, exist_ok=True)
    srot = ssr_rot(1.1, -0.35)
    sg, sms = ssr_inputs(sdir, sw, sh, sdp, proj, srot, False)
    col = np.zeros((sh, sw, 4), np.float32)
    col[..., 0] = 0.5 + 0.5 * np.sin(sx / 7.0)
    col[..., 1] = (sy / sh) * 4.0
    col[..., 2] = ((sx // 16 + sy // 16) % 2) * 0.9
    col[..., 3] = ((sx // 40 + sy // 30) % 3 != 0) & (sg[..., 3] > 0.0)
    ssr_job('ss_S', sdir, sw, sh, sg, sms, col, proj, srot, None, 0, [])


def main():
    if 'ssr' in WANT:
        ssr_main()
    if 'ao' in WANT:
        ao_main()
    if 'contact' in WANT:
        contact_main()
    if 'ssgi' in WANT:
        ssgi_main()
    print('LIGHT-GL-REF OK')


main()
