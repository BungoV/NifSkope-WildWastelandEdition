"""lane PERFFIX1: the vanilla bloom's GPU blur (res/shaders/cell_bloomblur.frag, src/gl/cellhdr.cpp bloomBlur) against a
numpy twin of the CPU blur it replaces (src/gl/celllights.cpp wwCellImageSpaceSetBloom, step 1), on the real driver in
a hidden GL 4.1 context (tests/spells/shader_link_check.py). No NifSkope.

    python -I tests/bloomgpu/bloomgpu_gate.py <shaders dir> [--red after|wrap|noclamp]

Inputs: a synthetic quarter-size measure (480 x 270 RGBA32F, the measure texture's level 2 as cellhdr.cpp reads it) with
HDR values, sub-threshold texels, NaN / +inf / -inf texels and bright spots on the edges, for three (threshold, scale)
pairs. Gate: every texel's |gpu - twin| <= 1e-5 x max(1, twin peak) (float order only; 1/255 of a unit bloom is 3.9e-3).
Reds (a doctored copy of the shader, must FAIL): after = the bright pass on the row pass instead of the column pass,
wrap = no edge clamp (the taps wrap), noclamp = a non-finite texel kept as is. Prints the GPU ms of the two passes."""
import ctypes, importlib.util, os, sys, time
import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('slc', os.path.join(here, '..', 'spells', 'shader_link_check.py'))
slc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(slc)
gl = slc.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint

def fn(name, res, *args):
    """an extension entry point through wglGetProcAddress, a GL 1.1 one from opengl32.dll"""
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *args)(addr)
    f = getattr(gl, name)
    f.restype = res
    f.argtypes = list(args)
    return f

QW, QH = 480, 270
R = 7

def twin(px, thresh, scale):
    """celllights.cpp wwCellImageSpaceSetBloom( step 1 ), float32 in the CPU's order"""
    src = np.where(np.isfinite(px[:, :, :3]), px[:, :, :3], 0.0).astype(np.float32)
    x = np.arange(-R, R + 1, dtype=np.float32)
    wt = np.exp(np.float32(-2.0) * (x * x) / np.float32(R * R)).astype(np.float32)
    wsum = np.float32(0.0)
    for v in wt:
        wsum = np.float32(wsum + v)
    wt = (wt / wsum).astype(np.float32)
    src = (np.float32(scale) * np.maximum(np.float32(0.0), src - np.float32(thresh))).astype(np.float32)
    h, w = src.shape[:2]
    mid = np.zeros_like(src)
    for k in range(-R, R + 1):
        yy = np.clip(np.arange(h) + k, 0, h - 1)
        mid = (mid + wt[k + R] * src[yy, :, :]).astype(np.float32)
    out = np.zeros_like(src)
    for k in range(-R, R + 1):
        xx = np.clip(np.arange(w) + k, 0, w - 1)
        out = (out + wt[k + R] * mid[:, xx, :]).astype(np.float32)
    return out, wt

