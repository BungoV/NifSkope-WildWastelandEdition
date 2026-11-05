"""lane SHAFTS1: sun shafts in the cell air, gated offline on the REAL cell_volair inject stage (tests/features/air/airsun's plumbing).
Scene: a shadow-casting layer 6000 units toward the sun with round gaps (beams), the sun 28 deg off the view axis (into the
sun, like Concord c3), FO4CS-level shadows (the world-unit disc, Ultra cascades). The integrated volume is read as surfaces
read it (bilinear in the screen at the backdrop's slice) on 1920 px scanlines, against an exact hard-shadow ray march.
usage: python -I tests/features/air/shafts/shafts_check.py <scratch> [--base <commit>] [--red soft|bead] [--only ZSBC] [--quick]
Bars (pre-registered from the attribution runs, notes/shafts1: today 0.647 / 0.752, one tap 0.759 / 0.911):
  Z  identity  the reds (volSunTap 0 and 1, volSunBead 0) == the base program (bf1c42a2, SHAFTS1's tip), integrated L and T
               bit for bit at 12 px x 64, frame lit
  S  sharpness gradient kept >= 0.72 at 12 px x 64 (Low) and >= 0.85 at 8 px x 64 (Medium); --red soft judges the soft
               disc as shipped and must FAIL. Lane AIRBEAD1 INFO rows: the switch's gradient, and the gradient projected
               on the exact one (sum gi gr / sum gr gr; along-ray beads count as "sharp" in mean |d/dx|)
  B  beading   lane AIRBEAD1: radial roughness on screen (second difference along the beams, as surfaces read the volume)
               with WW_CELL_AIR_SHADOW=bead (two reads) <= 0.85 x the one middle read at 12 px and 8 px; --red bead
               judges the middle read as the switch and must FAIL
  C  cost      sun-only inject, base and new interleaved, median of 41: new - base <= +0.05 ms at 160x90x64 AND 240x135x64
               as shipped; INFO: the same for WW_CELL_AIR_SHADOW=bead (over the bar at Medium, so it ships off)
               (lane AIRBEAD1: base = bf1c42a2 with its tap, the lane's bar; SHAFTS1 had +0.15 against the soft disc);
               also printed: the Medium grid vs the Low grid (the "Medium by default" row)
"""
import ctypes, math, os, shutil, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
args = sys.argv[1:]
SCR = args[0]
BASE = args[args.index('--base') + 1] if '--base' in args else 'bf1c42a2'
RED = args[args.index('--red') + 1] if '--red' in args else ''
ONLY = set(args[args.index('--only') + 1]) if '--only' in args else set('ZSBC')
QUICK = '--quick' in args
os.makedirs(SCR, exist_ok=True)
import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402

BDIR = os.path.join(SCR, 'base_shaders')
if os.path.isdir(BDIR):
    shutil.rmtree(BDIR)
shutil.copytree(os.path.join(TREE, 'res', 'shaders'), BDIR)
GIT = shutil.which('git') or 'E:/Tools/Git/cmd/git.exe'
r = subprocess.run([GIT, '-C', TREE, 'show', BASE + ':res/shaders/cell_volair.frag'], capture_output=True)
if r.returncode != 0:
    raise SystemExit('git show failed: ' + r.stderr.decode('latin-1'))
open(os.path.join(BDIR, 'cell_volair.frag'), 'wb').write(r.stdout)

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
G = C.Gl()
f_u4f = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
f_u3i = S.fn('glUniform3i', None, GLint, GLint, GLint, GLint)
f_um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
f_pget = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
f_active = S.fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_int),
                ctypes.POINTER(GLint), ctypes.POINTER(GLenum), ctypes.c_char_p)
f_tex3d = S.fn('glTexImage3D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum,
               ctypes.c_void_p)
