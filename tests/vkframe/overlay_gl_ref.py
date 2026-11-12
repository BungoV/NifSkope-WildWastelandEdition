"""lane VKFRAME1 (round 9): GL's side of the overlay target vs the present blit at 1920x1045 (GPU ms, median of 32 after
0.5 s warm). present_blit: glBlitFramebuffer of the 4x MS RGBA8 frame (the back buffer's shape) into an RGBA8 ring image
(today's present). ov_clear: glClear of an RGBA8 overlay target (the new GL cost before its fence); ov_clear_ms4: the same
on a 4x MS target. The MS frame is drawn from the real tone-mapped frame (cur8.bin, one value a pixel as the tone map's
interior). The Vulkan side is post2's job ov_frame (tests/vkframe/overlay.sh).
    python -I tests/vkframe/overlay_gl_ref.py <post2 out dir>/jobs
"""
import ctypes, os, sys, time
import numpy as np

TREE = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

S.context()
gl = S.gl


def fn(name, res, *a):
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *a)(addr)
    f = getattr(gl, name)
    f.restype, f.argtypes = res, list(a)
    return f


U, I, E = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
VP = ctypes.c_void_p
genTex = fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = fn('glBindTexture', None, E, U)
texImage = fn('glTexImage2D', None, E, I, I, I, I, I, E, E, VP)
texImageMS = fn('glTexImage2DMultisample', None, E, I, E, I, I, ctypes.c_ubyte)
texParam = fn('glTexParameteri', None, E, E, I)
genFB = fn('glGenFramebuffers', None, I, ctypes.POINTER(U)); bindFB = fn('glBindFramebuffer', None, E, U)
fbTex = fn('glFramebufferTexture2D', None, E, E, E, U, I)
checkFB = fn('glCheckFramebufferStatus', E, E)
blit = fn('glBlitFramebuffer', None, I, I, I, I, I, I, I, I, U, E)
clearColor = fn('glClearColor', None, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
clear = fn('glClear', None, U)
viewport = fn('glViewport', None, I, I, I, I)
finish = fn('glFinish', None)
genQ = fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = fn('glBeginQuery', None, E, U); endQ = fn('glEndQuery', None, E)
getQ64 = fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
pixelStore = fn('glPixelStorei', None, E, I)
getError = fn('glGetError', E)
T2D, T2DMS, FBO, READ, DRAW, COL0 = 0x0DE1, 0x9100, 0x8D40, 0x8CA8, 0x8CA9, 0x8CE0
RGBA8, RGBA, UBYTE, NEAREST, COLOR_BIT = 0x8058, 0x1908, 0x1401, 0x2600, 0x4000
W, H = 1920, 1045
pixelStore(0x0CF5, 1)


def tex(ms):
    t = U(); genTex(1, ctypes.byref(t))
    if ms:
        bindTex(T2DMS, t); texImageMS(T2DMS, 4, RGBA8, W, H, 1)
    else:
        bindTex(T2D, t); texImage(T2D, 0, RGBA8, W, H, 0, RGBA, UBYTE, None)
        texParam(T2D, 0x2801, NEAREST); texParam(T2D, 0x2800, NEAREST)
    f = U(); genFB(1, ctypes.byref(f)); bindFB(FBO, f)
    fbTex(FBO, COL0, T2DMS if ms else T2D, t, 0)
    if checkFB(FBO) != 0x8CD5:
        sys.exit('fbo incomplete')
    return f


def main():
    raw = open(os.path.join(sys.argv[1], 'in_real', 'cur8.bin'), 'rb').read()
    w, h = np.frombuffer(raw[:8], np.int32)
    assert (w, h) == (W, H), (w, h)
    src = U(); genTex(1, ctypes.byref(src)); bindTex(T2D, src)
    pix = np.frombuffer(raw[8:], np.uint8).copy()
    texImage(T2D, 0, RGBA8, W, H, 0, RGBA, UBYTE, pix.ctypes.data)
    sfb = U(); genFB(1, ctypes.byref(sfb)); bindFB(FBO, sfb); fbTex(FBO, COL0, T2D, src, 0)
    frame, ring, ov, ovms = tex(True), tex(False), tex(False), tex(True)
    S_draw_fill(src, frame)
    viewport(0, 0, W, H)

    def present_blit():
        bindFB(READ, frame); bindFB(DRAW, ring)
        blit(0, 0, W, H, 0, 0, W, H, COLOR_BIT, NEAREST)

    def ov_clear():
        bindFB(FBO, ov); clearColor(0, 0, 0, 0); clear(COLOR_BIT)

    def ov_clear_ms4():
        bindFB(FBO, ovms); clearColor(0, 0, 0, 0); clear(COLOR_BIT)

    jobs = [('present_blit', present_blit), ('ov_clear', ov_clear), ('ov_clear_ms4', ov_clear_ms4)]
    qs = (U * len(jobs))(); genQ(len(jobs), qs)
    t = [[] for _ in jobs]
    t0 = time.perf_counter()
    while time.perf_counter() - t0 < 0.5:
        for _, f in jobs:
            f()
        finish()
    for r in range(34):
        finish()
        for k, (_, f) in enumerate(jobs):
            beginQ(0x88BF, qs[k]); f(); endQ(0x88BF)
        for k in range(len(jobs)):
            v = ctypes.c_uint64(); getQ64(qs[k], 0x8866, ctypes.byref(v))
            if r >= 2:
                t[k].append(v.value / 1e6)
    if getError():
        sys.exit('gl error')
    print('GL ' + ' '.join('%s %.4f' % (n, float(np.median(x))) for (n, _), x in zip(jobs, t)))


def S_draw_fill(src, frame):
    """Per-sample draw of the picture into the MS frame (sample shading off: one fragment writes all 4 samples, as the
    tone map's interior pixels do)."""
    vs = '#version 410 core\nvoid main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); ' \
         'gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n'
    fs = '#version 410 core\nuniform sampler2D t;\nout vec4 o;\nvoid main() { o = texelFetch( t, ivec2( gl_FragCoord.xy ), 0 ); }\n'
    cs = fn('glCreateShader', U, E); ss = fn('glShaderSource', None, U, I, ctypes.POINTER(ctypes.c_char_p), VP)
    cc = fn('glCompileShader', None, U); cp = fn('glCreateProgram', U); at = fn('glAttachShader', None, U, U)
    ln = fn('glLinkProgram', None, U); use = fn('glUseProgram', None, U)
    gva = fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bva = fn('glBindVertexArray', None, U)
    draw = fn('glDrawArrays', None, E, I, I)
    p = cp()
    for kind, src_ in ((0x8B31, vs), (0x8B30, fs)):
        s = cs(kind); b = ctypes.c_char_p(src_.encode()); ss(s, 1, ctypes.byref(b), None); cc(s); at(p, s)
    ln(p); use(p)
    va = U(); gva(1, ctypes.byref(va)); bva(va)
    bindTex(T2D, src); bindFB(FBO, frame); viewport(0, 0, W, H); draw(0x0004, 0, 3); finish()
    use(0)


main()
