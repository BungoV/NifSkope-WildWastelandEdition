"""lane WETWEATHER2: the shader's cover test (res/shaders/features/weather/rain_wet.glsl wwRainWetExposed) against its CPU twin
(src/features/weather/rainwetlaw.h exposed), on the real driver, no NifSkope.

    python -I tests/features/weather/raincover/raincover_gpu.py <raincover exe> [--red cover|off]

The exe's `dump` gives the fixture map (uploaded R32F at unit 88 as cellprecip.cpp uploads it) and 40000 points (ground,
walls, slopes) with the CPU exposure. Gate: |GPU - CPU| <= 1e-3 for every point whose tapped texels stand more than
0.05 u from the open line (closer ones are counted apart: float order). The fixture must hold >= 2000 covered and
>= 2000 open points. Reds (must FAIL): --red cover = RAIN_COVER_RED (no cover test, FO76WEATHER1), --red off =
rainWet.w left 0 (the map never named, WW_CELL_RAINWET_RED=nocover). Prints RAINCOVER-GPU PASS|FAIL."""
import ctypes, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
UNIT = 88  # src/gl/cellunits.h WwCellUnit::RainCover
TOL, APART = 1e-3, 0.05


def dump(exe):
    env = dict(os.environ)
    env['PATH'] = r'C:\msys64\ucrt64\bin;' + env.get('PATH', '')
    out = subprocess.run([exe, 'dump'], capture_output=True, text=True, env=env, check=True, timeout=60).stdout.split('\n')
    hd = out[0].split()
    assert hd[0] == 'map'
    nx, ny = int(hd[1]), int(hd[2])
    x0, y0, cell = float(hd[3]), float(hd[4]), float(hd[5])
    Z = np.array([[float(v) for v in l.split()[1:]] for l in out[1:1 + ny]], dtype=np.float32)
    assert Z.shape == (ny, nx), Z.shape
    P = np.array([[float(v) for v in l.split()[1:]] for l in out[1 + ny:] if l.startswith('p ')], dtype=np.float64)
    return nx, ny, x0, y0, cell, Z, P