f_genq = S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(GLuint))
f_begq = S.fn('glBeginQuery', None, GLenum, GLuint)
f_endq = S.fn('glEndQuery', None, GLenum)
f_getq = S.fn('glGetQueryObjectui64v', None, GLuint, GLenum, ctypes.POINTER(ctypes.c_uint64))
SAMPLERS = set(range(0x8B5D, 0x8B65)) | set(range(0x8DC0, 0x8DDA)) | set(range(0x9108, 0x910E)) | set(range(0x900C, 0x9010))

W, H = 1920, 1080
FOVY = 60.0
P11 = 1.0 / math.tan(math.radians(FOVY / 2))
P00 = P11 * H / W
NEAR, FARS, DB = 16.0, 24000.0, 5000.0
SIG = 1.0 / 50000.0
Lv = np.array([0.15, 0.45, -0.88]); Lv /= np.linalg.norm(Lv)
E1 = np.cross(Lv, [0.0, 1.0, 0.0]); E1 /= np.linalg.norm(E1)
E2 = np.cross(Lv, E1)
HALF = [1000.0, 3250.0, 8000.0]
RANGE, MAPN, OCC, WR = 40000.0, 4096, 6000.0, 3.5
GAP = [25.0, 240.0]
results = []


def gate(name, ok, msg):
    results.append(bool(ok))
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


def gaps(a, b):
    cell = GAP[1]
    j = np.floor(b / cell)
    a2 = a + (j % 2) * cell * 0.5
    da = a2 - (np.floor(a2 / cell) + 0.5) * cell
    db = b - (j + 0.5) * cell
    return da * da + db * db < GAP[0] * GAP[0]


def lit_at(P):
    return (P @ Lv >= OCC) | gaps(P @ E1, P @ E2)


def shadow_map():
    data = np.ones((3, MAPN, MAPN), np.float32)
    c = (np.arange(MAPN) + 0.5) / MAPN - 0.5
    for i, h in enumerate(HALF):
        A, B = np.meshgrid(c * 2 * h, c * 2 * h)
        data[i][~gaps(A, B)] = 0.5 - OCC / (2 * RANGE)
    data = np.ascontiguousarray(data)
    t = GLuint()
    gl.glGenTextures(1, ctypes.byref(t))
    gl.glBindTexture(0x8C1A, t)
    f_tex3d(0x8C1A, 0, 0x8CAC, MAPN, MAPN, 3, 0, 0x1902, 0x1406, data.ctypes.data)
    for k, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F), (0x884C, 0x884E), (0x884D, 0x0203)):
        gl.glTexParameteri(0x8C1A, k, v)
    return t.value


def csm_mat(i):
    M = np.zeros((4, 4))
    M[0, :3] = E1 / (2 * HALF[i]); M[0, 3] = 0.5
    M[1, :3] = E2 / (2 * HALF[i]); M[1, 3] = 0.5
    M[2, :3] = -Lv / (2 * RANGE); M[2, 3] = 0.5
    M[3, 3] = 1.0
    return M


