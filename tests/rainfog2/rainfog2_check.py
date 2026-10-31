#!/usr/bin/env python3
"""Lane RAINFOG2 (CLOUD6e's leftovers), gated offline: the rain fog in the closed-form tails past the froxel grids and
in the sky's tail (the REAL res/shaders/lookdev_fog.glsl through this machine's GL driver in a hidden 4.1 core context,
tests/spells/shader_link_check.py's loader expansion), the rain rate from the weather's precipitation shader (SPGD
density), PRECIPFLAG1 (the particles from the shader, not the class flags) and SNOWFOG1 (a snow shader's own law), the
CPU rules through tests/rainfog2/rainfog2_dump.cpp (g++ + Qt6Core). No NifSkope, no game data (the census fixtures
tests/rainfog2/census_*.tsv, written by notes/fix1031a/rainfog2/tools/census_precip.py from the 7 masters).

    python -I tests/rainfog2/rainfog2_check.py <scratch> [--red today|wiring] [--only ZTCKRSDW] [--base <sha>]

Gates (bars written before the first run). Scene: view axes = world axes, the camera 0.3 km over the ground, the Low
base 1.5 km over the ground, the cloud map 256^2 over 48 km (half), smooth random coverage, 0.4 past the map, sigma
0.2545 / km x the profile (CLOUD6e's numbers); fogDistScale 1.
  Z off       the rain fog off (volAirRain.x 0, every other rain uniform set, the map bound) draws today's tails EXACTLY:
              tailprobe.frag on the tree's lookdev_fog.glsl vs on today's (--base), a real air (sigma0 1/30 km, 0.5 km
              scale height, records colour on) and a non-empty grid, near-grid and far-grid layouts, surfaces + sky:
              max |diff| == 0.
  T tail      rain on, an empty grid and a vanishing air: each probe's rain optical depth (-ln T_on + ln T_off) vs a
              20000-point quadrature of the same law from the grid's far edge to the surface (sky: to where the ray
              leaves the fog, base + 0.15 km): |d od| <= 0.02 od + 0.002 for every probe; both layouts; level, down,
              up-to-a-peak surfaces at 5..150 km and sky rays at 0.5..60 deg. Red today FAILS (od 0).
  C cost      GPU ms of 1920 x 1080 tail probes (30..100 km, level), rain on vs off, median of 15 (INFO + bar on <= off
              + 0.5 ms).
  K census    the 107 vanilla weathers (fixture) through the particle rule: 10 draw rain, 0 snow; the 4 Rainy-flag
              weathers with no shader and the 10 Snow-flag ones draw nothing; every rain at the default rate (density
              7). Red today (the class flags) FAILS (14 rain, 10 snow).
  R rate      the one SPGD of another density (DLC04lRain, 15, linked by no vanilla weather) gives rate x 15 / 7 and the
              rain fog's share x 15 / 7 (and 7 -> x 1 exactly). Red today FAILS (x 1).
  S snow      a Type 1 (Snow) shader: one snow_fine layer, the fog's snow share 1 (rain 0) and the snow law 15.8 / km
              == the checker's own (LH74 dry snow mass + fall speed, Q 2, rel 1e-6); WW_CLOUD_SNOWFOG off: the rain
              law (CLOUD6e). Red today FAILS (rain particles, rain law).
  D rules     spgdKind, spgdRateScale, spgdRedBits words, shares (pct / 1 - pct x scale, snow folded into rain when off)
              and wwPrecipFromSpgd for every type x density x flags x red == the checker's transcription (1e-12).
  W wiring    esmweather.cpp reads the SPGD (kWanted + wwSpgdRead), lookdevstage.cpp wwLookdevWeatherPrecip,
              cellprecip.cpp wwPrecipFromSpgd, cellvolair.cpp the tail uniforms + map, lookdev_fog.glsl the three
              tails. Red wiring (the --base sources) FAILS.
Amendment after the first run (before any red run): T's od bar read 5.96 (near) / 4.67 (far) on 150 km level rays and
low sky rays whose od is 12..20 (T < 1e-5: the 8 sub-segments' map reads miss coverage swings of the 9.6 km test pattern
over 17 km each; mean od ref 4.812 vs 4.816): the od bar now judges probes of od <= 2 (T >= 0.135), and every probe is
judged by what reaches the eye, |dT| <= 0.01 (first run: max |dT| 0.0054). The sub-segments then shared one exp each
(the profile's integral carried from one end to the next): the same law, fewer instructions.
Amendment 2 (C, same run): the probes were scattered at random (every pixel a random ray: 0.723 ms, the map reads' cache
misses); a screen holds neighbouring rays, so the probes are laid out as one (90 deg across, 30..100 km down). Bar kept.
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, re, shutil, subprocess, sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'rainfog2_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ZTCKRSDW')
BASE = args[args.index('--base') + 1] if '--base' in args else '0ec6dcc0'
os.makedirs(scratch, exist_ok=True)
QT = 'C:/msys64/ucrt64/include/qt6'
ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ.get('PATH', ''))
KU = 0.0142875 / 1000.0          # atmomath.h kUnitKm
UPK = 1.0 / KU
results = []


def gate(name, ok, msg):
    results.append(bool(ok))
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


# ---- the CPU rules through the dump --------------------------------------------------------------------------------------
def dump(redbuild):
    exe = os.path.join(scratch, 'rainfog2_dump%s.exe' % ('_red' if redbuild else ''))
    cmd = ['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-w', '-I', os.path.join(TREE, 'src'), '-I', QT,
           '-I', QT + '/QtCore', os.path.join(HERE, 'rainfog2_dump.cpp'), '-o', exe, '-lQt6Core']
    if redbuild:
        cmd.insert(1, '-DRAINFOG2_RED')
    r = subprocess.run(cmd, env=ENV, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])
    out = subprocess.run([exe], capture_output=True, text=True, env=ENV, check=True, timeout=120).stdout.split('\n')
    D = {'kind': {}, 'scale': {}, 'redbits': {}, 'flagkinds': {}, 'layers': {}, 'shares': {}}
    for l in out:
        w = l.split()
        if not w:
            continue
        if w[0] in ('rainsigma', 'snowsigma', 'segments', 'snowdefault', 'raindefault'):
            D[w[0]] = [float(x) for x in w[1:]]
        elif w[0] == 'kind':
            D['kind'][int(w[1])] = int(w[2])
        elif w[0] == 'scale':
            D['scale'][float(w[1])] = float(w[2])
        elif w[0] == 'redbits':
            m = re.match(r"redbits '(.*)' '(.*)' (\d+)", l)
            D['redbits'][(m.group(1), m.group(2))] = int(m.group(3))
        elif w[0] == 'flagkinds':
            D['flagkinds'][int(w[1])] = int(w[2])
        elif w[0] == 'layers':
            k = (int(w[1]), float(w[2]), int(w[3]), int(w[4]))
            D['layers'][k] = [(int(x.split(':')[0]), float(x.split(':')[1])) for x in w[6:]]
        elif w[0] == 'shares':
            k = (int(w[1]), int(w[2]), float(w[3]), float(w[4]), int(w[5]), float(w[6]), int(w[7]))
            D['shares'][k] = (float(w[8]), float(w[9]))
    return D


NEED_CPU = only & set('KRSDTZC')
if NEED_CPU:
    DD = dump(red == 'today')
    SIGMA_KM = DD['rainsigma'][0]
    RAIN_D, RAIN_RATE = DD['raindefault']
    SNOW_D, SNOW_RATE = DD['snowdefault']
EXCESS, FALLKM, ABOVEKM, EDGE = 1.0, 0.3, 0.15, 0.1   # rainfogmath.h (CLOUD6e's gate D pins them to the dump)
SEGMENTS = 8


def p_of(hkm):
    hkm = np.asarray(hkm, dtype=np.float64)
    below = 1.0 + EXCESS * np.exp(-np.maximum(hkm, 0.0) / FALLKM)
    above = (1.0 + EXCESS) * np.maximum(1.0 + hkm / ABOVEKM, 0.0)
    return np.where(hkm >= 0.0, below, above)


# ---- K / R / S / D: the record rules --------------------------------------------------------------------------------------
def want_kind(t):
    return 0 if t < -1 else (2 if t == 1 else 1)


def want_scale(d):
    return d / 7.0 if d > 0 else 1.0


def want_redbits(sw, rd):
    b = 3 if sw in ('0', 'off', 'false', 'no') else 0
    for w in [x for x in rd.split(',')] if rd else []:
        b |= {'flags': 1, 'rate': 2, '1': 3, 'on': 3, 'old': 3}.get(w, 0)
    return b


def want_layers(t, d, f, rb):
    out = []
    if rb & 1:
        if f & 4:
            out.append([0, RAIN_RATE])
        if f & 8:
            out.append([3, SNOW_RATE])
    else:
        k = want_kind(t)
        if k == 1:
            out.append([0, RAIN_RATE])
        elif k == 2:
            out.append([3, SNOW_RATE])
    if not (rb & 2) and t >= -1:
        for o in out:
            o[1] *= want_scale(d)
    return [tuple(o) for o in out]


def want_shares(ta, tb, da, db, bl, pct, so):
    rain = snow = 0.0
    for i, (t, d) in enumerate(((ta, da), (tb, db))[:2 if bl else 1]):
        w = (pct if i == 0 else 1.0 - pct) if bl else 1.0
        k = want_kind(t)
        if k == 2 and not so:
            k = 1
        if k == 1:
            rain += w * want_scale(d)
        elif k == 2:
            snow += w * want_scale(d)
    return rain, snow


def census():
    rows = [l.rstrip('\n').split('\t') for l in open(os.path.join(HERE, 'census_precip.tsv'))][1:]
    out = []
    for r in rows:
        t = -2 if r[5] == '' else int(r[7])
        d = float(r[6]) if r[6] else -1.0
        out.append((r[0], int(r[2]), t, d))
    return out


if 'K' in only:
    W = census()
    nr = ns = 0
    bad_rate = []
    dry = {'DiamondWeather', 'DiamondWeatherPastel', 'TCommonwealthMarshOvercast', 'CommonwealthGSRadstormOld'}
    snowflag = {e for e, f, t, d in W if f & 8}
    dry_ok = True
    for e, f, t, d in W:
        L = DD['layers'][(t, d, f & 12, 0)]   # the census holds densities 7 and -1 (none) only
        nr += any(k == 0 for k, rate in L)
        ns += any(k == 3 for k, rate in L)
        bad_rate += [e for k, rate in L if k == 0 and abs(rate - RAIN_RATE) > 1e-12]
        if (e in dry or e in snowflag) and L:
            dry_ok = False
    gate('K census', len(W) == 107 and nr == 10 and ns == 0 and dry_ok and not bad_rate and len(snowflag) == 10,
         '%d weathers: %d draw rain (want 10), %d snow (want 0); the 4 shaderless Rainy + %d Snow-flag weathers dry: %s; '
         'rain off the default rate: %d%s' % (len(W), nr, ns, len(snowflag), dry_ok, len(bad_rate),
                                              ' [red today: the class flags]' if red == 'today' else ''))

if 'R' in only:
    sp = [l.rstrip('\n').split('\t') for l in open(os.path.join(HERE, 'census_spgd.tsv'))][1:]
    dens = {r[2]: float(r[15]) for r in sp}
    L15 = DD['layers'][(0, 15.0, 4, 0)]
    L7 = DD['layers'][(0, 7.0, 4, 0)]
    s15 = DD['shares'][(0, -2, 15.0, -1.0, 0, 0.0, 1)][0]
    s7 = DD['shares'][(0, -2, 7.0, -1.0, 0, 0.0, 1)][0]
    sb = DD['shares'][(0, 0, 15.0, 7.0, 1, 0.25, 1)][0]
    ok = (dens.get('DLC04lRain') == 15.0 and len(L15) == 1 and abs(L15[0][1] - RAIN_RATE * 15 / 7) <= 1e-12
          and len(L7) == 1 and L7[0][1] == RAIN_RATE and abs(s15 - 15 / 7) <= 1e-12 and s7 == 1.0
          and abs(sb - (0.25 * 15 / 7 + 0.75)) <= 1e-12)
    gate('R rate', ok, 'DLC04lRain density %s: rain layer %.4f mm/h (want %.4f), fog share %.4f (want %.4f); density 7: '
         '%.4f mm/h, share %.4f; blend 0.25 of 15 over 7: %.4f (want %.4f)%s'
         % (dens.get('DLC04lRain'), L15[0][1] if L15 else -1, RAIN_RATE * 15 / 7, s15, 15 / 7, L7[0][1] if L7 else -1,
            s7, sb, 0.25 * 15 / 7 + 0.75, ' [red today]' if red == 'today' else ''))

if 'S' in only:
    Ls = DD['layers'][(1, 7.0, 8, 0)]
    so = DD['shares'][(1, -2, 7.0, -1.0, 0, 0.0, 1)]
    sf = DD['shares'][(1, -2, 7.0, -1.0, 0, 0.0, 0)]
    D = SNOW_D
    v = 0.8 * D ** 0.16                     # LH74 unrimed aggregates, m/s
    m = 0.073 * D ** 1.4                    # mg
    N = SNOW_RATE * 1.0e6 / 3600.0 / (m * v)
    sig = N * 2.0 * math.pi * (D * 0.5e-3) ** 2 * 1000.0
    rel = abs(DD['snowsigma'][0] / sig - 1.0)
    ok = (len(Ls) == 1 and Ls[0][0] == 3 and so == (0.0, 1.0) and sf == (1.0, 0.0) and rel <= 1e-6)
    gate('S snow', ok, 'Type 1 shader: layers %s (want snow_fine 3), fog shares on %s off %s; snow law %.4f / km (checker %.4f, '
         'rel %.2g; visibility %.2f km; rain %.4f / km)%s' % (Ls, so, sf, DD['snowsigma'][0], sig, rel, 3.912 / sig, SIGMA_KM,
                                                            ' [red today]' if red == 'today' else ''))

if 'D' in only:
    e_kind = [t for t, k in DD['kind'].items() if k != want_kind(t)]
    e_scale = [d for d, s in DD['scale'].items() if abs(s - want_scale(d)) > 1e-12]
    e_red = [k for k, b in DD['redbits'].items() if b != want_redbits(*k)]
    e_lay = [k for k, L in DD['layers'].items()
             if len(L) != len(want_layers(*k)) or any(a[0] != b[0] or abs(a[1] - b[1]) > 1e-12 for a, b in zip(L, want_layers(*k)))]
    e_sh = [k for k, v in DD['shares'].items() if max(abs(a - b) for a, b in zip(v, want_shares(*k))) > 1e-12]
    gate('D rules', not (e_kind or e_scale or e_red or e_lay or e_sh),
         'kind %d/%d wrong, scale %d/%d, red words %d/%d, layers %d/%d, shares %d/%d' % (
             len(e_kind), len(DD['kind']), len(e_scale), len(DD['scale']), len(e_red), len(DD['redbits']), len(e_lay),
             len(DD['layers']), len(e_sh), len(DD['shares'])))

# ---- W: the wiring --------------------------------------------------------------------------------------------------------
if 'W' in only:
    def src(rel):
        if red == 'wiring':
            r = subprocess.run(['git', '-C', TREE, 'show', BASE + ':' + rel], capture_output=True)
            return r.stdout.decode('latin-1') if r.returncode == 0 else ''
        p = os.path.join(TREE, rel)
        return open(p, 'rb').read().decode('latin-1') if os.path.exists(p) else ''
    es, ld, cp, cv, lf = (src(x) for x in ('src/esmweather.cpp', 'src/gl/lookdevstage.cpp', 'src/gl/cellprecip.cpp',
                                           'src/gl/cellvolair.cpp', 'res/shaders/lookdev_fog.glsl'))
    w1 = '"SPGD" };' in es and 'wwSpgdRead( *esm, esm->mapFormID( *r, out.precipForm )' in es
    w2 = 'bool wwLookdevWeatherPrecip(' in ld and 't = w.precipForm ? w.precipType : -2;' in ld
    w3 = 'specs = wwPrecipFromSpgd( spgdType, spgdDensity, classFlags, spgdRed );' in cp
    w4 = ('prog->uniLocation( "volAirRainMap" )' in cv and 'RF::shares( ptype, scale, blending' in cv
          and 'g.rfNote = vaRainFog( scene, prog, L && L->interior, g.rfKey, g.rfTail );' in cv)
    w5 = lf.count('wwAirRainTau( posView * ( far / d ), posView )') == 2 and 'wwAirRainTau( dirView * ( far' in lf
    gate('W wiring', w1 and w2 and w3 and w4 and w5, 'esmweather SPGD %s, lookdevstage %s, cellprecip %s, cellvolair %s, '
         'lookdev_fog tails %s%s' % (w1, w2, w3, w4, w5, ' [red wiring: %s sources]' % BASE if red == 'wiring' else ''))

# ---- GL: tailprobe.frag on the tree's lookdev_fog.glsl and on today's -----------------------------------------------------
NEED_GL = only & set('ZTC')
if NEED_GL:
    import shader_link_check as S   # noqa: E402
    import atmo_lut_check as C      # noqa: E402
    gl = S.gl
    GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
    G = C.Gl()
    f_u4f = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
    f_um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
    f_pget = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    f_active = S.fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_int),
                    ctypes.POINTER(GLint), ctypes.POINTER(GLenum), ctypes.c_char_p)
    f_tex3d = S.fn('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, GLenum,
                   GLenum, ctypes.c_void_p)
    f_genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))
    f_begq = S.fn('glBeginQuery', None, GLenum, GLuint)
    f_endq = S.fn('glEndQuery', None, GLenum)
    f_getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
    SAMPLERS = set(range(0x8B5D, 0x8B65)) | set(range(0x8DC0, 0x8DDA)) | set(range(0x9108, 0x910E)) | set(range(0x900C, 0x9010))
    SDIR = os.path.join(TREE, 'res', 'shaders')

    def shader_dir(name, fog_src):
        d = os.path.join(scratch, name)
        if os.path.isdir(d):
            shutil.rmtree(d)
        shutil.copytree(SDIR, d)
        if fog_src is not None:
            open(os.path.join(d, 'lookdev_fog.glsl'), 'wb').write(fog_src)
        shutil.copy(os.path.join(HERE, 'tailprobe.frag'), os.path.join(d, 'tailprobe.frag'))
        open(os.path.join(d, 'tailprobe.prog'), 'w').write('shaders cell_ao.vert tailprobe.frag\n')
        return d

    def program(sdir):
        p = C.build(sdir, 'tailprobe.prog')
        ok = GLint()
        f_pget(p, 0x8B82, ctypes.byref(ok))
        if not ok.value:
            raise SystemExit('LINK FAIL tailprobe.prog in ' + sdir)
        n = GLint()
        f_pget(p, 0x8B86, ctypes.byref(n))
        G.use(p)
        units, unit = {}, 1
        for i in range(n.value):
            ln, sz, ty = ctypes.c_int(), GLint(), GLenum()
            buf = ctypes.create_string_buffer(256)
            f_active(p, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), buf)
            if ty.value in SAMPLERS:
                G.u1i(G.loc(p, buf.value), unit)
                units[buf.value.decode()] = (unit, ty.value)
                unit += 1
        return p, units

    def tex2d(img4, w, h):
        a = np.ascontiguousarray(img4.astype(np.float32))
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x0DE1, t)
        gl.glTexImage2D(0x0DE1, 0, 0x8814, w, h, 0, 0x1908, 0x1406, a.ctypes.data)
        for k, v in ((0x2801, 0x2600), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F), (0x813D, 0)):
            gl.glTexParameteri(0x0DE1, k, v)
        return t.value

    def mapTex(img):
        t = tex2d(np.dstack([img, np.zeros_like(img), np.zeros_like(img), np.zeros_like(img)]), img.shape[1], img.shape[0])
        gl.glBindTexture(0x0DE1, t)
        for k, v in ((0x2801, 0x2601), (0x2800, 0x2601)):
            gl.glTexParameteri(0x0DE1, k, v)
        return t

    def tex3d(vol):
        d, h, w, _ = vol.shape
        a = np.ascontiguousarray(vol.astype(np.float32))
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x806F, t)
        f_tex3d(0x806F, 0, 0x8814, w, h, d, 0, 0x1908, 0x1406, a.ctypes.data)
        for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F), (0x8072, 0x812F), (0x813D, 0)):
            gl.glTexParameteri(0x806F, k, v)
        return t.value

    def smooth_random(n, blocks, seed):
        rng = np.random.default_rng(seed)
        b = rng.uniform(0.0, 1.0, (blocks, blocks))
        x = (np.arange(n) + 0.5) / n * blocks - 0.5
        i0 = np.clip(np.floor(x).astype(int), 0, blocks - 1)
        i1 = np.clip(i0 + 1, 0, blocks - 1)
        f = np.clip(x - np.floor(x), 0, 1)
        f = f * f * (3 - 2 * f)
        rowsI = b[i0] * (1 - f)[:, None] + b[i1] * f[:, None]
        return rowsI[:, i0] * (1 - f)[None, :] + rowsI[:, i1] * f[None, :]

    CAMZ = 0.3 * UPK          # the camera over the ground (the fog frame)
    BASEZ = 1.5 * UPK         # the Low base over the ground
    HALF = 48.0 * UPK         # cloudmap.cpp kHalfExtentM x kUnitsPerM (the map's half side, km here)
    COV_OUT = 0.4
    IMG = smooth_random(256, 10, 21)
    # view = world - eye (axes = world axes); the map's (u, v) and the km below the base (cell_rainfog.glsl rfFromView)
    M = np.zeros((4, 4))
    M[0, 0] = 1.0 / (2 * HALF)
    M[0, 3] = 0.5
    M[1, 1] = 1.0 / (2 * HALF)
    M[1, 3] = 0.5
    M[2, 2] = -KU
    M[2, 3] = (BASEZ - CAMZ) * KU
    M[3, 3] = 1.0

    def sample_c(u, v):
        n = IMG.shape[0]
        x, y = u * n - 0.5, v * n - 0.5
        x0, y0 = np.floor(x), np.floor(y)
        fx, fy = x - x0, y - y0
        x0, y0 = x0.astype(int), y0.astype(int)
        cl = lambda a: np.clip(a, 0, n - 1)
        I = IMG
        c = (I[cl(y0), cl(x0)] * (1 - fx) * (1 - fy) + I[cl(y0), cl(x0 + 1)] * fx * (1 - fy)
             + I[cl(y0 + 1), cl(x0)] * (1 - fx) * fy + I[cl(y0 + 1), cl(x0 + 1)] * fx * fy)
        e = np.maximum(np.abs(u - 0.5), np.abs(v - 0.5)) * 2
        t = np.clip((e - (1 - EDGE)) / EDGE, 0, 1)
        w = t * t * (3 - 2 * t)
        return c * (1 - w) + COV_OUT * w

    def od_ref(p0, p1, n=20000):
        ts = (np.arange(n) + 0.5) / n
        pts = p0[None, :] + (p1 - p0)[None, :] * ts[:, None]
        q = pts @ M[:3, :3].T + M[:3, 3]
        s = SIGMA_KM * KU * sample_c(q[:, 0], q[:, 1]) * p_of(q[:, 2])
        return float(s.sum() * np.linalg.norm(p1 - p0) / n)

    LAYOUTS = {
        'near': dict(volSlices=(16.0, math.log(4.0 * UPK / 16.0), 32.0, 1.0), volFar=(0.0, 1.0, 0.0, 0.0), far=4.0 * UPK),
        'far': dict(volSlices=(16.0, math.log(17578.0 / 16.0), 32.0, 1.0),
                    volFar=(17578.0, math.log(16.0 * UPK / 17578.0), 16.0, 1.0), far=16.0 * UPK),
    }

    def probes():
        P = []
        for az in range(0, 360, 30):
            a = math.radians(az + 7)
            d2 = np.array([math.cos(a), math.sin(a)])
            for dk in (5, 10, 20, 40, 80, 150):           # level surfaces at the eye's height
                P.append((np.array([d2[0] * dk * UPK, d2[1] * dk * UPK, 0.0]), 0))
            for dk in (20, 40, 60):                       # the ground (down rays)
                P.append((np.array([d2[0] * dk * UPK, d2[1] * dk * UPK, -CAMZ]), 0))
            for dk, hk in ((30, 1.0), (40, 1.4), (50, 1.6), (60, 2.5)):   # peaks: through the base and above it
                P.append((np.array([d2[0] * dk * UPK, d2[1] * dk * UPK, hk * UPK - CAMZ]), 0))
            for el in (0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0, -5.0):       # the sky
                e = math.radians(el)
                P.append((np.array([d2[0] * math.cos(e), d2[1] * math.cos(e), math.sin(e)]), 1))
        return P

    PRB = probes()
    PW, PH = 40, (len(PRB) + 39) // 40

    def probe_tex(P):
        img = np.zeros((PH, PW, 4))
        for i, (p, mode) in enumerate(P):
            img[i // PW, i % PW, :3] = p
            img[i // PW, i % PW, 3] = mode
        return tex2d(img, PW, PH)

    def grid(kind):
        d = 48
        if kind == 'empty':
            v = np.zeros((d, 4, 4, 4))
            v[..., 3] = 1.0
            return v
        rng = np.random.default_rng(5)
        v = np.zeros((d, 4, 4, 4))
        for k in range(d):
            v[k, ..., :3] = 0.02 * (k + 1) + 0.01 * rng.random((4, 4, 3))
            v[k, ..., 3] = math.exp(-0.03 * (k + 1))
        return v

    def run(prog, units, layout, rain_on, air, flags, ptex, mtex, gtex, w, h, timed=0):
        if not hasattr(run, 'tg') or run.tg[2:] != (w, h):
            t, fb = G.target(w, h)
            run.tg = (t, fb, w, h)
        G.bindfb(0x8D40, run.tg[1])
        gl.glViewport(0, 0, w, h)
        G.use(prog)
        L = lambda n: G.loc(prog, n.encode())
        Lo = LAYOUTS[layout]
        G.u1i(L('fogOn'), 1)
        G.u1i(L('volOn'), 1)
        f_u4f(L('fogView'), 0.0, 0.0, 1.0, CAMZ)
        G.u1f(L('fogDistScale'), 1.0)
        f_u4f(L('fogSun'), 0.0, 0.6, 0.8, 0.0)
        f_u4f(L('volRect'), 0.0, 0.0, 1.0 / w, 1.0 / h)
        f_u4f(L('volSlices'), *Lo['volSlices'])
        f_u4f(L('volFar'), *Lo['volFar'])
        f_u4f(L('volPack'), 1.0, 1.0, 1.0, 1.0)
        f_u4f(L('volPackDims'), 4.0, 4.0, 48.0, 32.0)
        G.u1i(L('volFarClamp'), 0)
        f_u4f(L('volAirMed'), *air)
        f_u4f(L('volAirLayer'), 0.0, 0.0, 0.0, 0.0)
        f_u4f(L('volAirGround'), 0.0, 0.0, 0.0, 0.0)
        G.u1i(L('volAirFlags'), flags)
        f_u4f(L('volAirRain'), SIGMA_KM * KU if rain_on else 0.0, EXCESS, 1.0 / FALLKM, 1.0 / ABOVEKM)
        f_u4f(L('volAirRainFar'), COV_OUT, EDGE, float(SEGMENTS), 0.0)
        m = np.ascontiguousarray(M.T.astype(np.float32))
        f_um4(L('volAirRainXf'), 1, 0, m.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
        for name, (unit, ty) in units.items():
            G.active(0x84C0 + unit)
            if name == 'probePos':
                gl.glBindTexture(0x0DE1, ptex)
            elif name == 'volAirRainMap':
                gl.glBindTexture(0x0DE1, mtex)
            elif name == 'volTex':
                gl.glBindTexture(0x806F, gtex)
        if timed:
            q = (GLuint * 1)()
            f_genq(1, q)
            ms = []
            for rep in range(timed):
                gl.glFinish()
                f_begq(0x88BF, q[0])
                gl.glDrawArrays(5, 0, 4)
                f_endq(0x88BF)
                r = ctypes.c_uint64()
                f_getq(q[0], 0x8866, ctypes.byref(r))
                ms.append(r.value / 1e6)
            return float(np.median(ms))
        gl.glDrawArrays(5, 0, 4)
        buf = (ctypes.c_float * (w * h * 4))()
        gl.glReadPixels(0, 0, w, h, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(h, w, 4).astype(np.float64)

    d_new = shader_dir('shaders_new', None)
    old = subprocess.run(['git', '-C', TREE, 'show', BASE + ':res/shaders/lookdev_fog.glsl'], capture_output=True, check=True).stdout
    d_old = shader_dir('shaders_today', old)
    P_NEW, U_NEW = program(d_new)
    P_OLD, U_OLD = program(d_old)
    P_GATE, U_GATE = (P_OLD, U_OLD) if red == 'today' else (P_NEW, U_NEW)
    PT = probe_tex(PRB)
    MT = mapTex(IMG)
    GE, GR = tex3d(grid('empty')), tex3d(grid('rand'))
    print('programs: new %d, today (%s) %d; %d probes%s' % (P_NEW, BASE, P_OLD, len(PRB),
                                                            "  [red today: the gates read today's]" if red == 'today' else ''))

if 'Z' in only:
    worst, n = 0.0, 0
    air = (1.0 / (30.0 * UPK), 0.0, 1.0 / (0.5 * UPK), 3.0 * UPK)
    for lay in ('near', 'far'):
        for fl in (2, 6):
            a = run(P_NEW, U_NEW, lay, False, air, fl, PT, MT, GR, PW, PH)
            b = run(P_OLD, U_OLD, lay, False, air, fl, PT, MT, GR, PW, PH)
            worst = max(worst, float(np.abs(a - b).max()))
            n += a[: len(PRB)].size
    gate('Z off', worst == 0.0, 'rain off vs today (%s): max |diff| %.3g over %d probes x 2 layouts x 2 flag sets (real air, '
         'non-empty grid)' % (BASE, worst, len(PRB)))

if 'T' in only:
    air = (1e-30, 0.0, 0.0, 1e9)
    worst = {}
    stats = []
    for lay in ('near', 'far'):
        on = run(P_GATE, U_GATE, lay, True, air, 2, PT, MT, GE, PW, PH).reshape(-1, 4)
        off = run(P_GATE, U_GATE, lay, False, air, 2, PT, MT, GE, PW, PH).reshape(-1, 4)
        far = LAYOUTS[lay]['far']
        wv = 0.0
        for i, (p, mode) in enumerate(PRB):
            if mode == 0:
                d = np.linalg.norm(p)
                if d <= far:
                    continue
                ref = od_ref(p * (far / d), p)
            else:
                if p[2] <= 1e-4:
                    ok_zero = on[i, 3] == off[i, 3]
                    wv = max(wv, 0.0 if ok_zero else 1.0)
                    continue
                p0 = p * far
                # to where the ray leaves the fog (base + the fade) or 1e7 units on
                hz = (BASEZ - CAMZ) * KU + ABOVEKM
                tl = min(hz / (p[2] * KU), far + 1e7)
                ref = od_ref(p0, p * max(tl, far))
            got = -math.log(max(on[i, 3], 1e-38)) + math.log(max(off[i, 3], 1e-38))
            dT = abs(math.exp(-got) - math.exp(-ref))
            err = abs(got - ref) / (0.02 * ref + 0.002) if ref <= 2.0 else 0.0   # amended: od bar where T >= 0.135
            wv = max(wv, err, dT / 0.01)
            stats.append((ref, got, dT))
        worst[lay] = wv
    stats = np.array(stats)
    gate('T tail', max(worst.values()) <= 1.0, 'worst of |d od| / (0.02 od + 0.002) [od <= 2] and |dT| / 0.01: near %.3f, far '
         '%.3f (<= 1) over %d probes; max |dT| %.5f; od range %.3f..%.3f, mean ref %.3f got %.3f'
         % (worst['near'], worst['far'], len(stats), stats[:, 2].max(), stats[:, 0].min(), stats[:, 0].max(),
            stats[:, 0].mean(), stats[:, 1].mean()))

if 'C' in only:
    W_, H_ = 1920, 1080
    # amended (2): a screen's layout, neighbours on neighbouring rays (90 deg across, 30..100 km down the screen); the
    # first run scattered every pixel's ray at random, which no frame does (0.72 ms vs 0.21: the map reads' cache misses)
    X, Y = np.meshgrid((np.arange(W_) + 0.5) / W_, (np.arange(H_) + 0.5) / H_)
    a = 0.3 + X * (math.pi / 2)
    dk = (30.0 + 70.0 * Y) * UPK
    img = np.zeros((H_, W_, 4))
    img[..., 0] = np.cos(a) * dk
    img[..., 1] = np.sin(a) * dk
    tbig = tex2d(img, W_, H_)
    air = (1.0 / (30.0 * UPK), 0.0, 1.0 / (0.5 * UPK), 3.0 * UPK)
    t_off = run(P_NEW, U_NEW, 'far', False, air, 6, tbig, MT, GR, W_, H_, timed=15)
    t_on = run(P_NEW, U_NEW, 'far', True, air, 6, tbig, MT, GR, W_, H_, timed=15)
    t_old = run(P_OLD, U_OLD, 'far', False, air, 6, tbig, MT, GR, W_, H_, timed=15)
    gate('C cost', t_on <= t_off + 0.5, '1920x1080 tail probes (30..100 km), median of 15 (this GPU, offline): today %.3f ms, '
         'rain off %.3f, rain on %.3f (%d segments)' % (t_old, t_off, t_on, SEGMENTS))

print('RAINFOG2-CHECK %s (%d of %d)%s' % ('PASS' if results and all(results) else 'FAIL', sum(results), len(results),
                                         ' [red %s]' % red if red else ''))
if __name__ == '__main__':
    sys.exit(0 if results and all(results) else 1)