def main():
    a = sys.argv[1:]
    exe = a[0]
    red = a[a.index('--red') + 1] if '--red' in a else None
    nx, ny, x0, y0, cell, Z, P = dump(exe)
    S.context()
    f = S.fn
    createShader = f('glCreateShader', GLuint, GLenum)
    shaderSource = f('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
    compileShader = f('glCompileShader', None, GLuint)
    getShaderiv = f('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    getShaderInfoLog = f('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
    createProgram = f('glCreateProgram', GLuint)
    attach = f('glAttachShader', None, GLuint, GLuint)
    link = f('glLinkProgram', None, GLuint)
    getProgramiv = f('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    getProgramInfoLog = f('glGetProgramInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
    tfVaryings = f('glTransformFeedbackVaryings', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), GLenum)
    bindAttrib = f('glBindAttribLocation', None, GLuint, GLuint, ctypes.c_char_p)
    useProgram = f('glUseProgram', None, GLuint)
    loc = f('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
    u1i = f('glUniform1i', None, GLint, GLint)
    u4f = f('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
    genVA = f('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindVA = f('glBindVertexArray', None, GLuint)
    genBuf = f('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindBuf = f('glBindBuffer', None, GLenum, GLuint)
    bindBufBase = f('glBindBufferBase', None, GLenum, GLuint, GLuint)
    bufData = f('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
    getBufSub = f('glGetBufferSubData', None, GLenum, ctypes.c_ssize_t, ctypes.c_ssize_t, ctypes.c_void_p)
    attribPtr = f('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, ctypes.c_void_p)
    enableAttrib = f('glEnableVertexAttribArray', None, GLuint)
    beginTF = f('glBeginTransformFeedback', None, GLenum)
    endTF = f('glEndTransformFeedback', None)
    active = f('glActiveTexture', None, GLenum)
    g = S.gl
    g.glEnable.argtypes = [GLenum]
    g.glDrawArrays.argtypes = [GLenum, GLint, ctypes.c_int]
    g.glGetError.restype = GLenum
    g.glGenTextures.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint)]
    g.glBindTexture.argtypes = [ctypes.c_uint, ctypes.c_uint]
    g.glTexParameteri.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_int]
    g.glPixelStorei.argtypes = [ctypes.c_uint, ctypes.c_int]
    g.glTexImage2D.argtypes = [ctypes.c_uint, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p]

    src = S.inject(S.expand(os.path.join(HERE, 'raincover_gpu.vert')), ['RAIN_COVER_RED'] if red == 'cover' else [])
    sh = createShader(0x8B31)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(sh, 1, ctypes.byref(b), None)
    compileShader(sh)
    ok = GLint(); getShaderiv(sh, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(sh, 1 << 16, None, log)
        raise SystemExit('COMPILE FAIL ' + log.value.decode(errors='replace')[:1500])
    p = createProgram()
    attach(p, sh)
    bindAttrib(p, 0, b'inP'); bindAttrib(p, 1, b'inN')
    names = (ctypes.c_char_p * 1)(b'vsE')
    tfVaryings(p, 1, names, 0x8C8C)
    link(p)
    getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getProgramInfoLog(p, 1 << 16, None, log)
        raise SystemExit('LINK FAIL ' + log.value.decode(errors='replace')[:1500])

    t = ctypes.c_uint(); g.glGenTextures(1, ctypes.byref(t))
    active(0x84C0 + UNIT)
    g.glBindTexture(0x0DE1, t)
    z = np.ascontiguousarray(Z, np.float32)
    g.glPixelStorei(0x0CF5, 4)
    g.glTexImage2D(0x0DE1, 0, 0x822E, nx, ny, 0, 0x1903, 0x1406, z.ctypes.data)
    for pn, v in ((0x2801, 0x2600), (0x2800, 0x2600), (0x813D, 0), (0x2802, 0x812F), (0x2803, 0x812F)):
        g.glTexParameteri(0x0DE1, pn, v)
    active(0x84C0)

    useProgram(p)
    u1i(loc(p, b'rainCover'), UNIT)
    u4f(loc(p, b'rainWet'), 1.0, 0.0, 0.0, 0.0 if red == 'off' else 1.0)
    u4f(loc(p, b'rainCoverGrid'), x0, y0, 1.0 / cell, cell)

    n = len(P)
    va = GLuint(); genVA(1, ctypes.byref(va)); bindVA(va)
    vb = GLuint(); genBuf(1, ctypes.byref(vb))
    verts = np.ascontiguousarray(P[:, 0:6], np.float32)
    bindBuf(0x8892, vb)
    bufData(0x8892, verts.nbytes, verts.ctypes.data, 0x88E4)
    attribPtr(0, 3, 0x1406, 0, 24, None); enableAttrib(0)
    attribPtr(1, 3, 0x1406, 0, 24, ctypes.c_void_p(12)); enableAttrib(1)
    tb = GLuint(); genBuf(1, ctypes.byref(tb))
    bindBuf(0x8C8E, tb)
    bufData(0x8C8E, n * 4, None, 0x88E5)
    bindBufBase(0x8C8E, 0, tb)
    g.glEnable(0x8C89)
    beginTF(0x0000)
    g.glDrawArrays(0x0000, 0, n)
    endTF()
    out = np.empty(n, np.float32)
    getBufSub(0x8C8E, 0, n * 4, out.ctypes.data)
    err = g.glGetError()
    if err:
        raise SystemExit('GL error 0x%x' % err)

    cpu = P[:, 6]
    apart = P[:, 7] <= APART
    d = np.abs(out.astype(np.float64) - cpu)
    bad = (~apart) & (d > TOL)
    covered = int(((~apart) & (cpu < 0.5)).sum())
    opened = int(((~apart) & (cpu >= 0.5)).sum())
    print('points %d: covered %d open %d apart %d | mismatches %d, worst %.6f' %
          (n, covered, opened, int(apart.sum()), int(bad.sum()), float(d[~apart].max())))
    for k in np.nonzero(bad)[0][:5]:
        print('  p %s cpu %.4f gpu %.4f' % (np.round(P[k, :6], 3).tolist(), cpu[k], out[k]))
    fails = int(bad.sum())
    if covered < 2000 or opened < 2000:
        print('fixture too thin')
        fails += 1
    print('RAINCOVER-GPU', 'PASS' if fails == 0 else 'FAIL', '(red %s)' % red)
    return 0 if fails == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
