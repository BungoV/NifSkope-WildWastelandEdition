"""SKYDISC1: offline gates for our sun disc (atmo_sky.frag, atDiscHue) and moon globe (sky_moon.frag), no NifSkope.
Draws the real shaders in the hidden 4.1 context (atmo_skyrender.Sky for the sky and the LUTs), then reads pixels back.

  python -I tests/spells/skydisc_check.py [--tex DIR] [--png DIR] [--dump EXE] [--red vanilla|nophase]

Gates: true angular sizes in px at known fovs (sun 0.2665 deg, moon 0.26 deg half-angle), no pixel written past the
disc + 1 px (no halo), the sun's limb profile (mu^a, Neckel-Labs a per channel) and its hue kept past the cap, the moon's
lit fraction against (1 + cos alpha) / 2 at six phase angles, its disc mean against Krisciunas-Schaefer, earthshine > 0
and redder low, and the OFF identity (atDiscHue 0 = the base's atmo_sky.frag, pixel for pixel).
Reds: vanilla = the old disc (per-channel min cap, no limb below High) and the vanilla moon quad (iMasserSize 90 /
fSunXExtreme 600): fails size/halo/limb; nophase = the moon lit from behind the eye: fails the phase gate."""
import argparse, ctypes, math, os, struct, subprocess, sys, tempfile, shutil
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atmo_skyrender as R   # noqa
S, A, gl = R.S, R.A, R.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
TREE = R.TREE
SDIR = TREE + '/res/shaders'

# ---- gl/skydisc.h, the Python twin (checked against tests/skydisc/skydisc_dump.cpp with --dump) ----
RAD = math.pi / 180
SUN_HALF, MOON_HALF, ALB_MEAN, MOON_OVER_SUN = 0.2665, 0.26, 0.30, 2500.0 / 1.6e9
SUN_SOLID, MOON_PEAK, EARTH_GAIN = 6.7967e-5, 0.6, 200.0
LIMB_A = np.array([0.397, 0.503, 0.652])


def ks(a):
    a = abs(a)
    return 10 ** (-0.4 * (0.026 * a + 4e-9 * a ** 4))


def ls_mean(a, n=120):
    sa, ca = math.sin(a * RAD), math.cos(a * RAD)
    x = (np.arange(n) + 0.5) / n * 2 - 1
    X, Y = np.meshgrid(x, x)
    r2 = X * X + Y * Y
    m = r2 < 1
    z = np.sqrt(np.clip(1 - r2, 0, None))
    mu0 = np.clip(X * sa + z * ca, 0, None)
    v = np.where(mu0 > 0, mu0 / np.maximum(mu0 + z, 1e-12), 0)
    return float(v[m].mean())


def earthshine(a):
    x = math.pi - abs(a) * RAD
    v = 1.0
    if x > 1e-6:
        v = 1 - math.sin(x / 2) * math.tan(x / 2) * math.log(1 / math.tan(x / 4))
    return max(0.0, 0.19 * 0.5 * v) / 1905.0


def moon_terms(a, sunK, expo, gain=EARTH_GAIN):
    R_ = min(sunK / SUN_SOLID * MOON_OVER_SUN, MOON_PEAK / max(expo, 1e-9))
    ls = ls_mean(a)
    return R_, (R_ * ks(a) / ls if ls > 1e-6 else 0.0), R_ * earthshine(a) * gain


def moon_lod(width, px):
    p = width / (2 * math.pi) * px / math.sin(MOON_HALF * RAD)
    return math.log2(p) if p > 1 else 0.0