def program(sdir):
    p = C.build(sdir, 'cell_volair.prog')
    ok = GLint()
    f_pget(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL ' + sdir)
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
            units[buf.value.decode()] = unit
            unit += 1
    return p, units


def setu(p, name, v):
    l = G.loc(p, name.encode())
    if l < 0:
        return
    if isinstance(v, (bool, int)):
        G.u1i(l, int(v))
    elif isinstance(v, float):
        G.u1f(l, v)
    elif isinstance(v, np.ndarray) and v.shape == (4, 4):
        m = np.ascontiguousarray(v.T.astype(np.float32))
        f_um4(l, 1, 0, m.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
    elif len(v) == 2:
        G.u2f(l, *[float(x) for x in v])
    elif len(v) == 3:
        G.u3f(l, *[float(x) for x in v])
    else:
        f_u4f(l, *[float(x) for x in v])


def uniforms(p, units, csm, cols, rows, sl, tap):
    G.use(p)
    setu(p, 'volStage', 1)
    f_u3i(G.loc(p, b'volDims'), cols, rows, sl)
    setu(p, 'volProj', (P00, P11))
    setu(p, 'volSlices', (NEAR, math.log(FARS / NEAR), float(sl), 1.0))
    setu(p, 'fogView', (0.0, 1.0, 0.0, 1000.0))
    setu(p, 'fogDistScale', 1.0)
    setu(p, 'fogOn', 1)
    setu(p, 'volPhase[0]', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'volPhase[1]', (1.0, 1.0, 1.0, 0.0))
    setu(p, 'volPhase[2]', (0.0, 0.0, 0.0, 0.0))
    setu(p, 'airMed', (SIG, 0.0, 0.0, 20000.0))
    for n in ('airLayer', 'airGround', 'volDrift', 'volMs'):
        setu(p, n, (0.0, 0.0, 0.0, 0.0))
    for n in ('airTerrOn', 'volTemporal', 'volDepthSkip', 'volFarMode', 'volLayerBase', 'rfOn', 'cellLightCount',
              'cellBulbCount', 'ussTerrOn', 'ussContactOn', 'ussCloudOn', 'ussView', 'ussRed', 'csmRed', 'csmProbe'):
        setu(p, n, 0)
    setu(p, 'volRed', 256)
    setu(p, 'volTerms', 1)
    setu(p, 'volJitter', 0.0)
    setu(p, 'cellOn', 1)
    setu(p, 'cellHasDir', 1)
    setu(p, 'cellInterior', 0)
    setu(p, 'cellDirTo', tuple(Lv))
    setu(p, 'cellDirColor', (1.0, 1.0, 1.0))
    for k in range(3):
        row = [0.0] * 4
        row[k] = 1.0
        setu(p, 'cellRow[%d]' % k, tuple(row))
    setu(p, 'volCsm', 1)
    setu(p, 'volSunAir', 1)
    setu(p, 'volSunTap', int(tap))
    setu(p, 'volSunBead', int(BEAD[0]))   # lane AIRBEAD1: the two reads a sub-step (base programs have no such uniform)
    setu(p, 'volSunRoom', 0)
    setu(p, 'ussOn', 1)
    setu(p, 'ussCsmOn', 1)
    setu(p, 'csmForce', -1.0)
    for i in range(3):
        setu(p, 'csmMat[%d]' % i, csm_mat(i))
    setu(p, 'csmInvRange', tuple([1.0 / (2 * RANGE)] * 3))
    setu(p, 'csmParams', (1.0, 16000.0, float(MAPN), 400.0))
    setu(p, 'csmSplitOffset', (2000.0, 6500.0, 0.275, 1.0))
    setu(p, 'ussCsmFilter', (1.0, WR, 32.0, 0.0))   # the air: vogel, Ultra radius, 32 taps, no receiver plane
    setu(p, 'ussCsmWorld', (1.0, WR * 0.8, WR * 3.0, WR * 12.0))   # WW_SHADOW_PAR=all (FO4CS-level shadows)
    setu(p, 'ussCsmTexel', tuple(2 * h / MAPN for h in HALF))
    setu(p, 'ussCsmBias', tuple(1.5 * 2 * h / MAPN for h in HALF))
    setu(p, 'ussCsmPhase', 0.0)
    setu(p, 'ussCsmStable', (1.0, 0.0, 0.0, 0.0))
    setu(p, 'ussCsmCellX', (0.0, 0.0, 0.0))
    setu(p, 'ussCsmCellY', (0.0, 0.0, 0.0))
    G.active(0x84C0 + units['csmMap'])
    gl.glBindTexture(0x8C1A, csm)


FB = {}


def target(cols, rows):
    if (cols, rows) not in FB:
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x0DE1, t)
        gl.glTexImage2D(0x0DE1, 0, 0x8814, cols, rows, 0, 0x1908, 0x1406, None)
        for k, v in ((0x2801, 0x2600), (0x2800, 0x2600)):
            gl.glTexParameteri(0x0DE1, k, v)
        fb = GLuint()
        G.genfb(1, ctypes.byref(fb))
        G.bindfb(0x8D40, fb)
        G.fbtex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        FB[(cols, rows)] = fb.value
    return FB[(cols, rows)]


def volume(p, units, csm, px, sl, tap):
    """integrated L and T at the backdrop's slice (cols x rows), and that slice's far distance"""
    cols, rows = int(round(W / px)), int(round(H / px))
    lr = math.log(FARS / NEAR)
    kb = int(round(math.log(DB / NEAR) / lr * sl)) - 1
    uniforms(p, units, csm, cols, rows, sl, tap)
    G.bindfb(0x8D40, target(cols, rows))
    gl.glViewport(0, 0, cols, rows)
    l = G.loc(p, b'volLayer')
    buf = (ctypes.c_float * (cols * rows * 4))()
    L = np.zeros((rows, cols))
    T = np.ones((rows, cols))
    for k in range(kb + 1):
        G.u1i(l, k)
        gl.glDrawArrays(5, 0, 4)
        gl.glReadPixels(0, 0, cols, rows, 0x1908, 0x1406, buf)
        a = np.frombuffer(buf, np.float32).reshape(rows, cols, 4).astype(np.float64)
        L += T * a[..., 1]
        T *= a[..., 3]
    return L, T, NEAR * math.exp(lr * (kb + 1) / sl)


def scan(L, ys):
    rows, cols = L.shape
    out = []
    for y in ys:
        gy = (H - 1 - y + 0.5) / H * rows - 0.5
        xs = (np.arange(W) + 0.5) / W * cols - 0.5
        x0 = np.clip(np.floor(xs).astype(int), 0, cols - 1); x1 = np.clip(x0 + 1, 0, cols - 1)
        fx = np.clip(xs - np.floor(xs), 0, 1)
        y0 = int(np.clip(math.floor(gy), 0, rows - 1)); y1 = min(y0 + 1, rows - 1)
        fy = min(max(gy - math.floor(gy), 0.0), 1.0)
        out.append((L[y0, x0] * (1 - fx) + L[y0, x1] * fx) * (1 - fy) + (L[y1, x0] * (1 - fx) + L[y1, x1] * fx) * fy)
    return np.array(out)


def reference(ys, tb, n=3000):
    out = []
    t = NEAR * np.exp(np.linspace(0, math.log(tb / NEAR), n + 1))
    tm = 0.5 * (t[1:] + t[:-1])
    w = SIG * np.exp(-SIG * tm) * (t[1:] - t[:-1])
    for y in ys:
        xn = (np.arange(W) + 0.5) / W * 2 - 1
        yn = 1 - (y + 0.5) / H * 2
        d = np.stack([xn / P00, np.full(W, yn / P11), -np.ones(W)], -1)
        d /= np.linalg.norm(d, axis=-1, keepdims=True)
        acc = np.zeros(W)
        for i0 in range(0, n, 500):
            P = d[:, None, :] * tm[None, i0:i0 + 500, None]
            acc += (lit_at(P) * w[None, i0:i0 + 500]).sum(1)
        out.append(acc)
    return np.array(out)


CSM = shadow_map()
PN, UN = program(os.path.join(TREE, 'res', 'shaders'))
PB, UB = program(BDIR)
TAP = 0 if RED == 'soft' else 1   # the shipped value (C++: volSunTap = !WW_CELL_AIR_SHADOW_RED=soft)
SHIP_BEAD = 0 if RED == 'mid' else 1   # lane AIRBEAD2: shipped on (bungo 11-03; C++: volSunBead = WW_CELL_AIR_SHADOW != mid); --red mid = the one middle read
ON_BEAD = 0 if RED == 'bead' else 1   # the switch's reads that B and C judge; --red bead judges the one middle read
BEAD = [SHIP_BEAD]

if 'Z' in ONLY:
    for tap in (0, 1):
        BEAD[0] = 0
        Lb, Tb, _ = volume(PB, UB, CSM, 12, 64, tap)
        Lr, Tr, _ = volume(PN, UN, CSM, 12, 64, tap)
        d = max(float(np.abs(Lb - Lr).max()), float(np.abs(Tb - Tr).max()))
        lit = int((Lb > 0).sum())
        gate('Z identity%d' % tap, d == 0.0 and lit > 0.5 * Lb.size, 'base %s vs the reds (volSunTap %d, volSunBead 0) at '
             '160x90x64: max |dL|,|dT| %g, lit froxel columns %d of %d' % (BASE, tap, d, lit, Lb.size))
    BEAD[0] = SHIP_BEAD


def box_rough(L, px):
    """lane AIRBEAD1: the screen as surfaces read it (the bilinear froxel read, 4 px samples), mean-normalised; rms of the
    second difference ALONG the beam (radial from the sun's screen point, 1 sample apart), below the sun"""
    rows, cols = L.shape
    A = L / L.mean()
    sx = (Lv[0] / -Lv[2] * P00 * 0.5 + 0.5) * cols - 0.5
    sy = (Lv[1] / -Lv[2] * P11 * 0.5 + 0.5) * rows - 0.5

    def bil(x, y):
        x = np.clip(x, 0, cols - 1.001); y = np.clip(y, 0, rows - 1.001)
        x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int); fx = x - x0; fy = y - y0
        return ((A[y0, x0] * (1 - fx) + A[y0, x0 + 1] * fx) * (1 - fy) + (A[y0 + 1, x0] * (1 - fx) + A[y0 + 1, x0 + 1] * fx) * fy)
    SY, SX = np.mgrid[0:270, 0:480].astype(float)
    gx, gy = (SX + 0.5) / 480 * cols - 0.5, (SY + 0.5) / 270 * rows - 0.5
    dx, dy = gx - sx, gy - sy
    dd = np.maximum(np.hypot(dx, dy), 1e-6)
    ux, uy = dx / dd * cols / 480, dy / dd * rows / 270
    d2 = bil(gx + ux, gy + uy) + bil(gx - ux, gy - uy) - 2 * bil(gx, gy)
    m = (dd > 6) & (gy < sy - 4) & (gx > 1) & (gx < cols - 2) & (gy > 1)
    return float(np.sqrt((d2[m] ** 2).mean()))


