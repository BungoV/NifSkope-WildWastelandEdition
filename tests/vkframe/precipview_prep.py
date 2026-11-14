"""Lane VKFAR2 (row L8 slice 1): the job of tests/vkframe/precipview.cpp, written as files the C++ harness reads: the GL
program (the tree's cell_precip_gpu.vert / .geom / .frag with the loader's #include expansion, no defines: the app's
cell_precip_gpu.prog), and per case the field's uniforms from precipmath.h gpuRun (through the PERFFIX3 dump exe,
tests/features/weather/precipgpu/precipgpu_dump.cpp: never re-typed), the view, an RGBA8 atlas with its mips, the run
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

OUT = os.path.abspath(sys.argv[1])
EXE = sys.argv[2]
W, H, FOV = 960, 540, 55.0
MINPX, SHUTTER = 0.75, 1.0 / 120.0
CELL_PX = 64
# name, type, size, rate, seed, t, cam, wind, far, drawScale, spin, sway, windShare, weight, hasTex, yaw, pitch, cap
CASES = [
    ('snow_near_tex', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 0, 3.0, 0.5, 6.0, 1.0, 1.0, 1,
     0.7, -0.15, 40000),
    ('rain_near_disc', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0, 0,
     2.0, -0.08, 40000),
    ('snow_far_mean', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6, 1,
     3.3, -0.01, 40000),
]


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


def fl(v):
    return ' '.join(repr(float(x)) for x in np.asarray(v, np.float64).reshape(-1))


os.makedirs(OUT, exist_ok=True)
for st, ext in (('vert', 'vert'), ('geom', 'geom'), ('frag', 'frag')):
    src = S.inject(S.expand(os.path.join(WEATHER, 'cell_precip_gpu.' + ext)), [])
    with open(os.path.join(OUT, 'prog.%s.glsl' % st), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)
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
    if has:
        img, tw, th = atlas(cols, rows, 7 + ci)
        img.tofile(os.path.join(d, 'tex_pcTex.bin'))
        with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
            f.write('pcTex 2d rgba8 %d %d 1 linear repeat 1 tex_pcTex.bin\n' % (tw, th))
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
    with open(os.path.join(d, 'uni.txt'), 'w', newline='\n') as f:
        for key, val in U.items():
            f.write('%s %s\n' % (key, fl(val)))
    with open(os.path.join(d, 'job.txt'), 'w', newline='\n') as f:
        f.write('%d %d %d %d %d %u %d\n%.9g\n' % (W, H, first, n, has, seed, c[8], zd))
    names.append(c[0])
    print('precipview case %s: %d particles (run %d + %d, %d in the view), atlas %dx%d (%d x %d cells), slab %.7f'
          % (c[0], n_all, first, n, int(inview.sum()), tw, th, cols, rows, zd))
with open(os.path.join(OUT, 'cases.txt'), 'w', newline='\n') as f:
    f.write('\n'.join(names) + '\n')
