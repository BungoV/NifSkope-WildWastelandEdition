"""lane POSTFX2: offline gate for the fused depth of field + motion blur pass (res/shaders/game_dofmblur.frag) against
the two-pass chain cellpost.cpp issued before it (game_dof.frag into an RGBA16F target, the copy back, game_mblur.frag).
The REAL shaders of the tree through the real driver in a hidden GL 4.1 context; no NifSkope window.

    python -I tests/postfx/postfx_fuse.py [--red nearest] [--ms]

Gates (exit 0 only when every gate passes; the red must make the gate fail):
  fuse still    no motion (every pixel takes the blur's early copy): fused == two-pass, max diff 0
  fuse moving   a turning camera with a mover of its own over a picture with focus, near and far blur and sky:
                max |fused - two-pass| <= 1/255 (display values)
  --red nearest the fused pass reads the composite at the nearest texel instead of the filter's four: must fail
  --ms          GPU ms at 1920 x 1080 (GL_TIME_ELAPSED median of 30): two-pass (composite, copy, blur), and fused;
                still and moving. The fused pass must be faster in both to ship ON.
"""
import ctypes, os, re, shutil, sys, tempfile
import numpy as np

TREE = os.environ.get('TREE', os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')))
sys.path.insert(0, TREE + '/tests/spells')
sys.path.insert(0, TREE + '/tests/postfx')
import shader_link_check as S   # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'features', 'sky'))  # sky helpers
import atmo_lut_check as C      # noqa: E402
import postfx_gates as P        # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
RGBA16F, RG16F, R32F = 0x881A, 0x822F, 0x822E
NEAREST, LINEAR = P.NEAREST, P.LINEAR
ZN, ZF = 1.0, 10000.0


def shader_dir():
    """the tree's shaders; the red swaps the fused pass's filtered read for a nearest one in a scratch copy"""
    if RED != 'nearest':
        return P.SD
    d = tempfile.mkdtemp(prefix='fusered_', dir=os.environ.get('TMPDIR') or None)
    for f in os.listdir(P.SD):
        if f.endswith(('.frag', '.vert', '.prog', '.glsl')):
            shutil.copy(os.path.join(P.SD, f), d)
    p = os.path.join(d, 'game_dofmblur.frag')
    s = open(p, newline='').read()
    s2 = re.sub(r'(vec3 dofAt\( vec2 uv, vec2 size \)\s*\{)', r'\1\n\treturn dofTexel( ivec2( uv * size ), size ).rgb;', s, 1)
    assert s2 != s, 'red: dofAt not found'
    open(p, 'w', newline='').write(s2)
    return d


class Fuse(P.Rig):
    def __init__(self, sd):
        P.SD = sd
        self.g = C.Gl()
        self.p = {k: C.build(sd, k + '.prog') for k in ('game_dof', 'game_mblur', 'game_dofmblur')}
        self.um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
        self.drawbufs = S.fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
        self.u4 = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
        self.q = GLuint(); S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(self.q))
        self.bq = S.fn('glBeginQuery', None, GLenum, GLuint)
        self.eq = S.fn('glEndQuery', None, GLenum)
        self.gq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
        self.blit = S.fn('glBlitFramebuffer', None, *([GLint] * 8), ctypes.c_uint, GLenum)
        fb = GLuint(); self.g.genfb(1, ctypes.byref(fb)); self.fb = fb.value
        fb2 = GLuint(); self.g.genfb(1, ctypes.byref(fb2)); self.fb2 = fb2.value
        self.texs = []

    def copy(self, src, dst, w, h):
        g = self.g
        g.bindfb(0x8CA8, self.fb2); g.fbtex(0x8CA8, 0x8CE0, 0x0DE1, src, 0); gl.glReadBuffer(0x8CE0)
        g.bindfb(0x8CA9, self.fb); g.fbtex(0x8CA9, 0x8CE0, 0x0DE1, dst, 0)
        self.drawbufs(1, (GLenum * 1)(0x8CE0))
        self.blit(0, 0, w, h, 0, 0, w, h, 0x4000, NEAREST)

    def run(self, w, h, col, blur, depth, mv, dof, mb, mode, timed=False):
        """mode 'two' = cellpost.cpp's chain before the fuse (composite -> out, out copied to cur, blur cur -> out);
        'fused' = the one pass. Returns the RGBA16F output (or the GPU ms)"""
        qw, qh = (w + 3) // 4, (h + 3) // 4
        tc = self.tex(w, h, RGBA16F, LINEAR, col)
        tb = self.tex(qw, qh, RGBA16F, LINEAR, blur)
        td = self.tex(w, h, R32F, NEAREST, depth[..., None])
        tv = self.tex(w, h, RG16F, LINEAR, mv)
        out = self.tex(w, h, RGBA16F, LINEAR)
        cur2 = self.tex(w, h, RGBA16F, LINEAR)
        dofu = {'c0': dof[0], 'c1': dof[1], 'c2': dof[2], 'c3': dof[3], 'farOnly': 0, 'dofRed': 0}
        times = []
        for it in range(32 if timed else 1):
            if timed:
                self.bq(0x88BF, self.q)
            if mode == 'two':
                self.pass_('game_dof', out, w, h, (('t0', tc), ('t1', tb), ('t2', td)), dofu)
                self.copy(out, cur2, w, h)
                self.pass_('game_mblur', out, w, h, (('t0', cur2), ('t1', tv)), {'c0': mb[0], 'c1': mb[1]})
            else:
                self.pass_('game_dofmblur', out, w, h, (('t0', tc), ('t1', tv), ('t2', tb), ('t3', td)),
                           {'dc0': dof[0], 'dc1': dof[1], 'dc2': dof[2], 'dc3': dof[3], 'mc0': mb[0], 'mc1': mb[1],
                            'farOnly': 0, 'dofRed': 0})
            if timed:
                self.eq(0x88BF)
                ns = ctypes.c_uint64(); self.gq(self.q, 0x8866, ctypes.byref(ns)); times.append(ns.value / 1e6)
        o = None if timed else self.read(out, w, h)
        self.free()
        return float(np.median(times[2:])) if timed else o


