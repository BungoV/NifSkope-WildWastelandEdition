"""lane DOFLENS1: offline gate for the thin-lens depth of field (res/shaders/game_doflens*.frag, src/gl/cellpost.cpp
row WW_CELL_DOF_LENS). The REAL shaders of the tree through the real driver in a hidden GL 4.1 context; no NifSkope
window. Reference: the thin-lens circle of confusion c = A f (z - S1) / (z (S1 - f)) computed here in float64.

    python -I tests/postfx/postfx_lens.py [--red ramp|game|noclamp|nonear|blurall|plain] [--ms]

Gates (exit 0 only when every gate passes; each red must fail its gate):
  coc       the prep pass's circle (quarter size, the nearest of four depths) == the reference, |err| <= 0.002 |c| + 0.02 px
            (half floats). Red ramp: a linear ramp in z (the game's rule) instead of the lens law.
  disk      a 4x4 light at two depths spreads into a disk whose diameter (2 sqrt(2 <r^2>), exact for a flat disk) is
            the reference circle +-20 %, and keeps its light +-15 %. Red game: the game's chain (downsample, Gaussian,
            composite) at the same record cannot match both circles.
  noleak    a mid-distance block (circle 8 px) in front of a bright far wall (circle 48 px): its inside (8 px in from the
            edge) keeps its colour within 0.02. Red noclamp: a tap behind the centre spreads at its own circle.
  focus     a frame all at the focus distance, near layer on: the output equals the input bit for bit. Red blurall: the
            composite takes the far layer everywhere.
  near      an out-of-focus near block (circle -24 px) over a focused wall: the near layer covers the scattered disk's
            share (0.196) +-0.08 at 6 px outside its edge, and the wall 24 px away is untouched. Red nonear: near off.
  fused     the lens composite inside the motion blur's pass (game_doflensmblur) == lens composite -> copy -> game_mblur,
            max 1/255, still and moving. Red plain: the fused pass without the lens (game_dofmblur).
  --ms      1080p GPU ms (median of 32 after 2 warm-ups, uploads outside the timed loop): prep + gather + composite
            must be < 0.15 ms; also printed: the game's chain, and the fused pass with and without the lens.
"""
import ctypes, os, re, shutil, sys, tempfile
import numpy as np

