"""Lane CLOUD6f (CLOUDBAND1 leftover, RDR2 gap f): lightning lit inside the volume clouds for storm weathers. Offline
gates through the real res/shaders/cloud_march.prog + cloud_composite.prog (hidden GL context, this machine's driver),
the CPU side src/gl/cloudlightning.h through tests/cloudlightning/cloudlightning_dump.cpp (g++), CLOUD2's helpers from
cloudvol_check (imported, none of its gates run). No NifSkope.

    python -I tests/spells/cloudlightning_check.py <scratch dir> [--red today|iso|thunderbyte] [--only TDHLIFSE]

RDR2 (Bauer 2019, slide 40 + note 40): a strike is a point light inside the cloud, exponential falloff + HG phase, a
simplified local extinction, added to the scattering term. Here: the diffusion Green's function exp(-r/delta)/r with
images across the Low band's extrapolated boundaries plus the first-order radiance term (cloudlightning.h), added in
the composite at the clouds' mean depth (no density read; the march is not touched).

Gates (bars written before the first run, CLOUD6f 2026-10-30; a bar changed later says so here):
  T timing   the schedule (dump, 0..7200 s at 1 ms, two seeds): the lit share 0.01 .. 0.08, the mean gap between strikes
             8 .. 14 s, every flash <= 1.0 s and inside its slot, no lit sample outside a stroke window, the clock read
             twice gives the same bits. Red today (no lightning, lit share 0) fails.
  D switch   WW_CLOUD_LIGHTNING: unset / empty / 1 / on / x = on; 0 / off / false / no = off (cloudvolmath.h rule).
  H change   the storm share during a weather change: no change = the storm flag (1 / 0 at every pct); a change into a
             storm is 0 up to its fade-in pct and 1 at pct 1; a change out of one is 1 at pct 0 and 0 from its fade-out on.
  L inside   a uniform slab (the Low band 1.5 .. 3.0 km, density 1: tau 60), the strike at 35 % of its depth, seen
             straight up from 0.5 km at horizontal offsets 0, 0.25, 0.5, 1.0, 1.5 km: the flash's radiance per unit
             source power (the real march's T and depth, then the real composite) within 0.5 .. 2.0 x a Monte Carlo of
             the same slab (isotropic point source, the droplet phase, albedo 0.99) at every offset whose Monte Carlo
             bin holds >= 400 photons, and the exitance over the offsets (trapezoid, 2 pi rho) within 0.67 .. 1.5 x.
             Red today (main's composite: no flash) fails. INFO: the iso candidate (no first-order term).
  I identity no storm, or a storm between flashes, or the switch off (cvLtOn 0): the composite == main's composite
             bit for bit in all three modes (backdrop, display over the scene, linear over the scene) on a storm
             march with the sun on. Control: a flash (cvLtOn 1) must change > 0 px, else the gate cannot tell.
  F field    a storm sky (Low cover 0.8), the camera on the ground looking 20 deg up at a strike 6 km ahead: sky px
             untouched, added light >= 0, the light-weighted mean angle from the strike's direction <= 12 deg; the
             GPU flash == cloudlightning.h fieldAt (the CPU twin) on 200 interior px, relative |d| <= 2e-3. Red today fails.
  S storms   (needs the game's masters; SKIP when absent) the rule "a WTHR SNAM of type 3 (thunder)" over the vanilla
             weathers: >= 1 storm, none of the Pleasant-only (DATA flag 1) weathers, every *Radstorm* but the unused
             *RadstormOld*, CommonwealthRain. Red thunderbyte (CLOUD5's `DATA thunder byte > 0`) fails.
  E cost     INFO: GPU ms of the composite at 1080p over a 480 x 270 history, flash on vs off.
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, struct, subprocess, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudlightning_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('TDHLIFSE')
BASE = os.environ.get('CLOUDLT_BASE', '0e7c2c3a')   # main before CLOUD6f (the composite with no flash)
os.makedirs(scratch, exist_ok=True)

sys.path.insert(0, HERE)
sys.argv = [sys.argv[0], scratch, '--only', '-']   # cloudvol_check: its helpers and dump, none of its gates
import cloudvol_check as K   # noqa: E402
import numpy as np            # noqa: E402

S, C, g, gl = K.S, K.C, K.g, K.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = os.path.join(TREE, 'res', 'shaders')
ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


DUMP = os.path.join(scratch, 'cloudlightning_dump.exe')
_r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                     os.path.join(TREE, 'tests', 'cloudlightning', 'cloudlightning_dump.cpp'), '-o', DUMP], env=ENV,
                    capture_output=True, text=True)
if _r.returncode:
    raise SystemExit('BUILD FAIL ' + _r.stderr[-2000:])


def dump(*a, inp=None):
    return subprocess.run([DUMP] + [str(x) for x in a], env=ENV, input=inp, capture_output=True, text=True, check=True,
                          timeout=300).stdout.split('\n')


f_linkst = S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))


def linked(p, name):
    ok = GLint()
    f_linkst(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL ' + name)
    return p


VERT = os.path.join(SDIR, 'cell_ao.vert')
P_COMP = linked(C.build(SDIR, 'cloud_composite.prog'), 'cloud_composite.prog')
# main's composite (the red, and the identity reference): its frag + include from git, in scratch
BDIR = os.path.join(scratch, 'base_shaders')
os.makedirs(BDIR, exist_ok=True)
for fn in ('cloud_composite.frag', 'lookdev_output.glsl'):
    b = subprocess.run(['git', '-C', TREE, 'show', '%s:res/shaders/%s' % (BASE, fn)], capture_output=True, check=True).stdout
    open(os.path.join(BDIR, fn), 'wb').write(b)
P_BASE = linked(C.build(SDIR, None, [VERT, os.path.join(BDIR, 'cloud_composite.frag')]), 'base cloud_composite')
P_USE = P_BASE if red == 'today' else P_COMP
f_um3 = K.f_um3
f_clearcol = gl.glClearColor   # a GL 1.1 export (no wglGetProcAddress)
f_clearcol.argtypes = [ctypes.c_float] * 4


def setmat3(prog, name, m):
    l = g.loc(prog, name.encode())
    if l >= 0:
        a = np.ascontiguousarray(np.asarray(m, np.float32))
        f_um3(l, 1, 0, a.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))


def field_consts(sigma, base, top, src, flux=True, sunK=1.0):
    out = dump('field', sigma, K.ALBEDO, K.MSG, base, top, src[0], src[1], src[2], 1 if flux else 0, sunK)
    v = [float(x) for x in [l for l in out if l.startswith('fieldc ')][0].split()[1:]]
    return {'src': v[0:3], 'invDelta': v[3], 'r0sq': v[4], 'mirLo': v[5], 'mirHi': v[6], 'flux': v[7], 'norm': v[8],
            'power': v[9]}


def lt_uni(fc, on=1, col=None):
    c = fc['norm'] if col is None else col
    return {'cvLtOn': on, 'cvLtSrc': (fc['src'][0], fc['src'][1], fc['src'][2], fc['invDelta']),
            'cvLtPrm': (fc['r0sq'], fc['mirLo'], fc['mirHi'], fc['flux']),
            'cvLtCol': (c, c, c) if isinstance(c, float) else tuple(c)}


SCENE_HALF = K.tex2d(np.full((1, 1, 4), 0.5), linear=False)   # a window depth of 0.5: never "sky"


def composite(prog, hist, W, H, uni, mode=2, scene=None, sceneZ=(0.0, 0.0, 0.0, 0.0), tgt=None, exposure=1.0):
    tgt = tgt or K.Mrt(W, H)
    g.bindfb(0x8D40, tgt.fb)
    gl.glViewport(0, 0, W, H)
    f_clearcol(0.0, 0.0, 0.0, 0.0)
    gl.glClear(0x4000)
    g.use(prog)
    u = {'cvRect': (0.0, 0.0, 1.0 / W, 1.0 / H), 'sceneExposure': (exposure,), 'viewTransform': 0, 'cvFront': mode,
         'cvSceneZ': sceneZ}
    u.update(uni)
    cam = u.pop('cvCam', None)
    K.setuni(prog, u)
    if cam is not None:
        setmat3(prog, 'cvCam', cam)
    K.bind(prog, [('cvHist', 0x0DE1, hist.col), ('cvHistDepth', 0x0DE1, hist.dep), ('cvScene', 0x0DE1, scene or SCENE_HALF)])
    gl.glDrawArrays(5, 0, 4)
    return tgt


def march(uni, W, H, texs):
    tgt = K.Mrt(W, H)
    K.draw(K.P_MARCH, tgt, uni, texs)
    return tgt


def wthr_census():
    """(edid, DATA flags, DATA thunder byte, thunder-type SNAM count) of the vanilla weathers; None without the masters"""
    data_dir = os.environ.get('FO4_DATA', r'X:/Programs/Steam/steamapps/common/Fallout 4/Data')
    masters = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm', 'DLCworkshop02.esm',
               'DLCworkshop03.esm', 'DLCNukaWorld.esm']
    if not os.path.exists(os.path.join(data_dir, masters[0])):
        return None

    def fields(body):
        out, p, big = [], 0, None
        while p + 6 <= len(body):
            t, n = struct.unpack_from('<4sH', body, p)
            p += 6
            if t == b'XXXX':
                big = struct.unpack_from('<I', body, p)[0]
                p += n
                continue
            if big is not None:
                n, big = big, None
            out.append((t, body[p:p + n]))
            p += n
        return out

    def records(data, pos, end):
        while pos < end:
            typ, size, flags, fid = struct.unpack_from('<4sIII', data, pos)
            if typ == b'GRUP':
                yield from records(data, pos + 24, pos + size)
                pos += size
            else:
                yield flags, fid, pos + 24, size
                pos += 24 + size

    out = {}
    for m in masters:
        p = os.path.join(data_dir, m)
        if not os.path.exists(p):
            continue
        data = open(p, 'rb').read()
        hsz = struct.unpack_from('<I', data, 4)[0]
        mast = [v.rstrip(b'\0').decode('latin1') for t, v in fields(data[24:24 + hsz]) if t == b'MAST']
        pos = 24 + hsz
        while pos < len(data):
            typ, size, label = struct.unpack_from('<4sI4s', data, pos)
            if label == b'WTHR':
                for flags, fid, off, sz in records(data, pos + 24, pos + size):
                    b = data[off:off + sz]
                    if flags & 0x40000:
                        b = zlib.decompress(b[4:])
                    fl = fields(b)
                    edid = [v for t, v in fl if t == b'EDID']
                    d = [v for t, v in fl if t == b'DATA']
                    if not d or len(d[0]) < 15:
                        continue
                    nth = sum(1 for t, v in fl if t == b'SNAM' and len(v) >= 8 and struct.unpack_from('<I', v, 4)[0] == 3)
                    hi = fid >> 24
                    owner = mast[hi] if hi < len(mast) else m
                    out[(owner, fid & 0xFFFFFF)] = (edid[0].rstrip(b'\0').decode('latin1') if edid else '', d[0][11],
                                                    d[0][10], nth)
            pos += size
    return list(out.values())


# ---- T: the schedule ----------------------------------------------------------------------------------------------
if 'T' in only:
    rows, ok = [], True
    for seed in (12345, 0x001CCE1F):
        l = [x for x in dump('timing', seed, 0, 7200, 0.001) if x.startswith('timing ')][0].split()
        v = dict(zip(l[1::2], l[2::2]))
        duty, gap, mx = float(v['duty']), float(v['meangap']), float(v['maxlen'])
        if red == 'today':
            duty, gap = 0.0, 0.0   # today: no lightning at all
        ok &= 0.01 <= duty <= 0.08 and 8.0 <= gap <= 14.0 and mx <= 1.0
        ok &= int(v['outside']) == 0 and int(v['cross']) == 0 and int(v['nondet']) == 0
        rows.append('seed %d: lit %.4f, %s strikes, mean gap %.2f s, longest %.3f s, outside %s, cross %s, nondet %s'
                    % (seed, duty, v['strikes'], gap, mx, v['outside'], v['cross'], v['nondet']))
    gate('T timing', ok, '; '.join(rows) + ' (bars lit 0.01 .. 0.08, gap 8 .. 14 s, flash <= 1 s, 0 / 0 / 0)')

# ---- D: the switch rule -------------------------------------------------------------------------------------------
if 'D' in only:
    want = {'-': 1, '': 1, '1': 1, 'on': 1, 'x': 1, '0': 0, 'off': 0, 'false': 0, 'no': 0}
    got = {l.split(' ')[1]: int(l.split(' ')[-1]) for l in dump('switch', *want.keys()) if l.startswith('switch ')}
    bad = [k for k in want if got.get(k) != want[k]]
    gate('D switch', not bad, '%d values, wrong: %s' % (len(want), bad or 'none'))

# ---- H: the storm share through a weather change -------------------------------------------------------------------
if 'H' in only:
    rows = [list(map(float, l.split()[1:])) for l in dump('share') if l.startswith('share ')]
    pct = [0.0, 0.25, 0.5, 0.75, 0.9, 1.0]
    ok = True
    for now, fi, ch, last, fo, *w in rows:
        if not ch:
            ok &= all(x == (1.0 if now else 0.0) for x in w)
            continue
        if now:
            ok &= all(x == 0.0 or p > fi for x, p in zip(w, pct) if not last) and w[-1] == 1.0
        if last and not now:
            ok &= w[0] == 1.0 and all(x == 0.0 for x, p in zip(w, pct) if p >= fo)
        if not now and not last:
            ok &= all(x == 0.0 for x in w)
    gate('H change', ok, '; '.join('now %d fi %.2f change %d last %d fo %.2f -> %s' % (r[0], r[1], r[2], r[3], r[4],
                                    ' '.join('%.2f' % x for x in r[5:])) for r in rows))


# ---- L: lit from inside vs a Monte Carlo ---------------------------------------------------------------------------
def mc_point(sigma, base, top, zs, prm, n=400000, cone=0.97, seed=5, edges=None):
    """radiance leaving the slab's base straight down (within acos(cone)) per unit source power, binned by the exit's
    horizontal distance from the source's axis; an isotropic point source at height zs; returns (L per bin, photons)"""
    rng = np.random.default_rng(seed)
    cs = np.linspace(-1, 1, 20001)
    pdf = K.droplet(prm, cs) * 2 * math.pi
    cdf = np.concatenate([[0], np.cumsum((pdf[1:] + pdf[:-1]) / 2 * np.diff(cs))])
    cdf /= cdf[-1]
    pos = np.zeros((n, 3))
    pos[:, 2] = zs
    u = rng.random(n) * 2 - 1
    ph = rng.random(n) * 2 * math.pi
    d = np.stack([np.sqrt(1 - u * u) * np.cos(ph), np.sqrt(1 - u * u) * np.sin(ph), u], 1)
    w = np.ones(n)
    alive = np.ones(n, bool)
    ex_r, ex_w = [], []
    for _ in range(20000):
        idx = np.nonzero(alive)[0]
        if idx.size == 0:
            break
        s = -np.log(1 - rng.random(idx.size)) / sigma
        np_ = pos[idx] + d[idx] * s[:, None]
        ob = np_[:, 2] < base
        ot = np_[:, 2] > top
        if ob.any():
            ii = idx[ob]
            # where the path crosses the base plane
            tb = (base - pos[ii, 2]) / d[ii, 2]
            xy = pos[ii, :2] + d[ii, :2] * tb[:, None]
            keep = -d[ii, 2] >= cone
            ex_r.append(np.hypot(xy[keep, 0], xy[keep, 1]))
            ex_w.append(w[ii][keep])
        gone = idx[ob | ot]
        alive[gone] = False
        st = idx[~(ob | ot)]
        pos[st] = np_[~(ob | ot)]
        w[st] *= K.ALBEDO
        dead = st[w[st] < 1e-4]
        alive[dead] = False
        st = st[w[st] >= 1e-4]
        ct = np.interp(rng.random(st.size), cdf, cs)
        phi = rng.random(st.size) * 2 * math.pi
        st_ = np.sqrt(np.maximum(0, 1 - ct * ct))
        dd = d[st]
        a = np.where(np.abs(dd[:, 2:3]) < 0.999, np.array([[0, 0, 1.0]]), np.array([[1.0, 0, 0]]))
        u1 = np.cross(dd, a)
        u1 /= np.linalg.norm(u1, axis=1, keepdims=True)
        u2 = np.cross(dd, u1)
        d[st] = dd * ct[:, None] + u1 * (st_ * np.cos(phi))[:, None] + u2 * (st_ * np.sin(phi))[:, None]
    r = np.concatenate(ex_r) if ex_r else np.zeros(0)
    ww = np.concatenate(ex_w) if ex_w else np.zeros(0)
    omega = 2 * math.pi * (1 - cone)
    mu = (1 + cone) / 2
    L, cnt = [], []
    for r0, r1 in edges:
        m = (r >= r0) & (r < r1)
        area = math.pi * (r1 * r1 - r0 * r0)
        L.append(float(ww[m].sum()) / n / (area * omega * mu))
        cnt.append(int(m.sum()))
    return L, cnt