def window_depth(y):
    """the GL window depth of view distance y (game_dof.frag: y = far near / ((1 - d)(far - near) + near))"""
    return 1.0 - (ZF * ZN / y - ZN) / (ZF - ZN)


def scene(w, h, moving):
    rng = np.random.default_rng(5)
    yy, xx = np.mgrid[0:h, 0:w] / np.array([h, w])[:, None, None]
    sm = 0.5 + 0.25 * np.sin(40 * xx) * np.cos(23 * yy) + 0.2 * np.sign(np.sin(31 * xx + 11 * yy))
    sm += 0.05 * rng.random((h, w))
    col = np.dstack([sm, np.clip(sm * 0.8 + 0.1, 0, 1), 1.0 - sm * 0.6, np.ones((h, w))])
    # distance: near ground at the bottom (20) to far hills (2000), a focused block (200), sky at the top
    dist = 20.0 * (2000.0 / 20.0) ** (1.0 - yy)
    dist[int(0.3 * h):int(0.7 * h), int(0.4 * w):int(0.6 * w)] = 200.0
    depth = window_depth(dist)
    depth[:int(0.12 * h), :] = 1.0
    col[int(0.3 * h):int(0.7 * h), int(0.4 * w):int(0.6 * w), :3] = 0.1
    # the depth of field's blur, a quarter size (any blur: both chains read the same one)
    qw, qh = (w + 3) // 4, (h + 3) // 4
    pad = np.zeros((qh * 4, qw * 4, 4)); pad[:h, :w] = col; pad[h:, :w] = col[-1:]; pad[:, w:] = pad[:, w - 1:w]
    blur = pad.reshape(qh, 4, qw, 4, 4).mean(axis=(1, 3))
    mv = np.zeros((h, w, 2))
    if moving:
        mv = np.dstack([-0.008 * (1 + 0.5 * np.sin(7 * xx + 3 * yy)), 0.003 * np.cos(5 * yy)])
        mv[int(0.3 * h):int(0.7 * h), int(0.4 * w):int(0.6 * w)] = (0.004, -0.002)   # the block moves its own way
    D, RG, STR = 200.0, 100.0, 1.0
    dof = ((RG, RG, D, 10000.0), (STR, 1.0, 1.0, 0.0), (-1e8, ZN, ZF - ZN, ZF * ZN), (1.0, 1.0, 1.0, 1.0))
    mb = ((50 * 0.001 * 60.0, 0.01, 70.0, 0.0), (1.0, 1.0, (w - 1.0) / w, (h - 1.0) / h))
    return col, blur, depth, mv, dof, mb


def main():
    sd = shader_dir()
    R = Fuse(sd)
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        fails += 0 if ok else 1
        print('%-14s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)

    for w, h in ((320, 180), (1920, 1080)):
        for moving in (False, True):
            args = scene(w, h, moving)
            a = R.run(w, h, *args, 'two')
            b = R.run(w, h, *args, 'fused')
            e = np.abs(a[..., :3] - b[..., :3]).max(axis=2) * 255
            name = 'fuse %s %d' % ('moving' if moving else 'still', w)
            bar = 1.0 if moving else 0.0
            gate(name, e.max() <= bar, 'max %.4f steps (bar %.0f), p99.9 %.4f, >= 0.5 step %.4f%%%s'
                 % (e.max(), bar, np.percentile(e, 99.9), 100 * float((e >= 0.5).mean()),
                    ' (RED: nearest)' if RED else ''))
    if '--ms' in sys.argv:
        for moving in (False, True):
            args = scene(1920, 1080, moving)
            t2 = R.run(1920, 1080, *args, 'two', timed=True)
            t1 = R.run(1920, 1080, *args, 'fused', timed=True)
            gate('ms %s' % ('moving' if moving else 'still'), t1 < t2,
                 'two-pass %.3f ms, fused %.3f ms (1080p, composite + copy + blur vs one pass)' % (t2, t1))
    print('POSTFUSE RESULT %s (%d fail)%s' % ('PASS' if not fails else 'FAIL', fails, ' red=' + RED if RED else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
