"""Lane PERFFIX3 (item 1): the aerial-perspective volume in ONE layered draw (atmo_apl.prog: gl_Layer from a geometry
shader) against today's draw-a-slice path (atmo_ap.prog + glFramebufferTextureLayer per slice), through the real GL
driver in a hidden 4.1 core context (tests/spells/shader_link_check.py). Runs no NifSkope.

    python -I tests/aplayer/aplayer_check.py [shaders dir] [--red shift|layer0] [--reps N] [--out results.txt]

Gates (each tier: Low 16x16x16, Medium 32x32x16, High 32x32x32, Ultra 64x64x32, the real RGBA16F 3D target, a hazy
and a clear medium, sun 30 deg + moon):
  I identity  every texel of every slice bit-identical (max |diff| 0, as uint16 halves) between the two paths
  C coverage  every slice of the layered draw was written (a NaN-filled target keeps no NaN)
  T cost      GL_TIME_ELAPSED and CPU submit time per build, both paths, median of N reps (INFO + the bar: layered GPU
              not slower than slices by > 5 %)
Reds (must FAIL gate I): shift = the geometry shader hands slice k + 1; layer0 = every invocation writes layer 0.
Exit 0 only when every gate passes.
"""
import ctypes, math, os, re, shutil, sys, tempfile, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SPELLS = os.path.join(HERE, '..', 'spells')
sys.path.insert(0, SPELLS)
import shader_link_check as S   # noqa: E402
import atmo_model as A          # noqa: E402
import atmo_lut_check as L      # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
STAGE = {'vert': 0x8B31, 'frag': 0x8B30, 'geom': 0x8DD9}


def build(sdir, prog):
    files = [os.path.join(sdir, f) for f in
             [l.split()[1:] for l in open(os.path.join(sdir, prog)) if l.startswith('shaders')][0]]
    p = S.fn('glCreateProgram', GLuint)()
    for f in files:
        src = S.expand(f)
        s = S.fn('glCreateShader', GLuint, GLenum)(STAGE[f.rsplit('.', 1)[1]])
        b = ctypes.c_char_p(src.encode('latin-1'))
        S.fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)(s, 1, ctypes.byref(b), None)
        S.fn('glCompileShader', None, GLuint)(s)
        ok = GLint()
        S.fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(4096)
            S.fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)(s, 4096, None, log)
            raise SystemExit('COMPILE FAIL %s %s\n%s' % (prog, f, log.value.decode('latin-1')[:800]))
        S.fn('glAttachShader', None, GLuint, GLuint)(p, s)
    S.fn('glLinkProgram', None, GLuint)(p)
    ok = GLint()
    S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL ' + prog)
    return p


def red_copy(sdir, red):
    """A scratch copy of the shaders with the red's geometry shader (the tree is never touched)."""
    d = tempfile.mkdtemp(prefix='aplayer_red_', dir=os.environ.get('APLAYER_TMP') or None)
    for f in os.listdir(sdir):
        if os.path.isfile(os.path.join(sdir, f)):
            shutil.copy(os.path.join(sdir, f), d)
    g = open(os.path.join(d, 'atmo_apl.geom'), encoding='latin-1').read()
    if red == 'shift':
        new = g.replace('atApSliceIn = float( gl_InvocationID );', 'atApSliceIn = float( gl_InvocationID + 1 );')
    else:
        new = g.replace('gl_Layer = gl_InvocationID;', 'gl_Layer = 0;')
    assert new != g, 'red anchor missing'
    open(os.path.join(d, 'atmo_apl.geom'), 'w', encoding='latin-1', newline='\n').write(new)
    return d


