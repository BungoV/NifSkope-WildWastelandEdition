"""lane POSTFX3: offline gates for the two multisample rows of the linear frame, the REAL shaders of the tree through
the real driver in a hidden GL 4.1 context (no NifSkope window).

    python -I tests/postfx/postfx_msaa.py [--red allb|noadd|additive|blit|noinv] [--ms]

The frame: a 4x RGBA16F multisampled linear frame (sky, ground, a grey wall, a 0.4 px bright wire, three lamps at 30),
every sample its own value at a rotated-grid position. The display is cell_hdr's per-sample tone map: the mean over
samples of T(sample), T = cellCurveW (vanilla, E = 0) of 2x the light, then 1/2.2 (postfx_linear.curve).

MSAALIN (src/gl/cellpost.cpp step 4b, res/shaders/cell_post_mscopy.frag): the linear post's picture B back into the
multisampled frame through the dual-source blend s -> s w + (B - w R), R = the frame's resolve (a blit), w = min(1, B/R).
The left half is still (B = R: the post copied it through), the right half blurred (B = a 9 px box of R).
  still exact   still samples after the copy == before, max diff 0           red allb (the old copy: B in every sample)
  still toned   still pixels' tone-mapped display vs the frame's own: <= 0.01 steps            red allb
  mean          every pixel's sample mean == B: max relative error <= 2e-3 (half floats)      red noadd (no B - wR)
  non-negative  no sample below 0                                                             red additive (w = 1)
TAALINEAR (src/gl/gametaa.cpp step 1, res/shaders/taa2_msresolve.frag): the samples into one value for the resolve.
  karis edge    on edge pixels, |T(resolve) - mean T(samples)|: mean <= 0.5 x the blit's       red blit
  karis flat    pixels whose samples are all equal: resolve == the sample, relative <= 2e-3    red noinv (no inverse)
  --ms          1080p 4x GPU ms of the copy and of the resolve (GL timestamps, median of 30): each <= 0.5 ms
Exit 0 only when every gate passes; the last line is POSTFX MSAA RESULT PASS/FAIL.
"""
import ctypes, os, re, sys, tempfile
import numpy as np

