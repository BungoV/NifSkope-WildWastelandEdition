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
    # L8 term 2b (element 21 = the cluster lists on): the lamp run with b0's projection, volRect and a list buffer
    ('rain_near_cluster', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 3, 1),
    # L8 term 3 (element 22 = the local sun law): rain_near_lit's run, the sun's colour from vpSun
    ('rain_near_sunlocal', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 1, 0, 1),
    # L8 terrain air (element 23 = airTerrOn): snow_far_lit's fogged run, the ground layer on the air's own grid
    ('snow_far_terrair', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 1, 0, 1, 0, 0, 1),
    # L8 term 1 (VKFAR5, element 20 = volTerms 5): rain_near_lit's run with the GI bit on (idle unless WW_CELL_PRT)
    ('rain_near_gi', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 5),
    # L8 term 2 (VKFAR5, element 24 = the ambient: 1 the Ambient Only volumes, 2 the air's keep): volTerms 9
    ('rain_near_amb', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 9, 0, 0, 0, 1),
    ('snow_far_amb', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 1, 0, 9, 0, 0, 0, 2),
    # L8 term 3 (VKFAR6, element 25 = the sun's cascades + the clouds): rain_near_lit's run unfogged, volTerms 1
    ('rain_near_csm', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 1, 0, 0, 0, 0, 1),
    # L8 term 4 (VKFAR6, element 26 = the air's drift): snow_far_lamps' run, fogged, volTerms 3
    ('snow_far_drift', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 1, 0, 3, 0, 0, 0, 0, 0, 1),
    # L8 term 5 (VKFAR6, element 27 = the lamps' shadow cubes): rain_near_lamps' run, volTerms 3
    ('rain_near_shadow', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 3, 0, 0, 0, 0, 0, 0, 1),
    # L8 term 6 (VKFAR6, element 28 = the lamps' gobos): rain_near_lamps' run, volTerms 3
    ('rain_near_gobo', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1),
    # L8 term 7 (VKFAR6, element 29 = the roof map), appended (the earlier cases' atlas seeds kept; bindJob resets an
    # unlit case's light chain): rain_near_lit's run lit, rain_near_disc's plain, snow_look_near's look
    ('rain_near_roof_lit', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1),
    ('rain_near_roof', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1),
    ('snow_look_roof', 3, 5.5, 1.0, 97, 37.25, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 0, 3.0, 0.5, 6.0, 1.0, 1.0, 1,
     0.7, -0.15, 40000, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1),
    # L8 term 8 (VKFAR6, element 30 = an interior: 1 dark, 2 sunlit), appended, volTerms 11 with the Ambient Only volumes:
    # rain_near_amb's run with the lamps, snow_far_lamps' fogged run
    ('rain_near_int', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 11, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1),
    ('snow_far_int_sun', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000, 1, 0, 11, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2),
    # L8 route lift 1 (VKFAR6, element 31 = the frame block), appended: rain_near_csm's run and fixture, drawn through pRoute
    # (the route default program, WW_FRAME_UBO): the gate uploads its cascades into wwShadowFrame at binding 2
    ('rain_near_csm_ubo', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 1,
     2.0, -0.08, 40000, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1),
]
SUN = np.array([0.4, 0.3, 0.35]) / np.linalg.norm([0.4, 0.3, 0.35])  # world, toward the sun (the air's fogSun too)
# a lit case's own sun (cellDirTo), chosen by the GS's mix: N = the eye's axis, thick / thin = 4 vpWrap( N.to, pcTrans )
# (volGi 1). With SUN, rain_near_lit's view gives N.to -0.13, ratio 0.99: thin = thick, pcTrans unseen (51 samples, the
# first lit run 11-15); this sun gives N.to 0.72, ratio 3.3, 44 degrees off the axis. SUN stays (the fogged cases' fogSun).
LIT_SUN = {'rain_near_lit': np.array([0.3, -0.6, 0.75]) / np.linalg.norm([0.3, -0.6, 0.75])}
LIT_SUN['rain_near_lamps'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_shadow'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_gobo'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_roof_lit'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_int'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_cluster'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_sunlocal'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_gi'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_amb'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_csm'] = LIT_SUN['rain_near_lit']
LIT_SUN['rain_near_csm_ubo'] = LIT_SUN['rain_near_lit']
TILE, SLICES = 64, 4  # the cluster grid (L8 term 2b): 64-pixel tiles, 4 log-depth slices


def cluster_lists(nc):
    """R32I, cellclustergl.cpp's layout: (offset, count) a cluster, then the indices from 2 nc. A third walk every light
    (count -1), a third list all five (offset 0), a third list 0, 2, 4 (offset 5: the spot and the box left out)"""
    head = []
    for c in range(nc):
        head += [[0, -1], [0, 5], [5, 3]][c % 3]
    return np.array(head + [0, 1, 2, 3, 4] + [0, 2, 4], dtype=np.int32)


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
        base_defs = ['WW_CELL_FOGLEAK'] if names_it else []
        # L8 term 1 (VKFAR5): the loader's GI variants of the plain program (glcontext.cpp: WW_CELL_PRT under its red,
        # WW_CELL_RTDDGI with the ray-traced GI on), each injected only where the source names it, as the loader does
        variants = [('', base_defs), ('_nofl', [])]
        if prog == 'prog':
            variants += [('_prt', base_defs + (['WW_CELL_PRT'] if '_CELL_PRT' in raw else [])),
                         ('_rtddgi', base_defs + (['WW_CELL_RTDDGI'] if '_CELL_RTDDGI' in raw else []))]
            # the route census (VKFAR6): the loader's programs, each define only where the source names it
            route = base_defs + [d for d, tok in (('WW_FRAME_UBO', 'WW_FRAME_UBO'), ('WW_CELL_AIRZONE', '_CELL_AIRZONE')) if tok in raw]
            allon = route + [d for d, tok in (('WW_CELL_RTDDGI', '_CELL_RTDDGI'), ('WW_CELL_RTDDGI_CASCADES', '_CELL_RTDDGI'),
                                              ('WW_CELL_RESTIR', '_CELL_RESTIR')) if tok in raw]
            variants += [('_route', route), ('_allon', allon),
                         ('_rgi', allon + (['WW_CELL_RESTIRGI'] if '_CELL_RESTIRGI' in raw else []))]
        for suf, defs in variants:
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
        # L8 term 3: the local sun law (vpSun.w 1) in its case, off in every other lit case (GL keeps the uniform)
        # (fixture move 11-15, the bar kept: (0.9, 1.3, 1.7) had cellDirColor's luminance, 1.244 vs 1.252, and the air colours
        # the sky terms by luminance (airEnvColour): 8 samples moved. This sun's is 0.414.)
        U['vpSun'] = [0.5, 0.4, 0.3, 1.0] if len(c) > 22 and c[22] else [0.0] * 4
        # the lamps' switches in every lit case (GL keeps a program's uniforms: the order cannot carry them)
        U.update({'cellLightCount': [5 if lit & 2 else 0], 'cellShadowOn': [0], 'cellGoboOn': [0], 'cellClusterOn': [0],
                  'cellLightPhys': [0.0] * 4, 'cellLightPhysB': [0.0] * 4, 'cellRed': [0], 'airTerrOn': [0]})
        if lit & 2:
            lamps(v[inview], R, np.array(c[6])).tofile(os.path.join(d, 'buf_cellLights.bin'))
            with open(os.path.join(d, 'buf.txt'), 'w', newline='\n') as f:
                f.write('cellLights rgba32f 40 buf_cellLights.bin\n')
            if len(c) > 27 and c[27]:
                # L8 term 5: the lamps' shadow cubes. Slots: the omni 0, the spot 1, the hemisphere 2 (t4.y 2), the box
                # casting past the slots (t4.x -1), the far omni not casting; 16 cubes of 64 x 64 D16, the used slots
                # holding stripes of near occluders (0.05) across the directions, else 1 (names kept off the module's)
                sh_L = lamps(v[inview], R, np.array(c[6]))
                sh_L[:, 4] = [[0.0, 1.0, 0.0, 0.0], [1.0, 1.0, 0.0, 0.0], [2.0, 2.0, 0.0, 0.0], [-1.0, 1.0, 0.0, 0.0], [-1.0, 0.0, 0.0, 0.0]]
                sh_L.astype(np.float32).tofile(os.path.join(d, 'buf_cellLights.bin'))
                sh_n = 64
                sh_s = (np.arange(sh_n) + 0.5) * 2.0 / sh_n - 1.0
                sh_sc, sh_tc = np.meshgrid(sh_s, sh_s)  # [row j = t, column i = s]
                sh_one = np.ones_like(sh_sc)
                sh_faces = [(sh_one, -sh_tc, -sh_sc), (-sh_one, -sh_tc, sh_sc), (sh_sc, sh_one, sh_tc), (sh_sc, -sh_one, -sh_tc),
                            (sh_sc, -sh_tc, sh_one), (-sh_sc, -sh_tc, -sh_one)]  # GL's cube face table, +X -X +Y -Y +Z -Z
                sh_F = [np.random.RandomState(270 + k).randn(3) for k in range(3)]
                sh_F = [9.0 * x / np.linalg.norm(x) for x in sh_F]
                sh_cube = np.full((96, sh_n, sh_n), 65535, np.uint16)
                for k in range(3):
                    for fc in range(6):
                        sh_dir = np.stack(sh_faces[fc], -1)
                        sh_dir = sh_dir / np.linalg.norm(sh_dir, axis=-1, keepdims=True)
                        sh_cube[6 * k + fc][np.sin(sh_dir @ sh_F[k]) > 0.0] = int(round(0.05 * 65535))
                sh_cube.tofile(os.path.join(d, 'cubes.bin'))
                with open(os.path.join(d, 'cubes.txt'), 'w', newline='\n') as f:
                    f.write('%d\n' % sh_n)
                U.update({'cellShadowOn': [1], 'cellShadowTexel': [1.0 / sh_n], 'cellShadowFadeOn': [1],
                          'cellShadowFade': [0.7, 0.4, 1.0] + [1.0] * 13})
                sh_P = (R.T @ v[inview].astype(np.float64).T).T + np.array(c[6])  # the in-view particles in the world
                sh_line = []
                for k in range(3):
                    sh_r = sh_P - sh_L[k, 0, :3]
                    sh_d = np.linalg.norm(sh_r, axis=1)
                    sh_in = sh_d < sh_L[k, 0, 3]
                    sh_occ = (np.sin((sh_r / sh_d[:, None]) @ sh_F[k]) > 0.0) & ((sh_d - 1.0) / sh_L[k, 0, 3] > 0.05)
                    sh_line.append('slot %d %.3f of %d in reach' % (k, float(np.mean(sh_occ[sh_in])) if sh_in.any() else 0.0, int(sh_in.sum())))
                print('precipview case %s: the shadow cubes, %d x %d, stripes shadow %s' % (c[0], sh_n, sh_n, ', '.join(sh_line)))
            if len(c) > 28 and c[28]:
                # L8 term 6: the lamps' gobos, all in cellLights past the 40 light texels: the table at 40 (2 texels: 1 +
                # each light's record, the omni's at 42, the hemisphere's at 45), pattern A (8 x 8, the spot's) at 48,
                # pattern B (8 x 16, two halves, the omni's and the hemisphere's) at 114 (names kept off the module's)
                gb_L = lamps(v[inview], R, np.array(c[6])).reshape(40, 4).astype(np.float64)
                gb_X = np.zeros((204, 4))
                gb_X[0] = [43.0, 0.0, 46.0, 0.0]
                for gb_at, gb_seed in ((2, 280), (5, 281)):
                    gb_q = np.linalg.qr(np.random.RandomState(gb_seed).randn(3, 3))[0]
                    gb_X[gb_at] = list(gb_q[0]) + [115.0]
                    gb_X[gb_at + 1] = list(gb_q[1]) + [0.0]
                    gb_X[gb_at + 2] = list(gb_q[2]) + [0.0]
                gb_rs = np.random.RandomState(290)
                for gb_h, gb_w, gb_hh in ((8, 8, 8), (74, 8, 16)):
                    gb_p = gb_rs.uniform(0.2, 1.5, (gb_w * gb_hh, 3))
                    gb_X[gb_h] = [gb_w, gb_hh, 0.0, 0.0]
                    gb_X[gb_h + 1, :3] = gb_p.mean(axis=0)
                    gb_X[gb_h + 2:gb_h + 2 + gb_w * gb_hh, :3] = gb_p
                gb_ax = gb_L[1 * 8 + 2, :3]
                gb_y = np.cross(gb_ax, [0.0, 0.0, 1.0])
                gb_y /= np.linalg.norm(gb_y)
                gb_L[1 * 8 + 5] = list(gb_y) + [49.0]  # the spot's t5: local y, 1 + pattern A's header
                gb_L[1 * 8 + 6] = list(np.cross(gb_ax, gb_y)) + [0.75]  # t6: local z, tan( FOV / 2 ) (outer cosine 0.8)
                np.concatenate([gb_L, gb_X]).astype(np.float32).tofile(os.path.join(d, 'buf_cellLights.bin'))
                with open(os.path.join(d, 'buf.txt'), 'w', newline='\n') as f:
                    f.write('cellLights rgba32f 244 buf_cellLights.bin\n')
                U.update({'cellGoboOn': [1], 'cellGoboRot': [0], 'cellGoboFlat': [0], 'cellGoboGam': [0], 'cellGoboTbl': [40],
                          'cellGoboMap': [0]})
                gb_P = (R.T @ v[inview].astype(np.float64).T).T + np.array(c[6])
                gb_line = []
                for gb_k, gb_nm in ((0, 'omni'), (1, 'spot'), (2, 'hemisphere')):
                    gb_n = int((np.linalg.norm(gb_P - gb_L[gb_k * 8, :3], axis=1) < gb_L[gb_k * 8, 3]).sum())
                    gb_line.append('%s %d' % (gb_nm, gb_n))
                print('precipview case %s: gobos, in reach of %s of %d in view' % (c[0], ', '.join(gb_line), len(gb_P)))
        if len(c) > 21 and c[21]:
            # L8 term 2b: the cluster lists. Depth = -(R[2] . (P - eye)) over the run's particles in the view, log slices
            tx, ty = -(-W // TILE), -(-H // TILE)
            nc = tx * ty * SLICES
            dep = -v[inview, 2]
            ln0, ln1 = math.log(float(dep.min())), math.log(float(dep.max()))
            eye = np.array(c[6], np.float64)
            U.update({'cellClusterOn': [1], 'cellClusterDims': [tx, ty, SLICES, TILE], 'cellClusterOrigin': [0, 0],
                      'cellClusterDepth': list(-R[2]) + [float(R[2] @ eye)], 'cellClusterLog': [ln0, SLICES / (ln1 - ln0)],
                      'cellClusterBase': [2 * nc], 'volRect': [0, 0, 1.0 / W, 1.0 / H]})
            lists = cluster_lists(nc)
            lists.tofile(os.path.join(d, 'buf_cellCluster.bin'))
            with open(os.path.join(d, 'buf.txt'), 'a', newline='\n') as f:
                f.write('cellCluster r32i %d buf_cellCluster.bin\n' % len(lists))
            # globalUniforms (std140, 336 bytes): zero but projectionMatrix at 96 (column-major, as pcProj)
            gb = np.zeros(84, np.float32)
            gb[24:40] = P.T.reshape(-1)
            gb.tofile(os.path.join(d, 'global.bin'))
            print('precipview case %s: cluster grid %d x %d x %d, depth %.1f - %.1f, %d list texels'
                  % (c[0], tx, ty, SLICES, math.exp(ln0), math.exp(ln1), len(lists)))
        if fog:  # the veil on, the air's column on the sun: the light chain's own names for the air's medium
            U.update({'volVeil': [1], 'airMed': AU['volAirMed'], 'airLayer': AU['volAirLayer'],
                      'airGround': AU['volAirGround'], 'airSunLayer': [0]})
            if len(c) > 23 and c[23]:
                # L8 terrain air: airTerr = the air's volAirTerr (2 layers: coarse 400000 u, fine 40000 u round the eye),
                # its world rows u = x X.x + X.y, v = y X.z + X.w; airTerrRef puts the anchor (airGround.x + terrH - ref)
                # at the particles' median height where the ground is its median (nearest texels: the choice only)
                eye = np.array(c[6], np.float64)
                tz = A['textures']['volAirTerr'].data[..., 0]
                Pw = v[inview] @ R + eye

                # (names kept off the module's: rows / at / n / th / ref are the atlas', the dump's and the run's, used below)
                def grid_rows(span):
                    return [1.0 / span, -(eye[0] - span / 2) / span, 1.0 / span, -(eye[1] - span / 2) / span]

                def grid_at(layer, xf):
                    gu, gv = Pw[:, 0] * xf[0] + xf[1], Pw[:, 1] * xf[2] + xf[3]
                    gn = tz.shape[1]
                    return tz[layer, np.clip((gv * gn).astype(int), 0, gn - 1), np.clip((gu * gn).astype(int), 0, gn - 1)], gu, gv

                xc, xfn = grid_rows(400000.0), grid_rows(40000.0)
                hc, _, _ = grid_at(0, xc)
                hf, uf, vf = grid_at(1, xfn)
                wf = np.clip(np.minimum(np.minimum(uf, 1 - uf), np.minimum(vf, 1 - vf)) * 16.0, 0.0, 1.0)
                gh = hc + (hf - hc) * wf  # (not th / ref: the atlas' height and the dump's records, used below)
                gref = float(np.median(gh) - np.median(Pw[:, 2]) + AU['volAirGround'][0])
                zg = AU['volAirGround'][0] + gh - gref
                print('precipview case %s: terrain air ref %.1f, ground %.1f - %.1f, particles z %.1f - %.1f, share under the '
                      'anchor %.3f (want 0.2 - 0.8)' % (c[0], gref, gh.min(), gh.max(), Pw[:, 2].min(), Pw[:, 2].max(),
                                                        float(np.mean(Pw[:, 2] < zg))))
                U.update({'airTerrOn': [1], 'airTerrXf': xc, 'airTerrFineXf': xfn, 'airTerrRef': [gref]})
        amb = c[24] if len(c) > 24 else 0
        if amb:
            # L8 term 2: the weather's ambient (cellExtDalc, the exterior law), three non-grey rows, unequal normal parts
            U.update({'cellExtDalc': [1], 'cellDalc': [0.25, 0.1, 0.35, 0.55, 0.1, 0.3, -0.15, 0.5, -0.2, 0.15, 0.4, 0.45]})
            eye = np.array(c[6], np.float64)
            Pw = v[inview] @ R + eye
        if amb == 1:
            # an Ambient Only sphere round the particles' median, a box-linked one round a far particle (the sphere first)
            c0 = np.median(Pw, axis=0)
            r0 = float(np.percentile(np.linalg.norm(Pw - c0, axis=1), 35))
            c1 = Pw[int(np.argmax(Pw[:, 0]))] * 0.5 + c0 * 0.5
            ins0 = np.linalg.norm(Pw - c0, axis=1) < r0
            for pct in (40, 50, 60, 70, 80, 90):  # fixture move (bar kept): the box grows until it holds >= 0.15 of the view
                h1 = np.maximum(np.percentile(np.abs(Pw - c1), pct, axis=0), 1.0)
                ins1 = np.all(np.abs(Pw - c1) < h1, axis=1) & ~ins0
                if ins1.mean() >= 0.15:
                    break
            box = np.zeros((6, 4))
            for a in range(3):
                box[3 + a, a] = 1.0 / h1[a]
                box[3 + a, 3] = -c1[a] / h1[a]
            U.update({'cellAmboCount': [2], 'cellAmbo': list(c0) + [r0] + list(c1) + [0.0],
                      'cellAmboK': [0.4, 0.6, 1.5, 0.0, 1.8, 0.7, 0.3, 1.0], 'cellAmboBox': box.reshape(-1)})
            print('precipview case %s: ambient volumes, sphere share %.3f, box share %.3f (want each 0.1 - 0.6)'
                  % (c[0], float(ins0.mean()), float(ins1.mean())))
        if amb == 2:
            # the air's keep: volEnvGrid (16, 9, slices, 1), a 16 x 9 x 6 slices RGBA16F history (the app's strata), b0's
            # projection (vpAmbKeep reads projectionMatrix)
            nx, ny = 16, 9
            vs = AU['volSlices']
            nz = int(round(vs[2]))
            zz, yy, xx = np.mgrid[0:6 * nz, 0:ny, 0:nx].astype(np.float64)
            k = zz % nz
            hist = np.zeros((6 * nz, ny, nx, 4))
            hist[..., 0] = 0.3 + 0.2 * np.sin(xx * 0.7 + k)
            hist[..., 1] = -0.4 + 1.3 * ((xx * 0.37 + yy * 0.61 + k * 0.23) % 1.0)
            hist[..., 2] = np.where((xx.astype(int) + 2 * yy.astype(int) + k.astype(int)) % 4 == 0, 0.5, 1.0)
            hist[..., 3] = 0.5 + 0.45 * np.sin(xx * 0.45 + yy * 0.8 + k * 0.6)
            t = Tex('3d', 'rgba16f', hist.astype(np.float32), 'nearest')
            t.data.tofile(os.path.join(d, 'tex_volEnvHist.bin'))
            with open(os.path.join(d, 'tex.txt'), 'a', newline='\n') as f:
                f.write('volEnvHist 3d rgba16f %d %d %d nearest clamp 0 tex_volEnvHist.bin\n' % (t.w, t.h, t.d))
            U['volEnvGrid'] = [nx, ny, nz, 1.0]
            gb = np.zeros(84, np.float32)
            gb[24:40] = P.T.reshape(-1)
            gb.tofile(os.path.join(d, 'global.bin'))
            # the keep a particle gets (vpAmbKeep on the prep's positions)
            pv = v[inview]
            dd = np.linalg.norm(pv, axis=1)
            cc = np.c_[pv, np.ones(len(pv))] @ P.T
            ff = np.log(np.maximum(dd, 1e-9) / vs[0]) / vs[1] * nz
            hd = t.data
            keeps = []
            for i in range(len(pv)):
                if dd[i] <= vs[0] or cc[i, 3] <= 0 or abs(cc[i, 0]) > cc[i, 3] or abs(cc[i, 1]) > cc[i, 3] or ff[i] >= nz:
                    keeps.append(1.0)
                    continue
                xi = min(max(int((cc[i, 0] / cc[i, 3] * 0.5 + 0.5) * nx), 0), nx - 1)
                yi = min(max(int((cc[i, 1] / cc[i, 3] * 0.5 + 0.5) * ny), 0), ny - 1)
                ki = min(max(int(ff[i]), 0), nz - 1)
                h = hd[5 * nz + ki, yi, xi]
                if h[2] <= 0.9999:
                    keeps.append(1.0)
                    continue
                fade, rr = float(hd[4 * nz + ki, yi, xi, 3]), float(h[1])
                keeps.append(1.0 - fade + fade * rr if rr >= 0.0 else 1.0 - fade)
            keeps = np.array(keeps)
            print('precipview case %s: the air keep, %d slices, share of in-view particles with keep < 1 %.3f (want 0.2 - 0.8), '
                  'keep %.3f - %.3f' % (c[0], nz, float(np.mean(keeps < 0.9999)), keeps.min(), keeps.max()))
    if lit and len(c) > 25 and c[25]:
        # L8 term 3: three orthographic cascades along the case's sun in view space (each fitted to its view-depth third
        # of the in-view particles), a 256 x 256 x 3 D16 map of stripe occluders, and a 64 x 64 clouds' map along the sun
        pv = v[inview].astype(np.float64)
        Lv = R @ sunL
        Lv = Lv / np.linalg.norm(Lv)
        ax = np.cross(Lv, [0.0, 1.0, 0.0])
        if np.linalg.norm(ax) < 0.1:
            ax = np.cross(Lv, [1.0, 0.0, 0.0])
        ax = ax / np.linalg.norm(ax)
        ay = np.cross(Lv, ax)
        dv = -pv[:, 2]
        s1, s2 = float(np.percentile(dv, 33.0)), float(np.percentile(dv, 67.0))
        band = 0.05 * float(dv.max() - dv.min())
        dist = 4.0 * float(np.linalg.norm(pv, axis=1).max())
        dd = pv @ -Lv
        pad = 0.1 * float(dd.max() - dd.min()) + 1.0
        dmin, dmax = float(dd.min()) - pad, float(dd.max()) + pad
        span = dmax - dmin
        pa, pb = pv @ ax, pv @ ay
        CN, mats, texel = 256, [], []
        for lo, hi in ((-1e30, s1 + band), (s1, s2 + band), (s2, 1e30)):
            m = (dv >= lo) & (dv <= hi)
            ca, cb = 0.5 * (pa[m].min() + pa[m].max()), 0.5 * (pb[m].min() + pb[m].max())
            h = 0.5 * max(float(np.ptp(pa[m])), float(np.ptp(pb[m]))) * 1.1 + 0.5
            M = np.zeros((4, 4))
            M[0, :3], M[0, 3] = ax / (2 * h), 0.5 - ca / (2 * h)
            M[1, :3], M[1, 3] = ay / (2 * h), 0.5 - cb / (2 * h)
            M[2, :3], M[2, 3] = -Lv / span, -dmin / span
            M[3, 3] = 1.0
            mats.append(M)
            texel.append(2 * h / CN)
        yy, xx = np.mgrid[0:CN, 0:CN]
        csm = np.full((3, CN, CN), 65535, np.uint16)
        for i, stripe in enumerate(((xx // 16) % 2, (yy // 16) % 2, ((xx + yy) // 16) % 2)):
            occ = 0.15 + 0.3 * (0.5 + 0.5 * np.sin(xx * 0.05 + yy * 0.031 + i))
            csm[i] = np.where(stripe == 1, np.round(occ * 65535), 65535).astype(np.uint16)
        csm.tofile(os.path.join(d, 'csm.bin'))
        casc = np.where(dv < s1, 0, np.where(dv < s2, 1, 2))
        shad = []
        for k in range(len(pv)):
            q = mats[casc[k]] @ np.r_[pv[k], 1.0]
            zr = q[2] - 0.25 * texel[casc[k]] / span
            tx, ty = int(np.floor(q[0] * CN)), int(np.floor(q[1] * CN))
            st = csm[casc[k], ty, tx] / 65535.0 if 0 <= tx < CN and 0 <= ty < CN else 1.0
            shad.append(zr > st)
        lz = pv @ Lv
        Hc = 0.5 * max(float(np.ptp(pa)), float(np.ptp(pb))) * 1.15 + 0.5
        cA, cB = 0.5 * (pa.min() + pa.max()), 0.5 * (pb.min() + pb.max())
        Mc = np.zeros((4, 4))
        Mc[0, :3], Mc[0, 3] = ax / (2 * Hc), 0.5 - cA / (2 * Hc)
        Mc[1, :3], Mc[1, 3] = ay / (2 * Hc), 0.5 - cB / (2 * Hc)
        Mc[2, :3], Mc[2, 3] = Lv / float(np.ptp(lz)), -float(lz.min()) / float(np.ptp(lz))
        Mc[3, 3] = 1.0
        QN = 64
        gy, gx = np.mgrid[0:QN, 0:QN] / float(QN - 1)
        cloud = np.zeros((QN, QN, 4), np.float32)
        cloud[..., 0] = 0.2 + 0.8 * (0.5 + 0.5 * np.sin(gx * 7.0 + gy * 3.0))
        cloud[..., 1] = 0.5 + 2.5 * (0.5 + 0.5 * np.sin(gy * 9.0 - gx * 2.0))
        cloud[..., 2] = 0.2 + 1.8 * (0.5 + 0.5 * np.cos(gx * 5.0 + gy * 6.0))
        cloud = cloud.astype(np.float16).astype(np.float32)
        cloud.tofile(os.path.join(d, 'cloud.bin'))
        qc = np.c_[pv, np.ones(len(pv))] @ Mc.T
        ci_ = np.clip((qc[:, :2] * QN).astype(int), 0, QN - 1)
        mc = cloud[ci_[:, 1], ci_[:, 0]]
        tc = np.exp(-np.minimum(mc[:, 2], mc[:, 1] * np.maximum(mc[:, 0] - qc[:, 2], 0.0)))
        with open(os.path.join(d, 'shadow.txt'), 'w', newline='\n') as f:
            f.write('%d %d\n' % (CN, QN))
        U.update({'volCsm': [1], 'ussOn': [0], 'csmForce': [-1.0], 'csmRed': [0], 'csmProbe': [0], 'ussCloudOn': [1],
                  'csmMat': np.concatenate([M.T.reshape(-1) for M in mats]), 'csmInvRange': [1.0 / span] * 3,
                  'csmParams': [1.0, dist, float(CN), band], 'csmSplitOffset': [s1, s2, 0.275, 1.0],
                  'ussCsmStable': [1.0, 0.0, 0.0, 1.0], 'ussCsmTexel': texel, 'ussCsmBias': [0.25 * t_ for t_ in texel],
                  'ussCloudFromView': Mc.T.reshape(-1), 'ussCloudFar': [0.6, 0.15, 0.0, 0.0]})
        print('precipview case %s: the cascades, in-view shares %.3f %.3f %.3f (want each >= 0.1), one hard tap shadows %.3f '
              '(want 0.2 - 0.8); the clouds, transmittance %.3f - %.3f, share < 0.9 %.3f' % (c[0], float(np.mean(casc == 0)),
              float(np.mean(casc == 1)), float(np.mean(casc == 2)), float(np.mean(shad)), tc.min(), tc.max(), float(np.mean(tc < 0.9))))
    if lit and fog and len(c) > 26 and c[26]:
        # L8 term 4: the drift (cellvolair.cpp volDrift): amp 0.5, the fade from the in-view particles' median distance over
        # their distance span, two skewed octaves (about 2 and 5 noise tiles across the run), a 32^3 R8 noise (linear,
        # repeat; as vaNoiseLoad's) centred on its mean
        # (names kept off the module's: rows / seed / t are the atlas', the run's and the textures', used below)
        dr_p = v[inview].astype(np.float64)
        dr_dist = np.linalg.norm(dr_p, axis=1) * AU['fogDistScale']
        dr_ext = float(max(np.ptp(dr_p[:, 0]), np.ptp(dr_p[:, 1]), np.ptp(dr_p[:, 2])))
        dr_a = np.random.RandomState(26).rand(32, 32, 32)
        for dr_ax in range(3):
            dr_a = (dr_a + np.roll(dr_a, 1, dr_ax) + np.roll(dr_a, -1, dr_ax)) / 3.0
        dr_noise = ((dr_a - dr_a.min()) / (dr_a.max() - dr_a.min()) * 255).astype(np.uint8)
        dr_t = Tex('3d', 'r8', dr_noise, 'linear', 'repeat')
        dr_t.data.tofile(os.path.join(d, 'tex_volAirNoise.bin'))
        with open(os.path.join(d, 'tex.txt'), 'a', newline='\n') as f:
            f.write('volAirNoise 3d r8 %d %d %d linear repeat 0 tex_volAirNoise.bin\n' % (dr_t.w, dr_t.h, dr_t.d))
        dr_rows = []
        for dr_tiles, dr_seed in ((2.0, 3), (5.0, 4)):
            dr_q, _ = np.linalg.qr(np.random.RandomState(dr_seed).randn(3, 3))
            dr_m = dr_q * (dr_tiles / dr_ext)
            dr_o = np.random.RandomState(dr_seed + 10).rand(3)
            dr_rows += [list(dr_m[i]) + [float(dr_o[i])] for i in range(3)]
        dr_mean = float(dr_noise.mean()) / 255.0
        dr_y0, dr_span = float(np.median(dr_dist)), float(np.ptp(dr_dist))
        U.update({'volAirDrift': [0.5, dr_y0, 1.0 / dr_span, dr_mean], 'volAirDriftUv': np.array(dr_rows).reshape(-1),
                  'volAirDriftRed': [0]})
        # the captured air (2.2e-5 / unit, density 0.06 at the particles) moves the sun column by under a half's ulp: the
        # case's air 150x denser (sun tau about 0.5 at the median particle), so the drift's factor is seen
        U['airMed'] = [float(AU['volAirMed'][0]) * 150.0] + [float(x) for x in AU['volAirMed'][1:]]
        dr_fade = 1.0 - np.clip((dr_dist - dr_y0) / dr_span, 0.0, 1.0)
        print('precipview case %s: the drift, noise mean %.4f, fade %.3f - %.3f (share 1: %.3f), %.2f / %.2f tiles across, air %.3g / unit'
              % (c[0], dr_mean, dr_fade.min(), dr_fade.max(), float(np.mean(dr_fade >= 1.0)), 2.0, 5.0, U['airMed'][0]))
    if hasN:
        nl = bc3_levels(normals(tw, th, 31 + ci), os.path.join(d, 'tex_pcTexN.bin'))
        with open(os.path.join(d, 'tex.txt'), 'a', newline='\n') as f:
            f.write('pcTexN 2d bc3 %d %d 1 linear clamp %d tex_pcTexN.bin\n' % (tw, th, nl))
    if lit and len(c) > 30 and c[30]:
        # L8 term 8: an interior (cellInterior 1): its ambient switch cellHasDalc on, cellGiAmb 0, cellExtDalc kept 1 (the
        # ambient's fixture: unread inside); 1 a dark interior (cellHasDir 0: the lamps and the ambient alone)
        U.update({'cellInterior': [1], 'cellHasDalc': [1], 'cellGiAmb': [0], 'cellExtDalc': [1]})
        if c[30] == 1:
            U['cellHasDir'] = [0]
        print('precipview case %s: an interior, %s' % (c[0], 'dark (cellHasDir 0)' if c[30] == 1 else 'sunlit (cellHasDir 1)'))
    if len(c) > 29 and c[29]:
        # L8 term 7: the roof map (cellprecip.cpp's R32F, the highest occluder z a texel, -1e30 open), 24 x 16, the texel
        # L / 16, the eye at texel (6.3, 11.7): the map covers part of the box, off the map at low x and high y; a quarter
        # open, the rest a roof at the eye's z + uniform( -L / 2, L / 2 )
        rf_nx, rf_ny, rf_inv = 24, 16, 16.0 / g['L']
        rf_rs = np.random.RandomState(290 + ci)
        rf_z = (c[6][2] + rf_rs.uniform(-0.5, 0.5, (rf_ny, rf_nx)) * g['L']).astype(np.float32)
        rf_z[rf_rs.rand(rf_ny, rf_nx) < 0.25] = np.float32(-1e30)
        rf_z.tofile(os.path.join(d, 'roof.bin'))  # row y = 0 first, as glTexImage2D takes it
        with open(os.path.join(d, 'roof.txt'), 'w', newline='\n') as f:
            f.write('%d %d\n' % (rf_nx, rf_ny))
        U.update({'pcRoofOn': [1], 'pcRoofCam': [6.3, 11.7, c[6][2]], 'pcRoofInv': [rf_inv]})
        # the prediction (the CPU particles in float32, pcRoofOpen's rule): how many in-view particles sit under a roof
        rf_p = rel[inview].astype(np.float32)
        rf_t = np.floor(np.array([6.3, 11.7], np.float32) + rf_p[:, :2] * np.float32(rf_inv)).astype(np.int64)
        rf_on = (rf_t >= 0).all(1) & (rf_t[:, 0] < rf_nx) & (rf_t[:, 1] < rf_ny)
        rf_under = np.zeros(len(rf_p), bool)
        rf_under[rf_on] = ~(rf_z[rf_t[rf_on, 1], rf_t[rf_on, 0]] - np.float32(c[6][2]) < rf_p[rf_on, 2])
        print('precipview case %s: the roof map %d x %d, %d of %d in-view particles on the map, %d under a roof (dropped)'
              % (c[0], rf_nx, rf_ny, int(rf_on.sum()), len(rf_p), int(rf_under.sum())))
    if len(c) > 31 and c[31]:
        # L8 route lift 1: the gate draws this case through pRoute and uploads its cascade values into the frame block
        U['wwGateFrameUbo'] = [1]
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
