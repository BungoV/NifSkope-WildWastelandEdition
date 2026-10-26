"""Lane ATMO1: render the physical sky's LUT shaders through the real GL driver (hidden 4.1 core context, the loader's
#include expansion) and compare the read-back texels with the numpy model (atmo_model.py). Runs no NifSkope.

    python -I tests/spells/atmo_lut_check.py [shaders dir] [--red noms|noozone|nosunt|double|noground|notint]

Gates: transmittance texels vs the exact optical-depth integral (< 1 %); multiple-scattering texels vs the model's
Psi (< 3 %); sky-view texels at sun 30 and 5 deg vs the model's march with the same Psi (< 2 %).
Lane SKYA: the aerial-perspective froxels vs the model's march stopped at each slice (< 3 %, |T| < 0.005; red
nosunt), and the AP + cell fog combine: summed optical depth = the larger of the two, never the sum (red double).
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


def build(sdir, prog, paths=None):
    files = paths or [os.path.join(sdir, f) for f in
                      [l.split()[1:] for l in open(os.path.join(sdir, prog)) if l.startswith('shaders')][0]]
    p = S.fn('glCreateProgram', GLuint)()
    for f in files:
        src = S.expand(f)
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

    def draw(self, prog, fb, w, h, uni, texs, alpha=False):
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
        return np.frombuffer(buf, np.float32).reshape(h, w, 4)[..., :4 if alpha else 3].astype(np.float64)


def sub_to_unit(u, n):
    return (u - 0.5 / n) / (1 - 1.0 / n)


def main():
    a = sys.argv[1:]
    sdir = a[0] if a and not a[0].startswith('--') else os.path.join(HERE, '..', '..', 'res', 'shaders')
    red = a[a.index('--red') + 1] if '--red' in a else ''
    P = A.params()
    base = dict(atRay=tuple(P['ray']), atMieS=(P['mieS'],), atMieE=(P['mieE'],),
                atOzone=(0.0, 0.0, 0.0) if red == 'noozone' else tuple(P['oz']), atG=(P['g'],),
                atHaze=(P['hazeE'], P['hazeH'], P['hazeMid']))
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
    # the same with the Koschmieder fixture's haze layer (3 km visibility fog, 143 m scale height)
    Ph = A.params(haze=A.haze_from_fog(3000.0 / A.UNIT_M, 1.0))
    Th = g.draw(pt, tfb, tw, th, dict(base, atHaze=(Ph['hazeE'], Ph['hazeH'], Ph['hazeMid'])), [])
    err = np.abs(Th[ys, xs] - A.transmittance(Ph, r, mu, 4000)).max()
    gate('transmittance-haze', err < 0.01, 'max |GPU - exact| with the haze layer = %.5f (< 0.01)' % err)
    g.draw(pt, tfb, tw, th, base, [])

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

    # 3b. lane SKY2B (SKYSTRIPE1): rays that end on the planet see its lit ground (albedo 0.3, the MS pass's own);
    # a camera 1 km up, texels under the horizon vs the model's march with ground=True (red noground: the black earth)
    camR = A.RB + 1.0
    worst = 0.0
    V = g.draw(pv, vfb, vw, vh, dict(base, atMsSize=(float(mw), float(mh)), atViewSize=(float(vw), float(vh)),
                                     atCamR=(camR,), atLightMu=(math.sin(math.radians(30.0)),), atMsOn=(1.0,),
                                     atGround=(0.0 if red == 'noground' else 1.0,)), [('atTransTex', tt), ('atMsTex', mt)])
    for (x, y) in ((60, 56), (120, 62), (10, 80), (180, 100), (96, 70)):
        ux, uy = sub_to_unit((x + 0.5) / vw, vw), sub_to_unit((y + 0.5) / vh, vh)
        beta = math.acos(math.sqrt(camR ** 2 - A.RB ** 2) / camR)
        za = math.pi - beta + beta * (uy * 2 - 1) ** 2
        cz, caz = math.cos(za), -(ux * ux * 2 - 1)
        sz = math.sqrt(max(0, 1 - cz * cz))
        d = np.array([[sz * caz, sz * math.sqrt(max(0, 1 - caz * caz)), cz]])
        ref = A.integrate(P, camR, d, A.sun_dir(30.0), 400, psi=psi_sub, ground=True)[0][0]
        worst = max(worst, A.rel(V[y, x], ref).max())
    gate('sky-view ground', worst < 0.02, 'worst relative error at 5 texels under the horizon, camera 1 km = %.4f (< 0.02)' % worst)

    # 4. lane SKYA (ATMO2): the aerial-perspective volume (Hillaire 2020 sec. 5.4), slice by slice into a 2D target,
    # vs the model's march stopped at the slice's distance; a hazy fixture (fog 20 km, Max 0.6: the layer the camera
    # stands in) and the clear air; the in-scatter (rgb) and the transmittance (a, the mean of the three channels)
    aw = ah = an = 32
    pa = build(sdir, 'atmo_ap.prog')
    at_, afb = g.target(aw, ah)
    f0, fx, fy = np.array([1.0, -0.5, -0.05]), np.array([0.0, 1.0, 0.0]), np.array([0.0, 0.0, 0.4])
    sw = np.array([math.cos(math.radians(30)) * math.cos(math.radians(40)),
                   math.cos(math.radians(30)) * math.sin(math.radians(40)), math.sin(math.radians(30))])
    worstL = worstT = 0.0
    hazy = []   # (distance km, GPU in-scatter, GPU transmittance) of the hazy weather, for gate 5
    for hz in ((0.0, 1.0, 0.0), A.haze_from_fog(20000.0 / A.UNIT_M, 0.6)):
        Pa = A.params(haze=hz)
        ua = dict(base, atHaze=tuple(hz), atMsSize=(float(mw), float(mh)), atApDims=(float(aw), float(ah), float(an)),
                  atApMaxKm=(32.0,), atFrustum0=tuple(f0), atFrustumX=tuple(fx), atFrustumY=tuple(fy), atCamR=(camR,),
                  atSunW=tuple(sw), atMoonW=(0.0, 0.0, 1.0), atSunK=(1.0,), atMoonK=(0.0,),
                  atMsOn=(0.0 if red == 'noms' else 1.0,), atApNoSunT=(1.0 if red == 'nosunt' else 0.0,))
        # the transmittance and MS LUTs of this haze (the MS LUT of the clear air is kept: Psi barely moves near
        # the ground and the model reads the same table)
        g.draw(pt, tfb, tw, th, dict(base, atHaze=tuple(hz)), [])
        for k in (0, 6, 15, 31):
            Va = g.draw(pa, afb, aw, ah, dict(ua, atApSlice=(float(k),)), [('atTransTex', tt), ('atMsTex', mt)], alpha=True)
            D = 32.0 * ((k + 0.5) / an) ** 2
            pts = ((0, 0), (31, 31), (16, 4), (5, 27), (24, 12))
            dirs = []
            for (x, y) in pts:
                v = f0 + (x + 0.5) / aw * fx + (y + 0.5) / ah * fy
                dirs.append(v / np.linalg.norm(v))
            Lm, _, Tm = A.integrate(Pa, camR, np.array(dirs), sw, 400, psi=psi_sub, dmax=D, want_T=True)
            for i, (x, y) in enumerate(pts):
                eL = A.rel(Va[y, x, :3], Lm[i]).max()
                eT = abs(Va[y, x, 3] - Tm[i].mean())
                if '-v' in a:
                    print('    haze %.4f slice %d (%.2f km) texel %d,%d: GPU %s a %.4f model %s T %.4f' % (
                        hz[0], k, D, x, y, np.round(Va[y, x, :3], 7), Va[y, x, 3], np.round(Lm[i], 7), Tm[i].mean()))
                worstL, worstT = max(worstL, eL), max(worstT, eT)
                if hz[0] > 0:
                    hazy.append((D, Va[y, x, :3].copy(), float(Va[y, x, 3])))
    g.draw(pt, tfb, tw, th, base, [])
    gate('aerial-perspective', worstL < 0.03 and worstT < 0.005,
         'worst relative in-scatter error = %.4f (< 0.03), worst |transmittance| = %.5f (< 0.005) at 40 froxels'
         % (worstL, worstT))
    # 4b. lane SKY2B (SKYTINT2): under Weather Tint the volume's in-scatter keeps its luminance and takes the dome's
    # chromaticity at the froxel ray's elevation (atmo_sky.frag's law); red notint draws the physical color
    tint = [np.array([1.0, 0.45, 0.15]) * (1 - i / 16.0) + np.array([0.15, 0.6, 1.0]) * (i / 16.0) for i in range(17)]
    ut = dict(ua, atApSlice=(15.0,), atTintOn=(0.0 if red == 'notint' else 1.0,))
    ut.update({'atTint[%d]' % i: tuple(tint[i]) for i in range(17)})
    Vp = g.draw(pa, afb, aw, ah, dict(ua, atApSlice=(15.0,), atTintOn=(0.0,)), [('atTransTex', tt), ('atMsTex', mt)], alpha=True)
    Vt = g.draw(pa, afb, aw, ah, ut, [('atTransTex', tt), ('atMsTex', mt)], alpha=True)
    Y = np.array([0.2126, 0.7152, 0.0722])
    lerr = cerr = 0.0
    for (x, y) in ((0, 0), (31, 31), (16, 4), (5, 27), (24, 12)):
        v = f0 + (x + 0.5) / aw * fx + (y + 0.5) / ah * fy
        el = math.degrees(math.asin(max(-1.0, min(1.0, v[2] / np.linalg.norm(v)))))
        t = math.sqrt(min(max(el, 0.0), 90.0) / 90.0) * 16.0
        i = min(int(t), 15)
        w = tint[i] * (1 - (t - i)) + tint[i + 1] * (t - i)
        lerr = max(lerr, abs(Vt[y, x, :3] @ Y - Vp[y, x, :3] @ Y) / max(Vp[y, x, :3] @ Y, 1e-12))
        cerr = max(cerr, np.abs(Vt[y, x, :3] / Vt[y, x, :3].sum() - w / w.sum()).max())
    gate('aerial-perspective tint', lerr < 1e-3 and cerr < 1e-3,
         'Weather Tint on the volume: luminance kept to %.5f, chromaticity vs the dome at the ray elevation %.5f (< 1e-3)'
         % (lerr, cerr))

    # 5. lane SKYA: the aerial perspective with the cell fog on (atmo_apfog.glsl, the shipped combine). One weather:
    # the hazy fixture above (Day Far 20 km, Max 0.6: the AP's haze rung is built from these numbers) with its own
    # game fog (linear from 0 to Far, capped at Max), plus a ground fog record (Far 3 km, Max 0.95) that is thicker
    # than the sky's air near the camera. Both media describe the SAME air, so the summed optical depth with both on
    # must be the larger of the two (min transmittance), never their sum; the fog's colour fills the residual only.
    # Red "double" (the fog laid on top at full strength) must fail.
    pf = build(sdir, None, [os.path.join(sdir, 'cell_ao.vert'), os.path.join(HERE, 'atmo_apfog_check.frag')])
    cf, cfb = g.target(64, 1)
    ap4, fog4, want = [], [], []
    fogC = np.array([0.55, 0.6, 0.65])
    for (farKm, mx) in ((20.0, 0.6), (3.0, 0.95)):
        for (D, Lg, Tg) in hazy:
            tfog = 1.0 - min(mx, mx * D / farKm)
            ap4.append((*Lg, Tg))
            fog4.append((*(fogC * (1.0 - tfog)), tfog))
            tc = min(Tg, tfog)
            tres = tc / Tg
            want.append((Lg * tres + fogC * (1.0 - tres), tc))
    n = len(ap4)
    u4 = S.fn('glUniform4fv', None, GLint, ctypes.c_int, ctypes.POINTER(ctypes.c_float))
    g.use(pf)
    for name, arr in (('apIn', ap4), ('fogIn', fog4)):
        flat = (ctypes.c_float * (4 * n))(*[c for v in arr for c in v])
        u4(g.loc(pf, name.encode()), n, flat)
    g.u1i(g.loc(pf, b'doubled'), 1 if red == 'double' else 0)
    C = g.draw(pf, cfb, 64, 1, {}, [], alpha=True)[0, :n]
    eOD = eL = 0.0
    for i in range(n):
        odw, odg = -math.log(max(want[i][1], 1e-9)), -math.log(max(C[i, 3], 1e-9))
        eOD = max(eOD, abs(odg - odw))
        eL = max(eL, np.abs(C[i, :3] - want[i][0]).max())
        if '-v' in a:
            print('    apfog %2d: Tap %.4f Tfog %.4f -> T %.4f (want %.4f) OD %.4f vs max(OD) %.4f' % (
                i, ap4[i][3], fog4[i][3], C[i, 3], want[i][1], odg, odw))
    gate('aerial-perspective + fog', eOD < 1e-3 and eL < 1e-3,
         'summed optical depth vs max(AP, fog) worst %.5f (< 0.001), in-scatter worst %.5f (< 0.001) at %d points'
         % (eOD, eL, n))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
