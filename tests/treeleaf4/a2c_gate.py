"""Lane TREELEAF4 (FIX1031A, 2026-10-31), A2CCOLOUR1: should alpha-tested cell shapes draw with alpha-to-coverage in the
COLOUR pass (casters keep the test: TREELEAF3 a2c_refusal.cpp)? Measured offline through the real driver in a hidden GL
4.1 context, no NifSkope: a leaf-like alpha field (64x64 texels, bilinear, magnified 4-8 px a texel, threshold 0.5)
drawn white on black into a 4x MSAA target (the cell view's default, glview.cpp Msaa Samples 2 -> 4) and resolved.

Engines:  test   today: `discard a < t` (fo4_default.frag), one coverage decision per pixel
          a2c    alpha-to-coverage on the raw alpha
          sharp  alpha-to-coverage on ( a - t ) / fwidth( a ) + 0.5 (the usual sharpened form), clamped
References:  ideal  the alpha test's coverage averaged over 16x16 sub-samples a pixel (perfect AA of the test)
             game   the game's look: 1 sample a pixel (the game's colour targets are single-sample: ww-taa-offline-gate,
                    AirOn.rdc pair, RGBA8 / R11G11B10F at the frame size) alpha-tested, jittered (Halton 2,3 as the
                    game's TAA), through the REAL game_taa.frag (TAA3's rig), 48 still frames, the last one
Metrics (edge pixels = 0 < ideal < 1, or any engine/reference not 0/1 there):
  err ideal / err game   mean |coverage - reference| over edge pixels
  shimmer                the texture panned 1/8 px a frame over 16 frames: mean temporal std over edge pixels
  levels                 the distinct coverage values a resolved pixel takes (5 = no dither)
  ms                     1920x1080, 8 full-screen alpha layers, depth-tested, GL_TIME_ELAPSED median of 20
Pre-registered gate (written before the first run): A2C ships ON only if `sharp` (1) is at least 30 % closer than `test`
to the GAME reference, (2) is at least 30 % closer than `test` to the ideal, (3) shimmers no more than `test`, and
(4) costs <= +0.10 ms over `test`; else refused with these numbers. rc 0 always (a measurement); last line
`A2C RESULT SHIP` or `A2C RESULT REFUSE (<which bars failed>)`.

usage: python -I tests/treeleaf4/a2c_gate.py [<out.txt>]   (env RIG=<taa3_gates.py>)"""
import ctypes
import math
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
os.environ['TREE'] = TREE
RIG = os.environ.get('RIG', 'E:/Projects/Claude/notes/taa3/sim/taa3_gates.py')
_src = open(RIG, encoding='utf-8').read().rstrip()
R = {'__name__': 'taa3_rig', '__file__': RIG}
exec(compile(_src[:-len('main()')], RIG, 'exec'), R)
S = R['S']
gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
W, H = R['W'], R['H']
rig = R['Rig']()	# makes the context
F = {}


def fn(name, res, *a):
    if name not in F:
        F[name] = S.fn(name, res, *a)
    return F[name]


VS = b"""#version 410 core
layout ( location = 0 ) in vec3 p;
uniform vec2 uvScale;
uniform vec2 shift;
uniform float z;
out vec2 uv;
void main() { uv = ( p.xy * 0.5 + 0.5 ) * uvScale + shift; gl_Position = vec4( p.xy, z, 1.0 ); }
"""
FS = b"""#version 410 core
uniform sampler2D tex;
uniform int mode;
uniform float t;
in vec2 uv;
out vec4 o;
void main()
{
	float a = texture( tex, uv ).a;
	if ( mode == 0 ) {
		if ( a < t )
			discard;
		o = vec4( 1.0 );
	} else if ( mode == 1 ) {
		o = vec4( 1.0, 1.0, 1.0, a );
	} else {
		o = vec4( 1.0, 1.0, 1.0, clamp( ( a - t ) / max( fwidth( a ), 1e-4 ) + 0.5, 0.0, 1.0 ) );
	}
}
"""