TREE = os.environ.get('TREE', os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')))
sys.path.insert(0, TREE + '/tests/spells')
sys.path.insert(0, TREE + '/tests/postfx')
import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402
import postfx_gates as P        # noqa: E402
from postfx_linear import curve  # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
RGBA16F, RGBA32F = 0x881A, 0x8814
T2D, T2DA, T2DMS = 0x0DE1, 0x8C1A, 0x9100
FB, RFB, DFB, COL0 = 0x8D40, 0x8CA8, 0x8CA9, 0x8CE0
NS = 4
OFFS = ((-2, -6), (6, -2), (-6, 2), (2, 6))   # the 4x rotated grid, in 1/16 px

HELPERS = {
    'ms_write.frag': """#version 410 core
uniform sampler2DArray src;
out vec4 o;
void main() { o = texelFetch( src, ivec3( ivec2( gl_FragCoord.xy ), gl_SampleID ), 0 ); }
""",
    'ms_read.frag': """#version 410 core
uniform sampler2DMS ms;
uniform int s;
out vec4 o;
void main() { o = texelFetch( ms, ivec2( gl_FragCoord.xy ), s ); }
""",
    'ms_all.frag': """#version 410 core
uniform sampler2D srcTex;
out vec4 o;
void main() { o = texelFetch( srcTex, ivec2( gl_FragCoord.xy ), 0 ); }
""",
}

PATCH = {   # red -> (shader, old, new): each red breaks one claim of the shipped shader
    'noadd': ('cell_post_mscopy.frag', 'addOut = vec4( b - w * r, 0.0 );', 'addOut = vec4( 0.0 );'),
    'additive': ('cell_post_mscopy.frag', 'addOut = vec4( b - w * r, 0.0 );', 'w = vec3( 1.0 ); addOut = vec4( b - w * r, 0.0 );'),
    'noinv': ('taa2_msresolve.frag', r'fragColor = vec4( t / ( ( 1.0 - max( t.r, max( t.g, t.b ) ) ) * max( msExpose, 1e-6 ) ), a * inv );',
              'fragColor = vec4( t, a * inv );'),
}


def shader_files():
    """the tree's two shaders (a red patches its copy) and the rig's helpers, in a scratch folder"""
    d = tempfile.mkdtemp(prefix='msaa_', dir=os.environ.get('TMPDIR') or None)
    for name, src in HELPERS.items():
        open(os.path.join(d, name), 'w', newline='\n').write(src)
    for f in ('cell_post_mscopy.frag', 'taa2_msresolve.frag', 'cell_ao.vert'):
        s = open(os.path.join(P.SD, f), newline='').read()
        if RED in PATCH and PATCH[RED][0] == f:
            s2 = s.replace(PATCH[RED][1], PATCH[RED][2])
            assert s2 != s, 'red %s: anchor not found' % RED
            s = s2
        open(os.path.join(d, f), 'w', newline='').write(s)
    return d


class Ms:
    def __init__(self):
        self.g = C.Gl()
        d = shader_files()
        vert = os.path.join(d, 'cell_ao.vert')
        self.p = {k: C.build(d, k, [vert, os.path.join(d, k + '.frag')])
                  for k in ('ms_write', 'ms_read', 'ms_all', 'cell_post_mscopy', 'taa2_msresolve')}
        self.tex3d = S.fn('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint,
                          GLenum, GLenum, ctypes.c_void_p)
        self.texms = S.fn('glTexImage2DMultisample', None, GLenum, ctypes.c_int, GLenum, ctypes.c_int, ctypes.c_int, ctypes.c_ubyte)
        self.blendfs = S.fn('glBlendFuncSeparate', None, GLenum, GLenum, GLenum, GLenum)
        self.blendeq = S.fn('glBlendEquation', None, GLenum)
        self.blit = S.fn('glBlitFramebuffer', None, *([GLint] * 8), ctypes.c_uint, GLenum)
        self.drawbufs = S.fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
        self.q = GLuint(); S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(self.q))
        self.bq = S.fn('glBeginQuery', None, GLenum, GLuint)
        self.eq = S.fn('glEndQuery', None, GLenum)
        self.gq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
        fb = GLuint(); self.g.genfb(1, ctypes.byref(fb)); self.fb = fb.value
        fb2 = GLuint(); self.g.genfb(1, ctypes.byref(fb2)); self.fb2 = fb2.value
        self.texs = []

    def _new(self, target):
        t = GLuint(); gl.glGenTextures(1, ctypes.byref(t)); gl.glBindTexture(target, t)
        self.texs.append(t.value)
        return t.value

    def tex(self, w, h, internal, data=None):
        t = self._new(T2D)
        buf = None
        if data is not None:
            a = np.zeros((h, w, 4), np.float32); a[..., :data.shape[2]] = data
            buf = np.ascontiguousarray(a[::-1], np.float32).ctypes.data_as(ctypes.c_void_p)
        gl.glTexImage2D(T2D, 0, internal, w, h, 0, 0x1908, 0x1406, buf)
        for k in (0x2801, 0x2800):
            gl.glTexParameteri(T2D, k, P.NEAREST)
        return t

    def mstex(self, w, h, samples):
        """(h, w, samples, 4) -> a 4x RGBA16F multisampled texture holding each sample's own value"""
        t = self._new(T2DMS)
        self.texms(T2DMS, NS, RGBA16F, w, h, 1)
        a = np.ascontiguousarray(np.transpose(samples, (2, 0, 1, 3))[:, ::-1], np.float32)
        ta = self._new(T2DA)
        self.tex3d(T2DA, 0, RGBA32F, w, h, NS, 0, 0x1908, 0x1406, a.ctypes.data_as(ctypes.c_void_p))
        for k in (0x2801, 0x2800):
            gl.glTexParameteri(T2DA, k, P.NEAREST)
        self.draw_ms('ms_write', t, w, h, (('src', T2DA, ta),), {})
        return t

    def free(self):
        n = len(self.texs)
        gl.glDeleteTextures(n, (GLuint * n)(*self.texs))
        self.texs = []

    def _bind(self, prog, texs, uni):
        g = self.g
        p = self.p[prog]
        g.use(p)
        for unit, (name, target, t) in enumerate(texs):
            g.active(0x84C0 + unit); gl.glBindTexture(target, t); g.u1i(g.loc(p, name.encode()), unit)
        for k, v in uni.items():
            l = g.loc(p, k.encode())
            if l >= 0:
                (g.u1i if isinstance(v, int) else g.u1f)(l, v)

    def draw(self, prog, dst, w, h, texs, uni):
        self.g.bindfb(FB, self.fb); self.g.fbtex(FB, COL0, T2D, dst, 0)
        self.drawbufs(1, (GLenum * 1)(COL0)); gl.glViewport(0, 0, w, h)
        self._bind(prog, texs, uni)
        gl.glDrawArrays(5, 0, 4)

    def draw_ms(self, prog, ms, w, h, texs, uni, blend=False):
        self.g.bindfb(FB, self.fb); self.g.fbtex(FB, COL0, T2DMS, ms, 0)
        self.drawbufs(1, (GLenum * 1)(COL0)); gl.glViewport(0, 0, w, h)
        self._bind(prog, texs, uni)
        if blend:   # cellpost.cpp 4b: ADD, (ONE, SRC1_COLOR) colour, (ZERO, ONE) alpha
            gl.glEnable(0x0BE2); self.blendeq(0x8006); self.blendfs(1, 0x88F9, 0, 1)
        gl.glDrawArrays(5, 0, 4)
        if blend:
            gl.glDisable(0x0BE2)

    def read(self, t, w, h):
        self.g.bindfb(FB, self.fb); self.g.fbtex(FB, COL0, T2D, t, 0)
        gl.glReadBuffer(COL0)
        buf = (ctypes.c_float * (w * h * 4))()
        gl.glReadPixels(0, 0, w, h, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(h, w, 4)[::-1].astype(np.float64)

    def samples(self, ms, w, h):
        out = self.tex(w, h, RGBA32F)
        r = []
        for s in range(NS):
            self.draw('ms_read', out, w, h, (('ms', T2DMS, ms),), {'s': s})
            r.append(self.read(out, w, h))
        return np.stack(r, axis=2)   # (h, w, samples, 4)

    def resolve_blit(self, ms, w, h):
        """the frame's blit to one sample a pixel (gametaa.cpp's old step 1, cellpost.cpp's step 1)"""
        t = self.tex(w, h, RGBA16F)
        g = self.g
        g.bindfb(RFB, self.fb2); g.fbtex(RFB, COL0, T2DMS, ms, 0); gl.glReadBuffer(COL0)
        g.bindfb(DFB, self.fb); g.fbtex(DFB, COL0, T2D, t, 0); self.drawbufs(1, (GLenum * 1)(COL0))
        self.blit(0, 0, w, h, 0, 0, w, h, 0x4000, P.NEAREST)
        return t

    def resolve_karis(self, ms, w, h, expose):
        t = self.tex(w, h, RGBA16F)
        self.draw('taa2_msresolve', t, w, h, (('msTex', T2DMS, ms),), {'msSamples': NS, 'msExpose': float(expose)})
        return t

    def copy_back(self, ms, b, r, w, h):
        """cellpost.cpp 4b (or, red allb, the old copy of the picture into every sample)"""
        if RED == 'allb':
            self.draw_ms('ms_all', ms, w, h, (('srcTex', T2D, b),), {})
        else:
            self.draw_ms('cell_post_mscopy', ms, w, h, (('srcTex', T2D, b), ('refTex', T2D, r)), {}, blend=True)

    def timed(self, fn, n=32):
        times = []
        for _ in range(n):
            self.bq(0x88BF, self.q); fn(); self.eq(0x88BF)
            ns = ctypes.c_uint64(); self.gq(self.q, 0x8866, ctypes.byref(ns)); times.append(ns.value / 1e6)
        return float(np.median(times[2:]))


def scene(w, h):
    """(h, w, samples, 4): each sample the analytic picture at its own position"""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    out = np.zeros((h, w, NS, 4)); out[..., 3] = 1
    lamps = ((0.15 * w, 0.62 * h), (0.62 * w, 0.58 * h), (0.88 * w, 0.40 * h))
    for s, (ox, oy) in enumerate(OFFS):
        X, Y = xx + 0.5 + ox / 16.0, yy + 0.5 + oy / 16.0
        c = np.zeros((h, w, 3))
        sky = Y < 0.45 * h + 0.12 * (X - 0.5 * w)
        c[sky] = (0.6, 0.65, 0.75); c[~sky] = 0.03
        wall = (X > 0.2 * w) & (X < 0.35 * w) & (Y > 0.25 * h + 0.3 * (X - 0.2 * w)) & (Y < 0.8 * h)
        c[wall] = 0.18
        wire = (np.abs(Y - (0.2 * h + 0.05 * X)) < 0.2) & (X > 0.05 * w) & (X < 0.95 * w)
        c[wire] = 8.0
        for lx, ly in lamps:
            c[(X - lx) ** 2 + (Y - ly) ** 2 < 2.3 ** 2] = 30.0
        out[:, :, s, :3] = c
    return out


def toned(smp):
    """cell_hdr's display: the mean over samples of the tone map of each"""
    return curve(smp[..., :3]).mean(axis=2)


def box9(img):
    k = np.ones(9) / 9
    return np.apply_along_axis(lambda r: np.convolve(np.pad(r, 4, mode='edge'), k, 'valid'), 1, img)


def main():
    R = Ms()
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        fails += 0 if ok else 1
        print('%-13s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)

    w, h = 320, 180
    sc = scene(w, h)
    ms = R.mstex(w, h, sc)
    orig = R.samples(ms, w, h)   # what the frame holds (half floats)
    rt = R.resolve_blit(ms, w, h)
    rr = R.read(rt, w, h)
    still = np.zeros((h, w), bool); still[:, :w // 2] = True
    b = rr.copy(); b[~still, :3] = box9(rr)[~still, :3]
    bt = R.tex(w, h, RGBA16F, b)
    bb = R.read(bt, w, h)        # B as the stage holds it (half floats)
    R.copy_back(ms, bt, rt, w, h)
    got = R.samples(ms, w, h)
    edge = (orig[..., :3].max(axis=2) - orig[..., :3].min(axis=2)).max(axis=2) > 1e-6
    tag = ' (RED: %s)' % RED if RED else ''

    e = np.abs(got - orig)[still].max()
    gate('still exact', e == 0, 'max |sample after - before| %.3g (bar 0), %d still edge pixels%s'
         % (e, int((edge & still).sum()), tag))
    d = np.abs(toned(got) - toned(orig)).max(axis=2) * 255
    gate('still toned', d[still].max() <= 0.01, 'max %.4f steps on still pixels (bar 0.01), mean on still edges %.4f%s'
         % (d[still].max(), d[still & edge].mean(), tag))
    mean = got[..., :3].mean(axis=2)
    rel = (np.abs(mean - bb[..., :3]) / np.maximum(bb[..., :3], 1e-3)).max()
    gate('mean', rel <= 2e-3, 'max relative |sample mean - B| %.2e (bar 2e-3)%s' % (rel, tag))
    mn = got[..., :3].min()
    gate('non-negative', mn >= 0, 'min sample %.4g (bar 0)%s' % (mn, tag))
    blur = ~still & edge
    dv = (np.abs(toned(got) - curve(bb[..., :3])).max(axis=2) * 255)[blur]
    print('blurred edges (no bar): the display differs from the old copy\'s T(B) by %.3f steps mean, p99 %.3f'
          ' (the samples\' spread the blur kept)' % (dv.mean(), np.percentile(dv, 99)), flush=True)

    # TAALINEAR: a fresh frame (the copy above wrote into ms)
    R.free()
    ms = R.mstex(w, h, sc)
    orig = R.samples(ms, w, h)
    ref = toned(orig)
    blit = R.read(R.resolve_blit(ms, w, h), w, h)[..., :3]
    eb = (np.abs(curve(blit) - ref).max(axis=2) * 255)[edge]
    for ex in (1.0, 0.25, 4.0):
        kr = blit if RED == 'blit' else R.read(R.resolve_karis(ms, w, h, ex), w, h)[..., :3]
        ek = (np.abs(curve(kr) - ref).max(axis=2) * 255)[edge]
        if ex == 1.0:
            gate('karis edge', ek.mean() <= 0.5 * eb.mean(),
                 'edge |T(resolve) - mean T(samples)| mean %.3f steps (p99 %.3f) vs the blit %.3f (p99 %.3f); bar 0.5 x'
                 ' the blit, ratio %.3f%s' % (ek.mean(), np.percentile(ek, 99), eb.mean(), np.percentile(eb, 99),
                                              ek.mean() / eb.mean(), tag))
            flat = ~edge
            relk = (np.abs(kr - orig[:, :, 0, :3]) / np.maximum(orig[:, :, 0, :3], 1e-3))[flat].max()
            gate('karis flat', relk <= 2e-3, 'flat pixels: max relative |resolve - sample| %.2e (bar 2e-3)%s' % (relk, tag))
        else:
            print('karis edge at expose %.2f (exposure off by %.0fx, no bar): mean %.3f steps, ratio %.3f'
                  % (ex, max(ex, 1 / ex), ek.mean(), ek.mean() / eb.mean()), flush=True)
    R.free()

    if '--ms' in sys.argv:
        W, H = 1920, 1080
        big = np.repeat(np.repeat(sc, 6, axis=0), 6, axis=1)[:H, :W]
        ms = R.mstex(W, H, big)
        rt = R.resolve_blit(ms, W, H)
        tc = R.timed(lambda: R.copy_back(ms, rt, rt, W, H))
        kt = R.tex(W, H, RGBA16F)
        tk = R.timed(lambda: R.draw('taa2_msresolve', kt, W, H, (('msTex', T2DMS, ms),), {'msSamples': NS, 'msExpose': 1.0}))
        bt2 = R.tex(W, H, RGBA16F)

        def blit_once():
            g = R.g
            g.bindfb(RFB, R.fb2); g.fbtex(RFB, COL0, T2DMS, ms, 0); gl.glReadBuffer(COL0)
            g.bindfb(DFB, R.fb); g.fbtex(DFB, COL0, T2D, bt2, 0); R.drawbufs(1, (GLenum * 1)(COL0))
            R.blit(0, 0, W, H, 0, 0, W, H, 0x4000, P.NEAREST)
        tb = R.timed(blit_once)
        gate('ms', tc <= 0.5 and tk <= 0.5, '1080p 4x: copy back %.3f ms, karis resolve %.3f ms (the blit it replaces %.3f ms);'
             ' bar 0.5 each' % (tc, tk, tb))
        R.free()

    print('POSTFX MSAA RESULT %s (%d fail)%s' % ('PASS' if not fails else 'FAIL', fails, ' red=' + RED if RED else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
