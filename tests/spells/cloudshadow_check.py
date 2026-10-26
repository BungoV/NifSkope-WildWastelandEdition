"""lane CLOUD3: offline gate of the cloud shadow map (src/gl/cloudshadow.cpp, res/shaders/cloud_shadowmap.frag) and its
receivers (res/shaders/ww_sunshadow.glsl ussCloudT / wwSunShadow), through the real driver, no NifSkope.

  python -I tests/spells/cloudshadow_check.py <scratch> [--red vertical|noinject|nofade] [--only CVSOWT]

The light space (basis, fit, matrix) comes from src/gl/cloudshadowmath.h through tests/cloudshadow/cloudshadow_dump.cpp;
the noise and calibration from cloudvol_check's dump (the exe's bytes). The judge computes every expectation from the sun
direction and the fixture alone (never from the basis).
  C  centroid: the ground shadow's centroid == the cloud disc's centre moved along the sun by h / tan(elev) (map mode)
  V  volume mode: the shadow moves along the sun between the base's and the top's projections; none off the sun's line
  S  shafts: a gap in a full deck lights a tilted shaft in a vertical air slice (the froxels' points), following the sun
  O  off: ussCloudOn 0 gives T == 1.0 exactly and wwSunShadow == wwSunShadowGeo exactly; on: wwSunShadow == Geo x T
  W  wiring: the froxel pass (cell_volair) reads ussCloudT; the noinject red zeroes it for volume programs only
  T  cost: GL_TIME_ELAPSED of the build per preset, full and one 1/16 row block (this machine's GPU, offline)
Reds: vertical (the C++ basis' own red: the shadow straight under the cloud) fails C, V and S; noinject (ussCloudOn 0
in the air) fails S.
"""
import ctypes, math, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudshadow_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('CVSOWT')
os.makedirs(scratch, exist_ok=True)

sys.path.insert(0, HERE)
sys.argv = [sys.argv[0], scratch, '--only', '-']   # cloudvol_check: its helpers and dump, none of its gates
import cloudvol_check as K   # noqa: E402
import numpy as np            # noqa: E402

S, C, g, gl = K.S, K.C, K.g, K.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = os.path.join(TREE, 'res', 'shaders')
UNIT_KM = 0.0142875 / 1000.0   # atmomath.h kUnitKm
LOW = (1.5, 3.5, 1.0)
f_um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
f_linkst = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))

ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
DUMP = os.path.join(scratch, 'cloudshadow_dump.exe')
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(TREE, 'tests', 'cloudshadow', 'cloudshadow_dump.cpp'), '-o', DUMP], env=ENV,
                   capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])


def light(sun, eyeKm, extent, size, vertical):
    out = subprocess.run([DUMP] + ['%.9g' % v for v in sun] + ['1' if vertical else '0'] + ['%.9g' % v for v in eyeKm]
                         + ['%.9g' % extent, str(size), '%.9g' % UNIT_KM], env=ENV, capture_output=True, text=True,
                         check=True).stdout.split('\n')
    d = {l.split()[0]: [float(x) for x in l.split()[1:]] for l in out if l.strip()}
    b = d['basis']
    return {'s': b[0:3], 'e1': b[3:6], 'e2': b[6:9], 'fit': d['fit'], 'mat': d['mat']}


def linked(p, name):
    ok = GLint()
    f_linkst(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL ' + name)
    return p


P_MAP = linked(C.build(SDIR, 'cloud_shadowmap.prog'), 'cloud_shadowmap.prog')
P_PROBE = linked(C.build(SDIR, None, [os.path.join(SDIR, 'cell_ao.vert'),
                                      os.path.join(TREE, 'tests', 'cloudshadow', 'cloudshadow_probe.frag')]), 'probe')


def target(w, h, fmt=0x881A):   # RGBA16F: the exe's format
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x0DE1, t)
    gl.glTexImage2D(0x0DE1, 0, fmt, w, h, 0, 0x1908, 0x1406, None)
    for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F)):
        gl.glTexParameteri(0x0DE1, k, v)
    fb = GLuint()
    g.genfb(1, ctypes.byref(fb))
    g.bindfb(0x8D40, fb)
    g.fbtex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
    return t.value, fb.value


