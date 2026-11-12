"""lane VKFRAME1 (P6): the GL tone map (the tree's res/shaders/features/exposure/cell_hdr.frag, whole, every include) on
fixed inputs, per sample, in a hidden GL 4.1 context (tests/spells/shader_link_check.py), no NifSkope.
tests/vkframe/tonemapmatch.cpp runs the HLSL twin (res/shaders/hlsl/post/tonemap.hlsl) on Vulkan and compares the bits.

    python -I tests/vkframe/tonemap_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr>]

The multisample frame (RGBA16F + D24S8, 4 samples) and the fog's bloom gain (R8, 4 samples) are drawn from data the way
srccomb_gl_ref.py draws them (a per-sample pass, one draw per stencil value, stencil REPLACE). cell_hdr.frag then draws
per sample into a 4x RGBA32F target cleared to -7 (a discarded sample keeps it), unpacked to W*S x H (column x*S+s).
Jobs (one folder each): job.txt (readable), params.txt (tonemapmatch's input), data.bin, sten.bin, fog.bin, comb.bin,
mb.bin, lut.bin (16 x 256 RGBA8: z-slices stacked), backdrop.bin, gl_out.bin (W*S, H, float32 RGBA), gl_ms.txt (timed jobs:
the tone map into RGBA32F, into RGBA8 as the view, and a composite pass that would lay a Vulkan result into the view;
GL_TIME_ELAPSED medians of 32 after 2 warm-ups).
"""
import ctypes, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
HDRFRAG = os.path.join(TREE, 'res', 'shaders', 'features', 'exposure', 'cell_hdr.frag')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = sys.argv[1]
HDR = sys.argv[2] if len(sys.argv) > 2 else 'X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr'
SAMPLES = 4
STEN = np.array([0, 1, 2, 3, 5, 7], np.uint8)
SENTINEL = -7.0

S.context()
gl = S.gl


def _fn(name, res, *a):
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *a)(addr)
    f = getattr(gl, name)
    f.restype, f.argtypes = res, list(a)
    return f


U, I, E, F, B = ctypes.c_uint, ctypes.c_int, ctypes.c_uint, ctypes.c_float, ctypes.c_ubyte
VP = ctypes.c_void_p
createShader = _fn('glCreateShader', U, E); shaderSource = _fn('glShaderSource', None, U, I, ctypes.POINTER(ctypes.c_char_p), VP)
compileShader = _fn('glCompileShader', None, U); getShaderiv = _fn('glGetShaderiv', None, U, E, ctypes.POINTER(I))
getShaderInfoLog = _fn('glGetShaderInfoLog', None, U, I, VP, ctypes.c_char_p)
createProgram = _fn('glCreateProgram', U); attach = _fn('glAttachShader', None, U, U); link = _fn('glLinkProgram', None, U)
getProgramiv = _fn('glGetProgramiv', None, U, E, ctypes.POINTER(I)); useProgram = _fn('glUseProgram', None, U)
getActiveUniform = _fn('glGetActiveUniform', None, U, U, I, VP, ctypes.POINTER(I), ctypes.POINTER(E), ctypes.c_char_p)
uloc = _fn('glGetUniformLocation', I, U, ctypes.c_char_p)
u1i = _fn('glUniform1i', None, I, I); u1f = _fn('glUniform1f', None, I, F); u2f = _fn('glUniform2f', None, I, F, F)
u3f = _fn('glUniform3f', None, I, F, F, F); u4f = _fn('glUniform4f', None, I, F, F, F, F)
genVA = _fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bindVA = _fn('glBindVertexArray', None, U)
genTex = _fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = _fn('glBindTexture', None, E, U)
texImage = _fn('glTexImage2D', None, E, I, I, I, I, I, E, E, VP)
texImage3 = _fn('glTexImage3D', None, E, I, I, I, I, I, I, E, E, VP)
texImageMS = _fn('glTexImage2DMultisample', None, E, I, E, I, I, B)
texParam = _fn('glTexParameteri', None, E, E, I); activeTex = _fn('glActiveTexture', None, E)
genFB = _fn('glGenFramebuffers', None, I, ctypes.POINTER(U)); bindFB = _fn('glBindFramebuffer', None, E, U)
fbTex = _fn('glFramebufferTexture2D', None, E, E, E, U, I); checkFB = _fn('glCheckFramebufferStatus', E, E)
viewport = _fn('glViewport', None, I, I, I, I); drawArrays = _fn('glDrawArrays', None, E, I, I)
readPixels = _fn('glReadPixels', None, I, I, I, I, E, E, VP)
pixelStore = _fn('glPixelStorei', None, E, I); finish = _fn('glFinish', None)
enable = _fn('glEnable', None, E); disable = _fn('glDisable', None, E)
clearColor = _fn('glClearColor', None, F, F, F, F); clearStencil = _fn('glClearStencil', None, I); clear = _fn('glClear', None, U)
stencilFunc = _fn('glStencilFunc', None, E, I, U); stencilOp = _fn('glStencilOp', None, E, E, E); stencilMask = _fn('glStencilMask', None, U)
getMultisample = _fn('glGetMultisamplefv', None, E, U, ctypes.POINTER(F))
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
getError = _fn('glGetError', E)