if 'S' in ONLY:
    ys = [330, 610] if QUICK else [200, 330, 470, 610, 760, 900]
    ref, tref = None, -1.0
    for px, bar in ((12, 0.72), (8, 0.85)):
        L, T, tb = volume(PN, UN, CSM, px, 64, TAP)
        if ref is None or abs(tb - tref) > 1.0:
            ref, tref = reference(ys, tb), tb
        img = scan(L, ys)
        img = img / (img.mean() / ref.mean())
        sharp = np.abs(np.diff(img, axis=1)).mean() / np.abs(np.diff(ref, axis=1)).mean()
        gate('S sharp %dpx' % px, sharp >= bar, 'gradient kept %.3f (bar >= %.2f; 1 = the exact hard-shadow march; '
             'volSunTap %d volSunBead %d)' % (sharp, bar, TAP, BEAD[0]))
        BEAD[0] = ON_BEAD   # lane AIRBEAD1: the switch, and the gradient projected on the exact one (beads do not count)
        img2 = scan(volume(PN, UN, CSM, px, 64, TAP)[0], ys)
        img2 = img2 / (img2.mean() / ref.mean())
        gr = np.diff(ref, axis=1)
        proj = [float((np.diff(v, axis=1) * gr).sum() / (gr * gr).sum()) for v in (img, img2)]
        print('%-12s INFO  WW_CELL_AIR_SHADOW=bead: gradient kept %.3f (off %.3f); projected on the exact gradient %.3f '
              '(off %.3f)' % ('S bead %dpx' % px, np.abs(np.diff(img2, axis=1)).mean() / np.abs(gr).mean(), sharp,
                             proj[1], proj[0]), flush=True)
        BEAD[0] = SHIP_BEAD