def read(fb, w, h):
    g.bindfb(0x8D40, fb)
    buf = (ctypes.c_float * (w * h * 4))()
    gl.glReadPixels(0, 0, w, h, 0x1908, 0x1406, buf)
    return np.frombuffer(buf, np.float32).reshape(h, w, 4).astype(np.float64)


def setmat4(prog, name, m):
    l = g.loc(prog, name.encode())
    if l >= 0:
        a = (ctypes.c_float * 16)(*m)
        f_um4(l, 1, 0, a)


def sundir(el, az):
    e, a = math.radians(el), math.radians(az)
    return (math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e))


def disc_map(n, half, centre, radius, value=1.0, inside=True):
    """CLOUD1's map image: R (Low) = value inside (or outside) a disc, world km square [-half, half]"""
    x = (np.arange(n) + 0.5) / n * 2 * half - half
    X, Y = np.meshgrid(x, x)
    d = np.hypot(X - centre[0], Y - centre[1])
    m = (d < radius) if inside else (d >= radius)
    img = np.zeros((n, n, 4))
    img[..., 0] = np.where(m, value, 0.0)
    return img


def build_map(L, size, mode, mapimg, half, steps=12, cov_out=0.0, rows=None, time_q=None):
    """runs cloud_shadowmap.prog as cloudshadow.cpp does; returns the RGBA16F texture, its fb and the texels"""
    tex, fb = target(size, size)
    mtex = K.tex2d(mapimg)
    sh, de = K.textures()
    uni = K.bands_uni(LOW, (5.0, 6.5, 0.0), K.cal, cov=(cov_out, 0.0, 0.0), rect=(-half, -half, 1.0 / (2 * half), 1.0))
    uni.update({'csE1': tuple(L['e1']), 'csE2': tuple(L['e2']), 'csS': tuple(L['s']), 'csFit': tuple(L['fit']),
                'csSize': (float(size), float(size)), 'csMode': mode, 'csSteps': max(steps, 1)})
    g.bindfb(0x8D40, fb)
    gl.glViewport(0, 0, size, size)
    g.use(P_MAP)
    K.setuni(P_MAP, uni)
    # three distinct units, as the exe's 47-49 (a sampler3D left on unit 0 beside cvMap fails the draw)
    K.bind(P_MAP, [('cvShape', 0x806F, sh), ('cvDetail', 0x806F, de), ('cvMap', 0x0DE1, mtex)])
    if rows is not None:
        gl.glEnable(0x0C11)
        gl.glScissor(0, rows[0], size, rows[1])
    if time_q:
        time_q[0]()
    gl.glDrawArrays(5, 0, 4)
    if time_q:
        time_q[1]()
    gl.glDisable(0x0C11)
    return tex, fb, (read(fb, size, size) if time_q is None else None)


def probe(L, maptex, n_w, n_h, mode, grid, plane, axis=(1.0, 0.0, 0.0), on=1, far=(1.0, 0.1, 0.0, 0.5)):
    t, fb = target(n_w, n_h, 0x8814)
    g.bindfb(0x8D40, fb)
    gl.glViewport(0, 0, n_w, n_h)
    g.use(P_PROBE)
    K.setuni(P_PROBE, {'prMode': mode, 'prGrid': grid, 'prPlane': plane, 'prAxis': axis, 'ussCloudOn': on,
                       'ussCloudFar': far, 'ussOn': 0, 'ussCsmOn': 0, 'ussTerrOn': 0, 'ussContactOn': 0,
                       'csmForce': (-1.0,), 'csmRed': 0, 'csmProbe': 0})
    setmat4(P_PROBE, 'ussCloudFromView', L['mat'])   # view == world here: worldFromView is the identity
    blank = K.tex2d(np.ones((1, 1, 4)))
    arr = GLuint()
    gl.glGenTextures(1, ctypes.byref(arr))
    K.bind(P_PROBE, [('ussCloudMap', 0x0DE1, maptex), ('ussTerrMap', 0x0DE1, blank), ('ussContactMap', 0x0DE1, blank),
                     ('csmMap', 0x8C1A, arr.value)])
    gl.glDrawArrays(5, 0, 4)
    return read(fb, n_w, n_h)