def main():
    a = sys.argv[1:]
    sdir = a[0] if a and not a[0].startswith('--') else os.path.join(HERE, '..', '..', 'res', 'shaders')
    red = a[a.index('--red') + 1] if '--red' in a else ''
    reps = int(a[a.index('--reps') + 1]) if '--reps' in a else 30
    out = a[a.index('--out') + 1] if '--out' in a else ''
    g = L.Gl()
    lines = []

    def say(s):
        print(s)
        lines.append(s)
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        say(('PASS ' if ok else 'FAIL ') + name + ': ' + msg)
        fails += 0 if ok else 1

    ldir = red_copy(sdir, red) if red else sdir
    P = A.params()
    base = dict(atRay=tuple(P['ray']), atMieS=(P['mieS'],), atMieE=(P['mieE'],), atOzone=tuple(P['oz']),
                atG=(P['g'],), atHaze=(P['hazeE'], P['hazeH'], P['hazeMid']))
    # the LUTs both paths read
    tw, th, mw = 256, 64, 32
    pt = L.build(sdir, 'atmo_transmittance.prog')
    tt, tfb = g.target(tw, th)
    pm = L.build(sdir, 'atmo_ms.prog')
    mt, mfb = g.target(mw, mw)
    ps = build(sdir, 'atmo_ap.prog')
    pl = build(ldir, 'atmo_apl.prog')

    fbtl = S.fn('glFramebufferTextureLayer', None, GLenum, GLenum, GLuint, GLint, GLint)
    fbt = S.fn('glFramebufferTexture', None, GLenum, GLenum, GLuint, GLint)
    status = S.fn('glCheckFramebufferStatus', GLenum, GLenum)
    tex3d = S.fn('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p)
    gl.glGetTexImage.argtypes = [GLenum, GLint, GLenum, GLenum, ctypes.c_void_p]
    genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))
    beginq = S.fn('glBeginQuery', None, GLenum, GLuint)
    endq = S.fn('glEndQuery', None, GLenum)
    getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
    drawbufs = S.fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
    q = GLuint()
    genq(1, ctypes.byref(q))

    def setuni(prog, uni):
        g.use(prog)
        for k, v in uni.items():
            l = g.loc(prog, k.encode())
            if l < 0:
                continue
            if isinstance(v, int):
                g.u1i(l, v)
            elif len(v) == 1:
                g.u1f(l, v[0])
            elif len(v) == 2:
                g.u2f(l, *v)
            else:
                g.u3f(l, *v)
        for unit, (name, tex) in enumerate((('atTransTex', tt), ('atMsTex', mt))):
            g.active(0x84C0 + unit)
            gl.glBindTexture(0x0DE1, tex)
            g.u1i(g.loc(prog, name.encode()), unit)

    def volume(w, n):
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x806F, t)
        nanfill = np.full((n, w, w, 4), np.nan, np.float32)
        tex3d(0x806F, 0, 0x881A, w, w, n, 0, 0x1908, 0x1406, nanfill.ctypes.data)   # RGBA16F, NaN everywhere
        for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x813D, 0)):
            gl.glTexParameteri(0x806F, k, v)
        fb = GLuint()
        g.genfb(1, ctypes.byref(fb))
        return t.value, fb.value

    def read(t, w, n):
        buf = np.zeros((n, w, w, 4), np.uint16)
        gl.glBindTexture(0x806F, t)
        gl.glGetTexImage(0x806F, 0, 0x1908, 0x140B, buf.ctypes.data)   # GL_HALF_FLOAT: the stored bits
        return buf

    one = (GLenum * 1)(0x8CE0)

    def draw_slices(fb, t, w, n):
        g.bindfb(0x8D40, fb)
        gl.glViewport(0, 0, w, w)
        g.use(ps)
        lk = g.loc(ps, b'atApSlice')
        for k in range(n):
            fbtl(0x8D40, 0x8CE0, t, 0, k)
            if k == 0:
                drawbufs(1, one)
                assert status(0x8D40) == 0x8CD5, 'slice target incomplete'
            g.u1f(lk, float(k))
            gl.glDrawArrays(5, 0, 4)

    def draw_layered(fb, t, w, n):
        g.bindfb(0x8D40, fb)
        gl.glViewport(0, 0, w, w)
        g.use(pl)
        fbt(0x8D40, 0x8CE0, t, 0)
        drawbufs(1, one)
        assert status(0x8D40) == 0x8CD5, 'layered target incomplete'
        gl.glDrawArrays(5, 0, 4)

    def timed(fn, *args):
        gl.glFinish()
        c0 = time.perf_counter()
        beginq(0x88BF, q)   # GL_TIME_ELAPSED
        fn(*args)
        endq(0x88BF)
        c1 = time.perf_counter()
        v = ctypes.c_uint64()
        getq(q, 0x8866, ctypes.byref(v))   # GL_QUERY_RESULT
        return v.value * 1e-6, (c1 - c0) * 1e3

    sw = (math.cos(math.radians(30)) * math.cos(math.radians(40)), math.cos(math.radians(30)) * math.sin(math.radians(40)),
          math.sin(math.radians(30)))
    camR = A.RB + 0.02
    tiers = (('Low', 16, 16), ('Medium', 32, 16), ('High', 32, 32), ('Ultra', 64, 32))
    worst = 0
    covered = True
    cost = []
    for hz in ((0.0, 1.0, 0.0), A.haze_from_fog(20000.0 / A.UNIT_M, 0.6)):
        g.draw(pt, tfb, tw, th, dict(base, atHaze=tuple(hz)), [])
        g.draw(pm, mfb, mw, mw, dict(base, atHaze=tuple(hz), atMsSize=(float(mw), float(mw))), [('atTransTex', tt)])
        for name, w, n in tiers:
            uni = dict(base, atHaze=tuple(hz), atMsSize=(float(mw), float(mw)), atApDims=(float(w), float(w), float(n)),
                       atApMaxKm=(16.0,), atFrustum0=(1.0, -0.6, -0.3), atFrustumX=(0.0, 1.2, 0.0),
                       atFrustumY=(0.0, 0.0, 0.7), atCamR=(camR,), atSunW=sw, atMoonW=(-0.3, 0.2, 0.93),
                       atSunK=(3.1,), atMoonK=(0.02,), atMsOn=(1.0,), atApNoSunT=(0.0,), atTintOn=(0.0,))
            ts, fs = volume(w, n)
            tl, fl = volume(w, n)
            setuni(ps, uni)
            setuni(pl, uni)
            draw_slices(fs, ts, w, n)
            draw_layered(fl, tl, w, n)
            gl.glFinish()
            A_ = read(ts, w, n)
            B_ = read(tl, w, n)
            fa = A_.view(np.float16).astype(np.float32)
            fb_ = B_.view(np.float16).astype(np.float32)
            nan_l = int(np.isnan(fb_).any(axis=(1, 2, 3)).sum())
            nan_s = int(np.isnan(fa).any(axis=(1, 2, 3)).sum())
            diff = int((A_ != B_).sum())
            worst = max(worst, diff)
            covered = covered and nan_l == 0 and nan_s == 0
            say('    haze %.4f %-6s %dx%dx%d: texels differing %d of %d, max |d| %.3g, slices left NaN: slices path %d, layered %d'
                % (hz[0], name, w, w, n, diff, A_.size, float(np.nanmax(np.abs(fa - fb_))) if diff else 0.0, nan_s, nan_l))
            if hz[0] > 0:
                gs, cs, gl_, cl_ = [], [], [], []
                for r in range(reps):
                    a_, b_ = timed(draw_slices, fs, ts, w, n)
                    c_, d_ = timed(draw_layered, fl, tl, w, n)
                    gs.append(a_); cs.append(b_); gl_.append(c_); cl_.append(d_)
                cost.append((name, w, n, np.median(gs), np.median(cs), np.median(gl_), np.median(cl_)))
            gl.glDeleteTextures(1, ctypes.byref(GLuint(ts)))
            gl.glDeleteTextures(1, ctypes.byref(GLuint(tl)))
    gate('I identity', worst == 0, 'texels differing between a draw a slice and one layered draw: %d (want 0, every tier, '
         'clear + hazy)' % worst)
    gate('C coverage', covered, 'every slice written by both paths (no NaN left in the NaN-filled RGBA16F targets)')
    slow = 0.0
    for name, w, n, gs, cs, gl_, cl_ in cost:
        say('    cost %-6s %dx%dx%d: a slice a draw GPU %.4f ms CPU %.4f ms | layered GPU %.4f ms CPU %.4f ms (median of %d)'
            % (name, w, w, n, gs, cs, gl_, cl_, reps))
        slow = max(slow, gl_ / max(gs, 1e-6))
    gate('T cost', slow <= 1.05, 'layered GPU / slices GPU worst %.3f (<= 1.05); this machine, offline, hazy medium' % slow)
    say('APLAYER %s (%d gate(s) failed)%s' % ('PASS' if not fails else 'FAIL', fails, ' RED=' + red if red else ''))
    if out:
        open(out, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    if red:
        shutil.rmtree(ldir, ignore_errors=True)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