def nrm(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def body_frame(D, P, l):
    o = -D
    u = nrm(o - np.dot(o, P) * P)
    pu = np.cross(P, u)
    ex = math.cos(l * RAD) * u - math.sin(l * RAD) * pu
    return ex, np.cross(P, ex), P


def sun_from_elong(M, P, e):
    w = nrm(np.cross(M, P))
    return nrm(math.cos(e * RAD) * M + math.sin(e * RAD) * w)


# ---- GL helpers ----
def ubo_free(src):
    import re
    s, n = re.subn(r'layout\s*\(\s*std140\s*\)\s*uniform\s+globalUniforms\s*\{.*?\};',
                   'uniform mat3 viewMatrix;\nuniform mat4 projectionMatrix;', src, flags=re.S)
    assert n == 1
    return s


def load_dds(path):
    b = open(path, 'rb').read()
    h, w, mips = struct.unpack('<III', b[12:24])
    cc = b[84:88]
    fmt, blk = {b'DXT1': (0x83F0, 8), b'ATI2': (0x8DBD, 16)}[cc]
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x0DE1, t)
    up = S.fn('glCompressedTexImage2D', None, GLenum, GLint, GLenum, ctypes.c_int, ctypes.c_int, GLint, ctypes.c_int, ctypes.c_void_p)
    off = 128
    for lv in range(max(mips, 1)):
        lw, lh = max(1, w >> lv), max(1, h >> lv)
        n = max(1, (lw + 3) // 4) * max(1, (lh + 3) // 4) * blk
        buf = ctypes.create_string_buffer(b[off:off + n], n)
        up(0x0DE1, lv, fmt, lw, lh, 0, n, buf)
        off += n
    gl.glTexParameteri(0x0DE1, 0x813D, max(mips, 1) - 1)   # GL_TEXTURE_MAX_LEVEL
    for k, v in ((0x2801, 0x2703), (0x2800, 0x2601), (0x2802, 0x2901), (0x2803, 0x812F)):
        gl.glTexParameteri(0x0DE1, k, v)
    return t.value, (w, h, mips)


def flat_tex(rgba):
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x0DE1, t)
    px = (ctypes.c_ubyte * 4)(*rgba)
    gl.glTexImage2D(0x0DE1, 0, 0x8058, 1, 1, 0, 0x1908, 0x1401, px)
    for k, v in ((0x2801, 0x2601), (0x2800, 0x2601)):
        gl.glTexParameteri(0x0DE1, k, v)
    return t.value


class Moon:
    def __init__(self, sky, tex_dir):
        self.sky = sky
        vs = ubo_free(S.expand(SDIR + '/lookdev_bill.vert'))
        fs = ubo_free(S.expand(SDIR + '/sky_moon.frag'))
        self.p = R.compile_src(vs, fs)
        self.flat = (flat_tex((149, 149, 149, 255)), flat_tex((128, 128, 255, 255)))
        self.real = None
        if tex_dir and os.path.exists(tex_dir + '/Moon_d.dds'):
            self.real = (load_dds(tex_dir + '/Moon_d.dds')[0], load_dds(tex_dir + '/Moon_n.dds')[0])
        self.bf = S.fn('glBlendFuncSeparate', None, GLenum, GLenum, GLenum, GLenum)

    def draw(self, W, H, D, Sd, P, lib=0.0, real=False, expo=1.0, blend=True, earth=True, half=None, alpha=1.0):
        """over the sky pass Sky.render just drew (same camera); returns RGBA (row 0 = top)"""
        L = self.sky.last
        g = self.sky.g
        if not blend:
            gl.glClearColor(0, 0, 0, 0)
            gl.glClear(0x4000)
        g.use(self.p)
        lm = lambda n: g.loc(self.p, n.encode())
        fp = ctypes.POINTER(ctypes.c_float)
        self.sky.um3(lm('viewMatrix'), 1, 1, L['M'].ctypes.data_as(fp))
        self.sky.um4(lm('projectionMatrix'), 1, 1, L['P'].ctypes.data_as(fp))
        px = 2.0 / (float(L['P'][1, 1]) * H)
        alph = 180 - math.degrees(math.acos(max(-1, min(1, float(np.dot(D, Sd))))))
        sunK = L['atSunK'][0]
        Rr, st, et = moon_terms(alph, sunK, expo)
        ex, ey, ez = body_frame(np.asarray(D), np.asarray(P), lib)
        u = dict(atViewSize=L['atViewSize'], atCamR=(L['camR'],), atSunW=L['atSunW'], atMoonW=(0.0, 0.0, -1.0),
                 atSunK=(sunK,), atMoonK=(0.0,), atTintOn=(0.0,), moonD=tuple(D), moonS=tuple(Sd), bodyX=tuple(ex),
                 bodyY=tuple(ey), bodyZ=tuple(ez), sinR=(math.sin(MOON_HALF * RAD),), pxAng=(px,),
                 lodD=(moon_lod(2048, px),), lodN=(moon_lod(1024, px),), albedoInv=(1 / ALB_MEAN,), sunTerm=(st,),
                 earthTerm=(et if earth else 0.0,), alpha=(alpha,), sceneExposure=(expo,), centerWorld=tuple(D),
                 halfSize=(half if half is not None else math.tan(MOON_HALF * RAD) * 1.5 + 2 * px,))
        for k, v in u.items():
            l = lm(k)
            if l >= 0:
                {1: g.u1f, 2: g.u2f, 3: g.u3f}[len(v)](l, *[float(x) for x in v])
        self.sky.u4f(lm('atRect'), 0, 0, 1.0 / W, 1.0 / H)
        g.u1i(lm('viewTransform'), 3)
        texs = self.real if real and self.real else self.flat
        for unit, (name, tex) in enumerate([('atAlbedo', texs[0]), ('atRelief', texs[1]), ('atTransTex', self.sky.trans),
                                            ('atSunView', L['sv']), ('atMoonView', L['sv'])]):
            g.active(0x84C0 + unit)
            gl.glBindTexture(0x0DE1, tex)
            g.u1i(lm(name), unit)
        if blend:
            gl.glEnable(0x0BE2)
            self.bf(1, 0x0303, 0, 1)
        gl.glDrawArrays(5, 0, 4)
        gl.glDisable(0x0BE2)
        buf = (ctypes.c_float * (W * H * 4))()
        gl.glReadPixels(0, 0, W, H, 0x1908, 0x1406, buf)
        img = np.frombuffer(buf, np.float32).reshape(H, W, 4)[::-1].astype(np.float64)
        return img, dict(alpha=alph, R=Rr, sunTerm=st, earthTerm=et, px=px)