results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg))


EL, AZ = 30.0, 40.0
SUN = sundir(EL, AZ)
EYE = (0.4, -0.3, 0.01)
EXT, HALF = 48.0, 24.0
vert = red == 'vertical'
sxy = np.array(SUN[:2]) / SUN[2]   # ground shift per km of height, along the sun (toward the sun)

# ---- C: map-mode centroid ------------------------------------------------------------------------------------------
if 'C' in only:
    N = 256
    L = light(SUN, EYE, EXT, N, vert)
    centre = (3.0, -2.0)
    tex, fb, m = build_map(L, N, 2, disc_map(512, HALF, centre, 2.0), HALF)
    G = 400
    span = 20.0
    grid = (-span / UNIT_KM, -span / UNIT_KM, 2 * span / G / UNIT_KM, 2 * span / G / UNIT_KM)
    T = probe(L, tex, G, G, 0, grid, (0.0, 0.0, 0.0))[..., 0]
    x = (np.arange(G) + 0.5) / G * 2 * span - span
    X, Y = np.meshgrid(x, x)
    w = 1.0 - T
    got = np.array([(w * X).sum() / w.sum(), (w * Y).sum() / w.sum()])
    h = 0.5 * (LOW[0] + LOW[1])   # map mode reads the coverage at the band's middle
    want = np.array(centre) - sxy * h
    err = float(np.hypot(*(got - want)))
    gate('C centroid', err < 0.1, 'shadow centroid (%.3f, %.3f) km, want (%.3f, %.3f) (disc - s.xy/s.z x %.1f km): %.3f km off '
         '(bar 0.1, texel %.3f); darkest T %.4f' % (got[0], got[1], want[0], want[1], h, err, L['fit'][3], T.min()))
    # the edge: past the map the far value, exactly
    pe = np.array(L['e1'][:2]) * (L['fit'][0] + 0.75 * EXT)   # ground points 0.25 of a side past the map's e1 edge
    far = probe(L, tex, 4, 1, 0, (pe[0] / UNIT_KM, pe[1] / UNIT_KM, 1.0, 1.0), (0.0, 0.0, 0.0),
                far=(0.37, 0.1, 0.0, 0.5))[..., 0]
    gate('C edge', bool(np.all(np.abs(far - 0.37) < 1e-6)), 'past the map T = %s (want the far value 0.37)' % np.round(far[0], 6))

# ---- V: volume mode, the real noise ----------------------------------------------------------------------------------
if 'V' in only:
    N = 512
    L = light(SUN, EYE, EXT, N, vert)
    Lv = light(SUN, EYE, EXT, N, True)
    centre = (2.0, 1.0)
    img = disc_map(512, HALF, centre, 5.0, value=0.7)
    texS, _, mS = build_map(L, N, 1, img, HALF, steps=12)
    texV, _, mV = build_map(Lv, N, 1, img, HALF, steps=12)
    G, span = 400, 16.0
    grid = (-span / UNIT_KM, -span / UNIT_KM, 2 * span / G / UNIT_KM, 2 * span / G / UNIT_KM)
    x = (np.arange(G) + 0.5) / G * 2 * span - span
    X, Y = np.meshgrid(x, x)

    def cen(tex, LL):
        T = probe(LL, tex, G, G, 0, grid, (0.0, 0.0, 0.0))[..., 0]
        w = 1.0 - T
        return np.array([(w * X).sum() / w.sum(), (w * Y).sum() / w.sum()]), T
    cs, Ts = cen(texS, L)
    cv, Tv = cen(texV, Lv)
    d = cv - cs                       # the shift toward... away from the sun: cs = cv - sxy h
    u = sxy / np.linalg.norm(sxy)
    along, perp = float(d @ u), float(abs(d[0] * u[1] - d[1] * u[0]))
    lo, hi = LOW[0] * np.linalg.norm(sxy), LOW[1] * np.linalg.norm(sxy)
    hbar = along / np.linalg.norm(sxy)
    gate('V volume', lo - 0.1 <= along <= hi + 0.1 and perp < 0.15,
         'shift along the sun %.3f km (base..top %.3f..%.3f) = mean height %.2f km; across %.3f km (bar 0.15); '
         'shadowed share %.3f, darkest T %.4f, texels with cloud %.3f' % (along, lo, hi, hbar, perp, float((Ts < 0.5).mean()),
                                                                           Ts.min(), float((mS[..., 3] > 0).mean())))
    # a receiver above the cloud's top is lit; inside the band partly
    zt = (LOW[1] + 0.2) / UNIT_KM
    up = probe(L, texS, G, 1, 0, (grid[0], (cv[1]) / UNIT_KM, grid[2], 1.0), (zt, 0.0, 0.0))[..., 0]
    gate('V above', bool(np.all(up == 1.0)), 'receivers 0.2 km over the top: min T %.6f (want exactly 1)' % up.min())

