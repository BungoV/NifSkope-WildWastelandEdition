"""lane VKFRAME1 (P4): the GL exposure measure (cell_is_measure_q.frag, the quarter-size draw cellhdr.cpp isMeasureFrame
runs) over a multisample frame with its stencil, in a hidden GL 4.1 context (tests/spells/shader_link_check.py), no
NifSkope. tests/vkframe/measurematch.cpp runs the HLSL twin (res/shaders/hlsl/exposure/measure_q.hlsl) on Vulkan and
compares the bits.

    python -I tests/vkframe/measure_gl_ref.py <out dir> [<WW_CELL_HDR_DUMP .hdr>]

The frame is drawn as srccomb_gl_ref.py draws it (data.bin W*S x H RGBA16F, sten.bin uint8; one draw a stencil value,
REPLACE; values 0 1 2 3 5 7, counted = & 3 == 1). Jobs (one folder each): job.txt, data.bin, sten.bin, gl_q.bin (qw, qh,
float32 RGBA), gl_ms.txt (the quarter draw: GL_TIME_ELAPSED median of 32 after 2 warm-ups).
"""
import ctypes, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
EXPO = os.path.join(TREE, 'res', 'shaders', 'features', 'exposure')
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
u1i = _fn('glUniform1i', None, I, I)
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
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
getError = _fn('glGetError', E)

T2D, T2DMS, FBO = 0x0DE1, 0x9100, 0x8D40
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
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('MEASURE-GL-REF FAIL (compile)'); sys.exit(2)
    return s


def program(frag_src):
    p = createProgram()
    attach(p, compile_(0x8B31, VERT)); attach(p, compile_(0x8B30, frag_src)); link(p)
    ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('MEASURE-GL-REF FAIL (link)'); sys.exit(2)
    return p


P_GEN = program(GEN)
P_Q = program(S.expand(os.path.join(EXPO, 'cell_is_measure_q.frag')))


def setu(p, name, v):
    loc = uloc(p, name.encode())
    if loc >= 0:
        u1i(loc, v)


def tex(w, h, ifmt, fmt, typ, data):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
    d = None if data is None else np.ascontiguousarray(data)
    texImage(T2D, 0, ifmt, w, h, 0, fmt, typ, None if d is None else d.ctypes.data)
    texParam(T2D, 0x2801, 0x2600); texParam(T2D, 0x2800, 0x2600)
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


def job(name, data, sten, time_it):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    H, WS = sten.shape
    W = WS // SAMPLES
    qw, qh = max(1, W >> 2), max(1, H >> 2)
    tData = tex(WS, H, RGBA16F, RGBA, HALF, data)
    tSten = tex(WS, H, R8UI, RED_INT, UBYTE, sten)
    frame = ms_tex(W, H, RGBA16F)
    ds = ms_tex(W, H, D24S8)
    texParam(T2DMS, 0x90EA, 0x1901)	# GL_DEPTH_STENCIL_TEXTURE_MODE = GL_STENCIL_INDEX (cellhdr.cpp's dsTex)
    qt = tex(qw, qh, RGBA32F, RGBA, FLOAT, None)
    fFrame, fQ = fbo(), fbo()
    bindFB(FBO, fFrame); fbTex(FBO, COL0, T2DMS, frame, 0); fbTex(FBO, DSATT, T2DMS, ds, 0)
    if checkFB(FBO) != 0x8CD5:
        print('MEASURE-GL-REF FAIL (frame fbo)'); sys.exit(2)
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
    bindFB(FBO, fQ); fbTex(FBO, COL0, T2D, qt, 0)

    def run_q():
        # cellhdr.cpp isMeasureFrame, the quarter route: no stencil test, the shader reads the stencil
        bindFB(FBO, fQ); viewport(0, 0, qw, qh)
        useProgram(P_Q)
        bind(8, T2DMS, frame); bind(9, T2DMS, ds)
        setu(P_Q, 'hdrSrc', 8); setu(P_Q, 'hdrStencil', 9); setu(P_Q, 'hdrSamples', SAMPLES)
        drawArrays(4, 0, 3)

    run_q(); finish()
    if getError() != 0:
        print('MEASURE-GL-REF FAIL (gl error)'); sys.exit(2)
    out = np.zeros((qh, qw, 4), np.float32)
    readPixels(0, 0, qw, qh, RGBA, FLOAT, out.ctypes.data)
    put(os.path.join(d, 'data.bin'), data, np.uint16)
    put(os.path.join(d, 'sten.bin'), sten, np.uint8)
    put(os.path.join(d, 'gl_q.bin'), out, np.float32)
    with open(os.path.join(d, 'job.txt'), 'w') as f:
        f.write('w %d\nh %d\nsamples %d\n' % (W, H, SAMPLES))
    msg = ''
    if time_it:
        ms = timed(run_q)
        with open(os.path.join(d, 'gl_ms.txt'), 'w') as f:
            f.write('measure %.5f\n' % ms)
        msg = ' GL measure %.4f ms (median of 32)' % ms
    nz = int(np.count_nonzero(out[..., 3]))
    print('job %s %dx%d x%d quarter %dx%d, %d texels with a counted pixel%s' % (name, W, H, SAMPLES, qw, qh, nz, msg))


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
    real = (px[:, :, None, :] * k).reshape(h, w * SAMPLES, 4).astype(np.float16).view(np.uint16)
    p = np.array([0.30, 0.40, 0.08, 0.07, 0.10, 0.05])
    sreal = rng.choice(STEN, size=(h, w * SAMPLES), p=p).astype(np.uint8)
    # the app's stencil is per pixel for most pixels (one draw wrote every sample): a frame with sample 0's value everywhere
    spix = np.repeat(rng.choice(STEN, size=(h, w), p=p).astype(np.uint8), SAMPLES, axis=1)
    sh, sw = 359, 643	# not a multiple of 4: the quarter's edge texels skip their outside pixels
    syn = edges(rng, np.exp(rng.normal(-0.5, 2.0, (sh, sw * SAMPLES, 4))).astype(np.float32)).astype(np.float16).view(np.uint16)
    ssyn = rng.choice(STEN, size=(sh, sw * SAMPLES), p=p).astype(np.uint8)
    job('real_A', real, sreal, True)
    job('real_P', real, spix, False)
    job('syn_A', syn, ssyn, False)
    print('MEASURE-GL-REF OK')


main()
