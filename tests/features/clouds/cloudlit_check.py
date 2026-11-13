"""Lane CLOUDLIT1 (bungo: "2d clouds should probably be lit like that, from our sky"): the 2D cloud layer
(res/shaders/hlsl/clouds/lookdev_clouds_ps.hlsl) lit as the volume's thin sheet (cloud_march_ps.hlsl, the High band) when
the physical sky is on, checked offline against the volume. Both programs are the readable HLSL compiled to SPIR-V
(tests/hlsllive/build_all.py), drawn through the real driver in the volume checker's context; no NifSkope.

    python -I -X utf8 tests/features/clouds/cloudlit_check.py <scratch dir> <build_all out dir>

<out>/spirv holds lookdev_clouds_vs/ps.spv, cell_ao_vs.spv and cloud_march_ps.spv
(build_all.py <tree> <out> --only cloud_march.prog, then --only lookdev_clouds).

Setup: the deck at 1.5 km, the camera 10 m under it looking straight up. The layer in probe mode (wdir = the zenith; left
half the colour, linear, right half the opacity); the volume: its thin sheet only (High band base 1.5 km, Low and Mid off,
cover 1), a 4-degree field, cvApOn 0. Sun at 30, 10, 2, -2 degrees; the same transmittance LUT (atmo_skyrender.Sky), the
same sky light (atmo_model single scattering, the irradiance at 1.5 km / pi), the same droplet phase, 4 octaves, tint 1.
The layer's opacity = the volume's median opacity; the volume's pixels within 0.01 of it are its sample.
Bars (written 11-13 in notes/cloudlit1/bounds.txt before the first run):
  spread  the volume drawn 8 times (cvFrame 0..7, cvPhase over the 4x4 slots): max - min over the draws / mean
  H hue   lit side (texel white): R/B and G/B of the layer vs the volume, |rel| <= max(4 x spread, 0.01)
  L ratio lit / shaded luminance (texel white vs black; the volume: cvKeyK 1 vs 0), |rel| <= max(4 x spread, 0.01)
  W white at 2 deg the layer's lit R/B >= 1 + 0.25 (T_R / T_B - 1), T = the model's transmittance from the deck to the sun
  O off   ldKey.w 0, ldApOn 0: the layer == tex x colLin x skyScale (the old path) within 1e-6
Reds, each must FAIL its gate: a colLin = the old path (ldKey.w 0, a white weather colour) fails H at >= 1 sun height;
b noT = a white LUT bound as ldTrans fails W.
Exit 0 only when every green gate passes and both reds fail.
Run 1 (11-13): H, L, O pass at every sun height; both reds caught; W FAILS as registered: the layer's lit R/B at 2 deg is
2.68 against the bar 7.42 (T R/B 26.7), and the volume's own sheet reads 2.68 too, so the bar (a quarter of the beam's
reddening) is stricter than the reference it is checked against. The bar is kept as written; a new one needs a yes.
"""
import ctypes, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
args = sys.argv[1:]
scratch, OUTD = args[0], args[1]
SPV = os.path.join(OUTD, 'spirv')
sys.path.insert(0, HERE)
sys.argv = [os.path.join(HERE, 'cloudvol_check.py'), scratch, '--only', '-']
import cloudvol_check as K   # noqa: E402  (its context, LUTs, noise and helpers; no gate runs)
sys.path.insert(0, os.path.join(TREE, 'tests', 'hlsllive'))
import spirv_probe as SP     # noqa: E402

np, gl, g, A = K.np, K.gl, K.g, K.A
GLuint, GLint = ctypes.c_uint, ctypes.c_int
fn = SP.fn
KRB = 6360.0
DECK, CAMZ, DENS = 1.5, 1.49, 0.4
SUNS = (30.0, 10.0, 2.0, -2.0)
W = H = 64
VFOV = 4.0
OCT = 4


def program(names):
    p, log = SP.build([(st, os.path.join(SPV, n)) for st, n in names])
    if p is None:
        raise SystemExit('BUILD FAIL %s: %s' % (names, log[:600]))
    return p


PL = program([('vs', 'lookdev_clouds_vs.spv'), ('ps', 'lookdev_clouds_ps.spv')])
PV = program([('vs', 'cell_ao_vs.spv'), ('ps', 'cloud_march_ps.spv')])
getuiv = fn('glGetUniformiv', None, GLuint, GLint, ctypes.POINTER(GLint))