# ---- S: shafts in the air under a gap --------------------------------------------------------------------------------
if 'S' in only:
    N = 256
    L = light(SUN, EYE, EXT, N, vert)
    gapc = (1.0, 0.5)
    tex, fb, m = build_map(L, N, 2, disc_map(512, HALF, gapc, 1.2, inside=False), HALF)
    # the slice: vertical, along the sun's azimuth through the gap's centre, heights 0.1 .. 1.4 km (the froxels' air)
    axis = (math.cos(math.radians(AZ)), math.sin(math.radians(AZ)), 0.0)
    GW, GH, span = 480, 14, 8.0
    z0, dz = 0.1, 0.1
    grid = (-span / UNIT_KM, z0 / UNIT_KM, 2 * span / GW / UNIT_KM, dz / UNIT_KM)
    on = 0 if red == 'noinject' else 1   # cloudshadow.cpp: volume programs get ussCloudOn 0 under the red
    T = probe(L, tex, GW, GH, 1, grid, (0.0, gapc[0] / UNIT_KM, gapc[1] / UNIT_KM), axis, on=on)[..., 0]
    s = (np.arange(GW) + 0.5) / GW * 2 * span - span
    h = 0.5 * (LOW[0] + LOW[1])
    cot = 1.0 / math.tan(math.radians(EL))
    errs, cons, cens = [], [], []
    for j in range(GH):
        z = z0 + j * dz
        row = T[j]
        lit = row > 0.5
        cons.append(float(row.max() / max(row.min(), 1e-6)))
        if not lit.any():
            errs.append(99.0)
            continue
        c = float((s * lit).sum() / lit.sum())
        cens.append(c)
        errs.append(abs(c - (-(h - z) * cot)))
    worst = max(errs)
    gate('S shafts', worst < 0.15 and min(cons) > 10.0,
         'the lit shaft follows the sun: worst centre %.3f km off (bar 0.15) over %d heights; contrast lit/dark min %.1f '
         '(bar 10); slope want %.3f km/km' % (worst, GH, min(cons), cot))
    moved = (cens[-1] - cens[0]) if len(cens) == GH else 0.0
    gate('S tilt', abs(moved - cot * (GH - 1) * dz) < 0.15,
         'the shaft centre moves %.3f km over %.1f km of height (want %.3f, bar 0.15)' % (moved, (GH - 1) * dz,
                                                                                     cot * (GH - 1) * dz))

