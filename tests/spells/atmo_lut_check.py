"""Lane ATMO1: render the physical sky's LUT shaders through the real GL driver (hidden 4.1 core context, the loader's
#include expansion) and compare the read-back texels with the numpy model (atmo_model.py). Runs no NifSkope.

    python -I tests/spells/atmo_lut_check.py [shaders dir] [--red noms|noozone]

Gates: transmittance texels vs the exact optical-depth integral (< 1 %); multiple-scattering texels vs the model's
Psi (< 3 %); sky-view texels at sun 30 and 5 deg vs the model's march with the same Psi (< 2 %).
Exit 0 only when every gate passes; each red must make one fail.
"""
import ctypes, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import shader_link_check as S   # noqa: E402
import atmo_model as A          # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint


def build(sdir, prog):
    files = [l.split()[1:] for l in open(os.path.join(sdir, prog)) if l.startswith('shaders')][0]
    p = S.fn('glCreateProgram', GLuint)()
    for f in files:
        src = S.expand(os.path.join(sdir, f))
        s = S.fn('glCreateShader', GLuint, GLenum)(0x8B30 if f.endswith('frag') else 0x8B31)
        b = ctypes.c_char_p(src.encode('latin-1'))
        S.fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)(s, 1, ctypes.byref(b), None)
        S.fn('glCompileShader', None, GLuint)(s)
        ok = GLint()
        S.fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            raise SystemExit('COMPILE FAIL ' + prog + ' ' + f)
        S.fn('glAttachShader', None, GLuint, GLuint)(p, s)
    S.fn('glLinkProgram', None, GLuint)(p)
    return p