def make_input(seed):
    rng = np.random.default_rng(seed)
    px = rng.exponential(0.35, size=(QH, QW, 4)).astype(np.float32)
    px[:, :, 3] = 1.0
    px[rng.random((QH, QW)) < 0.02, :3] *= 40.0          # bright texels
    px[0:3, 0:3, :3] = 25.0                               # corner spot (the clamp)
    px[QH - 2:QH, QW // 2, :3] = 18.0                     # top edge spot
    px[QH // 2, QW - 1, :3] = 30.0                        # right edge spot
    px[10, 10, 0] = np.nan; px[20, 30, 1] = np.inf; px[40, 50, 2] = -np.inf; px[60, 70, :3] = np.nan
    return px

def doctor(src, red):
    if red == 'after':      # the bright pass moved to the row pass
        a = "if ( bloomVertical == 1 )\n\t\t\tv = bloomPass.y"
        assert src.count(a) == 1
        return src.replace(a, "if ( bloomVertical == 0 )\n\t\t\tv = bloomPass.y")
    if red == 'wrap':       # the taps wrap at the edges
        a = "ivec2( p.x, clamp( p.y + k, 0, sz.y - 1 ) ) : ivec2( clamp( p.x + k, 0, sz.x - 1 ), p.y )"
        assert src.count(a) == 1
        return src.replace(a, "ivec2( p.x, ( p.y + k + sz.y ) % sz.y ) : ivec2( ( p.x + k + sz.x ) % sz.x, p.y )")
    if red == 'noclamp':    # a non-finite texel kept
        a = "vec3( finiteOr0( v.r ), finiteOr0( v.g ), finiteOr0( v.b ) )"
        assert src.count(a) == 1
        return src.replace(a, "v")
    return src

def main():
    a = sys.argv[1:]
    sdir = a[0]
    red = a[a.index('--red') + 1] if '--red' in a else ''
    slc.context()
    print('driver:', gl.glGetString(0x1F01).decode(), '|', gl.glGetString(0x1F02).decode())
    createShader = fn('glCreateShader', GLuint, GLenum)
    shaderSource = fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
    compileShader = fn('glCompileShader', None, GLuint)
    getShaderiv = fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    getShaderInfoLog = fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
    createProgram = fn('glCreateProgram', GLuint)
    attach = fn('glAttachShader', None, GLuint, GLuint)
    link = fn('glLinkProgram', None, GLuint)
    getProgramiv = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    useProgram = fn('glUseProgram', None, GLuint)
    getUniformLocation = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
    uniform1i = fn('glUniform1i', None, GLint, GLint)
    uniform2f = fn('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
    uniform1fv = fn('glUniform1fv', None, GLint, ctypes.c_int, ctypes.POINTER(ctypes.c_float))
    genTextures = fn('glGenTextures', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindTexture = fn('glBindTexture', None, GLenum, GLuint)
    texImage2D = fn('glTexImage2D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p)
    texParameteri = fn('glTexParameteri', None, GLenum, GLenum, GLint)
    activeTexture = fn('glActiveTexture', None, GLenum)
    genFramebuffers = fn('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindFramebuffer = fn('glBindFramebuffer', None, GLenum, GLuint)
    framebufferTexture2D = fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
    viewport = fn('glViewport', None, GLint, GLint, ctypes.c_int, ctypes.c_int)
    genVertexArrays = fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindVertexArray = fn('glBindVertexArray', None, GLuint)
    genBuffers = fn('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindBuffer = fn('glBindBuffer', None, GLenum, GLuint)
    bufferData = fn('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
    vertexAttribPointer = fn('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, ctypes.c_void_p)
    enableVertexAttribArray = fn('glEnableVertexAttribArray', None, GLuint)
    drawArrays = fn('glDrawArrays', None, GLenum, GLint, ctypes.c_int)
    readPixels = fn('glReadPixels', None, GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p)
    genQueries = fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))
    beginQuery = fn('glBeginQuery', None, GLenum, GLuint)
    endQuery = fn('glEndQuery', None, GLenum)
    getQueryObjectui64v = fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
    finish = fn('glFinish', None)

    files = None
    for line in open(os.path.join(sdir, 'cell_bloomblur.prog')):
        if line.startswith('shaders'):
            files = line.split()[1:]
    p = createProgram()
    for f in files:
        src = slc.expand(os.path.join(sdir, f))
        if f.endswith('frag'):
            src = doctor(src, red)
        s = createShader(0x8B30 if f.endswith('frag') else 0x8B31)
        b = ctypes.c_char_p(src.encode('latin-1'))
        shaderSource(s, 1, ctypes.byref(b), None)
        compileShader(s)
        ok = GLint(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
            print('COMPILE FAIL', f, log.value.decode(errors='replace')[:800]); return 2
        attach(p, s)
    link(p)
    ok = GLint(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('LINK FAIL'); return 2
    useProgram(p)
    loc = lambda n: getUniformLocation(p, n.encode())

    vao = GLuint(); genVertexArrays(1, ctypes.byref(vao)); bindVertexArray(vao)
    vbo = GLuint(); genBuffers(1, ctypes.byref(vbo)); bindBuffer(0x8892, vbo)
    quad = np.array([-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0], dtype=np.float32)
    bufferData(0x8892, quad.nbytes, quad.ctypes.data, 0x88E4)
    vertexAttribPointer(0, 3, 0x1406, 0, 0, None); enableVertexAttribArray(0)

    def tex2d():
        t = GLuint(); genTextures(1, ctypes.byref(t)); bindTexture(0x0DE1, t)
        return t
    # the measure: levels 0..2 (MAX_LEVEL 2, LINEAR_MIPMAP_NEAREST as cellhdr.cpp), level 2 = the quarter size
    activeTexture(0x84C0 + 8)
    mtex = tex2d()
    for lv, (w, h) in enumerate(((QW * 4, QH * 4), (QW * 2, QH * 2))):
        texImage2D(0x0DE1, lv, 0x8814, w, h, 0, 0x1908, 0x1406, None)
    texParameteri(0x0DE1, 0x813D, 2)
    texParameteri(0x0DE1, 0x2801, 0x2700)
    texParameteri(0x0DE1, 0x2800, 0x2601)
    finals = []
    for _ in range(2):
        t = tex2d()
        texImage2D(0x0DE1, 0, 0x8814, QW, QH, 0, 0x1908, 0x1406, None)
        texParameteri(0x0DE1, 0x813D, 0)
        texParameteri(0x0DE1, 0x2801, 0x2601); texParameteri(0x0DE1, 0x2800, 0x2601)
        texParameteri(0x0DE1, 0x2802, 0x812F); texParameteri(0x0DE1, 0x2803, 0x812F)
        finals.append(t)
    fbo = GLuint(); genFramebuffers(1, ctypes.byref(fbo)); bindFramebuffer(0x8D40, fbo)   # draw and read
    q = GLuint(); genQueries(1, ctypes.byref(q))

    worst = 0.0
    fails = 0
    ms = []
    for case, (thresh, scale) in enumerate(((0.6, 0.5), (1.0, 1.25), (0.0, 0.2))):
        px = make_input(11 + case)
        bindTexture(0x0DE1, mtex)
        texImage2D(0x0DE1, 2, 0x8814, QW, QH, 0, 0x1908, 0x1406, np.ascontiguousarray(px).ctypes.data)
        ref, wt = twin(px, thresh, scale)
        wts = (ctypes.c_float * 15)(*[float(v) for v in wt])
        uniform1fv(loc('bloomW[0]'), 15, wts)
        uniform2f(loc('bloomPass'), thresh, scale)
        uniform1i(loc('bloomSrc'), 8)
        viewport(0, 0, QW, QH)
        finish()
        beginQuery(0x88BF, q)
        framebufferTexture2D(0x8CA9, 0x8CE0, 0x0DE1, finals[0], 0)
        bindTexture(0x0DE1, mtex)
        uniform1i(loc('bloomLod'), 2); uniform1i(loc('bloomVertical'), 1)
        drawArrays(0x0005, 0, 4)
        framebufferTexture2D(0x8CA9, 0x8CE0, 0x0DE1, finals[1], 0)
        bindTexture(0x0DE1, finals[0])
        uniform1i(loc('bloomLod'), 0); uniform1i(loc('bloomVertical'), 0)
        drawArrays(0x0005, 0, 4)
        endQuery(0x88BF)
        ns = ctypes.c_uint64(); getQueryObjectui64v(q, 0x8866, ctypes.byref(ns))
        ms.append(ns.value / 1e6)
        out = np.zeros((QH, QW, 4), dtype=np.float32)
        readPixels(0, 0, QW, QH, 0x1908, 0x1406, out.ctypes.data)
        got = out[:, :, :3]
        peak = float(np.max(np.abs(ref))) if np.all(np.isfinite(ref)) else 1.0
        bad = ~np.isfinite(got)
        d = np.where(bad, np.inf, np.abs(got - ref))
        tol = 1e-5 * max(1.0, peak)
        nbad = int(np.count_nonzero(np.any(d > tol, axis=2)))
        dmax = float(np.max(d))
        worst = max(worst, dmax / max(1.0, peak))
        fails += 1 if nbad else 0
        print('case %d thresh %.2f scale %.2f: peak %.4f max|gpu-twin| %.3g (tol %.3g) texels over %d of %d; lit %d' % (
            case, thresh, scale, peak, dmax, tol, nbad, QW * QH, int(np.count_nonzero(np.any(
                np.where(np.isfinite(px[:, :, :3]), px[:, :, :3], 0) > thresh, axis=2)))))
    print('gpu ms (both passes, 480x270): ' + ' '.join('%.4f' % v for v in ms))
    verdict = 'BLOOMGPU PASS' if fails == 0 else 'BLOOMGPU FAIL'
    print('%s%s (worst relative %.3g)' % (verdict, ' red=' + red if red else '', worst))
    return 0 if fails == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