T2D, T3D, T2DMS, FBO = 0x0DE1, 0x806F, 0x9100, 0x8D40
HALF, FLOAT, UBYTE, RGBA, RGBA16F, RGBA32F, RGBA8 = 0x140B, 0x1406, 0x1401, 0x1908, 0x881A, 0x8814, 0x8058
R8, R8UI, RED_INT, D24S8 = 0x8229, 0x8232, 0x8D94, 0x88F0
COL0, DSATT, STENCIL_TEST = 0x8CE0, 0x821A, 0x0B90
SAMPLER_TYPES = {0x8B5D, 0x8B5E, 0x8B5F, 0x8B60, 0x8B62, 0x8DC1, 0x8DC2, 0x8DC4, 0x8DC5, 0x8DCA, 0x8DD2, 0x8DD8,
                 0x900C, 0x900D, 0x9108, 0x9109, 0x910A}
va = U(); genVA(1, ctypes.byref(va)); bindVA(va)
pixelStore(0x0CF5, 1); pixelStore(0x0D05, 1)
VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""
GEN = """#version 410 core
uniform sampler2D genData;
uniform usampler2D genSten;
uniform int genS;
uniform int genV;
out vec4 fragColor;
void main()
{
	ivec2 q = ivec2( int( gl_FragCoord.x ) * genS + gl_SampleID, int( gl_FragCoord.y ) );
	if ( int( texelFetch( genSten, q, 0 ).r ) != genV )
		discard;
	fragColor = texelFetch( genData, q, 0 );
}
"""
UNPACK = """#version 410 core
uniform sampler2DMS src;
uniform int S;
out vec4 o;
void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ); }
"""
# what a wired Vulkan tone map would cost GL afterwards: each sample's value laid into the view (per sample, no math)
COMPOSITE = """#version 410 core
uniform sampler2D src;
uniform int S;
out vec4 o;
void main() { o = texelFetch( src, ivec2( int( gl_FragCoord.x ) * S + gl_SampleID, int( gl_FragCoord.y ) ), 0 ); }
"""