TREE = os.environ.get('TREE', os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')))
sys.path.insert(0, TREE + '/tests/spells')
sys.path.insert(0, TREE + '/tests/postfx')
import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402
import postfx_gates as P        # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
RGBA16F, RG16F, R32F = 0x881A, 0x822F, 0x822E
NEAREST, LINEAR = P.NEAREST, P.LINEAR
ZN, ZF = 1.0, 100000.0
F_MM = 35.0
MM_PER_UNIT = 14.2875
F = F_MM / MM_PER_UNIT          # the focal length in game units
S1 = 200.0                      # the focus distance
CINF = 48.0                     # the circle at infinity (px)
A_PX = CINF * (S1 - F) / F      # the aperture in sensor px
MAXC = 64.0
PROGS = ('game_doflens_prep', 'game_doflens_gather', 'game_doflens', 'game_doflensmblur', 'game_dofmblur',
         'game_mblur', 'game_dof_down', 'game_dof_blur', 'game_dof')


def ref_coc(z, s1=S1, a=A_PX, f=F, near=True, far=True):
    c = a * f * (z - s1) / (z * (s1 - f))
    if not far:
        c = np.minimum(c, 0)
    if not near:
        c = np.maximum(c, 0)
    return np.clip(c, -MAXC, MAXC)


def z_of_coc(c, s1=S1):
    """the distance whose circle is c (inverse of ref_coc)"""
    k = A_PX * F / (s1 - F)
    return k * s1 / (k - c)


def window_depth(y):
    return 1.0 - (ZF * ZN / y - ZN) / (ZF - ZN)


def shader_dir():
    """the tree's shaders, or a scratch copy with the red's change"""
    if RED not in ('ramp', 'noclamp', 'blurall'):
        return P.SD
    d = tempfile.mkdtemp(prefix='lensred_', dir=os.environ.get('TMPDIR') or None)
    for f in os.listdir(P.SD):
        if f.endswith(('.frag', '.vert', '.prog', '.glsl')):
            shutil.copy(os.path.join(P.SD, f), d)
    if RED == 'ramp':
        fn, a, b = 'game_doflens.glsl', 'c = lensP.y * f * ( z - s1 ) / ( z * ( s1 - f ) );', \
            'c = lensP.y * f / ( s1 - f ) * clamp( ( z - s1 ) / s1, -1.0, 1.0 );'
    elif RED == 'noclamp':
        fn, a, b = 'game_doflens_gather.frag', 'rf = min( rf, rc );', 'rf = rf;'
    else:
        fn, a, b = 'game_doflens.glsl', 'if ( cf > 1.0 )\n\t\tm = mix( a.rgb, texture( LENS_FAR, uq ).rgb, lensSat( ( cf - 1.0 ) / 3.0 ) );', \
            'm = texture( LENS_FAR, uq ).rgb;'
    p = os.path.join(d, fn)
    s = open(p, newline='').read()
    assert s.count(a) == 1, 'red %s: anchor not found' % RED
    open(p, 'w', newline='').write(s.replace(a, b))
    return d


class Lens(P.Rig):
    def __init__(self, sd):
        P.SD = sd
        self.g = C.Gl()
        self.p = {k: C.build(sd, k + '.prog') for k in PROGS}
        self.um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
        self.u4v = S.fn('glUniform4fv', None, GLint, ctypes.c_int, ctypes.POINTER(ctypes.c_float))
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

    def pass2(self, prog, dsts, w, h, texs, uni):
        """pass_ with several targets and vec4 arrays (a list of 4-tuples)"""
        g = self.g
        g.bindfb(0x8D40, self.fb)
        for i in range(2):
            g.fbtex(0x8D40, 0x8CE0 + i, 0x0DE1, dsts[i] if i < len(dsts) else 0, 0)
        n = len(dsts)
        self.drawbufs(n, (GLenum * n)(*[0x8CE0 + i for i in range(n)]))
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
            elif isinstance(v, list):
                flat = [float(x) for t in v for x in t]
                self.u4v(l, len(v), (ctypes.c_float * len(flat))(*flat))
            elif len(v) == 2:
                g.u2f(l, *v)
            else:
                self.u4(l, *v)
        gl.glDrawArrays(5, 0, 4)

    def copy(self, src, dst, w, h):
        g = self.g
        g.bindfb(0x8CA8, self.fb2); g.fbtex(0x8CA8, 0x8CE0, 0x0DE1, src, 0); gl.glReadBuffer(0x8CE0)
        g.bindfb(0x8CA9, self.fb); g.fbtex(0x8CA9, 0x8CE0, 0x0DE1, dst, 0); g.fbtex(0x8CA9, 0x8CE1, 0x0DE1, 0, 0)
        self.drawbufs(1, (GLenum * 1)(0x8CE0))
        self.blit(0, 0, w, h, 0, 0, w, h, 0x4000, NEAREST)

    def timed(self, fn):
        times = []
        for _ in range(34):
            self.bq(0x88BF, self.q)
            fn()
            self.eq(0x88BF)
            ns = ctypes.c_uint64(); self.gq(self.q, 0x8866, ctypes.byref(ns)); times.append(ns.value / 1e6)
        return float(np.median(times[2:]))

    def lens(self, w, h, col, depth, lq=(MAXC, 1.0, 1.0, 0.0), lp=(S1, A_PX, F, 0.0), mode='two', mv=None, mb=None,
             timed=False, layers=False):
        """prep -> gather -> composite (mode 'two': game_doflens; 'fused': game_doflensmblur; 'mb': game_doflens, copy,
        game_mblur; 'plain': game_dofmblur with the lens layers). Returns the output (or ms, or the layers)"""
        qw, qh = (w + 3) // 4, (h + 3) // 4
        tc = self.tex(w, h, RGBA16F, LINEAR, col)
        td = self.tex(w, h, R32F, NEAREST, depth[..., None])
        tp = self.tex(qw, qh, RGBA16F, LINEAR)
        tf = self.tex(qw, qh, RGBA16F, LINEAR)
        tn = self.tex(qw, qh, RGBA16F, LINEAR)
        out = self.tex(w, h, RGBA16F, LINEAR)
        tv = self.tex(w, h, RG16F, LINEAR, mv if mv is not None else np.zeros((h, w, 2)))
        cur2 = self.tex(w, h, RGBA16F, LINEAR)
        lzv = (ZN, ZF - ZN, ZF * ZN, 0.0)
        lu = {'lz': lzv, 'lensP': lp, 'lensQ': lq}
        dofu = {'dc0': (100.0, 100.0, S1, 1e4), 'dc1': (1.0, 1.0, 1.0, 0.0), 'dc2': (-1e8, ZN, ZF - ZN, ZF * ZN),
                'dc3': (1.0, 1.0, 1.0, 1.0), 'farOnly': 0, 'dofRed': 0}

        def run():
            self.pass2('game_doflens_prep', [tp], qw, qh, (('t0', tc), ('t1', td)), lu)
            self.pass2('game_doflens_gather', [tf, tn], qw, qh, (('t0', tp),), lu)
            if mode == 'two':
                self.pass2('game_doflens', [out], w, h, (('t0', tc), ('t1', tf), ('t2', td), ('lensNear', tn)), lu)
            elif mode == 'mb':
                self.pass2('game_doflens', [out], w, h, (('t0', tc), ('t1', tf), ('t2', td), ('lensNear', tn)), lu)
                self.copy(out, cur2, w, h)
                self.pass2('game_mblur', [out], w, h, (('t0', cur2), ('t1', tv)), {'c0': mb[0], 'c1': mb[1]})
            else:
                prog = 'game_doflensmblur' if mode == 'fused' else 'game_dofmblur'
                u = dict(lu); u.update(dofu); u['mc0'] = mb[0]; u['mc1'] = mb[1]
                self.pass2(prog, [out], w, h, (('t0', tc), ('t1', tv), ('t2', tf), ('t3', td), ('lensNear', tn)), u)
        if timed:
            r = self.timed(run)
        else:
            run()
            r = (self.read(tp, qw, qh), self.read(tf, qw, qh), self.read(tn, qw, qh)) if layers else self.read(out, w, h)
        self.free()
        return r

    def game(self, w, h, col, depth, rec, radius=3, timed=False):
        """the game's chain as cellpost.cpp issues it: downsample, the Gaussian down the columns then the rows, composite"""
        qw, qh = (w + 3) // 4, (h + 3) // 4
        tc = self.tex(w, h, RGBA16F, LINEAR, col)
        td = self.tex(w, h, R32F, NEAREST, depth[..., None])
        b0 = self.tex(qw, qh, RGBA16F, NEAREST)
        b1 = self.tex(qw, qh, RGBA16F, NEAREST)
        out = self.tex(w, h, RGBA16F, LINEAR)
        k = np.arange(-radius, radius + 1)
        wt = np.exp(-0.5 * (k / (radius / 2.0)) ** 2); wt /= wt.sum()
        strength, dist, rg = rec

        def run():
            self.pass2('game_dof_down', [b0], qw, qh, (('t0', tc),),
                       {'c4': (1.0, 1.0, 2.0 / w, 2.0 / h), 'c7': (1.0 / w, 1.0 / h, 0.0, 0.0),
                        'taps': [(-1, -1, 0.25, 0), (1, -1, 0.25, 0), (-1, 1, 0.25, 0), (1, 1, 0.25, 0)],
                        'dstSize': (float(qw), float(qh))})
            for ps_, (src, dst) in enumerate(((b0, b1), (b1, b0))):
                o = [((kk / qw) if ps_ == 1 else 0.0, (kk / qh) if ps_ == 0 else 0.0, ww, 0.0) for kk, ww in zip(k, wt)]
                gl.glBindTexture(0x0DE1, src)
                self.pass2('game_dof_blur', [dst], qw, qh, (('t0', src),), {'o': o, 'taps': len(o), 'dstSize': (float(qw), float(qh))})
            gl.glBindTexture(0x0DE1, b0)
            for kk in (0x2801, 0x2800):
                gl.glTexParameteri(0x0DE1, kk, LINEAR)
            self.pass2('game_dof', [out], w, h, (('t0', tc), ('t1', b0), ('t2', td)),
                       {'c0': (rg, rg, dist, 1e4), 'c1': (strength, 1.0, 1.0, 0.0), 'c2': (-1e8, ZN, ZF - ZN, ZF * ZN),
                        'c3': (1.0, 1.0, 1.0, 1.0), 'farOnly': 0, 'dofRed': 0})
        if timed:
            r = self.timed(run)
        else:
            run()
            r = self.read(out, w, h)
        self.free()
        return r


def flat(w, h, z, v=0.0):
    col = np.zeros((h, w, 4)); col[..., :3] = v; col[..., 3] = 1.0
    return col, np.full((h, w), window_depth(z))


def main():
    sd = shader_dir()
    R = Lens(sd)
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        fails += 0 if ok else 1
        print('%-8s %s  %s%s' % (name, 'PASS' if ok else 'FAIL', msg, '  (RED %s)' % RED if RED else ''), flush=True)

    # coc: a ramp of distances, the near and far sides, no sky
    w, h = 256, 64
    yy, xx = np.mgrid[0:h, 0:w]
    z = 60.0 * (5000.0 / 60.0) ** (xx / (w - 1.0)) * (1.0 + 0.02 * np.sin(yy))
    col = np.dstack([np.full((h, w), 0.5)] * 3 + [np.ones((h, w))])
    d = window_depth(z)
    prep, _, _ = R.lens(w, h, col, d, layers=True)
    # the four depths the prep reads: GL texels 4q+1, 4q+3; the rig's rows are flipped (numpy row offsets 2, 0)
    zq = z.reshape(h // 4, 4, w // 4, 4)[:, 0::2, :, 1::2].min(axis=(1, 3))
    ref = ref_coc(zq)
    err = np.abs(prep[..., 3] - ref)
    bad = err > 0.002 * np.abs(ref) + 0.02
    gate('coc', not bad.any(), 'max err %.4f px over circles %.1f..%.1f px, %d of %d over the bar'
         % (err.max(), ref.min(), ref.max(), int(bad.sum()), bad.size))

    # disk: a 4x4 light (100) on black at two depths
    w = h = 192
    res = []
    for c in (16.0, 40.0):
        zc = z_of_coc(c)
        col, d = flat(w, h, zc)
        col[96:100, 96:100, :3] = 100.0
        o = R.game(w, h, col, d, (1.0, S1, zc - S1)) if RED == 'game' else R.lens(w, h, col, d)
        e = o[..., :3].mean(axis=2)
        yy, xx = np.mgrid[0:h, 0:w] + 0.5
        r2 = (xx - 98.0) ** 2 + (yy - 98.0) ** 2
        dm = 2.0 * np.sqrt(2.0 * (r2 * e).sum() / e.sum())
        keep = e.sum() / 1600.0
        res.append((c, dm, keep))
    ok = all(abs(dm / c - 1.0) <= 0.2 and abs(keep - 1.0) <= 0.15 for c, dm, keep in res)
    gate('disk', ok, '; '.join('circle %.0f px: diameter %.1f px (%.2f), light kept %.3f' % (c, dm, dm / c, k) for c, dm, k in res))

    # noleak: a mid block (circle 8) in front of a bright far wall (circle 48 -> clamped to the far asymptote)
    w, h = 256, 256
    col, d = flat(w, h, 1e5, 4.0)
    zm = z_of_coc(8.0)
    col[64:192, 64:192, :3] = 0.2
    d[64:192, 64:192] = window_depth(zm)
    o = R.lens(w, h, col, d)
    inside = o[72:184, 72:184, :3]
    lk = np.abs(inside - 0.2).max()
    gate('noleak', lk <= 0.02, 'the block 8 px in: max |out - 0.2| = %.4f (bar 0.02); wall circle %.1f px' % (lk, ref_coc(1e5)))

    # focus: everything at the focus distance, near on
    rng = np.random.default_rng(7)
    col = np.dstack([rng.random((h, w)) * 4.0 for _ in range(3)] + [np.ones((h, w))]).astype(np.float16).astype(np.float64)
    d = np.full((h, w), window_depth(S1))
    o = R.lens(w, h, col, d)
    fe = np.abs(o[..., :3] - col[..., :3]).max()
    gate('focus', fe == 0.0, 'max |out - in| = %.3g (bar 0, bit for bit)' % fe)

    # near: a near block (circle -24) over a focused wall
    col, d = flat(w, h, S1, 0.25)
    zn = z_of_coc(-24.0)
    col[96:160, 96:160, :3] = 3.0
    d[96:160, 96:160] = window_depth(zn)
    lq = (MAXC, 0.0 if RED == 'nonear' else 1.0, 1.0, 0.0)
    o = R.lens(w, h, col, d, lq=lq)
    _, _, nl = R.lens(w, h, col, d, lq=lq, layers=True)
    a6 = nl[(96 - 6) // 4, 128 // 4, 3]   # 6 px outside the top edge (quarter texel), mid column
    # a disk of radius 12 scattered from a half plane covers a point 6 px out by the circle's segment share
    seg = (np.arccos(0.5) - 0.5 * np.sqrt(0.75)) / np.pi
    far24 = np.abs(o[:96 - 24, :, :3] - 0.25).max()
    gate('near', abs(a6 - seg) <= 0.08 and far24 == 0.0, 'near cover 6 px outside %.3f (scatter %.3f +-0.08); wall 24 px '
         'away max diff %.3g (bar 0)' % (a6, seg, far24))

    # fused: the lens in the motion blur's pass vs the two passes
    for w, h in ((320, 180), (1920, 1080)):
        for moving in (False, True):
            yy, xx = np.mgrid[0:h, 0:w] / np.array([h, w])[:, None, None]
            sm = 0.5 + 0.25 * np.sin(40 * xx) * np.cos(23 * yy) + 0.2 * np.sign(np.sin(31 * xx + 11 * yy))
            col = np.dstack([sm, np.clip(sm * 0.8 + 0.1, 0, 1), 1.0 - sm * 0.6, np.ones((h, w))])
            dist = 20.0 * (4000.0 / 20.0) ** (1.0 - yy)
            dist[int(0.3 * h):int(0.7 * h), int(0.4 * w):int(0.6 * w)] = S1
            d = window_depth(dist)
            mv = np.zeros((h, w, 2))
            if moving:
                mv = np.dstack([-0.008 * (1 + 0.5 * np.sin(7 * xx + 3 * yy)), 0.003 * np.cos(5 * yy)])
            mb = ((50 * 0.001 * 60.0, 0.01, 70.0, 0.0), (1.0, 1.0, (w - 1.0) / w, (h - 1.0) / h))
            a = R.lens(w, h, col, d, mode='mb', mv=mv, mb=mb)
            b = R.lens(w, h, col, d, mode='plain' if RED == 'plain' else 'fused', mv=mv, mb=mb)
            e = np.abs(a[..., :3] - b[..., :3]).max(axis=2) * 255
            gate('fused', e.max() <= 1.0, '%s %d: max %.4f steps (bar 1), p99.9 %.4f'
                 % ('moving' if moving else 'still', w, e.max(), np.percentile(e, 99.9)))

    if '--ms' in sys.argv:
        w, h = 1920, 1080
        yy, xx = np.mgrid[0:h, 0:w] / np.array([h, w])[:, None, None]
        col = np.dstack([0.5 + 0.4 * np.sin(40 * xx) * np.cos(23 * yy)] * 3 + [np.ones((h, w))])
        d = window_depth(20.0 * (4000.0 / 20.0) ** (1.0 - yy))
        mv = np.dstack([-0.008 * (1 + 0.5 * np.sin(7 * xx + 3 * yy)), 0.003 * np.cos(5 * yy)])
        mb = ((50 * 0.001 * 60.0, 0.01, 70.0, 0.0), (1.0, 1.0, (w - 1.0) / w, (h - 1.0) / h))
        for nearOn in (1.0, 0.0):
            lq = (MAXC, nearOn, 1.0, 0.0)
            t = R.lens(w, h, col, d, lq=lq, timed=True)
            tf = R.lens(w, h, col, d, lq=lq, mode='fused', mv=mv, mb=mb, timed=True)
            gate('ms', t < 0.15, 'near %s: lens prep + gather + composite %.3f ms (bar 0.15); fused lens + blur %.3f ms'
                 % ('on' if nearOn else 'off', t, tf))
        tp = R.lens(w, h, col, d, mode='plain', mv=mv, mb=mb, timed=True)
        tg = R.game(w, h, col, d, (1.0, S1, 100.0), timed=True)
        print('ms info: the game chain (down + 2 blurs + composite) %.3f ms; prep + gather + fused game pass %.3f ms' % (tg, tp))
    print('DOFLENS RESULT %s (%d fail)%s' % ('PASS' if not fails else 'FAIL', fails, ' red=' + RED if RED else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
