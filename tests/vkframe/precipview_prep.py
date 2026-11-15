"""Lane VKFAR2 (row L8 slice 1): the job of tests/vkframe/precipview.cpp, written as files the C++ harness reads: the GL
program (the tree's cell_precip_gpu.vert / .geom / .frag with the loader's #include expansion and its WW_CELL_FOGLEAK
define, as the app's cell_precip_gpu.prog; _nofl copies without it for the harness's fogleak check), and per case the field's uniforms from precipmath.h gpuRun (through the PERFFIX3 dump exe,
tests/features/weather/precipgpu/precipgpu_dump.cpp: never re-typed), the view, the atlas as the app loads it (BC3,
every level, clamp: bc3enc.py; the rain cases too, rain_near_fog sRGB BC3; snow_near_disc keeps no atlas), the run
and the slab's depth (halfway across the widest gap between two particle depths near the median, from the dump's CPU
particles, so no particle sits on the slab).

    python -I tests/vkframe/precipview_prep.py <out dir> <precipgpu_dump.exe>
"""
import math, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
WEATHER = os.path.join(TREE, 'res', 'shaders', 'features', 'weather')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402
sys.path.insert(0, HERE)
from bc3enc import bc3, levels  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
EXE = sys.argv[2]
W, H, FOV = 960, 540, 55.0
MINPX, SHUTTER = 0.75, 1.0 / 120.0
CELL_PX = 64
# name, type, size, rate, seed, t, cam, wind, far, drawScale, spin, sway, windShare, weight, hasTex, yaw, pitch, cap, fog,
# look (0 cell_precip_gpu.prog, 1 the drop / 2 the snow of cell_precip_look.prog)
CASES = [
    ('snow_near_tex', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 0, 3.0, 0.5, 6.0, 1.0, 1.0, 1,
     0.7, -0.15, 40000, 0, 0),
    ('rain_near_disc', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0),
    ('snow_far_mean', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 0, 0),
    # slice 2a: the same runs seen through the air (wwFogFx's A / B in the record pass)
    ('rain_near_fog', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 1, 0),
    ('snow_far_fog', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 1, 0),
    # slice 3: the look program. The drop (swept-disc coverage, the flicker) unfogged; the snow with its normal map, the
    # clump threshold and the albedo through the air, at a small clock (snow_near_tex's 600 s sway phase is its own row)
    ('rain_drop_look', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 1),
    ('snow_look_near', 3, 5.5, 1.0, 97, 37.25, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 0, 3.0, 0.5, 6.0, 1.0, 1.0, 1,
     0.7, -0.15, 40000, 1, 2),
    # the near snow with no atlas (the round disc), plain and unfogged: splits the near textured snow's open flips into
    # the per-fragment atlas read (this case clean) or the quad / position path (this case red too)
    ('snow_near_disc', 3, 5.5, 1.0, 97, 37.25, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 0, 3.0, 0.5, 6.0, 1.0, 1.0, 0,
     0.7, -0.15, 40000, 0, 0),
    # slice 2b (lit = element 20, last: GL keeps a program's uniforms, the unlit cases reset theirs): the sun alone on
    # rain_near_disc's run unfogged, and on snow_far_fog's through the air (the veil on, the air's column on the sun)
    ('rain_near_lit', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 1),
    ('snow_far_lit', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 1, 0, 1),
    # L8 term 2a (element 20 = volTerms): the sun + the lamps (flat loop, no shadow / gobo) on the two lit cases' runs
    ('rain_near_lamps', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 3),
    ('snow_far_lamps', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 1, 0, 3),
]
SUN = np.array([0.4, 0.3, 0.35]) / np.linalg.norm([0.4, 0.3, 0.35])  # world, toward the sun (the air's fogSun too)
# a lit case's own sun (cellDirTo), chosen by the GS's mix: N = the eye's axis, thick / thin = 4 vpWrap( N.to, pcTrans )
# (volGi 1). With SUN, rain_near_lit's view gives N.to -0.13, ratio 0.99: thin = thick, pcTrans unseen (51 samples, the
# first lit run 11-15); this sun gives N.to 0.72, ratio 3.3, 44 degrees off the axis. SUN stays (the fogged cases' fogSun).
LIT_SUN = {'rain_near_lit': np.array([0.3, -0.6, 0.75]) / np.linalg.norm([0.3, -0.6, 0.75])}
LIT_SUN['rain_near_lamps'] = LIT_SUN['rain_near_lit']