def compile_(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = I(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('TONEMAP-GL-REF FAIL (compile)'); sys.exit(2)
    return s


def program(frag_src):
    p = createProgram()
    attach(p, compile_(0x8B31, VERT)); attach(p, compile_(0x8B30, frag_src)); link(p)
    ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('TONEMAP-GL-REF FAIL (link)'); sys.exit(2)
    return p


P_GEN, P_UNPACK, P_COMP = program(GEN), program(UNPACK), program(COMPOSITE)
P_HDR = program(S.expand(HDRFRAG))
UNITS = {'hdrSrc': 8, 'hdrStencil': 9, 'cellIsFoBloomComb': 10, 'cellIsBloom': 11, 'cellIsLut': 12, 'cellFogBloom': 13,
         'hdrBackdrop': 14}


def setu(p, name, *v, f=False):
    loc = uloc(p, name.encode())
    if loc < 0:
        return
    if len(v) == 1:
        (u1f if f else u1i)(loc, v[0])
    elif len(v) == 2:
        u2f(loc, *v)
    elif len(v) == 3:
        u3f(loc, *v)
    else:
        u4f(loc, *v)


def sampler_units():
    # every other active sampler on a unit of its own (two sampler types on one unit refuse the draw)
    useProgram(P_HDR)
    n = I(); getProgramiv(P_HDR, 0x8B86, ctypes.byref(n))
    nxt = 20
    for i in range(n.value):
        size, typ, name = I(), E(), ctypes.create_string_buffer(256)
        getActiveUniform(P_HDR, i, 256, None, ctypes.byref(size), ctypes.byref(typ), name)
        nm = name.value.decode().split('[')[0]
        if typ.value in SAMPLER_TYPES:
            setu(P_HDR, nm, UNITS.get(nm, nxt))
            if nm not in UNITS:
                nxt += 1


sampler_units()


def tex(w, h, ifmt, fmt, typ, data, linear=False):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
    d = None if data is None else np.ascontiguousarray(data)
    texImage(T2D, 0, ifmt, w, h, 0, fmt, typ, None if d is None else d.ctypes.data)
    flt = 0x2601 if linear else 0x2600
    texParam(T2D, 0x2801, flt); texParam(T2D, 0x2800, flt)
    texParam(T2D, 0x2802, 0x812F); texParam(T2D, 0x2803, 0x812F); texParam(T2D, 0x813D, 0)
    return t.value


def tex3(n, data):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T3D, t)
    d = np.ascontiguousarray(data)
    texImage3(T3D, 0, RGBA8, n, n, n, 0, RGBA, UBYTE, d.ctypes.data)
    texParam(T3D, 0x2801, 0x2601); texParam(T3D, 0x2800, 0x2601)
    for wrap in (0x2802, 0x2803, 0x8072):
        texParam(T3D, wrap, 0x812F)
    texParam(T3D, 0x813D, 0)
    return t.value


def ms_tex(w, h, ifmt):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2DMS, t)
    texImageMS(T2DMS, SAMPLES, ifmt, w, h, 1)
    return t.value


def fbo(*att):
    f = U(); genFB(1, ctypes.byref(f)); bindFB(FBO, f)
    for a, kind, t in att:
        fbTex(FBO, a, kind, t, 0)
    if checkFB(FBO) != 0x8CD5:
        print('TONEMAP-GL-REF FAIL (fbo)'); sys.exit(2)
    return f.value


def bind(unit, kind, t):
    activeTex(0x84C0 + unit); bindTex(kind, t)


def put(path, a, dtype):
    with open(path, 'wb') as f:
        f.write(np.array(a.shape[:2][::-1], np.int32).tobytes())
        f.write(np.ascontiguousarray(a, dtype).tobytes())


def timed(run):
    q = U(); genQ(1, ctypes.byref(q))
    t = []
    for r in range(34):
        finish(); beginQ(0x88BF, q); run(); endQ(0x88BF)
        v = ctypes.c_uint64(); getQ64(q, 0x8866, ctypes.byref(v))
        if r >= 2:
            t.append(v.value / 1e6)
    return float(np.median(t))


def gen_draw(target_fbo, W, H, tData, tSten, values, stencil):
    bindFB(FBO, target_fbo); viewport(0, 0, W, H)
    clearColor(0, 0, 0, 0); clearStencil(0); clear(0x4000 | (0x400 if stencil else 0))
    if stencil:
        enable(STENCIL_TEST); stencilMask(0xFF); stencilOp(0x1E00, 0x1E00, 0x1E01)
    useProgram(P_GEN)
    bind(8, T2D, tData); bind(9, T2D, tSten)
    setu(P_GEN, 'genData', 8); setu(P_GEN, 'genSten', 9); setu(P_GEN, 'genS', SAMPLES)
    for v in values:
        if stencil:
            stencilFunc(0x0207, int(v), 0xFF)
        setu(P_GEN, 'genV', int(v))
        drawArrays(4, 0, 3)
    disable(STENCIL_TEST)


