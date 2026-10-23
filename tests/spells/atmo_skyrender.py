"""SKY2B: render the physical sky pass (atmo_sky.frag) offline in the hidden GL context (no NifSkope window).
Builds the transmittance / MS / sky-view LUTs from the real .prog files, then the sky's pixels at screen size for a
camera, linear (viewTransform 3). Library for atmo_gi_capture_check.py (SKY3); notes/sky2b/blur has the blur and stripe checks built on it."""
import ctypes, math, os, re, sys
import numpy as np
TREE = os.environ.get('TREE', os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, TREE + '/tests/spells')
import shader_link_check as S   # noqa
import atmo_lut_check as C      # noqa
import atmo_model as A          # noqa
gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint


def compile_src(vsrc, fsrc):
    p = S.fn('glCreateProgram', GLuint)()
    for src, kind in ((vsrc, 0x8B31), (fsrc, 0x8B30)):
        s = S.fn('glCreateShader', GLuint, GLenum)(kind)
        b = ctypes.c_char_p(src.encode('latin-1'))
        S.fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)(s, 1, ctypes.byref(b), None)
        S.fn('glCompileShader', None, GLuint)(s)
        ok = GLint()
        S.fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(8192)
            S.fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)(s, 8192, None, log)
            raise SystemExit('COMPILE FAIL ' + log.value.decode('latin-1')[:2000])
        S.fn('glAttachShader', None, GLuint, GLuint)(p, s)
    S.fn('glLinkProgram', None, GLuint)(p)
    return p


def sundir(az_deg, el_deg):
    az, el = math.radians(az_deg), math.radians(el_deg)
    return (math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))


