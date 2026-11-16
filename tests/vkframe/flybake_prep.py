"""Lane VKCLOUD4: the job of tests/vkframe/flybakeview.cpp (flybakeview.sh): the fly-through box's occupancy bake
(cloud_flybake) and distance field (cloud_flysdf) programs as GL draws them (the tree's .frag with the loader's #include
expansion), the density's uniforms and noise from tests/vklight/frag_jobs.py's cloud march job (cloud_inputs.py's
out dir), a coverage map with structure, and the box at the Low preset (64 x 64 x 16 cells of 0.128 km) over the Low
band (1.5 km), as cloudfly.cpp runPass sets it (cfMargin = kMarginS, cfSlackKm = kSlackKm, cfProbe 0). No GL here.
    python -I tests/vkframe/flybake_prep.py <out dir> <cloud_inputs dir>
"""
import math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'vklight'))
OUT = os.path.abspath(sys.argv[1])
os.environ['FRAG_CLOUDIN'] = os.path.abspath(sys.argv[2])
import shader_link_check as S  # noqa: E402
import frag_jobs as FJ  # noqa: E402

FMT_DT = {'rgba32f': np.float32, 'rgba16f': np.float32, 'r32f': np.float32, 'rg32f': np.float32, 'r8': np.uint8, 'rgba8': np.uint8}


class Tex:  # fxview_prep.py's
    def __init__(self, kind, fmt, data, filt='linear', wrap='clamp', mips=0):
        a = np.asarray(data)
        if a.ndim == 2 + (kind != '2d'):
            a = a[..., None]
        self.kind, self.fmt, self.filt, self.wrap, self.mips = kind, fmt, filt, wrap, mips
        self.data = np.ascontiguousarray(a.astype(FMT_DT[fmt]))
        if kind == '2d':
            self.h, self.w = a.shape[:2]; self.d = 1
        else:
            self.d, self.h, self.w = a.shape[:3]


FJ.Tex = Tex


def flat(v):
    return [float(x) for x in np.asarray(v, np.float64).reshape(-1)]


def write_prog(d, name, frag):
    with open(os.path.join(d, name), 'w', newline='\n', encoding='latin-1') as f:
        f.write(S.inject(S.expand(os.path.join(FEAT, frag)), []))


cj = {j['name']: j for j in FJ.cloud_jobs()}
if 'cloud_march_s4' not in cj:
    sys.exit('no cloud inputs')
s4 = cj['cloud_march_s4']
d = os.path.join(OUT, 'flybake')
os.makedirs(d, exist_ok=True)
write_prog(d, 'bake.frag.glsl', 'clouds/cloud_flybake.frag')
write_prog(d, 'sdf.frag.glsl', 'clouds/cloud_flysdf.frag')
nxy, nz, cell = 64, 16, 0.128  # cloudflymath.h preset( 1 )
U = dict(s4['uniforms'])
U.update({'cfOrgKm': [-32 * cell, -32 * cell, math.floor(1.5 / cell) * cell], 'cfCellKm': cell, 'cfMargin': 0.05,
          'cfSlackKm': 0.25, 'cfProbe': 0, 'cfDim': [float(nxy), float(nxy), float(nz)]})
with open(os.path.join(d, 'bake.uni'), 'w', newline='\n') as f:
    for k, v in U.items():
        f.write('%s %s\n' % (k, ' '.join(repr(x) for x in flat(v))))
T = dict(s4['textures'])
r = np.random.RandomState(17)  # fxview_prep.py's structured coverage map
m = r.rand(32, 32, 4).astype(np.float32)
for ax in (0, 1):
    m = (m + np.roll(m, 1, ax) + np.roll(m, -1, ax)) / 3.0
T['cvMap'] = Tex('2d', 'rgba8', np.round(m * 255.0), 'linear', 'clamp')  # lane VKCLOUD11: the app's GL_RGBA8 (cloudmap.cpp)
T['cvShape'] = Tex('3d', 'r8', T['cvShape'].data, 'linear', 'repeat', mips=1)  # cloudvol.cpp: mipmapped
T['cvDetail'] = Tex('3d', 'r8', T['cvDetail'].data, 'linear', 'repeat', mips=1)
order = ['cvShape', 'cvDetail', 'cvMap']  # cloud_flybake_ps's t0.. (reflected)
with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
    for k in order:
        t = T[k]
        fn = 'tex_%s.bin' % k
        t.data.tofile(os.path.join(d, fn))
        f.write('%s %s %s %d %d %d %s %s %d %s\n' % (k, t.kind, t.fmt, t.w, t.h, t.d, t.filt, t.wrap, t.mips, fn))
with open(os.path.join(d, 'job.txt'), 'w', newline='\n') as f:
    f.write('%d %d\n' % (nxy, nz))
print('flybake job: %d x %d x %d cells of %.3f km, %d uniforms, textures %s' % (nxy, nxy, nz, cell, len(U), ' '.join(order)))
