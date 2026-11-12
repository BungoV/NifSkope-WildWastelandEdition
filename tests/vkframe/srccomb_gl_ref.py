"""lane VKFRAME1 (P2, P3): the GL bloom source (cell_bloom_src.frag over a multisample frame with its stencil, then the
resolve blit, as cellbloom.cpp step 1) and the GL combine (cell_bloom_comb.frag, lod 0, into RGBA32F) on fixed inputs, in
a hidden GL 4.1 context (tests/spells/shader_link_check.py), no NifSkope. tests/vkframe/srccombmatch.cpp runs the HLSL twins
(res/shaders/hlsl/post/bloom_src.hlsl, bloom_comb.hlsl) on Vulkan and compares the bits.

    python -I tests/vkframe/srccomb_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr>]

The multisample frame is drawn from data on both sides the same way: a per-sample pass reads the sample's colour from
data.bin (W*S x H RGBA16F: pixel x, sample s at column x*S+s) and keeps the sample only where sten.bin (uint8, same
layout) holds this draw's stencil value; one draw a value, stencil REPLACE. Values 0 1 2 3 5 7 (lit = & 3 == 1: 1 and 5).
Jobs (one folder each): job.txt, data.bin, sten.bin, top.bin, gl_src.bin (W, H, uint16 RGBA), gl_comb.bin (W, H, float32
RGBA), gl_ms.txt (source pass + resolve, combine: GL_TIME_ELAPSED medians of 32 after 2 warm-ups).
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
SAMPLES = 4
STEN = np.array([0, 1, 2, 3, 5, 7], np.uint8)

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
uloc = _fn('glGetUniformLocation', I, U, ctypes.c_char_p)
u1i = _fn('glUniform1i', None, I, I); u1f = _fn('glUniform1f', None, I, F); u2f = _fn('glUniform2f', None, I, F, F)
u3f = _fn('glUniform3f', None, I, F, F, F); u4f = _fn('glUniform4f', None, I, F, F, F, F)
genVA = _fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bindVA = _fn('glBindVertexArray', None, U)
genTex = _fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = _fn('glBindTexture', None, E, U)
texImage = _fn('glTexImage2D', None, E, I, I, I, I, I, E, E, VP)
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
blit = _fn('glBlitFramebuffer', None, I, I, I, I, I, I, I, I, U, E)
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
getError = _fn('glGetError', E)

T2D, T2DMS, FBO, RFBO, DFBO = 0x0DE1, 0x9100, 0x8D40, 0x8CA8, 0x8CA9
HALF, FLOAT, UBYTE, RGBA, RGBA16F, RGBA32F = 0x140B, 0x1406, 0x1401, 0x1908, 0x881A, 0x8814
R8UI, RED_INT, D24S8 = 0x8232, 0x8D94, 0x88F0
COL0, DSATT, STENCIL_TEST = 0x8CE0, 0x821A, 0x0B90
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


def compile_(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = I(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('SRCCOMB-GL-REF FAIL (compile)'); sys.exit(2)
    return s


def program(frag_src):
    p = createProgram()
    attach(p, compile_(0x8B31, VERT)); attach(p, compile_(0x8B30, frag_src)); link(p)
    ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('SRCCOMB-GL-REF FAIL (link)'); sys.exit(2)
    return p


P_GEN = program(GEN)
P_SRC = program(S.expand(os.path.join(POST, 'cell_bloom_src.frag')))
P_COMB = program(S.expand(os.path.join(POST, 'cell_bloom_comb.frag')))


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


def tex(w, h, ifmt, fmt, typ, data, linear=False):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
    d = None if data is None else np.ascontiguousarray(data)
    texImage(T2D, 0, ifmt, w, h, 0, fmt, typ, None if d is None else d.ctypes.data)
    flt = 0x2601 if linear else 0x2600
    texParam(T2D, 0x2801, flt); texParam(T2D, 0x2800, flt)
    texParam(T2D, 0x2802, 0x812F); texParam(T2D, 0x2803, 0x812F); texParam(T2D, 0x813D, 0)
    return t.value


def ms_tex(w, h, ifmt):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2DMS, t)
    texImageMS(T2DMS, SAMPLES, ifmt, w, h, 1)
    return t.value


def fbo():
    f = U(); genFB(1, ctypes.byref(f)); return f.value


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


def job(name, data, sten, top, arm, time_it):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    H, WS = sten.shape
    W = WS // SAMPLES
    tData = tex(WS, H, RGBA16F, RGBA, HALF, data)
    tSten = tex(WS, H, R8UI, RED_INT, UBYTE, sten)
    frame = ms_tex(W, H, RGBA16F)
    ds = ms_tex(W, H, D24S8)
    msc = ms_tex(W, H, RGBA16F)
    src = tex(W, H, RGBA16F, RGBA, HALF, None, linear=True)
    fFrame, fSrc, fRes = fbo(), fbo(), fbo()
    bindFB(FBO, fFrame); fbTex(FBO, COL0, T2DMS, frame, 0); fbTex(FBO, DSATT, T2DMS, ds, 0)
    if checkFB(FBO) != 0x8CD5:
        print('SRCCOMB-GL-REF FAIL (frame fbo)'); sys.exit(2)
    # the frame: one draw a stencil value (cellhdr.cpp's linear frame: colour + depth / stencil, 4 samples)
    viewport(0, 0, W, H)
    clearColor(0, 0, 0, 0); clearStencil(0); clear(0x4000 | 0x400)
    enable(STENCIL_TEST); stencilMask(0xFF); stencilOp(0x1E00, 0x1E00, 0x1E01)
    useProgram(P_GEN)
    bind(8, T2D, tData); bind(9, T2D, tSten)
    setu(P_GEN, 'genData', 8); setu(P_GEN, 'genSten', 9); setu(P_GEN, 'genS', SAMPLES)
    for v in STEN:
        stencilFunc(0x0207, int(v), 0xFF)
        setu(P_GEN, 'genV', int(v))
        drawArrays(4, 0, 3)
    disable(STENCIL_TEST)
    bindFB(FBO, fSrc); fbTex(FBO, COL0, T2DMS, msc, 0); fbTex(FBO, DSATT, T2DMS, ds, 0)
    bindFB(FBO, fRes); fbTex(FBO, COL0, T2D, src, 0)

    def run_src():
        # cellbloom.cpp step 1: clear, the cell-lit samples (stencil & 3 == 1) exposed, the resolve blit
        bindFB(FBO, fSrc); viewport(0, 0, W, H)
        clearColor(0, 0, 0, 0); clear(0x4000)
        if arm['impulse'] > 0:
            disable(STENCIL_TEST)
        else:
            enable(STENCIL_TEST); stencilMask(0); stencilOp(0x1E00, 0x1E00, 0x1E00); stencilFunc(0x0202, 1, 0x03)
        useProgram(P_SRC)
        bind(8, T2DMS, frame)
        setu(P_SRC, 'hdrSrc', 8)
        setu(P_SRC, 'cellOn', arm['on']); setu(P_SRC, 'cellIsOn', arm['on'])
        setu(P_SRC, 'cellIsExposure', arm['expo'], f=True); setu(P_SRC, 'cellIsRed', 0)
        setu(P_SRC, 'bloomImpulse', float(W // 2), float(H // 2), arm['impulse'])
        drawArrays(4, 0, 3)
        disable(STENCIL_TEST)
        bindFB(RFBO, fSrc); bindFB(DFBO, fRes)
        blit(0, 0, W, H, 0, 0, W, H, 0x4000, 0x2600)

    run_src(); finish()
    if getError() != 0:
        print('SRCCOMB-GL-REF FAIL (gl error, source)'); sys.exit(2)
    bindFB(FBO, fRes)
    out = np.zeros((H, W, 4), np.uint16)
    readPixels(0, 0, W, H, RGBA, HALF, out.ctypes.data)
    # the combine: the tent of the chain's top at every pixel's centre, lod 0, RGBA32F
    th, tw = top.shape[:2]
    tTop = tex(tw, th, RGBA16F, RGBA, HALF, top, linear=True)
    comb = tex(W, H, RGBA32F, RGBA, FLOAT, None)
    fComb = fbo()
    bindFB(FBO, fComb); fbTex(FBO, COL0, T2D, comb, 0)

    def run_comb():
        bindFB(FBO, fComb); viewport(0, 0, W, H)
        useProgram(P_COMB)
        bind(8, T2D, tTop)
        setu(P_COMB, 'cellIsFoBloom', 8)
        setu(P_COMB, 'cellIsFoBloomParams', arm['radius'], 0.8, arm['disp'], 1.0)
        setu(P_COMB, 'cellIsFoBloomFrame', float(W), float(H))
        drawArrays(4, 0, 3)

    run_comb(); finish()
    if getError() != 0:
        print('SRCCOMB-GL-REF FAIL (gl error, combine)'); sys.exit(2)
    cout = np.zeros((H, W, 4), np.float32)
    readPixels(0, 0, W, H, RGBA, FLOAT, cout.ctypes.data)
    put(os.path.join(d, 'data.bin'), data, np.uint16)
    put(os.path.join(d, 'sten.bin'), sten, np.uint8)
    put(os.path.join(d, 'top.bin'), top, np.uint16)
    put(os.path.join(d, 'gl_src.bin'), out, np.uint16)
    put(os.path.join(d, 'gl_comb.bin'), cout, np.float32)
    with open(os.path.join(d, 'job.txt'), 'w') as f:
        f.write('w %d\nh %d\nsamples %d\non %d\nexpo %.9g\nimpulse %.9g\nradius %.9g\ndisp %.9g\n'
                % (W, H, SAMPLES, arm['on'], arm['expo'], arm['impulse'], arm['radius'], arm['disp']))
    msg = ''
    if time_it:
        ms_s, ms_c = timed(run_src), timed(run_comb)
        with open(os.path.join(d, 'gl_ms.txt'), 'w') as f:
            f.write('src %.5f\ncomb %.5f\n' % (ms_s, ms_c))
        msg = ' GL source %.4f ms, combine %.4f ms (medians of 32)' % (ms_s, ms_c)
    print('job %s %dx%d x%d%s' % (name, W, H, SAMPLES, msg))


def edges(rng, a):
    m = rng.random(a.shape[:-1])
    a[m < 0.002] = np.nan
    a[(m >= 0.002) & (m < 0.004)] = np.inf
    a[(m >= 0.004) & (m < 0.005)] = -np.inf
    a[(m >= 0.005) & (m < 0.006)] = 1e6
    a[(m >= 0.006) & (m < 0.02)] *= -1.0
    return a


def main():
    rng = np.random.default_rng(23)
    with open(HDR, 'rb') as f:
        w, h = np.frombuffer(f.read(8), np.int32)
        px = np.frombuffer(f.read(), np.float32).reshape(h, w, 4).copy()
    # real: the dump's pixel at every sample, each sample scaled a little apart (an edge's samples differ)
    k = np.array([1.0, 0.93, 1.07, 0.85], np.float32)[None, None, :, None]
    real = (px[:, :, None, :] * k).reshape(h, w * SAMPLES, 4).astype(np.float16).view(np.uint16)
    p = np.array([0.30, 0.40, 0.08, 0.07, 0.10, 0.05])
    sreal = rng.choice(STEN, size=(h, w * SAMPLES), p=p).astype(np.uint8)
    sh, sw = 360, 640
    syn = edges(rng, np.exp(rng.normal(-0.5, 2.0, (sh, sw * SAMPLES, 4))).astype(np.float32)).astype(np.float16).view(np.uint16)
    ssyn = rng.choice(STEN, size=(sh, sw * SAMPLES), p=p).astype(np.uint8)

    def top_for(W, H):
        tw, th = (W + 1) // 2, (H + 1) // 2
        return edges(rng, np.exp(rng.normal(-1.0, 1.5, (th, tw, 4))).astype(np.float32)).astype(np.float16).view(np.uint16)

    A = dict(on=1, expo=0.37, impulse=0.0, radius=1.0, disp=0.15)
    B = dict(on=1, expo=1.0, impulse=0.0, radius=1.6, disp=0.4)
    C = dict(on=0, expo=0.37, impulse=0.0, radius=1.0, disp=0.15)
    D = dict(on=1, expo=0.37, impulse=4.0, radius=1.0, disp=0.15)
    E = dict(on=1, expo=0.37, impulse=0.0, radius=1.0, disp=0.0)
    job('real_A', real, sreal, top_for(w, h), A, True)
    job('real_B', real, sreal, top_for(w, h), B, False)
    job('syn_A', syn, ssyn, top_for(sw, sh), A, False)
    job('syn_C', syn, ssyn, top_for(sw, sh), C, False)
    job('syn_D', syn, ssyn, top_for(sw, sh), D, False)
    job('syn_E', syn, ssyn, top_for(sw, sh), E, False)
    print('SRCCOMB-GL-REF OK')


main()
