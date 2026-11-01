"""lane PERFFIX1 (item 4): the bounce's donor from the previous frame (res/shaders/cell_ssr.frag stage 5, cellssr.cpp
prevBounce) on the real driver in a hidden GL 4.1 context (tests/spells/shader_link_check.py), against an analytic twin.

    python -I tests/reflprev/reflprev_gate.py <shaders dir> [--red noflip|transpose|notrans]

Scene: a ground plane z = 0 seen from 180 units up, looking down 20 deg; the previous camera turned 6 deg and moved 40
units. The previous frame's image = a smooth function of the world point it shows, drawn from the previous camera
(the sky: a function of the view direction). gbuf = this view's linear depth (cellAoPassOut's a). Stage 5 must give,
at every pixel whose ground point (nearer than 1000 units: past it the test image itself aliases) the previous camera
saw, that point's colour: |err| <= 0.02 on >= 99.5 % of them (bilinear filtering of a smooth image). Uniforms as
cellssr.cpp sets them: M = Rp R^T (row-major, uploaded with transpose), T = tp - M t, the previous projection P00/P11.
Reds (a doctored upload, each must FAIL): noflip = the previous uv's v taken top row first, transpose = M uploaded
without transpose, notrans = T without its - M t (the camera's own move dropped)."""
import ctypes, importlib.util, math, os, sys
import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('slc', os.path.join(here, '..', 'spells', 'shader_link_check.py'))
slc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(slc)
gl = slc.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint

def fn(name, res, *args):
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *args)(addr)
    f = getattr(gl, name)
    f.restype = res
    f.argtypes = list(args)
    return f

W, H = 320, 200
P00, P11 = 1.0 / math.tan(math.radians(35)) * H / W, 1.0 / math.tan(math.radians(35))

def rot_cam(yaw, pitch):
    """world -> view rotation: the camera looks along +y at yaw 0, tilted down by pitch; view looks down -z"""
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    fwd = np.array([sy * cp, cy * cp, -sp])
    right = np.array([cy, -sy, 0.0])
    up = np.cross(right, fwd)
    return np.array([right, up, -fwd])   # rows: view x, y, z axes in the world

def world_color(X):
    """a smooth function of the world point"""
    return np.stack([0.5 + 0.4 * np.sin(X[..., 0] / 90.0), 0.5 + 0.4 * np.cos(X[..., 1] / 110.0),
                     0.3 + 0.2 * np.sin((X[..., 0] + X[..., 1]) / 150.0)], axis=-1)

def sky_color(d):
    return np.stack([0.2 + 0.1 * d[..., 0], 0.4 + 0.1 * d[..., 1], 0.8 + 0.1 * d[..., 2]], axis=-1)

def render(Rv, eye):
    """per pixel (bottom row first): linear depth (1e6 sky), the colour, the world point, ground hit mask"""
    ys, xs = np.mgrid[0:H, 0:W]
    u = (xs + 0.5) / W
    v = (ys + 0.5) / H
    dv = np.stack([(2 * u - 1) / P00, (2 * v - 1) / P11, -np.ones_like(u)], axis=-1)   # view ray, z = -1
    dw = dv @ Rv     # to the world (R^T d)
    t = np.where(dw[..., 2] < -1e-6, -eye[2] / np.minimum(dw[..., 2], -1e-6), np.inf)
    hit = np.isfinite(t)
    X = eye + dw * np.where(hit, t, 0.0)[..., None]
    depth = np.where(hit, t, 1.0e6)   # view z = -1 per unit t: linear depth = t
    dn = dw / np.linalg.norm(dw, axis=-1, keepdims=True)
    col = np.where(hit[..., None], world_color(X), sky_color(dn))
    return depth.astype(np.float32), col.astype(np.float32), X, hit