def samplers(prog):
    """(name, type, unit) of every active sampler (debug)"""
    n = GLint()
    fn('glGetProgramiv', None, GLuint, ctypes.c_uint, ctypes.POINTER(GLint))(prog, 0x8B86, ctypes.byref(n))
    gau = fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(GLint),
             ctypes.POINTER(ctypes.c_uint), ctypes.c_char_p)
    out = []
    for i in range(n.value):
        ln, sz, ty, nm = ctypes.c_int(), GLint(), ctypes.c_uint(), ctypes.create_string_buffer(256)
        gau(prog, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
        if 0x8B5D <= ty.value <= 0x8B62 or 0x8DC0 <= ty.value <= 0x8DD8:
            l = g.loc(prog, nm.value)
            u = GLint(-1)
            if l >= 0:
                getuiv(prog, l, ctypes.byref(u))
            out.append((nm.value.decode(), hex(ty.value), l, u.value))
    return out


if os.environ.get('CLOUDLIT_DEBUG'):
    print('DEBUG samplers layer', samplers(PL), '\nDEBUG samplers volume', samplers(PV), flush=True)


def bind_named(prog, texs, need):
    """each sampler on its own unit, set by glUniform1i as the app does (the patched SPIR-V reads every sampler as unit 0
    until set: a 2D and a 3D sampler on one unit drop the draw)"""
    for unit, (name, kind, tex) in enumerate(texs):
        l = g.loc(prog, name.encode())
        if l < 0:
            if name in need:
                raise SystemExit('SAMPLER MISSING %s' % name)
            continue
        g.active(0x84C0 + unit)
        gl.glBindTexture(kind, tex)
        g.u1i(l, unit)
    g.active(0x84C0)


# the layer's uniform block globalUniforms (unused on the probe path): a zero buffer on its binding, so no draw reads none
ubo = GLuint()
fn('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))(1, ctypes.byref(ubo))
fn('glBindBuffer', None, ctypes.c_uint, GLuint)(0x8A11, ubo.value)
zero = (ctypes.c_ubyte * 1024)()
fn('glBufferData', None, ctypes.c_uint, ctypes.c_ssize_t, ctypes.c_void_p, ctypes.c_uint)(0x8A11, 1024, zero, 0x88E4)
for bi in range(4):
    bd = GLint(-1)
    fn('glGetActiveUniformBlockiv', None, GLuint, GLuint, ctypes.c_uint, ctypes.POINTER(GLint))(PL, bi, 0x8A3F, ctypes.byref(bd))
    if bd.value >= 0 and not fn('glGetError', ctypes.c_uint)():
        fn('glBindBufferBase', None, ctypes.c_uint, GLuint, GLuint)(0x8A11, bd.value, ubo.value)
while fn('glGetError', ctypes.c_uint)():
    pass

# ---- inputs shared by both programs -----------------------------------------------------------------------------------
P = K.sky.P   # the medium the LUT was drawn from
TRANS = K.sky.trans
gl.glDisable(0x0BE2)   # no blend: the targets hold the programs' raw outputs
gl.glDisable(0x0B71)
# both vertex stages read the context's quad on attribute 0 (DXC numbers the one TEXCOORD0 input location 0)
WHITE = K.tex2d(np.ones((64, 256, 4)))


def amb_at(sun_el):
    """the sky light at the deck: single-scatter radiance over the upper hemisphere, cosine-weighted, / pi (sun irradiance 1)"""
    els = np.arange(2.5, 90.0, 5.0)
    azs = np.arange(0.0, 360.0, 15.0)
    views = [A.view_dir(e, a) for e in els for a in azs]
    L = np.asarray(A.sky(P, DECK, views, sun_el, None), np.float64).reshape(len(els), len(azs), 3)
    w = (np.sin(np.radians(els)) * np.cos(np.radians(els)))[:, None, None] * math.radians(5.0) * math.radians(15.0)
    return (L * w).sum(axis=(0, 1)) / math.pi


def trans_to_sun(sun_el):
    mu = math.sin(math.radians(sun_el))
    return np.asarray(A.transmittance(P, np.array([KRB + DECK]), np.array([mu])), np.float64).reshape(-1)[:3]


cal, drop = K.cal, list(K.drop)
projy = 1.0 / math.tan(math.radians(VFOV) / 2)
prm = list(drop)
prm[0] = min(prm[0], 1.0 - 8.0 / (projy * H))   # the volume's cvPrm cap (cloud_march_ps.hlsl main), handed to the layer
TV = K.Mrt(W, H)
TL = K.Mrt(W, H)


def vol(sun_el, keyK, amb, frame, phase):
    u = dict(K.bands_uni((1.5, 3.5, 0.0), (5.0, 6.5, 0.0), cal, cov=(0.0, 0.0, 1.0), high=(DECK, DECK + 0.2, DENS)))
    u.update(K.march_uni(K.camera(0, 90), (0.0, 0.0, CAMZ), (W, H), 1, phase, sun_el, drop=drop, vfov=VFOV,
                         frame=float(frame), octaves=OCT))
    u.update({'cvAmb': tuple(amb), 'cvAmbOn': (1.0,), 'cvKeyK': (keyK,), 'cvSunK': (keyK,), 'cvApOn': (0.0,),
              'cvIce': (0.0, 0.0, 0.0), 'cvAlbedoD': (0.0, 0.0, 0.0), 'cvBlueSlice': -1})
    g.bindfb(0x8D40, TV.fb)
    gl.glViewport(0, 0, W, H)
    g.use(PV)
    K.setuni(PV, u)
    bind_named(PV, K.march_texs(TRANS, TRANS), ('cvShape', 'cvTrans'))
    gl.glDrawArrays(5, 0, 4)
    o = TV.read()
    if os.environ.get('CLOUDLIT_DEBUG'):
        print('DEBUG vol sun %.1f keyK %.0f: L max %s, T min %.4f max %.4f, err 0x%x' % (sun_el, keyK, o[..., :3].max(axis=(0, 1)),
              o[..., 3].min(), o[..., 3].max(), fn('glGetError', ctypes.c_uint)()), flush=True)
    al = 1.0 - o[..., 3]
    S = o[..., :3] / np.maximum(al, 1e-9)[..., None]
    return al, S


def layer(sun_el, keyK, amb, texel, alpha, trans=None, apOn=1, colLin=(1.0, 1.0, 1.0)):
    sw = K.R.sundir(0.0, sun_el)
    u = {'texBlend': (0.0,), 'colLin': colLin, 'alpha': (alpha,), 'skyScale': (1.0,), 'probe': 1, 'sceneExposure': (1.0,),
         'viewTransform': 3, 'ldLtOn': 0, 'ldApOn': apOn, 'ldApR': (KRB + CAMZ, KRB + DECK), 'ldFsOn': 0,
         'ldKey': tuple(sw) + (keyK,), 'ldAmb': tuple(amb) + (float(OCT),), 'ldPhase': tuple(prm), 'ldTint': (1.0, 1.0, 1.0),
         'uvOffset': (0.0, 0.0), 'uvOffset2': (0.0, 0.0), 'probeUV': (0.5, 0.5)}
    tex = K.tex2d(np.array([[list(texel) + [1.0]]]))
    g.bindfb(0x8D40, TL.fb)
    gl.glViewport(0, 0, W, H)
    g.use(PL)
    K.setuni(PL, u)
    bind_named(PL, [('Tex', 0x0DE1, tex), ('Tex2', 0x0DE1, tex), ('ldTrans', 0x0DE1, trans or TRANS)], ('Tex', 'ldTrans'))
    gl.glDrawArrays(5, 0, 4)
    o = TL.read()
    gl.glDeleteTextures(1, ctypes.byref(GLuint(tex)))
    return o[:, :W // 2 - 2, :3].reshape(-1, 3).mean(axis=0), float(o[:, W // 2 + 2:, 0].mean())


def lum(c):
    return float(c[0] * 0.2126 + c[1] * 0.7152 + c[2] * 0.0722)


def hue(c):
    return np.array([c[0] / c[2], c[1] / c[2]])


results, reds = [], []


def gate(name, ok, msg):
    results.append(ok)
    print('%-10s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


rows = []
for el in SUNS:
    amb = amb_at(el)
    T = trans_to_sun(el)
    al0, _ = vol(el, 1.0, amb, 0, (0.0, 0.0))
    med = float(np.median(al0))
    m = np.abs(al0 - med) < 0.01
    hs, rs, lit_v, sh_v = [], [], None, None
    for k in range(8):
        ph = (float(k % 4), float(k // 4 * 2))
        al, Sl = vol(el, 1.0, amb, k, ph)
        _, Ss = vol(el, 0.0, amb, k, ph)
        mk = np.abs(al - med) < 0.01
        sl, ss = Sl[mk].mean(axis=0), Ss[mk].mean(axis=0)
        hs.append(hue(sl))
        rs.append(lum(sl) / lum(ss))
        if k == 0:
            lit_v, sh_v, n0 = sl, ss, int(mk.sum())
    hs, rs = np.array(hs), np.array(rs)
    spH = float(((hs.max(axis=0) - hs.min(axis=0)) / np.abs(hs.mean(axis=0))).max())
    spL = float((rs.max() - rs.min()) / rs.mean())
    bH, bL = max(4 * spH, 0.01), max(4 * spL, 0.01)
    lit_l, a_l = layer(el, 1.0, amb, (1.0, 1.0, 1.0), med)
    sh_l, _ = layer(el, 1.0, amb, (0.0, 0.0, 0.0), med)
    eH = float(np.abs(hue(lit_l) / hue(lit_v) - 1).max())
    eL = abs((lum(lit_l) / lum(sh_l)) / (lum(lit_v) / lum(sh_v)) - 1)
    gate('H hue', eH <= bH, 'sun %+5.1f: lit R/B G/B layer %.4f %.4f vs volume %.4f %.4f, worst rel %.5f (bar %.4f; spread %.2e; '
         'opacity %.3f, %d px)' % (el, *hue(lit_l), *hue(lit_v), eH, bH, spH, med, n0))
    gate('L ratio', eL <= bL, 'sun %+5.1f: lit/shaded layer %.4f vs volume %.4f, rel %.5f (bar %.4f; spread %.2e)'
         % (el, lum(lit_l) / lum(sh_l), lum(lit_v) / lum(sh_v), eL, bL, spL))
    # red a: the old colLin path (a white weather colour) on the same draw
    old_l, _ = layer(el, 0.0, amb, (1.0, 1.0, 1.0), med)
    eA = float(np.abs(hue(old_l) / hue(lit_v) - 1).max())
    rows.append((el, eA, bH, T, lit_l, amb, lit_v))
    print('INFO       sun %+5.1f: key T R/B %.3f, sky light %s, layer lit %s, layer opacity %.4f (AP to the deck x tm)'
          % (el, T[0] / T[2] if T[2] > 0 else 0.0, np.array2string(amb, precision=4), np.array2string(lit_l, precision=4), a_l),
          flush=True)

# W: not white at 2 deg; red b: no transmittance
el, eA, bH, T, lit_l, amb, lit_v = [r for r in rows if r[0] == 2.0][0]
barW = 1 + 0.25 * (T[0] / T[2] - 1)
gate('W white', lit_l[0] / lit_l[2] >= barW, 'sun +2.0: lit R/B %.3f (bar >= %.3f, T R/B %.3f; the volume itself %.3f)'
     % (lit_l[0] / lit_l[2], barW, T[0] / T[2], lit_v[0] / lit_v[2]))
nt, _ = layer(2.0, 1.0, amb, (1.0, 1.0, 1.0), 0.5, trans=WHITE)
redB = nt[0] / nt[2] < barW
print('RED b noT   %s  sun +2.0: lit R/B %.3f with a white LUT (bar >= %.3f): %s' % ('ok' if redB else 'NOT CAUGHT', nt[0] / nt[2], barW,
      'fails W as it must' if redB else 'passes W'), flush=True)
redA = any(r[1] > r[2] for r in rows)
print('RED a colLin %s  the old path\'s hue error per sun: %s' % ('ok' if redA else 'NOT CAUGHT',
      ', '.join('%+.0f %.4f (bar %.4f)' % (r[0], r[1], r[2]) for r in rows)), flush=True)
reds = [redA, redB]

# O: the old path when the sky-lit path is off
off, a_off = layer(10.0, 0.0, rows[0][5], (0.8, 0.6, 0.4), 0.7, apOn=0, colLin=(0.9, 0.8, 0.7))
want = ((np.array([0.8, 0.6, 0.4]) + 0.055) / 1.055) ** 2.4 * (0.9, 0.8, 0.7)
eO = float(np.abs(off / want - 1).max())
gate('O off', eO <= 1e-6 and abs(a_off - 0.7) <= 1e-6, 'ldKey.w 0, ldApOn 0: colour rel %.2e, opacity %.7f (bar 1e-6)' % (eO, a_off))

ok = all(results) and all(reds)
print('CLOUDLIT-CHECK %s (%d gates, reds %s)' % ('PASS' if ok else 'FAIL', len(results), 'caught' if all(reds) else 'NOT caught'))
if __name__ == '__main__':
    sys.exit(0 if ok else 1)
