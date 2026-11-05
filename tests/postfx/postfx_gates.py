"""lane POSTFX1: offline gates for the cell view's post passes (src/gl/cellpost.h). The REAL shaders of the tree go
through the real driver in a hidden GL 4.1 context (tests/spells/shader_link_check.py); no NifSkope window.

    python -I tests/postfx/postfx_gates.py [--red camera|rg32|noshutter] [--ms]

Gates (exit 0 only when every gate passes; each red must make its gate fail):
  objblur still     a still world under a 6 px/frame camera pan, no object: the output equals the input bit for bit
  objblur world     the same pan with a bar moving 8 px/frame of its own: a step edge in the world keeps
                    blur < 0.1 px (bungo's rule: the camera never blurs). Red camera: > 2 px (the old camera blur)
  objblur bar       the bar's leading edge blurs >= 0.8 x shutter x 8 px (shutter 0.5 = a 180 degree shutter)
  mblur rg16        the game's motion blur (game_mblur.frag) fed the same vectors in RG16F and RG32F: the frames
                    differ by < 0.5 of an 8-bit step everywhere. Red rg32 compares RG32F with an RG8 vector target
                    (a format too coarse; it must fail)
  --ms              GPU ms of the four object-blur passes at 1920 x 1080 (GL_TIME_ELAPSED median of 30)
Blur length of an edge = its spread (sum over the row of 4 z (1 - z)) / (2 / 3): a box blur of length L spreads 2 L / 3.
"""
import ctypes, os, sys
import numpy as np