def job(name, data, sten, fog, comb, mb, lut, backdrop, a, time_it):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    H, WS = sten.shape
    W = WS // SAMPLES
    tData = tex(WS, H, RGBA16F, RGBA, HALF, data)
    tSten = tex(WS, H, R8UI, RED_INT, UBYTE, sten)
    tFog = tex(WS, H, RGBA16F, RGBA, HALF, fog)
    tZero = tex(WS, H, R8UI, RED_INT, UBYTE, np.zeros_like(sten))
    frame, ds, fogms = ms_tex(W, H, RGBA16F), ms_tex(W, H, D24S8), ms_tex(W, H, R8)
    bindTex(T2DMS, ds); texParam(T2DMS, 0x90EA, 0x1901)	# DEPTH_STENCIL_TEXTURE_MODE = STENCIL_INDEX
    fFrame = fbo((COL0, T2DMS, frame), (DSATT, T2DMS, ds))
    gen_draw(fFrame, W, H, tData, tSten, STEN, True)
    fFog = fbo((COL0, T2DMS, fogms))
    gen_draw(fFog, W, H, tFog, tZero, [0], False)
    tComb = tex(W, H, RGBA32F, RGBA, FLOAT, comb)
    tMb = tex(mb.shape[1], mb.shape[0], RGBA16F, RGBA, HALF, mb, linear=True)
    tLut = tex3(16, lut)
    tBack = tex(W, H, RGBA16F, RGBA, HALF, backdrop)
    out32, out8 = ms_tex(W, H, RGBA32F), ms_tex(W, H, RGBA8)
    fOut32, fOut8 = fbo((COL0, T2DMS, out32)), fbo((COL0, T2DMS, out8))
    bindFB(FBO, fOut32)
    pos = []
    for i in range(SAMPLES):
        v = (F * 2)(); getMultisample(0x8E50, i, v)
        pos += [v[0] * 16.0, v[1] * 16.0]
    if any(abs(p - round(p)) > 1e-6 or not 0 <= p < 16 for p in pos):
        print('TONEMAP-GL-REF FAIL (sample positions off the 1/16 grid: %s)' % pos); sys.exit(2)
    pos = [int(round(p)) for p in pos]
    P = P_HDR

    def run_tone(target):
        bindFB(FBO, target); viewport(0, 0, W, H)
        useProgram(P)
        bind(8, T2DMS, frame); bind(9, T2DMS, ds); bind(10, T2D, tComb); bind(11, T2D, tMb); bind(12, T3D, tLut)
        bind(13, T2DMS, fogms); bind(14, T2D, tBack)
        setu(P, 'hdrMode', a['mode']); setu(P, 'hdrStencilTest', a['test']); setu(P, 'hdrStencilRef', a['ref'])
        setu(P, 'hdrBackdropOn', a['backdrop'])
        setu(P, 'cellOn', a['cellon']); setu(P, 'cellIsOn', a['cellon'])
        setu(P, 'cellIsExposure', a['expo'], f=True); setu(P, 'cellIsE', a['E'], f=True); setu(P, 'cellIsAdapted', a['adapted'], f=True)
        setu(P, 'cellIsCine', *a['cine']); setu(P, 'cellIsTint', *a['tint']); setu(P, 'cellIsWbD', *a['wb'])
        setu(P, 'cellIsLutOn', a['lut']); setu(P, 'cellIsRed', a['red']); setu(P, 'cellIsVanCurve', a['van'])
        setu(P, 'cellIsBloomOn', a['measure']); setu(P, 'cellIsBloomRect', 0.0, 0.0, 1.0 / W, 1.0 / H)
        setu(P, 'cellIsFoBloomOn', a['fo']); setu(P, 'cellIsFoBloomCombOn', 1)
        setu(P, 'cellIsFoBloomParams', 1.0, a['foW'], 0.15, 0.0); setu(P, 'cellIsFoBloomFrame', float(W), float(H))
        setu(P, 'cellIsFoBloomEc', a['ec'], f=True); setu(P, 'cellIsFoBloomFogCap', a['cap'], f=True)
        setu(P, 'cellFogBloomOn', a['fog'])
        setu(P, 'wwDitherOn', a['dither']); setu(P, 'wwDitherSeed', a['seed'])
        drawArrays(4, 0, 3)

    bindFB(FBO, fOut32); viewport(0, 0, W, H)
    clearColor(SENTINEL, SENTINEL, SENTINEL, SENTINEL); clear(0x4000); clearColor(0, 0, 0, 0)
    run_tone(fOut32); finish()
    if getError() != 0:
        print('TONEMAP-GL-REF FAIL (gl error, tone map)'); sys.exit(2)
    unp = tex(WS, H, RGBA32F, RGBA, FLOAT, None)
    fUnp = fbo((COL0, T2D, unp))
    viewport(0, 0, WS, H)
    useProgram(P_UNPACK); bind(8, T2DMS, out32); setu(P_UNPACK, 'src', 8); setu(P_UNPACK, 'S', SAMPLES)
    drawArrays(4, 0, 3); finish()
    out = np.zeros((H, WS, 4), np.float32)
    readPixels(0, 0, WS, H, RGBA, FLOAT, out.ctypes.data)
    put(os.path.join(d, 'data.bin'), data, np.uint16)
    put(os.path.join(d, 'sten.bin'), sten, np.uint8)
    put(os.path.join(d, 'fog.bin'), fog, np.uint16)
    put(os.path.join(d, 'comb.bin'), comb, np.float32)
    put(os.path.join(d, 'mb.bin'), mb, np.uint16)
    put(os.path.join(d, 'lut.bin'), lut.reshape(16 * 16, 16, 4), np.uint8)
    put(os.path.join(d, 'backdrop.bin'), backdrop, np.uint16)
    put(os.path.join(d, 'gl_out.bin'), out, np.float32)
    ints = [W, H, SAMPLES, a['mode'], a['test'], a['ref'], a['cellon'], a['van'], a['lut'], a['measure'], a['fo'], a['fog'],
            a['backdrop'], a['dither'], a['seed'], a['red']] + pos
    flts = [a['E'], a['expo'], a['adapted']] + list(a['cine']) + list(a['tint']) + list(a['wb']) + [a['foW'], a['ec'], a['cap']]
    with open(os.path.join(d, 'params.txt'), 'w') as f:
        f.write(' '.join(str(int(v)) for v in ints) + '\n' + ' '.join('%.9g' % v for v in flts) + '\n')
    with open(os.path.join(d, 'job.txt'), 'w') as f:
        for k in sorted(a):
            f.write('%s %s\n' % (k, a[k]))
        f.write('w %d\nh %d\nsamples %d\nsample positions (1/16) %s\n' % (W, H, SAMPLES, pos))
    msg = ''
    if time_it:
        ms32, ms8 = timed(lambda: run_tone(fOut32)), timed(lambda: run_tone(fOut8))

        def run_comp():
            bindFB(FBO, fOut8); viewport(0, 0, W, H)
            useProgram(P_COMP); bind(8, T2D, unp); setu(P_COMP, 'src', 8); setu(P_COMP, 'S', SAMPLES)
            drawArrays(4, 0, 3)
        msc = timed(run_comp)
        with open(os.path.join(d, 'gl_ms.txt'), 'w') as f:
            f.write('tone32 %.5f\ntone8 %.5f\ncomposite %.5f\n' % (ms32, ms8, msc))
        msg = ' GL tone map %.4f ms into RGBA32F, %.4f ms into RGBA8 (the view), composite %.4f ms (medians of 32)' % (ms32, ms8, msc)
    print('job %s %dx%d x%d mode %d%s' % (name, W, H, SAMPLES, a['mode'], msg))


