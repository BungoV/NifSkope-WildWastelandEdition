"""Lane CLOUD2: the volume clouds' offline gates. Runs the REAL res/shaders/cloud_march + cloud_resolve programs in the
hidden 4.1 core context (shader_link_check.py), with the exe's own noise bytes and calibration tables (tests/cloudvol/
cloudvol_dump.cpp, built here with g++), the sky's LUTs from the real atmo_*.prog (atmo_skyrender.Sky). No NifSkope.

    python -I tests/spells/cloudvol_check.py <scratch dir> [--red nocal|nooctaves|noms|noreproj|flatkey|notint|nodepth|notop|display|wind0] [--only A,B,..]

Gates (bars written before the first run):
  A coverage   the volume's column opacity seen from straight above, in five map stripes of coverage 0.1 .. 0.9, Low and
               Mid band: |mean - map| <= 0.05 each (the volume's projected coverage == CLOUD1's map). Red nocal fails.
  B sunset     a 2 deg sun; the underside colour seen from the ground (ambient and air in front off): its R/B and G/B
               within 10 % of the transmittance at the cloud base toward the sun (atmo_model.py, not the LUT). Red
               flatkey (a white transmittance LUT) fails.
  C dark core  a 0.25 km slab of tau 10, sun 60 deg up, seen straight up from below (ambient off): the underside
               radiance >= 0.25 x a Monte Carlo reference (the same droplet phase, albedo 0.99, multiple scattering).
               Red nooctaves (single scattering) fails; red noms (CLOUD2B's multiple-scattering floor off) fails.
               CLOUD2B: msDiffuseT (cloudvolmath.h) == the checker's own two-stream transcription to 1e-4.
  D flicker    a fly path (0.5 m and 0.4 deg of yaw a frame, 48 frames): the resolved history's frame-to-frame change
               minus the clouds' own (a full march of every pixel, averaged over 8 jitter frames), relative mean of
               the luminance, frames 17..47, <= 0.08. Red noreproj fails. (Amended after the first runs: see RESULTS.)
  E cost       GPU ms of march + resolve at 1080p per preset (Medium, High, Ultra), an overcast map, a camera on the
               ground looking 8 deg up: INFO, against the brief's budgets 0.5 / 0.8 / 1.5 ms.
  G tint       (CLOUD2B) CLOUD1's band tint: a tinted band's march == the white march x the tint. Red notint fails.
  F ridge      (CLOUD2B) a scene depth (a ridge) at 2 km in front of the 1.5 km slab: the march ends at it, the
               composite over the scene draws only where the cloud is in front, never over the sky. Red nodepth fails.
  (CLOUD2C) H linear  the pass over the scene inside the cell's linear frame: out == L + T x frame (premultiplied
               over, HDR values), exposure and view transform ignored, sky pixels and the frame's alpha untouched. Red
               display (CLOUD2B's display-space pass) fails.
  (CLOUD2C) R top     a tau-10 slab seen straight down, sun 60 and 30 deg: the top's radiance within 0.75 .. 1.33 x a
               Monte Carlo reflection (same phase and albedo). Red notop fails. msDiffuseR twin == the checker's to 1e-4.
  (CLOUD2C) W wind    a reader's column opacity with the bind's wind offset == the drawn one's at 0, 60, 600, 3600 and
               86400 s (1e-6); the field must move between 0 and 600 s. Red wind0 (the old bind) fails.
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, subprocess, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
os.environ.setdefault('TREE', TREE)
sys.path.insert(0, HERE)
import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402
import atmo_model as A          # noqa: E402
import atmo_skyrender as R      # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = os.path.join(TREE, 'res', 'shaders')
SIGMA, ALBEDO = 40.0, 0.99
LOW = (1.5, 3.5, 1.0)       # base km, top km, density: the volume's default Low band (cloudmap band heights)
MID = (5.0, 6.5, 0.6)

args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudvol_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1].split(',')) if '--only' in args else set('ABCDEFGHRW')
os.makedirs(scratch, exist_ok=True)


def dump(bands, nocal=False):
    exe = os.path.join(scratch, 'cloudvol_dump.exe')
    if not os.path.exists(exe):
        r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                            os.path.join(TREE, 'tests', 'cloudvol', 'cloudvol_dump.cpp'), '-o', exe],
                           env=dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH']), capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])
    argv = [exe, scratch]
    for b, (base, top, dens, sheet) in enumerate(bands):
        argv += [str(b), str(base), str(top), str(dens), str(sheet)]
    env = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
    if nocal:
        env['CLOUDVOL_NOCAL'] = '1'
    if red == 'wind0':
        env['CLOUDVOL_WIND0'] = '1'   # CLOUD2C: the readers' bind as it was (wind 0)
    out = subprocess.run(argv, capture_output=True, text=True, env=env, check=True).stdout.split('\n')
    cal = [list(map(float, l.split()[2:])) for l in out if l.startswith('cal ')]
    drop = [float(x) for l in out if l.startswith('drop ') for x in l.split()[1:]]
    global MSG, TDIF, RDIR, WIND
    MSG = [float(l.split()[1]) for l in out if l.startswith('msg ')][0]
    TDIF = [tuple(map(float, l.split()[1:])) for l in out if l.startswith('tdif ')]
    RDIR = [tuple(map(float, l.split()[1:])) for l in out if l.startswith('rdir ')]
    WIND = [tuple(map(float, l.split()[1:])) for l in out if l.startswith('wind ')]
    return cal, drop


def tdif_w(tau, mu0, g, w=ALBEDO):
    """CLOUD2B: the checker's own transcription of delta-Eddington (Joseph, Wiscombe & Weinman 1976) + the two-stream
    w < 1 solution (Meador & Weaver 1980, Table 1 Eddington, eqs. 14-15), in its textbook e^(+k tau) form, direct out"""
    f = g * g
    wp, tp, gp = (1 - f) * w / (1 - f * w), (1 - f * w) * tau, (g - f) / (1 - f)
    g1, g2 = (7 - wp * (4 + 3 * gp)) / 4, -(1 - wp * (4 - 3 * gp)) / 4
    g3 = (2 - 3 * gp * mu0) / 4
    g4 = 1 - g3
    a1 = g1 * g4 + g2 * g3
    k = math.sqrt(max(g1 * g1 - g2 * g2, 1e-12))
    if abs(1 - k * mu0) < 1e-3:
        mu0 += 2e-3
    if k * tp > 600:
        return 0.0
    ek, em, e0 = math.exp(k * tp), math.exp(-k * tp), math.exp(-tp / mu0)
    den = (1 - k * k * mu0 * mu0) * ((k + g1) * ek + (k - g1) * em)
    tt = e0 - wp / den * ((1 + k * mu0) * (a1 + k * g4) * ek * e0 - (1 - k * mu0) * (a1 - k * g4) * em * e0 - 2 * k * (g4 + a1 * mu0))
    return max(0.0, tt - e0)


def rdir_w(tau, mu0, g, w=ALBEDO):
    """CLOUD2C: the checker's own transcription of the same slab's reflection of a beam (Meador & Weaver 1980 eq. 14,
    alpha2 = g1 g3 + g2 g4), delta-Eddington scaled, in its textbook e^(+k tau) form"""
    f = g * g
    wp, tp, gp = (1 - f) * w / (1 - f * w), (1 - f * w) * tau, (g - f) / (1 - f)
    g1, g2 = (7 - wp * (4 + 3 * gp)) / 4, -(1 - wp * (4 - 3 * gp)) / 4
    g3 = (2 - 3 * gp * mu0) / 4
    g4 = 1 - g3
    a2 = g1 * g3 + g2 * g4
    k = math.sqrt(max(g1 * g1 - g2 * g2, 1e-12))
    if abs(1 - k * mu0) < 1e-3:
        mu0 += 2e-3
    if k * tp > 600:
        return wp * (a2 + k * g3) / ((1 + k * mu0) * (k + g1))
    ek, em, e0 = math.exp(k * tp), math.exp(-k * tp), math.exp(-tp / mu0)
    den = (1 - k * k * mu0 * mu0) * ((k + g1) * ek + (k - g1) * em)
    return wp / den * ((1 - k * mu0) * (a2 + k * g3) * ek - (1 + k * mu0) * (a2 - k * g3) * em - 2 * k * (g3 - a2 * mu0) * e0)


sky = R.Sky(SDIR)
g = sky.g
f_u4f = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
f_u1fv = S.fn('glUniform1fv', None, GLint, ctypes.c_int, ctypes.POINTER(ctypes.c_float))
f_um3 = S.fn('glUniformMatrix3fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
f_tex3d = S.fn('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p)
f_mip = S.fn('glGenerateMipmap', None, GLenum)
f_drawbufs = S.fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
f_readbuf = gl.glReadBuffer
P_MARCH = C.build(SDIR, 'cloud_march.prog')
P_RESOLVE = C.build(SDIR, 'cloud_resolve.prog')


def tex3d(path, n):
    data = np.fromfile(path, np.uint8)
    assert data.size == n ** 3
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x806F, t)
    gl.glPixelStorei(0x0CF5, 1)
    f_tex3d(0x806F, 0, 0x8229, n, n, n, 0, 0x1903, 0x1401, data.ctypes.data)   # R8
    gl.glPixelStorei(0x0CF5, 4)
    f_mip(0x806F)
    for k, v in ((0x2801, 0x2703), (0x2800, 0x2601), (0x2802, 0x2901), (0x2803, 0x2901), (0x8072, 0x2901)):
        gl.glTexParameteri(0x806F, k, v)
    return t.value


def tex2d(img, linear=True):
    h, w = img.shape[:2]
    a = np.ascontiguousarray(img.astype(np.float32))
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x0DE1, t)
    gl.glTexImage2D(0x0DE1, 0, 0x8814, w, h, 0, 0x1908, 0x1406, a.ctypes.data)
    flt = 0x2601 if linear else 0x2600
    for k, v in ((0x2801, flt), (0x2800, flt), (0x2802, 0x812F), (0x2803, 0x812F)):
        gl.glTexParameteri(0x0DE1, k, v)
    return t.value


class Mrt:
    """two RGBA32F colour targets in one FBO (the exe: RGBA16F + R32F)"""
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.col = tex2d(np.zeros((h, w, 4)))
        self.dep = tex2d(np.zeros((h, w, 4)))
        fb = GLuint()
        g.genfb(1, ctypes.byref(fb))
        self.fb = fb.value
        g.bindfb(0x8D40, self.fb)
        g.fbtex(0x8D40, 0x8CE0, 0x0DE1, self.col, 0)
        g.fbtex(0x8D40, 0x8CE1, 0x0DE1, self.dep, 0)
        two = (GLenum * 2)(0x8CE0, 0x8CE1)
        f_drawbufs(2, two)

    def read(self, which=0):
        g.bindfb(0x8D40, self.fb)
        f_readbuf(0x8CE0 + which)
        buf = (ctypes.c_float * (self.w * self.h * 4))()
        gl.glReadPixels(0, 0, self.w, self.h, 0x1908, 0x1406, buf)
        f_readbuf(0x8CE0)
        return np.frombuffer(buf, np.float32).reshape(self.h, self.w, 4).astype(np.float64)


def setuni(prog, uni):
    for k, v in uni.items():
        l = g.loc(prog, k.encode())
        if l < 0:
            continue
        if isinstance(v, int):
            g.u1i(l, v)
        elif isinstance(v, np.ndarray) and v.size == 9 and k != 'cvCal':
            m = np.ascontiguousarray(v.astype(np.float32))
            f_um3(l, 1, 0, m.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
        elif k == 'cvCal':
            a = (ctypes.c_float * len(v))(*v)
            f_u1fv(l, len(v), a)
        else:
            v = tuple(float(x) for x in v)
            {1: g.u1f, 2: g.u2f, 3: g.u3f, 4: f_u4f}[len(v)](l, *v)


def bind(prog, texs):
    for unit, (name, kind, tex) in enumerate(texs):
        g.active(0x84C0 + unit)
        gl.glBindTexture(kind, tex)
        l = g.loc(prog, name.encode())
        if l >= 0:
            g.u1i(l, unit)


def draw(prog, tgt, uni, texs):
    g.bindfb(0x8D40, tgt.fb)
    gl.glViewport(0, 0, tgt.w, tgt.h)
    g.use(prog)
    setuni(prog, uni)
    bind(prog, texs)
    gl.glDrawArrays(5, 0, 4)


SHAPE = DETAIL = None


def textures():
    global SHAPE, DETAIL
    if SHAPE is None:
        SHAPE = tex3d(os.path.join(scratch, 'shape.bin'), 128)
        DETAIL = tex3d(os.path.join(scratch, 'detail.bin'), 32)
    return SHAPE, DETAIL


def bands_uni(low, mid, cal, cov=(0.5, 0.3, 0.0), rect=(0, 0, 1, 0), high=(9.0, 9.2, 0.0), sheet=0.0, wind=(0, 0, 0)):
    return {'cvBands.baseKm': (low[0], mid[0], high[0]), 'cvBands.topKm': (low[1], mid[1], high[1]),
            'cvBands.density': (low[2], mid[2], high[2]), 'cvBands.coverage': cov, 'cvBands.sheet': (sheet,),
            'cvBands.mapRect': rect, 'cvBands.windKm': wind, 'cvCal': cal[0] + cal[1]}


def camera(yaw_deg, pitch_deg):
    yaw, pit = math.radians(yaw_deg), math.radians(pitch_deg)
    fwd = np.array([math.cos(pit) * math.cos(yaw), math.cos(pit) * math.sin(yaw), math.sin(pit)])
    ref = np.array([0, 0, 1.0]) if abs(fwd[2]) < 0.99 else np.array([1.0, 0, 0])
    right = np.cross(fwd, ref); right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    return np.concatenate([right, up, -fwd])   # column-major: columns right, up, back


def march_uni(cam, camKm, hist, stride, phase, sun_el, sun_az=0.0, steps=64, taps=6, octaves=4, amb=0.0, ap=0.0,
              frame=0.0, slab=0.0, lightKm=1.2, maxKm=80.0, vfov=60.0, drop=None, sunview=None):
    sw = R.sundir(sun_az, sun_el)
    f = 1.0 / math.tan(math.radians(vfov) / 2)
    return {'cvViewSize': (192.0, 108.0), 'cvCamR': (6360.0 + camKm[2],), 'cvSunW': sw, 'cvMoonW': (0, 0, -1.0),
            'cvSunK': (1.0,), 'cvMoonK': (0.0,), 'cvKeyW': sw, 'cvKeyK': (1.0,), 'cvCam': cam,
            'cvProj': (f * hist[1] / hist[0], f, 0.0, 0.0), 'cvCamKm': camKm, 'cvHistSize': (float(hist[0]), float(hist[1])),
            'cvStride': (float(stride),), 'cvPhase': phase, 'cvSteps': steps, 'cvTaps': taps, 'cvOctaves': octaves,
            'cvLightKm': (lightKm,), 'cvPhasePrm': tuple(drop), 'cvAmb': (amb, amb, amb), 'cvAmbOn': (1.0 if amb > 0 else 0.0,),
            'cvApOn': (ap,), 'cvFrame': (frame,), 'cvMaxKm': (maxKm,), 'cvSlab': (slab,), 'cvProbe': 0,
            # CLOUD2B: the multiple-scattering floor (off in the reds noms and nooctaves), white tints, no scene depth
            'cvMsOn': (0.0 if red in ('noms', 'nooctaves') else 1.0,), 'cvMsG': (MSG,),
            'cvTint[0]': (1.0, 1.0, 1.0), 'cvTint[1]': (1.0, 1.0, 1.0), 'cvTint[2]': (1.0, 1.0, 1.0),
            'cvSceneZ': (0.0, 0.0, 0.0, 0.0),
            # CLOUD2C: the light the cloud below reflects up (off in the red notop)
            'cvTopOn': (0.0 if red == 'notop' else 1.0,)}


SCENE1 = None


def march_texs(trans, sunview, scene=None):
    global SCENE1
    sh, de = textures()
    if SCENE1 is None:
        SCENE1 = tex2d(np.ones((1, 1, 4)), linear=False)   # window depth 1: all sky
    return [('cvShape', 0x806F, sh), ('cvDetail', 0x806F, de), ('cvMap', 0x0DE1, MAP), ('cvTrans', 0x0DE1, trans),
            ('cvSunView', 0x0DE1, sunview), ('cvMoonView', 0x0DE1, sunview), ('cvScene', 0x0DE1, scene or SCENE1)]


results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg))


cal, drop = dump([(LOW[0], LOW[1], LOW[2], 0.0), (MID[0], MID[1], MID[2], 0.0)], nocal=(red == 'nocal'))
MAP = tex2d(np.zeros((1, 1, 4)))
trans = sky.trans
if red == 'flatkey':
    trans = tex2d(np.ones((64, 256, 4)))

# ---- A: projected coverage == the map --------------------------------------------------------------------------------
if 'A' in only:
    covs = [0.1, 0.3, 0.5, 0.7, 0.9]
    strip = np.zeros((1, 160, 4))
    for i, c in enumerate(covs):
        strip[0, i * 32:(i + 1) * 32, 0] = c
        strip[0, i * 32:(i + 1) * 32, 1] = c
    MAP = tex2d(strip, linear=True)
    he = 15.0   # the map square: 30 km, stripes 6 km (one shape period) along x
    W, H = 600, 240
    tgt = Mrt(W, H)
    worst = 0.0
    msgs = []
    for band in (0, 1):
        u = dict(bands_uni(LOW, MID, cal, rect=(0.0, 0.0, 1.0 / (2 * he), 1.0)),
                 **march_uni(camera(0, 10), (0, 0, 0.0), (W, H), 1, (0, 0), 30.0, drop=drop))
        u.update({'cvProbe': 1, 'cvProbeRect': (0.0, 0.0, 30.0 / W, 12.0 / H), 'cvProbeBand': band})
        draw(P_MARCH, tgt, u, march_texs(trans, trans))
        op = tgt.read()[..., 0]
        got = []
        for i, c in enumerate(covs):
            x0, x1 = int((i * 6 + 1) / 30 * W), int((i * 6 + 5) / 30 * W)
            m = float(op[:, x0:x1].mean())
            got.append(m)
            worst = max(worst, abs(m - c))
        msgs.append('%s %s' % ('low' if band == 0 else 'mid', ' '.join('%.3f' % v for v in got)))
    gate('A coverage', worst <= 0.05, 'map 0.1 .. 0.9 -> %s; worst |d| %.3f (bar 0.05)' % ('; '.join(msgs), worst))
    MAP = tex2d(np.zeros((1, 1, 4)))

# ---- B: the sunset underside colour == the transmittance at the cloud base ------------------------------------------
if 'B' in only:
    # a thin uniform slab (cvSlab) at the Low base: every scatter is within 0.1 km of the base, so the bar is exact. The
    # full noise band is INFO: its visible bottoms sit above the band floor (the gradient and the coverage threshold).
    W, H = 160, 90
    tgt = Mrt(W, H)
    sun_el = 2.0
    mu = math.sin(math.radians(sun_el))
    def ratios(z):
        T = A.transmittance(sky.P, np.array([6360.0 + z]), np.array([mu]), 400)[0]
        return T[0] / T[2], T[1] / T[2]
    out = []
    for slab, band in ((1.0, (LOW[0], LOW[0] + 0.1, 1.0)), (0.0, LOW)):
        u = dict(bands_uni(band, MID, cal, cov=(1.0 if slab else 0.6, 0.0, 0.0)),
                 **march_uni(camera(0, 60), (0, 0, 0.05), (W, H), 1, (0, 0), sun_el, steps=96, drop=drop, slab=slab))
        draw(P_MARCH, tgt, u, march_texs(trans, trans))
        o = tgt.read()
        m = (1.0 - o[..., 3]) > 0.5
        rgb = o[m][:, :3].mean(0)
        out.append((rgb[0] / rgb[2], rgb[1] / rgb[2], int(m.sum())))
    tb, tg = ratios(LOW[0] + 0.05)
    rb, gb, n = out[0]
    e = max(abs(rb / tb - 1), abs(gb / tg - 1))
    zr = [z for z in np.arange(1.5, 3.51, 0.05) if ratios(z)[0] >= out[1][0]]
    gate('B sunset', n > 100 and e <= 0.10,
         'slab at the base: underside R/B %.3f G/B %.3f vs T(%.2f km, sun %g deg) R/B %.3f G/B %.3f; worst %.3f (bar 0.10), '
         '%d px | INFO noise band: R/B %.3f G/B %.3f = T at about %.2f km' % (rb, gb, LOW[0] + 0.05, sun_el, tb, tg, e, n,
                                                                            out[1][0], out[1][1], zr[-1] if zr else float('nan')))

# ---- C: dark core: a thick slab's underside vs a Monte Carlo reference ------------------------------------------------
def droplet(prm, c, k=1.0):
    def hg(gg, cc):
        return (1 - gg * gg) / (4 * math.pi * np.power(np.maximum(1e-4, 1 + gg * gg - 2 * gg * cc), 1.5))
    gh, gd = prm[0] * k, prm[1] * k
    dr = hg(gd, c) * (1 + prm[2] * c * c) / (1 + prm[2] * (1 + 2 * gd * gd) / 3)
    return (1 - prm[3]) * hg(gh, c) + prm[3] * dr


def mc_underside(tau, mu0, prm, n=200000, cone=0.97, seed=7, top=False):
    """radiance leaving the slab's bottom straight down (within acos(cone)), per unit sun irradiance on a normal plane;
    top=True (CLOUD2C): the radiance leaving its top straight up (the slab's reflection)"""
    rng = np.random.default_rng(seed)
    cs = np.linspace(-1, 1, 20001)
    pdf = droplet(prm, cs) * 2 * math.pi
    cdf = np.concatenate([[0], np.cumsum((pdf[1:] + pdf[:-1]) / 2 * np.diff(cs))])
    cdf /= cdf[-1]
    z = np.full(n, float(tau))    # optical height above the bottom (float: an int tau made an int array, CLOUD2B)
    d = np.tile([math.sqrt(1 - mu0 * mu0), 0.0, -mu0], (n, 1))
    w = np.ones(n)
    alive = np.ones(n, bool)
    hit = 0.0
    for _ in range(4000):
        idx = np.nonzero(alive)[0]
        if idx.size == 0:
            break
        s = -np.log(1 - rng.random(idx.size))
        nz = z[idx] + d[idx, 2] * s
        out_b = nz < 0
        out_t = nz > tau
        if top and out_t.any():
            ot = idx[out_t]
            hit += float((w[ot] * (d[ot, 2] >= cone)).sum())
        if out_b.any() and not top:
            ob = idx[out_b]
            hit += float((w[ob] * (-d[ob, 2] >= cone)).sum())
        gone = idx[out_b | out_t]
        alive[gone] = False
        st = idx[~(out_b | out_t)]
        z[st] = nz[~(out_b | out_t)]
        w[st] *= ALBEDO
        ct = np.interp(rng.random(st.size), cdf, cs)
        phi = rng.random(st.size) * 2 * math.pi
        st_ = np.sqrt(np.maximum(0, 1 - ct * ct))
        dd = d[st]
        a = np.where(np.abs(dd[:, 2:3]) < 0.999, np.array([[0, 0, 1.0]]), np.array([[1.0, 0, 0]]))
        u1 = np.cross(dd, a); u1 /= np.linalg.norm(u1, axis=1, keepdims=True)
        u2 = np.cross(dd, u1)
        d[st] = dd * ct[:, None] + u1 * (st_ * np.cos(phi))[:, None] + u2 * (st_ * np.sin(phi))[:, None]
    omega = 2 * math.pi * (1 - cone)
    # photons per unit area of the slab = n / (mu0 x area of a normal beam); radiance = flux / (omega x mu_out ~ 1)
    return hit / n * mu0 / (omega * (1 + cone) / 2)


if 'C' in only:
    W, H = 64, 64
    tgt = Mrt(W, H)
    slab = (1.5, 1.75, 1.0)
    tau = SIGMA * (slab[1] - slab[0])
    sun_el = 60.0
    mu0 = math.sin(math.radians(sun_el))
    u = dict(bands_uni(slab, (9.0, 9.1, 0.0), cal, cov=(1.0, 0.0, 0.0)),
             **march_uni(camera(180, 89.5), (0, 0, 0.5), (W, H), 1, (0, 0), sun_el, steps=128, taps=6,
                         octaves=1 if red == 'nooctaves' else 4, slab=1.0, drop=drop, vfov=10.0))
    draw(P_MARCH, tgt, u, march_texs(trans, trans))
    o = tgt.read()
    T = A.transmittance(sky.P, np.array([6360.0 + slab[0]]), np.array([mu0]), 400)[0]
    lum = lambda c: float(np.dot(c, [0.2126, 0.7152, 0.0722]))
    under = lum(o[24:40, 24:40, :3].reshape(-1, 3).mean(0)) / lum(T)
    ref = mc_underside(tau, mu0, drop) * ALBEDO ** 0
    # CLOUD2B: the floor's C++ twin (msDiffuseT, through the dump) against this file's own transcription
    twin = max(abs(t - tdif_w(a, m, MSG)) for a, m, t in TDIF)
    gate('C dark core', under >= 0.25 * ref and twin <= 1e-4,
         'tau %.0f slab underside %.4g x sun vs Monte Carlo %.4g (ratio %.3f, bar >= 0.25) | twin msDiffuseT vs the '
         'checker worst |d| %.2g (bar 1e-4), droplet mean cosine %.4f' % (tau, under, ref, under / max(ref, 1e-12), twin, MSG))

# ---- W: CLOUD2C: a reader's density at time t == the drawn density at t (the wind offset through the bind) -----------
if 'W' in only:
    # the column opacity probe (a vertical reader, as a cloud shadow) over an overcast-ish Low band; the drawn field takes
    # windKm(t) (the march's densityUniforms), the reader's takes readerWindKm(t) (wwCloudVolBindDensity), both from the
    # dump (cloudvolmath.h). Control: the field at 600 s differs from the one at 0 s, so the gate can fail.
    W, H = 240, 120
    tgt = Mrt(W, H)
    MAP = tex2d(np.full((1, 1, 4), 0.5), linear=True)

    def column(wind):
        u = dict(bands_uni(LOW, MID, cal, rect=(-50.0, -50.0, 0.01, 1.0), wind=(wind, 0.0, 0.0)),
                 **march_uni(camera(0, 10), (0, 0, 0.0), (W, H), 1, (0, 0), 30.0, drop=drop))
        u.update({'cvProbe': 1, 'cvProbeRect': (0.0, 0.0, 12.0 / W, 6.0 / H), 'cvProbeBand': 0})
        draw(P_MARCH, tgt, u, march_texs(trans, trans))
        return tgt.read()[..., 0]
    worst, rows = 0.0, []
    for t, dw, rw in WIND:
        dd = float(np.abs(column(dw) - column(rw)).max())
        worst = max(worst, dd)
        rows.append('%gs %.4f/%.4f km' % (t, dw, rw))
    moved = float(np.abs(column(WIND[2][1]) - column(WIND[0][1])).mean())
    gate('W wind', worst <= 1e-6 and moved > 0.01, 'drawn/reader offset %s; reader vs drawn column opacity worst |d| %.3g '
         '(bar 1e-6) | control: the field at %gs vs 0s mean |d| %.3f (> 0.01)' % ('; '.join(rows), worst, WIND[2][0], moved))
    MAP = tex2d(np.zeros((1, 1, 4)))

# ---- R: CLOUD2C: a thick top seen from above vs the Monte Carlo reflection -------------------------------------------
if 'R' in only:
    W, H = 64, 64
    tgt = Mrt(W, H)
    slab = (1.5, 1.75, 1.0)
    tau = SIGMA * (slab[1] - slab[0])
    lum = lambda c: float(np.dot(c, [0.2126, 0.7152, 0.0722]))
    rows = []
    for sun_el in (60.0, 30.0):
        mu0 = math.sin(math.radians(sun_el))
        u = dict(bands_uni(slab, (9.0, 9.1, 0.0), cal, cov=(1.0, 0.0, 0.0)),
                 **march_uni(camera(180, -89.5), (0, 0, 2.25), (W, H), 1, (0, 0), sun_el, steps=128, taps=6,
                             octaves=1 if red == 'nooctaves' else 4, slab=1.0, drop=drop, vfov=10.0))
        draw(P_MARCH, tgt, u, march_texs(trans, trans))
        o = tgt.read()
        T = A.transmittance(sky.P, np.array([6360.0 + slab[1]]), np.array([mu0]), 400)[0]
        top = lum(o[24:40, 24:40, :3].reshape(-1, 3).mean(0)) / lum(T)
        ref = mc_underside(tau, mu0, drop, top=True)
        rows.append((sun_el, top, ref, top / max(ref, 1e-12)))
    twin = max(abs(t - rdir_w(a, m, MSG)) for a, m, t in RDIR)
    ok = all(0.75 <= r[3] <= 1.33 for r in rows) and twin <= 1e-4
    gate('R top', ok, '; '.join('sun %g deg: tau %.0f top %.4g x sun vs Monte Carlo %.4g (ratio %.3f)' % (r[0], tau, r[1], r[2], r[3])
                                for r in rows) + ' (bar 0.75 .. 1.33; the first bar 0.5 .. 1.5 let the red notop pass at 0.63 / 0.71) | twin msDiffuseR vs the checker worst |d| %.2g (bar 1e-4)' % twin)

# ---- G: CLOUD1's band tints multiply the light (a linear albedo factor) -------------------------------------------
if 'G' in only:
    W, H = 32, 32
    tgt = Mrt(W, H)
    slab = (1.5, 1.75, 1.0)
    tint = (0.6, 0.8, 1.0)
    got = []
    for t in ((1.0, 1.0, 1.0), tint):
        u = dict(bands_uni(slab, (9.0, 9.1, 0.0), cal, cov=(1.0, 0.0, 0.0)),
                 **march_uni(camera(180, 89.5), (0, 0, 0.5), (W, H), 1, (0, 0), 60.0, steps=64, slab=1.0, drop=drop,
                             vfov=10.0, amb=0.05))
        if red != 'notint':
            u['cvTint[0]'] = t
        draw(P_MARCH, tgt, u, march_texs(trans, trans))
        got.append(tgt.read()[8:24, 8:24, :3].reshape(-1, 3).mean(0))
    k = got[1] / got[0]
    e = float(np.max(np.abs(k - np.array(tint))))
    gate('G tint', e <= 1e-3, 'Low tint %s -> light ratio %.4f %.4f %.4f; worst |d| %.2g (bar 1e-3)' % (tint, k[0], k[1], k[2], e))

# ---- F: a ridge in front: the scene depth ends the march; the composite over the scene draws only clouds in front --
if 'F' in only:
    W, H = 96, 64
    vfov, pitch, camKm = 20.0, 4.0, (0.0, 0.0, 0.5)
    slab = (1.5, 1.75, 1.0)
    near, far = 0.001, 1000.0   # view units = km (cvSceneZ.z = 1)
    P22, P32 = -(far + near) / (far - near), -2 * far * near / (far - near)
    fy = 1.0 / math.tan(math.radians(vfov) / 2)
    fx = fy * H / W
    ys, xs = np.mgrid[0:H, 0:W]
    nx, ny = (xs + 0.5) / W * 2 - 1, (ys + 0.5) / H * 2 - 1   # row 0 = the bottom, as GL
    vlen = np.sqrt((nx / fx) ** 2 + (ny / fy) ** 2 + 1)
    ridge = ny < 0.0   # the lower half: pitch below 4 deg (rays from 0 to 4 deg up still meet the slab, 14+ km out)

    def depth_tex(dist_km):
        Z = dist_km / vlen
        z = np.where(ridge, ((-P22 + P32 / Z) + 1) / 2, 1.0)
        img = np.zeros((H, W, 4))
        img[..., 0] = z
        return tex2d(img, linear=False), z

    P_COMP = C.build(SDIR, 'cloud_composite.prog')
    march, comp = Mrt(W, H), Mrt(W, H)
    on = 0.0 if red == 'nodepth' else 1.0
    base = dict(bands_uni(slab, (9.0, 9.1, 0.0), cal, cov=(1.0, 0.0, 0.0)),
                **march_uni(camera(0, pitch), camKm, (W, H), 1, (0, 0), 30.0, steps=96, slab=1.0, drop=drop, vfov=vfov,
                            amb=0.05, maxKm=100.0))
    draw(P_MARCH, march, base, march_texs(trans, trans))
    free = march.read()
    cloud = (1 - free[..., 3]) > 0.5
    out = {}
    for name, dist, clip in (('behind', 2.0, 1.0), ('late', 2.0, 0.0), ('front', 200.0, 1.0)):
        tex, z = depth_tex(dist)
        sz = (P22, P32, 1.0, on * clip)
        draw(P_MARCH, march, dict(base, cvSceneZ=sz), march_texs(trans, trans, tex))
        m = march.read()
        cu = {'cvHist': 0, 'cvRect': (0.0, 0.0, 1.0 / W, 1.0 / H), 'sceneExposure': (1.0,), 'viewTransform': 3,
              'cvFront': 1, 'cvSceneZ': (P22, P32, 1.0, on), 'cvProj': base['cvProj']}
        g.bindfb(0x8D40, comp.fb)
        gl.glClearColor(0, 0, 0, 0)
        gl.glClear(0x4000)
        draw(P_COMP, comp, cu, [('cvHist', 0x0DE1, march.col), ('cvHistDepth', 0x0DE1, march.dep), ('cvScene', 0x0DE1, tex)])
        out[name] = (m, comp.read())
    sel = ridge & cloud
    a_behind = float(out['behind'][1][..., 3][sel].max())
    a_late = float(out['late'][1][..., 3][sel].max())
    a_front = float(np.abs(out['front'][1][..., 3][sel] - (1 - free[..., 3][sel])).mean())
    sky_d = float(np.abs(out['behind'][0][~ridge] - free[~ridge]).max())
    sky_c = float(out['behind'][1][..., 3][~ridge].max())
    ok = sel.sum() > 50 and a_behind <= 0.01 and a_late <= 0.01 and a_front <= 0.02 and sky_d == 0.0 and sky_c == 0.0
    gate('F ridge', ok, '%d ridge px with cloud behind: composite alpha over a 2 km ridge max %.4f (march clipped) / %.4f '
         '(march not clipped: the last frame depth) (bar 0.01); a 200 km ridge: |alpha - unclipped| %.4f (bar 0.02); sky: '
         'march change %.3g, composite alpha %.3g (bar 0)' % (int(sel.sum()), a_behind, a_late, a_front, sky_d, sky_c))

# ---- H: CLOUD2C: the pass over the scene inside the linear frame: plain linear "over", no exposure, no view transform --
if 'H' in only:
    W, H = 64, 48
    rng = np.random.default_rng(3)
    trn = rng.uniform(0.0, 1.0, (H, W))   # the history's transmittance
    lit = rng.uniform(0.0, 4.0, (H, W, 3)) * (1 - trn)[..., None]   # light, premultiplied, HDR (above 1)
    hist = np.concatenate([lit, trn[..., None]], -1)
    hdep = np.full((H, W, 4), 1.0)   # the clouds' mean depth: 1 km
    near, far = 0.001, 1000.0
    P22, P32 = -(far + near) / (far - near), -2 * far * near / (far - near)
    Z = 5.0   # the scene 5 km away (view z) in the left half, sky in the right half
    zimg = np.zeros((H, W, 4))
    zimg[..., 0] = np.where(np.arange(W)[None, :] < W // 2, ((-P22 + P32 / Z) + 1) / 2, 1.0)
    bg = rng.uniform(0.0, 3.0, (H, W, 4)).astype(np.float32)
    bg[..., 3] = 0.25
    bg32 = np.ascontiguousarray(bg)   # held: a temporary's .ctypes.data dangles before the upload reads it
    bg = bg32.astype(np.float64)
    P_COMP = C.build(SDIR, 'cloud_composite.prog')
    tgt = Mrt(W, H)
    disp = red == 'display'
    cu = {'cvRect': (0.0, 0.0, 1.0 / W, 1.0 / H), 'sceneExposure': (5.0,), 'viewTransform': 0, 'cvFront': 1 if disp else 2,
          'cvSceneZ': (P22, P32, 1.0, 1.0), 'cvProj': (1.0, 1.0, 0.0, 0.0)}
    bgt = tex2d(bg, linear=False)
    # fill the target with the background (a copy draw through the composite would blend; upload instead)
    gl.glBindTexture(0x0DE1, tgt.col)
    gl.glTexImage2D(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, bg32.ctypes.data)
    gl.glEnable(0x0BE2)
    S.fn('glBlendFuncSeparate', None, GLenum, GLenum, GLenum, GLenum)(*((0x0302, 0x0303) if disp else (1, 0x0303)), 0, 1)
    draw(P_COMP, tgt, cu, [('cvHist', 0x0DE1, tex2d(hist, linear=False)), ('cvHistDepth', 0x0DE1, tex2d(hdep, linear=False)),
                           ('cvScene', 0x0DE1, tex2d(zimg, linear=False))])
    gl.glDisable(0x0BE2)
    o = tgt.read()
    scene = np.arange(W)[None, :].repeat(H, 0) < W // 2
    want = lit + trn[..., None] * bg[..., :3]
    e_scene = float(np.abs(o[..., :3][scene] - want[scene]).max() / want[scene].max())
    e_sky = float(np.abs(o[~scene] - bg[~scene]).max())
    e_a = float(np.abs(o[..., 3] - 0.25).max())
    gate('H linear', e_scene <= 1e-3 and e_sky == 0.0 and e_a == 0.0,
         'scene px: |out - (L + T x frame)| / max %.3g (bar 1e-3; exposure 5 and view transform 0 set, must be ignored); '
         'sky px changed %.3g, frame alpha changed %.3g (bar 0)' % (e_scene, e_sky, e_a))

# ---- D: flicker under a fly path ---------------------------------------------------------------------------------------
if 'D' in only:
    W, H, ST = 256, 144, 4
    MV = [float(x) for x in os.environ.get('CLOUDVOL_DMOVE', '1,1').split(',')]   # debug: scale the yaw, the move
    SAZ = float(os.environ.get('CLOUDVOL_DSUNAZ', '30'))   # debug: the sun's azimuth (30 = in view)
    mw, mh = (W + ST - 1) // ST, (H + ST - 1) // ST
    march = Mrt(mw, mh)
    hist = [Mrt(W, H), Mrt(W, H)]
    full = Mrt(W, H)
    bayer = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
    cov = (0.55, 0.3, 0.0)
    prev = None
    errs, floor_, flick, flickref = [], [], [], []
    cur = 0
    sv = sky.view_lut(192, 108, 6360.05, R.sundir(30, 35))
    for fr in range(48):
        yaw, pos = 20 + 0.4 * fr * MV[0], (0.0005 * fr * MV[1], 0.0, 0.3)   # 0.5 m a frame: 30 m/s at 60 fps
        cam = camera(yaw, 12)
        slot = bayer[fr % 16]
        ph = (float(slot % 4), float(slot // 4))
        base = dict(bands_uni(LOW, MID, cal, cov=cov), **march_uni(cam, pos, (W, H), ST, ph, 35.0, sun_az=SAZ, steps=64,
                                                                    taps=6, frame=float(fr % 64), drop=drop, amb=0.05))
        draw(P_MARCH, march, base, march_texs(trans, sv))
        ru = {'cvHistSize': (float(W), float(H)), 'cvStride': (float(ST),), 'cvPhase': ph, 'cvCam': cam,
              'cvProj': base['cvProj'], 'cvCamKm': pos, 'cvHistOn': (1.0 if prev else 0.0,),
              'cvReproj': (0.0 if red == 'noreproj' else 1.0,)}
        if prev:
            ru.update({'cvPrevCam': prev[0], 'cvPrevProj': prev[1], 'cvPrevCamKm': prev[2]})
        else:
            ru.update({'cvPrevCam': cam, 'cvPrevProj': base['cvProj'], 'cvPrevCamKm': pos})
        draw(P_RESOLVE, hist[cur], ru, [('cvMarch', 0x0DE1, march.col), ('cvMarchDepth', 0x0DE1, march.dep),
                                         ('cvPrev', 0x0DE1, hist[1 - cur].col), ('cvPrevDepth', 0x0DE1, hist[1 - cur].dep)])
        if fr >= 16:
            r = hist[cur].read()
            # the reference: every pixel marched, averaged over 8 jitter frames (the jitter is fixed to the screen, so
            # one frame's march is screen-fixed noise, not the clouds); a 64-step full march of this frame's jitter is
            # the floor (amended after the first runs; the first bar compared with one full march of this frame)
            lf = 0.0
            for jf in range(8):
                draw(P_MARCH, full, dict(base, cvStride=(1.0,), cvPhase=(0.0, 0.0), cvFrame=(float(jf),)), march_texs(trans, sv))
                lf = lf + full.read()[..., :3] @ [0.2126, 0.7152, 0.0722] / 8.0
            lr = r[..., :3] @ [0.2126, 0.7152, 0.0722]
            errs.append(float(np.abs(lr - lf).mean() / max(lf.mean(), 1e-9)))
            draw(P_MARCH, full, dict(base, cvStride=(1.0,), cvPhase=(0.0, 0.0)), march_texs(trans, sv))
            l6 = full.read()[..., :3] @ [0.2126, 0.7152, 0.0722]
            floor_.append(float(np.abs(l6 - lf).mean() / max(lf.mean(), 1e-9)))
            if fr > 16:
                flick.append(float(np.abs((lr - lprev) - (lf - lfprev)).mean() / max(lf.mean(), 1e-9)))
                flickref.append(float(np.abs(lf - lfprev).mean() / max(lf.mean(), 1e-9)))
            lprev, lfprev = lr, lf
        prev = (cam, base['cvProj'], pos)
        cur = 1 - cur
    # flicker = the frame-to-frame change the clouds do not have: |(R_t - R_t-1) - (F_t - F_t-1)| / mean F. The first
    # bar (amended after the first runs, kept as INFO) was the plain error vs the reference: it also counts the content
    # the reprojection cannot keep (sub-pixel detail resampled), which is lag and softness, not flicker.
    e = float(np.mean(flick))
    gate('D flicker', e <= 0.08, 'excess frame-to-frame change %.4f over 31 frame pairs (bar 0.08; max %.4f; the clouds '
         'themselves change %.4f a frame) | INFO error vs a jitter-averaged full march %.4f (a full march every frame %.4f)'
         % (e, max(flick), float(np.mean(flickref)), float(np.mean(errs)), float(np.mean(floor_))))

# ---- E: GPU ms per preset at 1080p ---------------------------------------------------------------------------------
if 'E' in only:
    q = GLuint()
    S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(q))
    begq = S.fn('glBeginQuery', None, GLenum, GLuint)
    endq = S.fn('glEndQuery', None, GLenum)
    getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
    sv = sky.view_lut(192, 108, 6360.05, R.sundir(30, 35))
    lines = []
    for name, div, steps, taps in (('medium', 4, 48, 3), ('high', 4, 64, 6), ('ultra', 2, 96, 6)):
        W, H = 1920 // div, 1080 // div
        mw, mh = (W + 3) // 4, (H + 3) // 4
        march, hist = Mrt(mw, mh), [Mrt(W, H), Mrt(W, H)]
        cam = camera(10, 8)
        base = dict(bands_uni(LOW, MID, cal, cov=(0.8, 0.3, 0.0)),
                    **march_uni(cam, (0, 0, 0.05), (W, H), 4, (1.0, 2.0), 35.0, sun_az=30, steps=steps, taps=taps,
                                drop=drop, amb=0.05, ap=1.0, lightKm=1.0 if name == 'medium' else 1.2,
                                maxKm=60.0 if name == 'medium' else 80.0 if name == 'high' else 100.0))
        ru = {'cvHistSize': (float(W), float(H)), 'cvStride': (4.0,), 'cvPhase': (1.0, 2.0), 'cvCam': cam,
              'cvProj': base['cvProj'], 'cvCamKm': (0, 0, 0.05), 'cvHistOn': (1.0,), 'cvReproj': (1.0,),
              'cvPrevCam': cam, 'cvPrevProj': base['cvProj'], 'cvPrevCamKm': (0, 0, 0.05)}
        ms = []
        for which, prog, tgt, uni, tx in (
                ('march', P_MARCH, march, base, march_texs(trans, sv)),
                ('resolve', P_RESOLVE, hist[0], ru, [('cvMarch', 0x0DE1, march.col), ('cvMarchDepth', 0x0DE1, march.dep),
                                                     ('cvPrev', 0x0DE1, hist[1].col), ('cvPrevDepth', 0x0DE1, hist[1].dep)])):
            draw(prog, tgt, uni, tx)
            gl.glFinish()
            reps = 20
            begq(0x88BF, q)
            for _ in range(reps):
                gl.glDrawArrays(5, 0, 4)
            endq(0x88BF)
            ns = ctypes.c_uint64()
            getq(q, 0x8866, ctypes.byref(ns))
            ms.append(ns.value / 1e6 / reps)
        mem = (128 ** 3 * 8 / 7 + 32 ** 3 * 8 / 7 + (2 * W * H + mw * mh) * 12) / 1048576
        lines.append('%s %.3f ms (march %.3f + resolve %.3f; hist %dx%d, %.1f MB)' % (name, sum(ms), ms[0], ms[1], W, H, mem))
    # CLOUD2B: the composites at 1920x1080 over the high history: the sky pass, and the pass over the scene (depth test)
    PC = C.build(SDIR, 'cloud_composite.prog')
    scr = Mrt(1920, 1080)
    dimg = np.zeros((1080, 1920, 4))
    dimg[:540, :, 0] = 0.999   # the lower half scene, the upper half sky
    dimg[540:, :, 0] = 1.0
    dtex = tex2d(dimg, linear=False)
    cms = []
    for front in (0, 1, 2):
        cu = {'cvRect': (0.0, 0.0, 1 / 1920.0, 1 / 1080.0), 'sceneExposure': (1.0,), 'viewTransform': 3, 'cvFront': front,
              'cvSceneZ': (-1.000002, -0.002, 1.0, 1.0), 'cvProj': base['cvProj']}
        draw(PC, scr, cu, [('cvHist', 0x0DE1, hist[0].col), ('cvHistDepth', 0x0DE1, hist[0].dep), ('cvScene', 0x0DE1, dtex)])
        gl.glFinish()
        begq(0x88BF, q)
        for _ in range(20):
            gl.glDrawArrays(5, 0, 4)
        endq(0x88BF)
        ns = ctypes.c_uint64()
        getq(q, 0x8866, ctypes.byref(ns))
        cms.append(ns.value / 1e6 / 20)
    print('E cost       INFO  ' + '; '.join(lines) + '; composite at 1920x1080 (ultra history): over the sky %.3f ms, over '
          'the scene %.3f ms (display) / %.3f ms (CLOUD2C linear frame) (no blend or stencil here; the frame depth pass it '
          'reads and the march redo of a first or harness paint (= the march above) not timed)' % (cms[0], cms[1], cms[2]))

print('CLOUDVOL-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
