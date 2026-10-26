"""Lane CLOUD2: the volume clouds' offline gates. Runs the REAL res/shaders/cloud_march + cloud_resolve programs in the
hidden 4.1 core context (shader_link_check.py), with the exe's own noise bytes and calibration tables (tests/cloudvol/
cloudvol_dump.cpp, built here with g++), the sky's LUTs from the real atmo_*.prog (atmo_skyrender.Sky). No NifSkope.

    python -I tests/spells/cloudvol_check.py <scratch dir> [--red nocal|nooctaves|noreproj|flatkey] [--only A,B,..]

Gates (bars written before the first run):
  A coverage   the volume's column opacity seen from straight above, in five map stripes of coverage 0.1 .. 0.9, Low and
               Mid band: |mean - map| <= 0.05 each (the volume's projected coverage == CLOUD1's map). Red nocal fails.
  B sunset     a 2 deg sun; the underside colour seen from the ground (ambient and air in front off): its R/B and G/B
               within 10 % of the transmittance at the cloud base toward the sun (atmo_model.py, not the LUT). Red
               flatkey (a white transmittance LUT) fails.
  C dark core  a 0.25 km slab of tau 10, sun 60 deg up, seen straight up from below (ambient off): the underside
               radiance >= 0.25 x a Monte Carlo reference (the same droplet phase, albedo 0.99, multiple scattering).
               Red nooctaves (single scattering) fails.
  D flicker    a fly path (0.5 m and 0.4 deg of yaw a frame, 48 frames): the resolved history's frame-to-frame change
               minus the clouds' own (a full march of every pixel, averaged over 8 jitter frames), relative mean of
               the luminance, frames 17..47, <= 0.08. Red noreproj fails. (Amended after the first runs: see RESULTS.)
  E cost       GPU ms of march + resolve at 1080p per preset (Medium, High, Ultra), an overcast map, a camera on the
               ground looking 8 deg up: INFO, against the brief's budgets 0.5 / 0.8 / 1.5 ms.
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
only = set(args[args.index('--only') + 1].split(',')) if '--only' in args else set('ABCDE')
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
    out = subprocess.run(argv, capture_output=True, text=True, env=env, check=True).stdout.split('\n')
    cal = [list(map(float, l.split()[2:])) for l in out if l.startswith('cal ')]
    drop = [float(x) for l in out if l.startswith('drop ') for x in l.split()[1:]]
    return cal, drop


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
            'cvApOn': (ap,), 'cvFrame': (frame,), 'cvMaxKm': (maxKm,), 'cvSlab': (slab,), 'cvProbe': 0}


def march_texs(trans, sunview):
    sh, de = textures()
    return [('cvShape', 0x806F, sh), ('cvDetail', 0x806F, de), ('cvMap', 0x0DE1, MAP), ('cvTrans', 0x0DE1, trans),
            ('cvSunView', 0x0DE1, sunview), ('cvMoonView', 0x0DE1, sunview)]


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


def mc_underside(tau, mu0, prm, n=200000, cone=0.97, seed=7):
    """radiance leaving the slab's bottom straight down (within acos(cone)), per unit sun irradiance on a normal plane"""
    rng = np.random.default_rng(seed)
    cs = np.linspace(-1, 1, 20001)
    pdf = droplet(prm, cs) * 2 * math.pi
    cdf = np.concatenate([[0], np.cumsum((pdf[1:] + pdf[:-1]) / 2 * np.diff(cs))])
    cdf /= cdf[-1]
    z = np.full(n, tau)    # optical height above the bottom
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
        if out_b.any():
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
    gate('C dark core', under >= 0.25 * ref,
         'tau %.0f slab underside %.4g x sun vs Monte Carlo %.4g (ratio %.3f, bar >= 0.25)' % (tau, under, ref, under / max(ref, 1e-12)))

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
    print('E cost       INFO  ' + '; '.join(lines) + '; composite at 1920x1080 not timed (one bilinear read a pixel)')

print('CLOUDVOL-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