def lamps(v, R, eye):
    """the lamp cases' cellLights (celllights.cpp's 8 texels a light): an omni, a spot, a hemisphere, a box and one out of
    reach, placed round the in-view particles' middle (view units = world units, s = their median spread). t0 pos +
    radius, t1 colour + shape (-2 omni, -3 hemisphere, -4 box, else the spot's outer cosine), t2 axis + cone exponent,
    t3 bias / scale / exponent / flags (the game's curve 1 - x), t4 shadow slot -1, t5..t7 the box's rows"""
    cen = np.median(v, axis=0)
    s = float(np.median(np.linalg.norm(v - cen, axis=1)))
    w = lambda p: R.T @ (cen + s * np.array(p)) + eye  # noqa: E731 (view offset -> world)
    L = np.zeros((5, 8, 4))
    def put(k, p, r, col, shape, axis=(0.0, 0.0, 1.0), cone=0.0):
        L[k, 0] = list(w(p)) + [r * s]
        L[k, 1] = list(col) + [shape]
        L[k, 2] = list(axis) + [cone]
        L[k, 3] = [0.0, 1.0, 1.0, 0.0]
        L[k, 4] = [-1.0, 0.0, 0.0, 0.0]
    put(0, (0.5, 0.3, 0.2), 2.0, (2.4, 1.3, 0.5), -2.0)
    sp = w((-1.2, 0.2, 0.6))
    ax = w((-0.3, 0.0, 0.0)) - sp
    put(1, (-1.2, 0.2, 0.6), 3.5, (0.4, 1.0, 2.4), 0.8, ax / np.linalg.norm(ax), 1.5)
    put(2, (0.0, -1.0, 0.0), 2.5, (1.5, 0.3, 1.2), -3.0, R.T @ np.array([0.0, 1.0, 0.0]))
    put(3, (0.8, -0.6, -0.4), 3.0, (0.6, 1.8, 0.7), -4.0)
    bc, h = w((0.5, -0.4, -0.2)), 1.2 * s
    for i in range(3):  # k_i = dot( b_i.xyz, P ) + b_i.w in [-1, 1]: a world-axis box round bc
        L[3, 5 + i, i] = 1.0 / h
        L[3, 5 + i, 3] = -bc[i] / h
    put(4, (60.0, 0.0, 0.0), 0.5, (3.0, 3.0, 3.0), -2.0)
    return L.astype(np.float32)
FOG_TEX = ['atApTex', 'volTex', 'volTile', 'volAirTerr', 'volAirRainMap']  # precipRun's tex[2..6]
FMT_DT = {'rgba32f': np.float32, 'rgba16f': np.float32, 'r32f': np.float32, 'rg32f': np.float32, 'r8': np.uint8, 'rgba8': np.uint8}


class Tex:
    """frag_jobs' texture record, as farview_prep.py gives it (the data the C++ harness uploads)"""
    def __init__(self, kind, fmt, data, filt='linear', wrap='clamp', mips=0):
        a = np.asarray(data)
        if a.ndim == 2 + (kind != '2d'):
            a = a[..., None]
        if fmt == 'rgba16f':
            a = a.astype(np.float16).astype(np.float32)
        self.kind, self.fmt, self.filt, self.wrap, self.mips = kind, fmt, filt, wrap, mips
        self.data = np.ascontiguousarray(a.astype(FMT_DT[fmt]))
        if kind == '2d':
            self.h, self.w = a.shape[:2]; self.d = 1
        else:
            self.d, self.h, self.w = a.shape[:3]


_AIR = []


def air():
    """tests/vklight/frag_jobs.py's volsky job (the air the HLSL ports were gated on), made once"""
    if not _AIR:
        sys.path.insert(0, os.path.join(TREE, 'tests', 'vklight'))
        import frag_jobs as FJ  # noqa: E402
        FJ.Tex = Tex
        _AIR.append(FJ.volsky('precip', 'air/volsky_view_ps.hlsl', []))
    return _AIR[0]


def dump(c):
    args = [EXE] + [str(v) for v in (c[1], c[2], c[3], c[4], c[5], *c[6], *c[7], c[8], c[9], c[10], c[11], c[12], c[13])]
    out = subprocess.run(args, capture_output=True, text=True, check=True, stdin=subprocess.DEVNULL).stdout.split('\n')
    v = out[0].split()[1:]
    g = {'seedS': int(v[0]), 'L': float(v[1]), 'inner': float(v[2]), 'thin': float(v[3]),
         'offXY': (float(v[4]), float(v[5])), 'offZ': [float(x) for x in v[6:22]], 'nCand': int(v[22]), 'best': int(v[23]),
         'cand': [int(x) for x in v[24:40]], 'fallU': [float(x) for x in v[40:56]], 'halfU': [float(x) for x in v[56:72]],
         'wind': (float(v[72]), float(v[73])), 'sway': float(v[74]), 'spin': float(v[75]), 'count': int(v[76]),
         'cols': int(v[77]), 'cells': int(v[78])}
    P = np.array([[float(x) for x in l.split()[1:]] for l in out[1:] if l.startswith('p ')], dtype=np.float64)
    return g, P