if 'B' in ONLY:
    for px, bar in ((12, 0.85), (8, 0.85)):
        BEAD[0] = 0
        r0 = box_rough(volume(PN, UN, CSM, px, 64, TAP)[0], px)
        BEAD[0] = ON_BEAD
        r1 = box_rough(volume(PN, UN, CSM, px, 64, TAP)[0], px)
        BEAD[0] = SHIP_BEAD
        gate('B bead %dpx' % px, r1 <= bar * r0, 'radial roughness on screen (beads along the beams) %.4f vs the middle read '
             '%.4f: x%.3f (bar <= x%.2f; WW_CELL_AIR_SHADOW=bead, volSunBead %d)' % (r1, r0, r1 / r0, bar, ON_BEAD))

if 'C' in ONLY:
    q = (GLuint * 1)()
    f_genq(1, q)

    def once(p, units, cols, rows, tap, bead=SHIP_BEAD):
        BEAD[0] = bead
        uniforms(p, units, CSM, cols, rows, 64, tap)
        G.bindfb(0x8D40, target(cols, rows))
        gl.glViewport(0, 0, cols, rows)
        l = G.loc(p, b'volLayer')
        gl.glFinish()
        f_begq(0x88BF, q[0])
        for k in range(64):
            G.u1i(l, k)
            gl.glDrawArrays(5, 0, 4)
        f_endq(0x88BF)
        r = ctypes.c_uint64()
        f_getq(q[0], 0x8866, ctypes.byref(r))
        return r.value / 1e6

    arms = {'base low': (PB, UB, 160, 90, TAP), 'new low': (PN, UN, 160, 90, TAP),
            'base med': (PB, UB, 240, 135, TAP), 'new med': (PN, UN, 240, 135, TAP),
            'bead low': (PN, UN, 160, 90, TAP, ON_BEAD), 'bead med': (PN, UN, 240, 135, TAP, ON_BEAD)}
    ms = {k: [] for k in arms}
    for k in arms:
        once(*arms[k])   # warm
    for rep in range(41):
        for k in (arms if rep % 2 == 0 else list(arms)[::-1]):
            ms[k].append(once(*arms[k]))
    m = {k: float(np.median(v)) for k, v in ms.items()}
    dl, dm = m['new low'] - m['base low'], m['new med'] - m['base med']
    BEAD[0] = SHIP_BEAD
    gate('C cost', dl <= 0.10 and dm <= 0.10, 'sun-only inject x64, interleaved median of 41, as shipped: Low 160x90 base %.3f new %.3f '
         '(%+.3f ms); Medium 240x135 base %.3f new %.3f (%+.3f ms); bar <= +0.10 (AIRBEAD2: bungo took the beads at +0.07)' % (m['base low'], m['new low'], dl,
                                                                                  m['base med'], m['new med'], dm))
    bl, bm = m['bead low'] - m['base low'], m['bead med'] - m['base med']
    print('%-12s INFO  the beaded reads (volSunBead %d): Low %+.3f ms, Medium %+.3f ms (AIRBEAD1 bar +0.05: %s; shipped on '
          'at +0.07, bungo 11-03)' % ('C bead', ON_BEAD, bl, bm, 'within' if bl <= 0.05 and bm <= 0.05 else 'over'), flush=True)
    print('%-12s INFO  Medium grid by default (new med - base low): %+.3f ms in the sun-only inject; the whole air pass '
          'scales with the froxel count (x2.25)' % ('C grid', m['new med'] - m['base low']), flush=True)

print('SHAFTS-CHECK %s (%d of %d)%s' % ('PASS' if results and all(results) else 'FAIL', sum(results), len(results),
                                      ' [red %s]' % RED if RED else ''))
sys.exit(0 if results and all(results) else 1)