def main():
    a = sys.argv[1:]
    sdir = a[0]
    red = a[a.index('--red') + 1] if '--red' in a else ''
    eye = np.array([1000.0, -500.0, 180.0])
    R = rot_cam(math.radians(10), math.radians(20))
    eyeP = eye + np.array([30.0, 26.0, 4.0])
    Rp = rot_cam(math.radians(16), math.radians(19))
    depth, _, X, hit = render(R, eye)
    _, prevCol, _, _ = render(Rp, eyeP)
    t, tp = -R @ eye, -Rp @ eyeP
    M = Rp @ R.T
    T = tp - M @ t
    # the twin: each pixel's ground point as the previous camera saw it
    Pp = (X - eyeP) @ Rp.T
    seenPrev = hit & (-Pp[..., 2] > 15.0)
    zp = np.maximum(-Pp[..., 2], 15.0)
    upx = 0.5 + 0.5 * P00 * Pp[..., 0] / zp
    upy = 0.5 + 0.5 * P11 * Pp[..., 1] / zp
    inPrev = seenPrev & (upx > 0.01) & (upx < 0.99) & (upy > 0.01) & (upy < 0.99) & (depth < 1000.0)
    want = world_color(X)

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
    loc_ = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
    u1i = fn('glUniform1i', None, GLint, GLint)
    u2i = fn('glUniform2i', None, GLint, GLint, GLint)
    u2f = fn('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
    u3f = fn('glUniform3f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float)
    um3 = fn('glUniformMatrix3fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
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
    vap = fn('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, ctypes.c_void_p)
    eva = fn('glEnableVertexAttribArray', None, GLuint)
    drawArrays = fn('glDrawArrays', None, GLenum, GLint, ctypes.c_int)
    readPixels = fn('glReadPixels', None, GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p)

    files = [l.split()[1:] for l in open(os.path.join(sdir, 'cell_ssr.prog')) if l.startswith('shaders')][0]
    p = createProgram()
    for f in files:
        src = slc.expand(os.path.join(sdir, f))
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
    L = lambda n: loc_(p, n.encode())
    vao = GLuint(); genVertexArrays(1, ctypes.byref(vao)); bindVertexArray(vao)
    vbo = GLuint(); genBuffers(1, ctypes.byref(vbo)); bindBuffer(0x8892, vbo)
    quad = np.array([-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0], dtype=np.float32)
    bufferData(0x8892, quad.nbytes, quad.ctypes.data, 0x88E4)
    vap(0, 3, 0x1406, 0, 0, None); eva(0)

    def tex(unit, data, linear):
        activeTexture(0x84C0 + unit)
        t = GLuint(); genTextures(1, ctypes.byref(t)); bindTexture(0x0DE1, t)
        texImage2D(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, np.ascontiguousarray(data).ctypes.data)
        texParameteri(0x0DE1, 0x2801, 0x2601 if linear else 0x2600); texParameteri(0x0DE1, 0x2800, 0x2601 if linear else 0x2600)
        texParameteri(0x0DE1, 0x2802, 0x812F); texParameteri(0x0DE1, 0x2803, 0x812F)
        return t
    gb = np.zeros((H, W, 4), np.float32); gb[..., 2] = 1.0; gb[..., 3] = depth
    pv = np.ones((H, W, 4), np.float32); pv[..., :3] = prevCol
    tex(0, gb, False)
    tex(8, pv, True)
    out = tex(5, np.zeros((H, W, 4), np.float32), False)
    fbo = GLuint(); genFramebuffers(1, ctypes.byref(fbo)); bindFramebuffer(0x8D40, fbo)
    activeTexture(0x84C0 + 5); bindTexture(0x0DE1, 0)   # stage 5 writes this one (cellssr.cpp unbinds unit 5 too)
    framebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, out, 0)
    viewport(0, 0, W, H)
    u1i(L('gbuf'), 0); u1i(L('prevScene'), 8); u1i(L('ssrStage'), 5)
    u2i(L('fullSize'), W, H); u2f(L('proj'), P00, P11); u2f(L('ssrClip'), 15.0, 10000.0)
    Mrow = (ctypes.c_float * 9)(*[float(x) for x in M.reshape(-1)])
    um3(L('ssrPrevRot'), 1, 0 if red == 'transpose' else 1, Mrow)
    u3f(L('ssrPrevT'), *[float(x) for x in (tp if red == 'notrans' else T)])
    # the noflip red: the previous projection's P11 sign flipped (= v top row first)
    u2f(L('ssrPrevProj'), P00, -P11 if red == 'noflip' else P11)
    drawArrays(0x0005, 0, 4)
    got = np.zeros((H, W, 4), np.float32)
    readPixels(0, 0, W, H, 0x1908, 0x1406, got.ctypes.data)
    err = np.abs(got[..., :3] - want).max(axis=-1)
    for lo, hi in ((0, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 1e9)):
        m = inPrev & (depth >= lo) & (depth < hi)
        if m.any():
            print('  depth %6.0f-%-6.0f px %6d over 0.02: %6d' % (lo, min(hi, 99999), int(m.sum()), int((err[m] > 0.02).sum())))
    n = int(inPrev.sum())
    good = int((err[inPrev] <= 0.02).sum())
    share = good / max(n, 1)
    print('pixels seen in both frames %d of %d: within 0.02 %d (%.4f), median err %.5f, max %.4f' % (
        n, W * H, good, share, float(np.median(err[inPrev])), float(err[inPrev].max())))
    ok = n > W * H * 0.3 and share >= 0.995
    print('REFLPREV %s%s' % ('PASS' if ok else 'FAIL', ' red=' + red if red else ''))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