def view_rot(yaw, pitch):
    d = np.array([math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch)])
    r = np.cross(d, [0, 0, 1.0]); r /= np.linalg.norm(r)
    u = np.cross(r, d)
    return np.array([r, u, -d])


def proj():
    f = 1.0 / math.tan(math.radians(FOV) / 2)
    n, fa = 1.0, 200000.0
    return np.array([[f * H / W, 0, 0, 0], [0, f, 0, 0], [0, 0, (n + fa) / (n - fa), 2 * n * fa / (n - fa)], [0, 0, -1, 0]])


def atlas(cols, rows, seed):
    """RGBA8, CELL_PX a cell: a soft disc a cell, its own tint, a little noise (so every mip carries detail)"""
    rng = np.random.default_rng(seed)
    w, h = cols * CELL_PX, rows * CELL_PX
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    cx = (x % CELL_PX + 0.5) / CELL_PX * 2 - 1
    cy = (y % CELL_PX + 0.5) / CELL_PX * 2 - 1
    ci = (y // CELL_PX) * cols + (x // CELL_PX)
    tint = rng.uniform(0.55, 1.0, (cols * rows, 3))
    a = np.clip(1.1 - np.hypot(cx, cy), 0, 1) ** 0.7 * rng.uniform(0.75, 1.0, (h, w))
    img = np.zeros((h, w, 4))
    img[..., :3] = tint[ci.astype(int)] * rng.uniform(0.85, 1.0, (h, w, 1))
    img[..., 3] = a
    return np.ascontiguousarray(np.round(img * 255).astype(np.uint8)), w, h


def normals(w, h, seed):
    """RGBA8 tangent-space normals (DirectX green), a smooth bump a cell plus noise, every texel z > 0"""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    cx = (x % CELL_PX + 0.5) / CELL_PX * 2 - 1
    cy = (y % CELL_PX + 0.5) / CELL_PX * 2 - 1
    n = np.stack([0.6 * cx + rng.uniform(-0.25, 0.25, (h, w)), -0.6 * cy + rng.uniform(-0.25, 0.25, (h, w)), np.ones((h, w))], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    img = np.zeros((h, w, 4))
    img[..., 0] = n[..., 0] * 0.5 + 0.5
    img[..., 1] = 0.5 - n[..., 1] * 0.5
    img[..., 2] = n[..., 2] * 0.5 + 0.5
    img[..., 3] = 1.0
    return np.ascontiguousarray(np.round(img * 255).astype(np.uint8))


def bc3_levels(img, path):
    """an RGBA8 picture -> every level as BC3, concatenated (the harness's uploadBc reads them in order); the count"""
    lv = levels(img.astype(np.float64) / 255.0)
    with open(path, 'wb') as f:
        for p in lv:
            f.write(bc3(p))
    return len(lv)


def fl(v):
    return ' '.join(repr(float(x)) for x in np.asarray(v, np.float64).reshape(-1))


os.makedirs(OUT, exist_ok=True)
fl_seen = 0
for st, ext in (('vert', 'vert'), ('geom', 'geom'), ('frag', 'frag')):
    # cell_precip_look.prog: the gpu vert, the look geom / frag (each #define PC_LOOK 1 + the gpu file)
    for prog, base in (('prog', 'cell_precip_gpu.'), ('prog_look', 'cell_precip_gpu.' if st == 'vert' else 'cell_precip_look.')):
        raw = S.expand(os.path.join(WEATHER, base + ext))
        # the loader injects WW_CELL_FOGLEAK into every source naming it (glcontext.cpp, on by default); the _nofl copies
        # without it are the fogleak check's reference only
        names_it = 'WW_CELL_FOGLEAK' in raw
        fl_seen += names_it
        for suf, defs in (('', ['WW_CELL_FOGLEAK'] if names_it else []), ('_nofl', [])):
            with open(os.path.join(OUT, '%s%s.%s.glsl' % (prog, suf, st)), 'w', newline='\n', encoding='latin-1') as f:
                f.write(S.inject(raw, defs))
print('precipview: WW_CELL_FOGLEAK named in %d of the 6 stage sources (injected there)' % fl_seen)
P = proj()
focal = P[1, 1] * 0.5 * H
names = []
for ci, c in enumerate(CASES):
    g, ref = dump(c)
    R = view_rot(c[15], c[16])
    cols, cells = g['cols'], g['cells']
    rows = max(1, cells // max(1, cols))
    n_all = g['count']
    first = min(n_all // 8, 3000)
    n = min(n_all - first, c[17])
    # the slab: the run's particles in the view, their window depths; the widest gap near the median
    rel = ref[first:first + n, 2:5]
    v = rel @ R.T
    cl = np.c_[v, np.ones(len(v))] @ P.T
    inview = (-v[:, 2] > 1.0) & (np.abs(cl[:, 0]) < cl[:, 3]) & (np.abs(cl[:, 1]) < cl[:, 3])
    z = np.unique(np.sort((cl[inview, 2] / cl[inview, 3]) * 0.5 + 0.5))
    if len(z) < 20:
        raise SystemExit('case %s: %d particles in the view' % (c[0], len(z)))
    lo, hi = int(len(z) * 0.4), int(len(z) * 0.6)
    k = lo + int(np.argmax(np.diff(z[lo:hi + 1])))
    zd = 0.5 * (z[k] + z[k + 1])
    has = c[14]
    d = os.path.join(OUT, c[0])
    os.makedirs(d, exist_ok=True)
    tw = th = 1
    fmt = 'bc3srgb' if c[0] == 'rain_near_fog' else 'bc3'  # the app's atlases are BC (loadDds); one case sRGB
    if has:
        img, tw, th = atlas(cols, rows, 7 + ci)
        nl = bc3_levels(img, os.path.join(d, 'tex_pcTex.bin'))
        with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
            f.write('pcTex 2d %s %d %d 1 linear clamp %d tex_pcTex.bin\n' % (fmt, tw, th, nl))
    mips = int(math.floor(math.log2(max(tw, th)))) + 1
    maxLod = min(mips - 1, max(0.0, math.log2(CELL_PX / 16.0))) if has else 0.0
    oneLod = min(mips - 1, max(0.0, math.log2(CELL_PX))) if has else 0.0
    seed = g['seedS']
    U = {
        'pcSeedS': [seed - (1 << 32) if seed >= (1 << 31) else seed],  # the bits; setUnis hands them as a uint
        'pcL': [g['L']], 'pcInner': [g['inner']], 'pcThin': [g['thin']], 'pcOffXY': list(g['offXY']),
        'pcOffZ': g['offZ'], 'pcNCand': [g['nCand']], 'pcBest': [g['best']], 'pcCand': g['cand'],
        'pcFallU': g['fallU'], 'pcHalfU': g['halfU'], 'pcWind': list(g['wind']), 'pcSway': [g['sway']],
        'pcSpin': [g['spin']], 'pcT': [c[5]], 'pcCols': [cols], 'pcCells': [cells], 'pcCellUv': [1.0 / cols, 1.0 / rows],
        'pcRoofOn': [0], 'pcRow': np.c_[R, np.zeros(3)].reshape(-1), 'pcProj': P.T.reshape(-1),  # column-major
        'pcFocal': [focal], 'pcMinPx': [MINPX], 'pcShutter': [SHUTTER], 'pcNear': [8.0, 40.0],
        'pcViewport': [0, 0, W, H], 'pcAtlas': [1.0 / cols, 1.0 / rows, maxLod, oneLod],
        'pcInset': [0.5 * 2 ** maxLod / tw if has else 0.0, 0.5 * 2 ** maxLod / th if has else 0.0],
        'pcHasTex': [has], 'pcFar': [c[8]], 'pcTrans': [0.5],
    }
    look = c[19]
    hasN = 1 if look == 2 and has else 0
    if look:
        # the drop: the fixture's halves are ~3 view units, so 14.28 mm a unit gives ~90 mm drops (kf flat, ~0.003
        # flicker cycles); 0.3 puts D at ~1-4 mm, where kf ramps and the flicker runs ~1 cycle over the streak
        U.update({'pcLook': [look], 'pcMmPerView': [0.3 if look == 1 else 14.28 / c[9]], 'pcHasN': [hasN],
                  'pcClump': [0.2, 0.6], 'pcAlbedo': [0.9]})
    fog = c[18]
    if fog:
        # the air's uniforms and textures; its view-bound rows follow this case's view (frag_jobs' R is view -> world)
        A = air()
        sun = SUN
        AU ={k: v for k, v in A['uniforms'].items()
              if k not in ('viewMatrix', 'projectionMatrix', 'envMapRotation', 'viewportDimensions')}
        AU['volRect'] = [0, 0, 1.0 / W, 1.0 / H]
        AU['atApProj'] = [P[0, 0], P[1, 1], P[0, 2], P[1, 2]]
        AU['fogView'] = list(R @ np.array([0.0, 0.0, 1.0])) + [c[6][2]]
        AU['fogSun'] = list(R @ sun) + [1.0]
        U.update(AU)
        with open(os.path.join(d, 'tex.txt'), 'a' if has else 'w', newline='\n') as f:
            for k in FOG_TEX:
                t = A['textures'][k]
                fn = 'tex_%s.bin' % k
                t.data.tofile(os.path.join(d, fn))
                f.write('%s %s %s %d %d %d %s %s %d %s\n' % (k, t.kind, t.fmt, t.w, t.h, t.d, t.filt, t.wrap, t.mips, fn))
    lit = c[20] if len(c) > 20 else 0
    if lit:
        # the sun alone (volTerms 1): a non-grey sun off the view axis, the view's world rows (cellRow: view -> world, the
        # eye at w), the phase (Rayleigh + two HG lobes, unequal per channel; each channel's weights sum to 1) and its
        # isotropic level (volGi.rgb = the weights' sums), the thin / thick mix 0.5
        sunL = LIT_SUN.get(c[0], SUN)
        nto = float(R[2] @ sunL)  # N = cellWorldDir( 0, 0, 1 ) = R's third row (r, u, -d)
        print('precipview case %s: lit N.to %.3f, thick / thin %.3f (want far from 1)' % (c[0], nto, 4 * max(nto + 0.5, 0) / 1.5))
        U.update({'cellOn': [1], 'cellHasDir': [1], 'cellInterior': [0], 'cellDirColor': [1.6, 1.2, 0.75],
                  'cellDirTo': list(sunL),'cellRow': np.c_[R.T, np.array(c[6])].reshape(-1), 'volTerms': [lit],
                  'volPhase': [0.2, 0.25, 0.3, 0.0, 0.5, 0.45, 0.4, 0.6, 0.3, 0.3, 0.3, -0.3], 'volGi': [1.0, 1.0, 1.0, 0.0],
                  'pcTrans': [0.5]})
        # the lamps' switches in every lit case (GL keeps a program's uniforms: the order cannot carry them)
        U.update({'cellLightCount': [5 if lit & 2 else 0], 'cellShadowOn': [0], 'cellGoboOn': [0], 'cellClusterOn': [0],
                  'cellLightPhys': [0.0] * 4, 'cellLightPhysB': [0.0] * 4, 'cellRed': [0]})
        if lit & 2:
            lamps(v[inview], R, np.array(c[6])).tofile(os.path.join(d, 'buf_cellLights.bin'))
            with open(os.path.join(d, 'buf.txt'), 'w', newline='\n') as f:
                f.write('cellLights rgba32f 40 buf_cellLights.bin\n')
        if fog:  # the veil on, the air's column on the sun: the light chain's own names for the air's medium
            U.update({'volVeil': [1], 'airMed': AU['volAirMed'], 'airLayer': AU['volAirLayer'],
                      'airGround': AU['volAirGround'], 'airSunLayer': [0]})
    if hasN:
        nl = bc3_levels(normals(tw, th, 31 + ci), os.path.join(d, 'tex_pcTexN.bin'))
        with open(os.path.join(d, 'tex.txt'), 'a', newline='\n') as f:
            f.write('pcTexN 2d bc3 %d %d 1 linear clamp %d tex_pcTexN.bin\n' % (tw, th, nl))
    with open(os.path.join(d, 'uni.txt'), 'w', newline='\n') as f:
        for key, val in U.items():
            f.write('%s %s\n' % (key, fl(val)))
    with open(os.path.join(d, 'job.txt'), 'w', newline='\n') as f:
        f.write('%d %d %d %d %d %u %d\n%.9g\n%d %d %d\n' % (W, H, first, n, has, seed, c[8], zd, fog, look, 1 if lit else 0))
    names.append(c[0])
    print('precipview case %s: %d particles (run %d + %d, %d in the view), atlas %dx%d (%d x %d cells), slab %.7f'
          % (c[0], n_all, first, n, int(inview.sum()), tw, th, cols, rows, zd))
with open(os.path.join(OUT, 'cases.txt'), 'w', newline='\n') as f:
    f.write('\n'.join(names) + '\n')