def focal(H, vfov):
    return (H / 2) / math.tan(math.radians(vfov) / 2)


def radial(W, H, vfov, ang_center=None):
    """each pixel's angle (rad) off the view centre, for the camera aimed at the body"""
    f = focal(H, vfov)
    x = np.arange(W) + 0.5 - W / 2
    y = np.arange(H) + 0.5 - H / 2
    X, Y = np.meshgrid(x, y)
    return np.arctan(np.hypot(X, Y) / f), np.arctan2(Y, X)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tex', default='E:/Projects/Fallout 4 Mods/mods/FO4CS/Textures/Sky/Moon')
    ap.add_argument('--png', default=None)
    ap.add_argument('--dump', default=None)
    ap.add_argument('--red', default='', choices=['', 'vanilla', 'nophase'])
    ap.add_argument('--base', default='origin/merge1026-20261026', help='the commit whose atmo_sky.frag is the OFF reference')
    a = ap.parse_args()
    fails = []

    def gate(name, ok, msg):
        print(('PASS ' if ok else 'FAIL ') + name + ': ' + msg)
        if not ok:
            fails.append(name)

    sky = R.Sky(SDIR)
    van = a.red == 'vanilla'
    hueOn = 0.0 if van else 1.0
    limbOn = 0.0 if van else 1.0   # the old disc below High had no limb darkening
    CAP = 48.0                      # atmosphere.cpp kDiscCap

    # ---- 1. sun: size, halo, limb, hue ----
    for (W, H, vfov, tol) in ((1080, 1080, 55.0, 0.06), (1080, 1080, 10.0, 0.015), (1080, 1080, 2.0, 0.005)):
        sun = (30.0, 40.0)
        ex = dict(atLimb=(limbOn,), atDiscHue=(hueOn,))
        on, _ = sky.render(W, H, sun, sun, vfov=vfov, disc=CAP, extra=ex)
        off, _ = sky.render(W, H, sun, sun, vfov=vfov, disc=0.0, extra=ex)
        d = (on - off).max(axis=2)
        n = int((d > 1e-6).sum())
        want = 2 * focal(H, vfov) * math.tan(SUN_HALF * RAD)
        got = 2 * math.sqrt(n / math.pi)
        gate('sun size %.0f deg' % vfov, abs(got / want - 1) < tol, 'diameter %.2f px, true %.2f px (%+.2f%%)' % (got, want, 100 * (got / want - 1)))
        th, _ = radial(W, H, vfov)
        far = int(((d > 1e-6) & (th > SUN_HALF * RAD + 1.0 / focal(H, vfov))).sum())
        gate('sun no halo %.0f deg' % vfov, far == 0, '%d pixels past the disc + 1 px' % far)
        if vfov == 2.0:
            r = th / (SUN_HALF * RAD)
            disc = on - off
            c0 = disc[(r < 0.05)].mean(axis=0)
            errs = []
            for rr in (0.3, 0.5, 0.7, 0.85, 0.95):
                m = np.abs(r - rr) < 0.01
                v = disc[m].mean(axis=0) / c0
                want_v = np.power(np.sqrt(1 - rr * rr), LIMB_A)
                errs.append(float(np.abs(v - want_v).max()))
            gate('sun limb profile', max(errs) < 0.02, 'max |I(r)/I(0) - mu^a| %.4f over r 0.3..0.95' % max(errs))
            hue = c0 / c0.max()
            gate('sun hue kept', hue.min() < 0.995, 'centre rgb / max %.3f %.3f %.3f (the min cap makes it 1 1 1)' % tuple(hue))
            gate('sun centre at the cap', abs(c0.max() / CAP - 1) < 0.01, 'centre max %.3f vs cap %.1f' % (c0.max(), CAP))
            if a.png:
                img8, _ = R.to8(np.clip(on, 0, None), 0.5 / CAP)
                cy, cx = H // 2, W // 2
                k = int(want * 0.8)
                from PIL import Image
                Image.fromarray(img8[cy - k:cy + k, cx - k:cx + k]).resize((4 * k, 4 * k), Image.NEAREST).save(a.png + '/sun_disc%s.png' % ('_red' if van else ''))

    # ---- 2. OFF identity: atDiscHue 0 against the base's atmo_sky.frag ----
    if not van:
        tmp = tempfile.mkdtemp(prefix='skydisc_base_', dir=os.environ.get('SKYDISC_TMP', None))
        try:
            for f in os.listdir(SDIR):
                shutil.copy(os.path.join(SDIR, f), tmp)
            base = subprocess.run(['git', '-C', TREE, 'show', a.base + ':res/shaders/atmo_sky.frag'], capture_output=True).stdout
            assert base, 'git show of the base frag gave nothing'
            open(os.path.join(tmp, 'atmo_sky.frag'), 'wb').write(base)
            # the base program in THIS context (a second Sky makes a new context whose object names alias ours)
            ps0 = R.compile_src(S.expand(os.path.join(tmp, 'cell_ao.vert')), ubo_free(S.expand(os.path.join(tmp, 'atmo_sky.frag'))))
            ps1 = sky.ps

            def pair(sun, lim, hue):
                sky.ps = ps1
                p1, _ = sky.render(640, 360, sun, sun, vfov=8.0, disc=CAP, extra=dict(atLimb=(lim,), atDiscHue=(hue,)))
                sky.ps = ps0
                p0, _ = sky.render(640, 360, sun, sun, vfov=8.0, disc=CAP, extra=dict(atLimb=(lim,)))
                sky.ps = ps1
                return float(np.abs(p1 - p0).max())
            worst = max(pair(sun, lim, 0.0) for lim in (0.0, 1.0) for sun in ((30.0, 40.0), (100.0, 3.0)))
            on = pair((30.0, 40.0), 1.0, 1.0)
            gate('off compare can fail', on > 1.0, 'atDiscHue 1 vs the base: max |diff| %.3g (must differ)' % on)
            gate('off identity', worst == 0.0, 'atDiscHue 0 vs %s atmo_sky.frag: max |diff| %g (limb 0/1, two suns)' % (a.base, worst))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        sky.g.use(0)

    # ---- 3. moon: size and halo (coverage only), the vanilla quad as the red ----
    moon = Moon(sky, a.tex)
    P = nrm([0.0, math.cos(42.36 * RAD), math.sin(42.36 * RAD)])
    for (W, H, vfov, tol) in ((1080, 1080, 55.0, 0.04), (1080, 1080, 10.0, 0.01), (1080, 1080, 2.0, 0.005)):
        az, el = 200.0, 35.0
        D = np.array(R.sundir(az, el))
        Sd = sun_from_elong(D, P, 180.0)
        sky.render(W, H, (az, el), (20.0, -30.0), vfov=vfov, disc=0.0)
        half = 90.0 / 600.0 if van else None
        img, info = moon.draw(W, H, D, Sd, P, blend=False, half=half)
        cov = img[..., 3]
        if van:
            # the vanilla billboard writes its whole quad (a texture with disc + glow): coverage = the quad
            cov = np.zeros_like(cov)
            f = focal(H, vfov)
            k = min(W // 2, int(round(f * half)))
            cov[H // 2 - k:H // 2 + k, W // 2 - k:W // 2 + k] = 1.0
        want = 2 * focal(H, vfov) * math.tan(MOON_HALF * RAD)
        got = 2 * math.sqrt(cov.sum() / math.pi)
        gate('moon size %.0f deg' % vfov, abs(got / want - 1) < tol, 'diameter %.2f px, true %.2f px (%+.2f%%)' % (got, want, 100 * (got / want - 1)))
        th, _ = radial(W, H, vfov)
        far = int(((cov > 0) & (th > MOON_HALF * RAD + 1.0 / focal(H, vfov))).sum())
        gate('moon no halo %.0f deg' % vfov, far == 0, '%d pixels past the disc + 1 px' % far)

    # ---- 4. moon phase: lit fraction vs (1 + cos a) / 2, disc mean vs KS(a) (flat albedo, no earthshine) ----
    W = H = 720
    vfov = 1.0
    az, el = 160.0, 50.0
    D = np.array(R.sundir(az, el))
    sky.render(W, H, (az, el), (20.0, -40.0), vfov=vfov, disc=0.0)
    bg = sky.render(W, H, (az, el), (20.0, -40.0), vfov=vfov, disc=0.0)[0]
    th, _ = radial(W, H, vfov)
    inside = th < MOON_HALF * RAD * 0.995
    worst_f, worst_m, rows = 0.0, 0.0, []
    for alph in (0, 30, 60, 90, 120, 150):
        Sd = sun_from_elong(D, P, 180.0 + alph)   # waning side: the sun east of the moon
        if a.red == 'nophase':
            Sd = -D
        sky.render(W, H, (az, el), (20.0, -40.0), vfov=vfov, disc=0.0)
        img, info = moon.draw(W, H, D, Sd, P, earth=False)
        m = (img[..., :3] - bg).max(axis=2)
        lit = float((m[inside] > info['R'] * 1e-3).mean())
        want = 0.5 * (1 + math.cos(alph * RAD))
        y = (img[..., :3] - bg) @ np.array([0.2126, 0.7152, 0.0722])
        mean = float(y[th < MOON_HALF * RAD].sum() / (math.pi * (focal(H, vfov) * math.tan(MOON_HALF * RAD)) ** 2)) / info['R']
        tt = float(np.dot(A.transmittance(sky.P, np.array([6360.02]), np.array([D[2]]), 400)[0], [0.2126, 0.7152, 0.0722]))
        km = ks(alph) * tt
        worst_f = max(worst_f, abs(lit - want))
        worst_m = max(worst_m, abs(mean / km - 1) if km > 1e-3 else 0.0)
        rows.append('a=%d lit %.3f want %.3f mean/R %.4f KS*T %.4f' % (alph, lit, want, mean, km))
    for r_ in rows:
        print('  ' + r_)
    gate('moon phase lit fraction', worst_f < 0.02, 'max |lit - (1+cos a)/2| %.4f' % worst_f)
    gate('moon disc mean vs KS', worst_m < 0.03, 'max |mean / (KS(a) T) - 1| %.4f (alpha <= 150)' % worst_m)

    # ---- 5. earthshine: > 0 on the night side, redder and dimmer low ----
    es = {}
    for el2 in (60.0, 6.0):
        D2 = np.array(R.sundir(az, el2))
        Sd = sun_from_elong(D2, P, 180.0 + 150.0)
        sky.render(W, H, (az, el2), (20.0, -40.0), vfov=vfov, disc=0.0)
        bg2 = sky.render(W, H, (az, el2), (20.0, -40.0), vfov=vfov, disc=0.0)[0]
        img, info = moon.draw(W, H, D2, Sd, P, real=moon.real is not None)
        # the night side: pixels lit only by the Earth (the no-earth render has nothing there)
        img0, _ = moon.draw(W, H, D2, Sd, P, real=moon.real is not None, earth=False)
        night = inside & ((img0[..., :3] - bg2).max(axis=2) <= 1e-9)
        e = (img[..., :3] - bg2)[night].mean(axis=0)
        es[el2] = e
        if a.png and el2 == 60.0:
            from PIL import Image
            c = img[..., :3]
            Image.fromarray(R.to8(np.clip(c, 0, None), 0.5 / max(info['R'], 1e-9))[0]).save(a.png + '/moon_crescent150.png')
            Image.fromarray(R.to8(np.clip(c, 0, None), 25.0 / max(info['R'], 1e-9))[0]).save(a.png + '/moon_earthshine_x50.png')
    e60, e6 = es[60.0], es[6.0]
    gate('earthshine > 0', e60.min() > 0, 'night side mean rgb %.3g %.3g %.3g (scene units, el 60)' % tuple(e60))
    gate('earthshine reddened low', e6[0] / e6[2] > e60[0] / e60[2] and e6.mean() < e60.mean(),
         'R/B %.3f at el 6 vs %.3f at el 60; mean %.3g vs %.3g' % (e6[0] / e6[2], e60[0] / e60[2], e6.mean(), e60.mean()))

    # ---- 6. pictures: the phases with the real surface, and the moon at true size in a 55-deg day sky ----
    if a.png and moon.real:
        from PIL import Image
        tiles = []
        for alph in (0, 45, 90, 135, 160):
            Sd = sun_from_elong(D, P, 180.0 + alph)
            sky.render(360, 360, (az, el), (20.0, -40.0), vfov=1.0, disc=0.0)
            img, info = moon.draw(360, 360, D, Sd, P, real=True)
            tiles.append(R.to8(np.clip(img[..., :3], 0, None), 0.5 / info['R'])[0])
        Image.fromarray(np.concatenate(tiles, axis=1)).save(a.png + '/moon_phases%s.png' % ('_nophase' if a.red == 'nophase' else ''))
        sun = (250.0, 30.0)
        Dm = np.array(R.sundir(az, 40.0))
        sky.render(1920, 1080, (az, 40.0), sun, vfov=55.0, disc=CAP, extra=dict(atLimb=(1.0,), atDiscHue=(1.0,)))
        Sd = nrm(R.sundir(*sun))
        img, info = moon.draw(1920, 1080, Dm, Sd, P, real=True)
        Image.fromarray(R.to8(np.clip(img[..., :3], 0, None))[0]).save(a.png + '/moon_day_55deg.png')

    # ---- 7. the C++ math against this twin ----
    if a.dump:
        out = subprocess.run([a.dump], capture_output=True, text=True).stdout.split('\n')
        worst = 0.0
        for line in out:
            p = line.split()
            if p and p[0] == 'phase':
                al = float(p[1])
                worst = max(worst, abs(float(p[2]) - ls_mean(al, 160)), abs(float(p[3]) - ks(al)),
                            abs(float(p[5]) - earthshine(al)) * 1e4)
        gate('twin vs skydisc_dump', worst < 1e-5, 'max diff %.2g (LS mean, KS, earthshine x1e4)' % worst)

    print('%d gates failed%s' % (len(fails), (': ' + ', '.join(fails)) if fails else ''))
    print('SKYDISC %s' % ('FAIL' if fails else 'PASS'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