# ---- O: off is exact; on composes by multiply ------------------------------------------------------------------------
if 'O' in only:
    N = 256
    L = light(SUN, EYE, EXT, N, False)
    tex, fb, m = build_map(L, N, 2, disc_map(512, HALF, (3.0, -2.0), 2.0), HALF)
    G, span = 200, 20.0
    grid = (-span / UNIT_KM, -span / UNIT_KM, 2 * span / G / UNIT_KM, 2 * span / G / UNIT_KM)
    off = probe(L, tex, G, G, 0, grid, (0.0, 0.0, 0.0), on=0)
    onp = probe(L, tex, G, G, 0, grid, (0.0, 0.0, 0.0), on=1)
    ok_off = bool(np.all(off[..., 0] == 1.0) and np.all(off[..., 1] == off[..., 2]))
    comp = float(np.abs(onp[..., 1] - onp[..., 2] * onp[..., 0]).max())
    gate('O off', ok_off, 'ussCloudOn 0: T == 1 at %d of %d, wwSunShadow == Geo at %d of %d' % (
        int((off[..., 0] == 1.0).sum()), G * G, int((off[..., 1] == off[..., 2]).sum()), G * G))
    gate('O compose', comp < 1e-6 and float(onp[..., 0].min()) < 0.1,
         'on: |wwSunShadow - Geo x T| max %.2e (bar 1e-6), darkest T %.4f' % (comp, onp[..., 0].min()))

# ---- W: source wiring ------------------------------------------------------------------------------------------------
if 'W' in only:
    va = open(os.path.join(SDIR, 'cell_volair.frag'), encoding='latin-1').read()
    cs = open(os.path.join(TREE, 'src', 'gl', 'cloudshadow.cpp'), encoding='latin-1').read()
    ss = open(os.path.join(TREE, 'src', 'gl', 'shadowset.cpp'), encoding='latin-1').read()
    fn = re.search(r'float airCloudShadow\( vec3 posView \)\s*\{\s*return ussCloudT\( posView \);', va)
    uses = len(re.findall(r'airCloudShadow\( posView \)', va))
    noinj = '!( volume && red( "noinject" ) )' in cs
    call = 'wwCloudShadowUniforms( scene, prog, volume, s.airAmb );' in ss
    gate('W wiring', bool(fn) and uses >= 3 and bool(noinj) and call,
         'cell_volair airCloudShadow -> ussCloudT %s, its uses %d (>= 3: the march, the far tail, the no-cascade sun), '
         'noinject only for volume programs %s, shadowset passes volume %s' % (bool(fn), uses, bool(noinj), call))

# ---- T: cost ---------------------------------------------------------------------------------------------------------
if 'T' in only:
    genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))
    beginq = S.fn('glBeginQuery', None, GLenum, GLuint)
    endq = S.fn('glEndQuery', None, GLenum)
    getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
    q = GLuint()
    genq(1, ctypes.byref(q))
    img = disc_map(512, HALF, (0.0, 0.0), 1e3, value=0.6)   # the worst case: cloud over the whole map
    L512 = light(SUN, EYE, EXT, 512, False)
    L1024 = light(SUN, EYE, EXT, 1024, False)
    for name, LL, size, mode, steps in (('Low', L512, 512, 2, 0), ('Medium', L512, 512, 1, 12), ('High', L1024, 1024, 1, 16),
                                        ('Ultra', L1024, 1024, 1, 16)):
        line = []
        for part, rows in (('full', None), ('rows', (0, size // 16))):
            ts = []
            for rep in range(12):
                gl.glFinish()
                build_map(LL, size, mode, img, HALF, steps=steps, rows=rows,
                          time_q=(lambda: beginq(0x88BF, q), lambda: endq(0x88BF)))
                v = ctypes.c_uint64()
                getq(q, 0x8866, ctypes.byref(v))
                ts.append(v.value / 1e6)
            line.append('%s %.3f ms' % (part, sorted(ts)[len(ts) // 2]))
        mb = size * size * 8 / 2 ** 20
        print('T cost       INFO  %-6s %4d^2 %s steps %2d: %s; a frame pays the rows (a fit move pays full once); '
              'memory %.0f MB RGBA16F' % (name, size, 'map ' if mode == 2 else 'vol ', steps, ', '.join(line), mb))

if __name__ == '__main__':
    print('CLOUDSHADOW-CHECK %s (%d of %d)%s' % ('PASS' if all(results) else 'FAIL', sum(results), len(results),
                                                 ' red=' + red if red else ''))
    sys.exit(0 if all(results) else 1)