if 'L' in only:
    base, top, zs = 1.5, 3.0, 1.5 + 0.35 * 1.5
    offs = [0.0, 0.25, 0.5, 1.0, 1.5]
    edges = [(0.0, 0.08), (0.17, 0.33), (0.42, 0.58), (0.9, 1.1), (1.4, 1.6)]
    mc_file = os.path.join(scratch, 'mc_point.npy')
    if os.path.exists(mc_file):
        mc, cnt = [list(x) for x in np.load(mc_file)]
        cnt = [int(c) for c in cnt]
    else:
        mc, cnt = mc_point(K.SIGMA, base, top, zs, K.drop, edges=edges)
        np.save(mc_file, np.array([mc, cnt], float))
    W, H = 16, 16
    slab = (base, top, 1.0)
    out = {}
    for cand, flux in (('flux', red != 'iso'), ('iso', False)):
        fc = field_consts(K.SIGMA, base, top, (0.0, 0.0, zs), flux=flux)
        vals = []
        for rho in offs:
            camKm = (rho, 0.0, 0.5)
            cam = K.camera(180, 89.5)
            u = K.bands_uni(slab, (9.0, 9.1, 0.0), K.cal, cov=(1.0, 0.0, 0.0))
            u.update(K.march_uni(cam, camKm, (W, H), 1, (0, 0), 60.0, steps=192, taps=6, slab=1.0, drop=K.drop, vfov=4.0))
            u['cvKeyK'] = (0.0,)
            u['cvSunK'] = (0.0,)
            m = march(u, W, H, K.march_texs(K.trans, K.trans))
            cu = dict(lt_uni(fc), cvCam=cam, cvCamKm=camKm, cvProj=u['cvProj'])
            o = composite(P_USE, m, W, H, cu).read()
            mo = m.read()
            c = (o[6:10, 6:10, :3] - mo[6:10, 6:10, :3]).reshape(-1, 3).mean(0)
            vals.append(float(c[1]))
        out[cand] = vals

    def exitance(v):
        return sum(0.5 * (v[i] * 2 * math.pi * offs[i] + v[i + 1] * 2 * math.pi * offs[i + 1]) * (offs[i + 1] - offs[i])
                   for i in range(len(offs) - 1))
    ok = True
    rows = []
    ship = out['flux']
    for i, rho in enumerate(offs):
        rr = ship[i] / mc[i] if mc[i] > 0 else float('nan')
        judged = cnt[i] >= 400
        if judged:
            ok &= 0.5 <= rr <= 2.0
        rows.append('rho %.2f: %.4g vs MC %.4g (%d ph) ratio %.3f%s | iso %.3f' % (
            rho, ship[i], mc[i], cnt[i], rr, '' if judged else ' INFO', out['iso'][i] / mc[i] if mc[i] > 0 else float('nan')))
    ex, exm = exitance(ship), exitance(mc)
    ok &= exm > 0 and 0.67 <= ex / exm <= 1.5
    gate('L inside', ok, '; '.join(rows) + '; exitance %.4g vs MC %.4g ratio %.3f (bars 0.5 .. 2.0 per offset, 0.67 .. 1.5 '
         'exitance)%s' % (ex, exm, ex / exm if exm > 0 else float('nan'), ' (red %s)' % red if red else ''))