TREE = os.environ.get('TREE', os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')))
sys.path.insert(0, TREE + '/tests/spells')
import shader_link_check as S   # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'features', 'sky'))  # sky helpers
import atmo_lut_check as C      # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SD = os.environ.get('SHADERS', TREE + '/res/shaders')
RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
RGBA32F, RG16F, RG32F, RG8, R32F = 0x8814, 0x822F, 0x8230, 0x822B, 0x822E
NEAREST, LINEAR = 0x2600, 0x2601


class Rig:
    def __init__(self):
        self.g = C.Gl()
        self.p = {k: C.build(SD, k + '.prog') for k in ('cell_objblur_vec', 'cell_objblur_tile', 'cell_objblur', 'game_mblur')}
        self.um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
        self.drawbufs = S.fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
        self.u4 = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
        self.q = GLuint(); S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(self.q))
        self.bq = S.fn('glBeginQuery', None, GLenum, GLuint)
        self.eq = S.fn('glEndQuery', None, GLenum)
        self.gq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
        fb = GLuint(); self.g.genfb(1, ctypes.byref(fb)); self.fb = fb.value
        self.texs = []

    def tex(self, w, h, internal, filt=NEAREST, data=None):
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t)); gl.glBindTexture(0x0DE1, t)
        buf = None
        if data is not None:
            a = np.zeros((h, w, 4), np.float32)
            a[..., :data.shape[2]] = data
            buf = np.ascontiguousarray(a[::-1], np.float32).ctypes.data_as(ctypes.c_void_p)
        gl.glTexImage2D(0x0DE1, 0, internal, w, h, 0, 0x1908, 0x1406, buf)
        for k, v in ((0x2801, filt), (0x2800, filt), (0x2802, 0x812F), (0x2803, 0x812F)):
            gl.glTexParameteri(0x0DE1, k, v)
        self.texs.append(t.value)
        return t.value

    def free(self):
        n = len(self.texs)
        gl.glDeleteTextures(n, (GLuint * n)(*self.texs))
        self.texs = []

    def read(self, t, w, h):
        self.g.bindfb(0x8D40, self.fb)
        self.g.fbtex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        gl.glReadBuffer(0x8CE0)
        buf = (ctypes.c_float * (w * h * 4))()
        gl.glReadPixels(0, 0, w, h, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(h, w, 4)[::-1].astype(np.float64)

    def pass_(self, prog, dst, w, h, texs, uni):
        g = self.g
        g.bindfb(0x8D40, self.fb)
        g.fbtex(0x8D40, 0x8CE0, 0x0DE1, dst, 0)
        self.drawbufs(1, (GLenum * 1)(0x8CE0))
        gl.glViewport(0, 0, w, h)
        p = self.p[prog]
        g.use(p)
        for unit, (name, t) in enumerate(texs):
            g.active(0x84C0 + unit); gl.glBindTexture(0x0DE1, t); g.u1i(g.loc(p, name.encode()), unit)
        for k, v in uni.items():
            l = g.loc(p, k.encode())
            if l < 0:
                continue
            if isinstance(v, int):
                g.u1i(l, v)
            elif isinstance(v, np.ndarray):
                self.um4(l, 1, 0, (ctypes.c_float * 16)(*v.T.flatten()))
            elif len(v) == 1:
                g.u1f(l, v[0])
            elif len(v) == 2:
                g.u2f(l, *v)
            else:
                self.u4(l, *v)
        gl.glDrawArrays(5, 0, 4)

    def objblur(self, w, h, col, vel, mask, depth, cam_px, red=0, shutter=0.5, tile=16, timed=False):
        """the four passes as cellpost.cpp issues them; returns the output frame (or the GPU ms when timed)"""
        tc = self.tex(w, h, RGBA32F, NEAREST, col)
        tv = self.tex(w, h, RG16F, NEAREST, vel)
        tm = self.tex(w, h, RGBA32F, NEAREST, mask)
        td = self.tex(w, h, R32F, NEAREST, depth[..., None])
        tb = self.tex(w, h, RG16F)
        tw, th = (w + tile - 1) // tile, (h + tile - 1) // tile
        t1, t2 = self.tex(tw, th, RG16F), self.tex(tw, th, RG16F)
        out = self.tex(w, h, RGBA32F)
        reproj = np.eye(4); reproj[0, 3] = -2.0 * cam_px / w   # prev ndc = ndc - 2 P / W: the content moved right P px
        times = []
        for it in range(32 if timed else 1):
            if timed:
                self.bq(0x88BF, self.q)
            self.pass_('cell_objblur_vec', tb, w, h, (('velTex', tv), ('maskTex', tm), ('depthTex', td)),
                       {'reproj': reproj, 'jitterNdc': (0.0, 0.0), 'invSize': (1.0 / w, 1.0 / h), 'shutter': (shutter,),
                        'maxPx': (32.0,), 'objRed': red})
            self.pass_('cell_objblur_tile', t1, tw, th, (('srcTex', tb),), {'tile': tile, 'tileMode': 0})
            self.pass_('cell_objblur_tile', t2, tw, th, (('srcTex', t1),), {'tile': tile, 'tileMode': 1})
            self.pass_('cell_objblur', out, w, h, (('colTex', tc), ('blurTex', tb), ('neighTex', t2), ('depthTex', td)),
                       {'tile': tile, 'samples': 15, 'zNear': (1.0,), 'zFar': (10000.0,), 'noiseSeed': (0.0,)})
            if timed:
                self.eq(0x88BF)
                ns = ctypes.c_uint64(); self.gq(self.q, 0x8866, ctypes.byref(ns)); times.append(ns.value / 1e6)
        o = None if timed else self.read(out, w, h)
        self.free()
        return float(np.median(times[2:])) if timed else o

    def mblur(self, w, h, col, mv, fmt):
        tc = self.tex(w, h, RGBA32F, LINEAR, col)
        tv = self.tex(w, h, fmt, LINEAR, mv)
        out = self.tex(w, h, RGBA32F)
        self.pass_('game_mblur', out, w, h, (('t0', tc), ('t1', tv)),
                   {'c0': (50 * 0.001 * 60.0, 0.01, 70.0, 0.0), 'c1': (1.0, 1.0, (w - 1.0) / w, (h - 1.0) / h)})
        o = self.read(out, w, h)
        self.free()
        return o


def blur_len(row):
    z = np.clip((row - row.min()) / max(row.max() - row.min(), 1e-9), 0, 1)
    return float((4 * z * (1 - z)).sum() / (2.0 / 3.0))


def scene(w, h, cam_px, bar_px):
    """a world (light ground, a dark block left of x = 60, a noise strip below y = 150) at depth 0.99, and a dark bar
    x in [150, 170), y in [40, 140) at depth 0.9 that moves bar_px of its own. vel = previous minus current, uv"""
    rng = np.random.default_rng(3)
    col = np.full((h, w, 4), 0.8); col[..., 3] = 1
    col[:, :60, :3] = 0.2
    col[150:, :, :3] = (0.3 + 0.5 * rng.random((h - 150, w)))[..., None]
    depth = np.full((h, w), 0.99)
    vel = np.zeros((h, w, 2)); vel[..., 0] = -cam_px / w
    mask = np.zeros((h, w, 3))
    if bar_px:
        col[40:140, 150:170, :3] = 0.05
        depth[40:140, 150:170] = 0.9
        vel[40:140, 150:170, 0] = -(cam_px + bar_px) / w
        mask[40:140, 150:170, 2] = 1.0
    return col, vel, mask, depth


def main():
    w, h = 320, 180
    R = Rig()
    fails, lines = 0, []

    def gate(name, ok, msg):
        nonlocal fails
        fails += 0 if ok else 1
        lines.append('%-16s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg))
        print(lines[-1], flush=True)

    red = 1 if RED == 'camera' else 0
    shutter = 1.0 if RED == 'noshutter' else 0.5
    col, vel, mask, depth = scene(w, h, 6.0, 0.0)
    o = R.objblur(w, h, col, vel, mask, depth, 6.0, red, shutter)
    d = np.abs(o[..., :3] - col[..., :3].astype(np.float32)).max()
    gate('objblur still', d == 0.0, 'max |out - in| %.3g (6 px/frame pan, no object)' % d)
    col, vel, mask, depth = scene(w, h, 6.0, 8.0)
    o = R.objblur(w, h, col, vel, mask, depth, 6.0, red, shutter)
    Y = o[..., 1]
    wl = float(np.median([blur_len(Y[y, 40:80]) for y in range(45, 135)]))
    bl = float(np.median([blur_len(Y[y, 160:200]) for y in range(45, 135)]))
    gate('objblur world', wl < 0.1, 'world step blur %.3f px (bar < 0.1, the old camera blur > 2)' % wl)
    gate('objblur bar', 0.8 * 0.5 * 8 <= bl <= 1.25 * 0.5 * 8, 'bar edge blur %.3f px (want %.1f..%.1f: 0.5 x 8 px)' % (bl, 3.2, 5.0))
    # the game's blur on RG16F vs RG32F vectors (a turning camera: up to ~0.012 uv, 23 px at 1920)
    yy, xx = np.mgrid[0:h, 0:w] / np.array([h, w])[:, None, None]
    mv = np.dstack([-0.008 * (1 + 0.5 * np.sin(7 * xx + 3 * yy)), 0.003 * np.cos(5 * yy)])
    # a picture with edges every ~10 px (white noise reads the game shader's own copy/blur switch at rr = 0.001,
    # a 1 px jump that a change in the vector's last bit flips)
    sm = 0.5 + 0.25 * np.sin(40 * xx) * np.cos(23 * yy) + 0.2 * np.sign(np.sin(31 * xx + 11 * yy))
    img = np.dstack([sm] * 3 + [np.ones((h, w))])
    a = R.mblur(w, h, img, mv, RG32F)
    b = R.mblur(w, h, img, mv, RG8 if RED == 'rg32' else RG16F)
    e = np.abs(a[..., :3] - b[..., :3]).max(axis=2) * 255
    over = float((e >= 0.5).mean())
    gate('mblur rg16', over < 0.001, 'pixels >= 0.5 step %.4f%% (max %.3f, p99.9 %.3f steps)%s'
         % (100 * over, e.max(), np.percentile(e, 99.9), ' (RED: RG8)' if RED == 'rg32' else ''))
    if '--ms' in sys.argv:
        col, vel, mask, depth = scene(1920, 1080, 6.0, 0.0)
        col2, vel2, mask2, depth2 = scene(1920, 1080, 6.0, 8.0)
        ms0 = R.objblur(1920, 1080, col, vel, mask, depth, 6.0, timed=True)
        ms1 = R.objblur(1920, 1080, col2, vel2, mask2, depth2, 6.0, timed=True)
        lines.append('objblur ms 1080p: %.3f (no mover), %.3f (one bar)' % (ms0, ms1))
        print(lines[-1], flush=True)
    print('POSTFX RESULT %s (%d fail)%s' % ('PASS' if not fails else 'FAIL', fails, ' red=' + RED if RED else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