class Gl:
    def __init__(self):
        S.context()
        self.use = S.fn('glUseProgram', None, GLuint)
        self.loc = S.fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
        self.u1f = S.fn('glUniform1f', None, GLint, ctypes.c_float)
        self.u2f = S.fn('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
        self.u3f = S.fn('glUniform3f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float)
        self.u1i = S.fn('glUniform1i', None, GLint, GLint)
        self.active = S.fn('glActiveTexture', None, GLenum)
        genva = S.fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
        bindva = S.fn('glBindVertexArray', None, GLuint)
        genb = S.fn('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
        bindb = S.fn('glBindBuffer', None, GLenum, GLuint)
        bdata = S.fn('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
        vap = S.fn('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, ctypes.c_void_p)
        eva = S.fn('glEnableVertexAttribArray', None, GLuint)
        va, vb = GLuint(), GLuint()
        genva(1, ctypes.byref(va)); bindva(va)
        q = (ctypes.c_float * 12)(-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0)
        genb(1, ctypes.byref(vb)); bindb(0x8892, vb); bdata(0x8892, ctypes.sizeof(q), q, 0x88E4)
        vap(0, 3, 0x1406, 0, 0, None); eva(0)
        self.genfb = S.fn('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
        self.bindfb = S.fn('glBindFramebuffer', None, GLenum, GLuint)
        self.fbtex = S.fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
        gl.glTexImage2D.argtypes = [GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p]
        gl.glReadPixels.argtypes = [GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p]

    def target(self, w, h):
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x0DE1, t)
        gl.glTexImage2D(0x0DE1, 0, 0x8814, w, h, 0, 0x1908, 0x1406, None)   # RGBA32F
        for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F)):
            gl.glTexParameteri(0x0DE1, k, v)
        fb = GLuint()
        self.genfb(1, ctypes.byref(fb))
        self.bindfb(0x8D40, fb)
        self.fbtex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        return t.value, fb.value

    def draw(self, prog, fb, w, h, uni, texs):
        self.bindfb(0x8D40, fb)
        gl.glViewport(0, 0, w, h)
        self.use(prog)
        for k, v in uni.items():
            l = self.loc(prog, k.encode())
            if l < 0:
                continue
            if isinstance(v, int):
                self.u1i(l, v)
            elif len(v) == 1:
                self.u1f(l, v[0])
            elif len(v) == 2:
                self.u2f(l, *v)
            else:
                self.u3f(l, *v)
        for unit, (name, tex) in enumerate(texs):
            self.active(0x84C0 + unit)
            gl.glBindTexture(0x0DE1, tex)
            self.u1i(self.loc(prog, name.encode()), unit)
        gl.glDrawArrays(5, 0, 4)   # GL_TRIANGLE_STRIP
        buf = (ctypes.c_float * (w * h * 4))()
        gl.glReadPixels(0, 0, w, h, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(h, w, 4)[..., :3].astype(np.float64)


def sub_to_unit(u, n):
    return (u - 0.5 / n) / (1 - 1.0 / n)


def main():
    a = sys.argv[1:]
    sdir = a[0] if a and not a[0].startswith('--') else os.path.join(HERE, '..', '..', 'res', 'shaders')
    red = a[a.index('--red') + 1] if '--red' in a else ''
    P = A.params()
    base = dict(atRay=tuple(P['ray']), atMieS=(P['mieS'],), atMieE=(P['mieE'],),
                atOzone=(0.0, 0.0, 0.0) if red == 'noozone' else tuple(P['oz']), atG=(P['g'],))
    g = Gl()
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        print(('PASS ' if ok else 'FAIL ') + name + ': ' + msg)
        fails += 0 if ok else 1

    # 1. transmittance
    tw, th = 256, 64
    pt = build(sdir, 'atmo_transmittance.prog')
    tt, tfb = g.target(tw, th)
    T = g.draw(pt, tfb, tw, th, base, [])
    rng = np.random.default_rng(3)
    xs, ys = rng.integers(0, tw, 64), rng.integers(0, th, 64)
    u, v = sub_to_unit((xs + 0.5) / tw, tw), sub_to_unit((ys + 0.5) / th, th)
    r, mu = A.trans_uv_to_rmu(u, v)
    ref = A.transmittance(P, r, mu, 2000)
    got = T[ys, xs]
    err = np.abs(got - ref).max()   # absolute: grazing texels are ~0
    gate('transmittance', err < 0.01, 'max |GPU - exact| over 64 texels = %.5f (< 0.01)' % err)

    # 2. multiple scattering
    mw = mh = 32
    pm = build(sdir, 'atmo_ms.prog')
    mt, mfb = g.target(mw, mh)
    M = g.draw(pm, mfb, mw, mh, dict(base, atMsSize=(float(mw), float(mh))), [('atTransTex', tt)])
    worst = 0.0
    for (x, y) in ((16, 0), (24, 0), (20, 4), (28, 10), (31, 31), (12, 2)):
        mus = sub_to_unit((x + 0.5) / mw, mw) * 2 - 1
        h = max(0.01, sub_to_unit((y + 0.5) / mh, mh) * (A.RT - A.RB))
        ref = A.ms_psi(P, A.RB + h, mus, 8, 20)
        e = A.rel(M[y, x], ref).max()
        worst = max(worst, e if ref.max() > 1e-6 else 0.0)
    gate('ms-lut', worst < 0.03, 'worst relative error at 6 texels = %.4f (< 0.03)' % worst)

    # 3. sky-view at sun 30 and 5 deg, camera 0.1 km; the model's Psi from its own table of the GPU texels' grid
    vw, vh = 192, 108
    pv = build(sdir, 'atmo_skyview.prog')
    vt, vfb = g.target(vw, vh)
    psi = A.MsTable.__new__(A.MsTable)
    psi.t = np.zeros((mh, mw, 3))
    # the GPU table sits on sub-uv texel centers: rebuild the model's table there so both read the same Psi
    for y in range(mh):
        for x in range(mw):
            mus = sub_to_unit((x + 0.5) / mw, mw) * 2 - 1
            h = max(0.01, sub_to_unit((y + 0.5) / mh, mh) * (A.RT - A.RB))
            psi.t[y, x] = A.ms_psi(P, A.RB + h, mus, 8, 20)

    def psi_sub(rr, mus):
        x = np.clip(((mus * 0.5 + 0.5) * (1 - 1.0 / mw) + 0.5 / mw) * mw - 0.5, 0, mw - 1.001)
        y = np.clip((np.clip((rr - A.RB) / (A.RT - A.RB), 0, 1) * (1 - 1.0 / mh) + 0.5 / mh) * mh - 0.5, 0, mh - 1.001)
        x0, y0 = x.astype(int), y.astype(int)
        fx, fy = (x - x0)[:, None], (y - y0)[:, None]
        t = psi.t
        return (t[y0, x0] * (1 - fx) * (1 - fy) + t[y0, x0 + 1] * fx * (1 - fy) + t[y0 + 1, x0] * (1 - fx) * fy
                + t[y0 + 1, x0 + 1] * fx * fy)

    camR = A.RB + 0.1
    worst = 0.0
    for se in (30.0, 5.0):
        V = g.draw(pv, vfb, vw, vh, dict(base, atMsSize=(float(mw), float(mh)), atViewSize=(float(vw), float(vh)),
                                         atCamR=(camR,), atLightMu=(math.sin(math.radians(se)),),
                                         atMsOn=(0.0 if red == 'noms' else 1.0,)),
                   [('atTransTex', tt), ('atMsTex', mt)])
        for (x, y) in ((0, 10), (60, 30), (120, 50), (191, 20), (96, 53), (30, 5), (150, 40)):
            ux, uy = sub_to_unit((x + 0.5) / vw, vw), sub_to_unit((y + 0.5) / vh, vh)
            vhz = math.sqrt(camR ** 2 - A.RB ** 2)
            beta = math.acos(vhz / camR)
            zh = math.pi - beta
            if uy < 0.5:
                c = 1 - 2 * uy
                za = zh * (1 - c * c)
            else:
                c = uy * 2 - 1
                za = zh + beta * c * c
            cz = math.cos(za)
            caz = -(ux * ux * 2 - 1)
            sz = math.sqrt(max(0, 1 - cz * cz))
            d = np.array([[sz * caz, sz * math.sqrt(max(0, 1 - caz * caz)), cz]])
            ref = A.integrate(P, camR, d, A.sun_dir(se), 400, psi=psi_sub)[0][0]
            e = A.rel(V[y, x], ref).max()
            if '-v' in a:
                print('    sun %g texel %d,%d elev %.2f: GPU %s model %s err %.4f' % (se, x, y, 90 - math.degrees(za), np.round(V[y, x], 6), np.round(ref, 6), e))
            worst = max(worst, e)
    gate('sky-view', worst < 0.02, 'worst relative error at 14 texels (sun 30, 5 deg) = %.4f (< 0.02)' % worst)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