# ---- a storm sky's march (I, F, E) ---------------------------------------------------------------------------------
def storm_march(W, H, cam, camKm, sun_el=35.0):
    sv = K.sky.view_lut(192, 108, 6360.0 + camKm[2], K.R.sundir(30, sun_el))
    u = K.bands_uni(K.LOW, K.MID, K.cal, cov=(0.8, 0.3, 0.0))
    u.update(K.march_uni(cam, camKm, (W, H), 1, (0, 0), sun_el, sun_az=30, steps=96, taps=6, drop=K.drop, amb=0.05,
                         ap=1.0, lightKm=1.2, maxKm=100.0, vfov=60.0))
    return march(u, W, H, K.march_texs(K.trans, sv)), u


if 'I' in only or 'F' in only:
    W, H = 320, 180
    cam = K.camera(0, 20)
    camKm = (0.0, 0.0, 0.05)
    SM, SU = storm_march(W, H, cam, camKm)
    src = (6.0 * math.cos(math.radians(0)), 0.0, 1.5 + 0.35 * (K.LOW[1] - K.LOW[0]))
    FC = field_consts(K.SIGMA * K.LOW[2], K.LOW[0], K.LOW[1], src, sunK=1.0)
    col = FC['norm'] * FC['power']   # a level-1 flash at sunK 1 (the sky's own scale here)