def edges(rng, a):
    m = rng.random(a.shape[:-1])
    a[m < 0.002] = np.nan
    a[(m >= 0.002) & (m < 0.004)] = np.inf
    a[(m >= 0.004) & (m < 0.005)] = -np.inf
    a[(m >= 0.005) & (m < 0.006)] = 1e6
    a[(m >= 0.006) & (m < 0.02)] *= -1.0
    return a


def main():
    rng = np.random.default_rng(29)
    with open(HDR, 'rb') as f:
        w, h = np.frombuffer(f.read(8), np.int32)
        px = np.frombuffer(f.read(), np.float32).reshape(h, w, 4).copy()
    k = np.array([1.0, 0.93, 1.07, 0.85], np.float32)[None, None, :, None]
    realf = (px[:, :, None, :] * k).reshape(h, w * SAMPLES, 4)
    realf[..., 3] = 1.0
    real = realf.astype(np.float16).view(np.uint16)
    reala = realf.copy()
    reala[..., 3] = rng.choice([0.0, 0.0005, 0.25, 0.6, 1.0, 1.3], size=reala.shape[:2])
    reala[..., :3] *= np.clip(reala[..., 3:4], 0.0, 1.0)
    reala = reala.astype(np.float16).view(np.uint16)
    p = np.array([0.30, 0.40, 0.08, 0.07, 0.10, 0.05])
    sreal = rng.choice(STEN, size=(h, w * SAMPLES), p=p).astype(np.uint8)
    sh, sw = 360, 640
    synf = edges(rng, np.exp(rng.normal(-0.5, 2.0, (sh, sw * SAMPLES, 4))).astype(np.float32))
    syn = synf.astype(np.float16).view(np.uint16)
    ssyn = rng.choice(STEN, size=(sh, sw * SAMPLES), p=p).astype(np.uint8)

    def side(W, H):
        fog = np.zeros((H, W * SAMPLES, 4), np.float32)
        fog[..., 0] = rng.integers(0, 256, (H, W * SAMPLES)) / 255.0
        fog[rng.random((H, W * SAMPLES)) < 0.5, 0] = 0.0	# clear air: gain 1 exactly
        comb = np.exp(rng.normal(-2.0, 1.5, (H, W, 4))).astype(np.float32)
        mb = np.exp(rng.normal(-2.5, 1.5, ((H + 3) // 4, (W + 3) // 4, 4))).astype(np.float32).astype(np.float16).view(np.uint16)
        g = np.linspace(0.0, 1.0, 16)
        lut = np.stack(np.meshgrid(g, g, g, indexing='ij')[::-1], -1)	# [b][g][r] = r, g, b
        lut = np.concatenate([lut ** 1.1, np.ones(lut.shape[:3] + (1,))], -1)
        lut = np.clip(lut * 255.0 + rng.normal(0, 6, lut.shape), 0, 255).astype(np.uint8)
        back = rng.uniform(0.0, 1.2, (H, W, 4)).astype(np.float16).view(np.uint16)
        return fog.astype(np.float16).view(np.uint16), comb, mb, lut, back

    base = dict(mode=0, test=1, ref=1, cellon=1, van=0, lut=1, measure=1, fo=1, fog=1, backdrop=0, dither=1, seed=7, red=0,
                E=0.02, expo=0.37, adapted=0.3, cine=(1.1, 1.0, 1.05), tint=(0.1, 1.0, 0.9, 0.8), wb=(0.0, 0.0, 0.0),
                foW=0.8, ec=0.0, cap=0.0)
    R = side(w, h)
    Y = side(sw, sh)
    job('real_A', real, sreal, *R, base, True)
    job('real_B', reala, sreal, *R, dict(base, mode=2), False)
    job('real_C', real, sreal, *R, dict(base, mode=3, test=2, backdrop=1), False)
    job('syn_A', syn, ssyn, *Y, dict(base, test=0, van=1, E=0.4, lut=0, dither=0, ec=0.3, cap=2.0), False)
    job('syn_B', syn, ssyn, *Y, dict(base, mode=3, test=2, van=1, E=0.4, backdrop=1), False)
    job('syn_C', syn, ssyn, *Y, dict(base, cellon=0, test=2), False)
    job('syn_D', syn, ssyn, *Y, dict(base, mode=1, red=7), False)
    job('syn_E', syn, ssyn, *Y, dict(base, mode=2, red=4, wb=(0.1, -0.05, 0.2), measure=0, fog=0, seed=12), False)
    job('syn_F', syn, ssyn, *Y, dict(base, mode=3, test=2, backdrop=1, fo=1, ec=0.25, cap=1.5), False)
    print('TONEMAP-GL-REF OK')


main()