def program():
    p = fn('glCreateProgram', GLuint)()
    for src, kind in ((VS, 0x8B31), (FS, 0x8B30)):
        s = fn('glCreateShader', GLuint, GLenum)(kind)
        b = ctypes.c_char_p(src)
        fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)(s, 1, ctypes.byref(b), None)
        fn('glCompileShader', None, GLuint)(s)
        ok = GLint()
        fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(s, 0x8B81, ctypes.byref(ok))
        assert ok.value, 'compile'
        fn('glAttachShader', None, GLuint, GLuint)(p, s)
    fn('glLinkProgram', None, GLuint)(p)
    return p


PROG = program()
loc = lambda n: fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)(PROG, n.encode())
u1i = fn('glUniform1i', None, GLint, GLint)
u1f = fn('glUniform1f', None, GLint, ctypes.c_float)
u2f = fn('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
genrb = fn('glGenRenderbuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindrb = fn('glBindRenderbuffer', None, GLenum, GLuint)
rbms = fn('glRenderbufferStorageMultisample', None, GLenum, ctypes.c_int, GLenum, ctypes.c_int, ctypes.c_int)
fbrb = fn('glFramebufferRenderbuffer', None, GLenum, GLenum, GLenum, GLuint)
blit = fn('glBlitFramebuffer', None, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, ctypes.c_uint, GLenum)
genfb = fn('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindfb = fn('glBindFramebuffer', None, GLenum, GLuint)
fbtex = fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
gl.glTexImage2D.argtypes = [GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p]
gl.glReadPixels.argtypes = [GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p]

# the alpha field: smooth blobs (a leaf atlas's alpha after mip / filtering), 64x64 texels, RGBA8 (8-bit alpha)
rng = np.random.default_rng(23)
N = 64
yy, xx = np.mgrid[0:N, 0:N] + 0.5
A = np.zeros((N, N))
for _ in range(14):
    cx, cy, r = rng.random() * N, rng.random() * N, 4 + rng.random() * 9
    A = np.maximum(A, np.clip(1.25 - np.hypot(((xx - cx + N / 2) % N) - N / 2, ((yy - cy + N / 2) % N) - N / 2) / r, 0, 1))
A8 = np.round(np.clip(A, 0, 1) * 255).astype(np.uint8)
tex_data = np.zeros((N, N, 4), np.uint8)
tex_data[..., :3] = 255
tex_data[..., 3] = A8
T = GLuint()
gl.glGenTextures(1, ctypes.byref(T))
gl.glBindTexture(0x0DE1, T)
gl.glTexImage2D(0x0DE1, 0, 0x8058, N, N, 0, 0x1908, 0x1401, tex_data.ctypes.data_as(ctypes.c_void_p))
for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x2901), (0x2803, 0x2901)):
    gl.glTexParameteri(0x0DE1, k, v)	# LINEAR, REPEAT


def alpha_at(u, v):
    """GL's bilinear REPEAT sample of the 8-bit alpha at texture coords (u, v) (the numpy twin)"""
    x, y = u * N - 0.5, v * N - 0.5
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0
    a8 = A8.astype(np.float64) / 255.0
    g = lambda i, j: a8[j % N, i % N]
    return (g(x0, y0) * (1 - fx) * (1 - fy) + g(x0 + 1, y0) * fx * (1 - fy) + g(x0, y0 + 1) * (1 - fx) * fy
            + g(x0 + 1, y0 + 1) * fx * fy)


def uv_of(px, py, scale, shift):
    """pixel-space points (x right from the left, y up from the bottom, GL convention) -> uv, as VS does"""
    return px / W * scale[0] + shift[0], py / H * scale[1] + shift[1]


def ideal(scale, shift, t, ss=16):
    acc = np.zeros((H, W))
    for ky in range(ss):
        for kx in range(ss):
            px = np.arange(W)[None, :] + (kx + 0.5) / ss
            py = np.arange(H)[:, None] + (ky + 0.5) / ss
            u, v = uv_of(px, py, scale, shift)
            acc += alpha_at(u, v) >= t
    return acc[::-1] / (ss * ss)	# rows top-down


class Target:
    def __init__(self, w, h, samples):
        self.w, self.h = w, h
        fb, rb, db = GLuint(), GLuint(), GLuint()
        genfb(1, ctypes.byref(fb))
        bindfb(0x8D40, fb)
        genrb(1, ctypes.byref(rb))
        bindrb(0x8D41, rb)
        rbms(0x8D41, samples, 0x8058, w, h)
        fbrb(0x8D40, 0x8CE0, 0x8D41, rb)
        genrb(1, ctypes.byref(db))
        bindrb(0x8D41, db)
        rbms(0x8D41, samples, 0x81A6, w, h)	# DEPTH_COMPONENT24
        fbrb(0x8D40, 0x8D00, 0x8D41, db)
        self.fb = fb.value
        rfb, rt = GLuint(), GLuint()
        genfb(1, ctypes.byref(rfb))
        bindfb(0x8D40, rfb)
        gl.glGenTextures(1, ctypes.byref(rt))
        gl.glBindTexture(0x0DE1, rt)
        gl.glTexImage2D(0x0DE1, 0, 0x8058, w, h, 0, 0x1908, 0x1401, None)	# RGBA8: a resolve blit keeps the format
        fbtex(0x8D40, 0x8CE0, 0x0DE1, rt, 0)
        self.rfb = rfb.value


def draw(tg, mode, scale, shift, t, layers=1, resolve=True):
    bindfb(0x8D40, tg.fb)
    gl.glViewport(0, 0, tg.w, tg.h)
    gl.glClearColor(0, 0, 0, 0)
    gl.glClear(0x4100)
    gl.glEnable(0x0B71)	# depth test
    gl.glDepthFunc(0x0201)	# LESS
    gl.glEnable(0x809D)	# MULTISAMPLE
    if mode:
        gl.glEnable(0x809E)	# SAMPLE_ALPHA_TO_COVERAGE
    rig.g.use(PROG)
    fn('glActiveTexture', None, GLenum)(0x84C0)
    gl.glBindTexture(0x0DE1, T)
    u1i(loc('tex'), 0)
    u1i(loc('mode'), mode)
    u1f(loc('t'), t)
    for k in range(layers):
        u2f(loc('uvScale'), *scale)
        u2f(loc('shift'), shift[0] + 0.137 * k, shift[1] + 0.071 * k)
        u1f(loc('z'), 0.9 - 0.1 * k)	# back to front: every layer passes the depth test
        gl.glDrawArrays(5, 0, 4)
    gl.glDisable(0x809E)
    if not resolve:
        return None
    bindfb(0x8CA8, tg.fb)
    bindfb(0x8CA9, tg.rfb)
    blit(0, 0, tg.w, tg.h, 0, 0, tg.w, tg.h, 0x4000, 0x2600)
    bindfb(0x8D40, tg.rfb)
    gl.glReadBuffer(0x8CE0)
    buf = (ctypes.c_float * (tg.w * tg.h * 4))()
    gl.glReadPixels(0, 0, tg.w, tg.h, 0x1908, 0x1406, buf)
    return np.frombuffer(buf, np.float32).reshape(tg.h, tg.w, 4)[::-1, :, 0].astype(np.float64)


def game_reference(scale, shift, t):
    """1 sample a pixel at p - jitter, alpha-tested, through the real game_taa.frag (the rig's 'game' engine)"""
    def frame(f, jx, jy):
        px = np.arange(W)[None, :] + 0.5 - jx
        py = (H - 1 - np.arange(H))[:, None] + 0.5 - jy	# rows top-down; the rig's y is up
        u, v = uv_of(px, py, scale, shift)
        c = (alpha_at(u, v) >= t).astype(np.float64)
        return np.repeat(c[..., None], 3, 2), 0.0
    R['frame_fn'] = frame
    outs = list(rig.run('game', 48, W, H, 0))
    return R['Yf'](outs[-1])


def main(a):
    out = a[0] if a else ''
    lines = []
    say = lambda s: (print(s), lines.append(s), sys.stdout.flush())
    tg = Target(W, H, 4)
    t = 0.5
    say('TREELEAF4 A2C gate: %dx%d, 4x MSAA, alpha field %dx%d texels, threshold %.2f' % (W, H, N, N, t))
    res = {}
    for mag in (4.0, 8.0):
        scale = (W / (N * mag), H / (N * mag))
        shift = (0.013, 0.029)
        ref = ideal(scale, shift, t)
        game = game_reference(scale, shift, t)
        pics = {m: draw(tg, i, scale, shift, t) for i, m in enumerate(('test', 'a2c', 'sharp'))}
        edge = (ref > 0) & (ref < 1)
        for p in (pics['test'], pics['sharp'], game):	# not plain a2c: it half-covers every blob's inside
            edge |= (p > 0.001) & (p < 0.999)
        say('\n== %.0f px a texel: %d edge pixels ==' % (mag, int(edge.sum())))
        say('%-6s %10s %10s %8s %s' % ('engine', 'err ideal', 'err game', 'shimmer', 'levels'))
        # the slow pan: 1/8 px a frame, 16 frames
        for m, i in (('test', 0), ('a2c', 1), ('sharp', 2)):
            seq = []
            for f in range(16):
                sh = (shift[0] + f / 8.0 / W * scale[0], shift[1])
                seq.append(draw(tg, i, scale, sh, t))
            st = np.stack(seq)
            moving_edge = edge.copy()	# the still edge, plus where this engine's own sequence is partial
            if m != 'a2c':
                for s_ in seq:
                    moving_edge |= (s_ > 0.001) & (s_ < 0.999)
            shim = float(st.std(0)[moving_edge].mean())
            p = pics[m]
            e_i = float(np.abs(p - ref)[edge].mean())
            e_g = float(np.abs(p - game)[edge].mean())
            lev = np.unique(np.round(p[edge] * 64) / 64)
            res[(mag, m)] = (e_i, e_g, shim)
            say('%-6s %10.4f %10.4f %8.4f %d (%s)' % (m, e_i, e_g, shim, len(lev), ' '.join('%.3g' % x for x in lev[:9])))
        say('%-6s %10.4f %10s %8s' % ('game', float(np.abs(game - ref)[edge].mean()), '-', '-'))
    # cost at 1080p: 8 layers
    big = Target(1920, 1080, 4)
    q = GLuint()
    fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(q))
    bq, eq = fn('glBeginQuery', None, GLenum, GLuint), fn('glEndQuery', None, GLenum)
    gq = fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
    ms = {}
    for m, i in (('test', 0), ('a2c', 1), ('sharp', 2)) * 2:	# twice, interleaved: the second set counts
        ts = []
        for _ in range(24):
            bq(0x88BF, q)
            draw(big, i, (1920 / (N * 4), 1080 / (N * 4)), (0.01, 0.02), t, layers=8, resolve=False)
            eq(0x88BF)
            ns = ctypes.c_uint64()
            gq(q, 0x8866, ctypes.byref(ns))
            ts.append(ns.value / 1e6)
        ms[m] = float(np.median(ts[4:]))
    say('\n== GPU ms, 1920x1080 4x MSAA, 8 full-screen alpha layers (median of 20) ==')
    for m in ('test', 'a2c', 'sharp'):
        say('%-6s %.3f ms (%+.3f vs test)' % (m, ms[m], ms[m] - ms['test']))
    fails = []
    for mag in (4.0, 8.0):
        ti, tgm, tsh = res[(mag, 'test')]
        si, sgm, ssh = res[(mag, 'sharp')]
        if not sgm <= 0.7 * tgm:
            fails.append('game %.0fpx %.4f vs test %.4f' % (mag, sgm, tgm))
        if not si <= 0.7 * ti:
            fails.append('ideal %.0fpx %.4f vs test %.4f' % (mag, si, ti))
        if not ssh <= tsh:
            fails.append('shimmer %.0fpx %.4f vs test %.4f' % (mag, ssh, tsh))
    if not ms['sharp'] - ms['test'] <= 0.10:
        fails.append('cost %+.3f ms' % (ms['sharp'] - ms['test']))
    say('\nA2C RESULT %s' % ('SHIP' if not fails else 'REFUSE (%s)' % '; '.join(fails)))
    if out:
        open(out, 'w').write('\n'.join(lines) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