if 'I' in only:
    scene = K.tex2d(np.full((H, W, 4), 0.995), linear=False)
    # a scene 100 km x |v| away (z 0.995): every cloud is in front of it, so modes 1 and 2 draw (amended after the first
    # run: -0.20002 put the scene at 20 m and both programs discarded every px, the control read 0)
    sz = (-1.0002, -1020.0, 0.001, 1.0)
    rows, ok = [], True
    for mode in (0, 1, 2):
        base_o = composite(P_BASE, SM, W, H, {'cvProj': SU['cvProj']}, mode=mode, scene=scene, sceneZ=sz).read()
        for tag, extra in (('unset', {}), ('cvLtOn 0', dict(lt_uni(FC, on=0, col=col), cvCam=cam, cvCamKm=camKm))):
            # 'unset' on a FRESH program: a program keeps its uniforms between draws (the first run read the last
            # mode's flash there), so cloudvol.cpp sets cvLtOn on every draw
            prog = linked(C.build(SDIR, 'cloud_composite.prog'), 'fresh') if tag == 'unset' else P_COMP
            o = composite(prog, SM, W, H, dict(extra, cvProj=SU['cvProj']), mode=mode, scene=scene, sceneZ=sz).read()
            d = int(np.count_nonzero(o != base_o))
            ok &= d == 0
            rows.append('mode %d %s: %d values differ' % (mode, tag, d))
        on = composite(P_USE, SM, W, H, dict(lt_uni(FC, on=1, col=col), cvCam=cam, cvCamKm=camKm, cvProj=SU['cvProj']),
                       mode=mode, scene=scene, sceneZ=sz).read()
        ctrl = int(np.count_nonzero(np.any(on != base_o, axis=2)))
        ok &= ctrl > 0
        rows.append('mode %d flash: %d px change (control)' % (mode, ctrl))
    gate('I identity', ok, '; '.join(rows) + ' (bar: 0 off, > 0 px with a flash)%s' % (' (red %s)' % red if red else ''))