class Sky:
    def __init__(self, sdir, haze=(0.0, 1.0, 0.0)):
        self.sdir = sdir
        self.g = C.Gl()
        self.P = A.params(haze=haze)
        P = self.P
        self.base = dict(atRay=tuple(P['ray']), atMieS=(P['mieS'],), atMieE=(P['mieE'],), atOzone=tuple(P['oz']),
                         atG=(P['g'],), atHaze=(P['hazeE'], P['hazeH'], P['hazeMid']))
        self.pt = C.build(sdir, 'atmo_transmittance.prog')
        self.pm = C.build(sdir, 'atmo_ms.prog')
        self.pv = C.build(sdir, 'atmo_skyview.prog')
        src = S.expand(os.path.join(sdir, 'atmo_sky.frag'))
        src, n = re.subn(r'layout\s*\(\s*std140\s*\)\s*uniform\s+globalUniforms\s*\{.*?\};',
                         'uniform mat3 viewMatrix;\nuniform mat4 projectionMatrix;', src, flags=re.S)
        assert n == 1
        self.ps = compile_src(S.expand(os.path.join(sdir, 'cell_ao.vert')), src)
        self.trans, tfb = self.g.target(256, 64)
        self.g.draw(self.pt, tfb, 256, 64, self.base, [])
        self.ms, mfb = self.g.target(32, 32)
        self.g.draw(self.pm, mfb, 32, 32, dict(self.base, atMsSize=(32.0, 32.0)), [('atTransTex', self.trans)])
        for t in (self.trans, self.ms):
            gl.glBindTexture(0x0DE1, t)
            gl.glTexParameteri(0x0DE1, 0x2801, 0x2601)
            gl.glTexParameteri(0x0DE1, 0x2800, 0x2601)
        self.q = GLuint()
        S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(self.q))
        self.um3 = S.fn('glUniformMatrix3fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
        self.um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
        self.u4f = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
        self.begq = S.fn('glBeginQuery', None, GLenum, GLuint)
        self.endq = S.fn('glEndQuery', None, GLenum)
        self.getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
        self.cache = {}
        self.targets = {}

    def target(self, w, h):
        if (w, h) not in self.targets:
            self.targets[(w, h)] = self.g.target(w, h)
        return self.targets[(w, h)]

    def view_lut(self, w, h, camR, light, ground=1.0):
        key = (w, h, camR, tuple(light), ground)
        if key in self.cache:
            return self.cache[key]
        t, fb = self.g.target(w, h)
        u = dict(self.base, atMsSize=(32.0, 32.0), atViewSize=(float(w), float(h)), atCamR=(camR,),
                 atLightMu=(float(light[2]),), atMsOn=(1.0,), atGround=(ground,))
        self.g.draw(self.pv, fb, w, h, u, [('atTransTex', self.trans), ('atMsTex', self.ms)])
        gl.glBindTexture(0x0DE1, t)
        gl.glTexParameteri(0x0DE1, 0x2801, 0x2601)   # linear, as the renderer's targets
        gl.glTexParameteri(0x0DE1, 0x2800, 0x2601)
        self.cache[key] = t
        return t

    def lut_ms(self, w, h, camR, light, ground=1.0, reps=20):
        """GPU ms of one sky-view LUT pass"""
        t, fb = self.target(w, h)
        u = dict(self.base, atMsSize=(32.0, 32.0), atViewSize=(float(w), float(h)), atCamR=(camR,),
                 atLightMu=(float(light[2]),), atMsOn=(1.0,), atGround=(ground,))
        self.g.draw(self.pv, fb, w, h, u, [('atTransTex', self.trans), ('atMsTex', self.ms)])
        gl.glFinish()
        self.begq(0x88BF, self.q)
        for _ in range(reps):
            gl.glDrawArrays(5, 0, 4)
        self.endq(0x88BF)
        ns = ctypes.c_uint64()
        self.getq(self.q, 0x8866, ctypes.byref(ns))
        return ns.value / 1e6 / reps

    def render(self, W, H, cam, sun, camZkm=0.02, lut=(192, 108), perpixel=False, ground=1.0, vfov=55.0, reps=1, disc=1e4):
        """cam = (yaw deg, pitch deg); sun = (az deg, elev deg); returns (linear rgb HxWx3 row 0 = top, gpu ms)"""
        camR = 6360.0 + camZkm
        sw = sundir(*sun)
        sv = self.view_lut(lut[0], lut[1], camR, sw, ground)
        yaw, pit = map(math.radians, cam)
        fwd = np.array([math.cos(pit) * math.cos(yaw), math.cos(pit) * math.sin(yaw), math.sin(pit)])
        right = np.cross(fwd, [0, 0, 1.0]); right /= np.linalg.norm(right)
        up = np.cross(right, fwd)
        M = np.ascontiguousarray(np.array([right, up, -fwd], np.float32))   # rows = camera axes; d = vr * M
        f = 1.0 / math.tan(math.radians(vfov) / 2)
        Pm = np.zeros((4, 4), np.float32)
        Pm[0, 0] = f * H / W; Pm[1, 1] = f; Pm[2, 2] = -1; Pm[3, 2] = -1; Pm[2, 3] = -1
        Pm = np.ascontiguousarray(Pm)
        t, fb = self.target(W, H)
        g = self.g
        g.bindfb(0x8D40, fb); gl.glViewport(0, 0, W, H); g.use(self.ps)
        lm = lambda n: g.loc(self.ps, n.encode())
        fp = ctypes.POINTER(ctypes.c_float)
        # GLSL m[c][r]: viewMatrix = M as rows -> pass M row-major with transpose; P[2][0] etc. are 0 here
        self.um3(lm('viewMatrix'), 1, 1, M.ctypes.data_as(fp))
        self.um4(lm('projectionMatrix'), 1, 1, Pm.ctypes.data_as(fp))
        tl = A.transmittance(self.P, np.array([6360.0]), np.array([0.70710678]), 400)
        tl = float(np.dot(tl[0], [0.2126, 0.7152, 0.0722]))
        uni = dict(self.base, atViewSize=(float(lut[0]), float(lut[1])), atCamR=(camR,), atSunW=sw,
                   atMoonW=(0.0, 0.0, -1.0), atSunK=(math.pi / tl,), atMoonK=(0.0,), atLimb=(1.0,), atDiscCap=(disc,),
                   sceneExposure=(1.0,), atTintOn=(0.0,), atMsSize=(32.0, 32.0), atMsOn=(1.0,), atGround=(ground,),
                   atPerPixel=(1.0 if perpixel else 0.0,))
        for k, v in uni.items():
            l = lm(k)
            if l >= 0:
                {1: g.u1f, 2: g.u2f, 3: g.u3f}[len(v)](l, *v)
        self.u4f(lm('atRect'), 0, 0, 1.0 / W, 1.0 / H)
        g.u1i(lm('viewTransform'), 3)
        for unit, (name, tex) in enumerate([('atTransTex', self.trans), ('atSunView', sv), ('atMoonView', sv), ('atMsTex', self.ms)]):
            g.active(0x84C0 + unit); gl.glBindTexture(0x0DE1, tex); l = lm(name)
            if l >= 0:
                g.u1i(l, unit)
        gl.glFinish()
        self.begq(0x88BF, self.q)
        for _ in range(reps):
            gl.glDrawArrays(5, 0, 4)
        self.endq(0x88BF)
        ns = ctypes.c_uint64()
        self.getq(self.q, 0x8866, ctypes.byref(ns))
        buf = (ctypes.c_float * (W * H * 4))()
        gl.glReadPixels(0, 0, W, H, 0x1908, 0x1406, buf)
        img = np.frombuffer(buf, np.float32).reshape(H, W, 4)[::-1, :, :3].astype(np.float64)
        return img, ns.value / 1e6 / reps


def to8(img, k=None):
    y = img @ np.array([0.2126, 0.7152, 0.0722])
    if k is None:
        k = 0.5 / max(float(np.median(y)), 1e-9)
    c = np.clip(img * k, 0, None)
    c = np.where(c <= 0.0031308, 12.92 * c, 1.055 * np.power(c, 1 / 2.4) - 0.055)
    return (np.clip(c, 0, 1) * 255 + 0.5).astype(np.uint8), k