if 'F' in only:
    off = composite(P_BASE, SM, W, H, {'cvProj': SU['cvProj']}, mode=2).read()
    on = composite(P_USE, SM, W, H, dict(lt_uni(FC, on=1, col=col), cvCam=cam, cvCamKm=camKm, cvProj=SU['cvProj']),
                   mode=2).read()
    add = (on[..., :3] - off[..., :3])[..., 1]
    m = SM.read()
    a = 1.0 - m[..., 3]
    sky = a <= 1e-4
    sky_same = int(np.count_nonzero(np.any(on[sky] != off[sky], axis=1)))
    neg = float(add.min())
    # the light-weighted mean angle from the strike's direction
    ys, xs = np.mgrid[0:H, 0:W]
    ndc = np.stack([(xs + 0.5) / W * 2 - 1, (ys + 0.5) / H * 2 - 1], -1)
    pr = SU['cvProj']
    camM = np.asarray(cam).reshape(3, 3).T   # columns right, up, back
    v = np.stack([(ndc[..., 0] + pr[2]) / pr[0], (ndc[..., 1] + pr[3]) / pr[1], -np.ones_like(ndc[..., 0])], -1)
    dirs = v @ camM.T
    dirs /= np.linalg.norm(dirs, axis=-1, keepdims=True)
    sdir = np.array([src[0] - camKm[0], src[1] - camKm[1], src[2] - camKm[2]])
    sdir /= np.linalg.norm(sdir)
    ang = np.degrees(np.arccos(np.clip(dirs @ sdir, -1, 1)))
    wsum = float(np.clip(add, 0, None).sum())
    mean_ang = float((np.clip(add, 0, None) * ang).sum() / wsum) if wsum > 0 else float('inf')
    # the CPU twin: the composite's px centre is the history texel's centre here (same size), so the gather's 2x2 is
    # {x, x+1} x {y, y+1} or, by rounding, {x-1, x} x {y-1, y}: px whose own texel is the 3x3's min have one dm either way
    # (amended after the first run, before any bar moved: "3x3 flat to 1e-4" found 0 px on a noise sky)
    dep = SM.read(1)[..., 0]
    worst, nt = 0.0, 0
    cand = [(y, x) for y in range(2, H - 2, 2) for x in range(2, W - 2, 2)
            if a[y, x] > 0.2 and dep[y, x] < 1e3 and dep[y, x] == dep[y - 1:y + 2, x - 1:x + 2].min() and add[y, x] > 0]
    pts = []
    for y, x in cand[::max(1, len(cand) // 200)][:200]:
        dm = float(dep[y, x])
        dd = dirs[y, x]
        e = np.array([dd[0] * dm, dd[1] * dm, 6360.0 + camKm[2] + dd[2] * dm])
        p = (camKm[0] + dd[0] * dm, camKm[1] + dd[1] * dm, float(np.linalg.norm(e)) - 6360.0)
        pts.append((y, x, p, dd))
    if pts and red != 'today':
        inp = ''.join('%.9g %.9g %.9g %.9g %.9g %.9g\n' % (*p, *dd) for _, _, p, dd in pts)
        fv = [float(l.split()[1]) for l in dump('field', K.SIGMA * K.LOW[2], K.ALBEDO, K.MSG, K.LOW[0], K.LOW[1], src[0],
                                                src[1], src[2], 1, 1.0, inp=inp) if l.startswith('f ')]
        # fieldAt includes norm, the shader's colour does: want = a x fieldAt x col / norm
        want = np.array([a[y, x] * fval * col / FC['norm'] for (y, x, p, dd), fval in zip(pts, fv)])
        got = np.array([add[y, x] for y, x, p, dd in pts])
        nt = len(pts)
        worst = float((np.abs(got - want) / np.maximum(want, 1e-3 * want.max())).max()) if nt else 1.0
    ok = sky_same == 0 and neg >= 0.0 and wsum > 0 and mean_ang <= 12.0 and nt >= 50 and worst <= 2e-3
    gate('F field', ok, 'sky px changed %d; min added %.3g; added light %.4g; mean angle from the strike %.2f deg (bar 12); '
         'twin on %d px worst rel %.2g (bar 2e-3)%s' % (sky_same, neg, wsum, mean_ang, nt, worst,
                                                       ' (red %s)' % red if red else ''))

# ---- S: which weathers storm ---------------------------------------------------------------------------------------
if 'S' in only:
    ws = wthr_census()
    if ws is None:
        print('S storms     SKIP  no Fallout 4 masters (FO4_DATA)', flush=True)
    else:
        storm = (lambda e: e[2] > 0) if red == 'thunderbyte' else (lambda e: e[3] > 0)
        st = [e for e in ws if storm(e)]
        pleasant = [e for e in ws if e[1] == 1 and storm(e)]
        rad = [e for e in ws if 'radstorm' in e[0].lower() and 'old' not in e[0].lower()]
        rad_miss = [e[0] for e in rad if not storm(e)]
        rain = [e for e in ws if e[0] == 'CommonwealthRain']
        ok = len(st) >= 1 and not pleasant and not rad_miss and rain and storm(rain[0])
        gate('S storms', ok, '%d weathers: %d storm (%s); Pleasant-only storms %d; radstorms missed %s; CommonwealthRain %s'
             % (len(ws), len(st), ', '.join(sorted(e[0] for e in st))[:400], len(pleasant), rad_miss or 'none',
                'storm' if rain and storm(rain[0]) else 'not storm'))

# ---- E: cost ------------------------------------------------------------------------------------------------------
if 'E' in only:
    q = ctypes.c_uint()
    S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(ctypes.c_uint))(1, ctypes.byref(q))
    begq = S.fn('glBeginQuery', None, ctypes.c_uint, ctypes.c_uint)
    endq = S.fn('glEndQuery', None, ctypes.c_uint)
    getq = S.fn('glGetQueryObjectui64v', None, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint64))
    hw, hh = 480, 270
    camE = K.camera(0, 20)
    hm, hu = storm_march(hw, hh, camE, (0.0, 0.0, 0.05))
    big = K.Mrt(1920, 1080)
    srcE = (6.0, 0.0, 2.2)
    fcE = field_consts(K.SIGMA, K.LOW[0], K.LOW[1], srcE)
    ms = []
    for on in (0, 1):
        uni = dict(lt_uni(fcE, on=on, col=fcE['norm'] * fcE['power']), cvCam=camE, cvCamKm=(0.0, 0.0, 0.05), cvProj=hu['cvProj'])
        g.bindfb(0x8D40, big.fb)
        gl.glViewport(0, 0, 1920, 1080)
        g.use(P_COMP)
        u = {'cvRect': (0.0, 0.0, 1.0 / 1920, 1.0 / 1080), 'sceneExposure': (1.0,), 'viewTransform': 0, 'cvFront': 0,
             'cvSceneZ': (0.0, 0.0, 0.0, 0.0)}
        u.update(uni)
        c3 = u.pop('cvCam')
        K.setuni(P_COMP, u)
        setmat3(P_COMP, 'cvCam', c3)
        K.bind(P_COMP, [('cvHist', 0x0DE1, hm.col), ('cvHistDepth', 0x0DE1, hm.dep), ('cvScene', 0x0DE1, SCENE_HALF)])
        gl.glDrawArrays(5, 0, 4)
        gl.glFinish()
        begq(0x88BF, q)
        for _ in range(50):
            gl.glDrawArrays(5, 0, 4)
        endq(0x88BF)
        ns = ctypes.c_uint64()
        getq(q, 0x8866, ctypes.byref(ns))
        ms.append(ns.value / 1e6 / 50)
    print('E cost       INFO  composite at 1080p over a 480x270 history (this machine\'s GPU, offline): off %.4f ms, '
          'flash %.4f ms' % (ms[0], ms[1]), flush=True)

print('CLOUDLIGHTNING-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
