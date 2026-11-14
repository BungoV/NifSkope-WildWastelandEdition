"""Lane VKFAR2 (row P4b): the job of tests/vkframe/fogbloomview.cpp, written as files the C++ harness reads: the GL program
(the tree's cell_fogbloom.frag with the loader's #include expansion and the WW_CELL_FOGLEAK define the shipped SPIR-V
twin), the air's uniforms and its 4 textures (tests/vklight/frag_jobs.py's volsky job: the same air the HLSL ports were
gated on) and the projection terms the pass reads (fbProj, fbProjZ). The frame's depth and stencil are drawn by the
harness (seeded per job name).

    python -I tests/vkframe/fogbloomview_prep.py <out dir>
"""
import os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'vklight'))
OUT = os.path.abspath(sys.argv[1])
import shader_link_check as S  # noqa: E402
import frag_jobs as FJ  # noqa: E402

FMT_DT = {'rgba32f': np.float32, 'rgba16f': np.float32, 'r32f': np.float32, 'rg32f': np.float32, 'r8': np.uint8, 'rgba8': np.uint8}


class Tex:
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


FJ.Tex = Tex


def flat(v):
    return [float(x) for x in np.asarray(v, np.float64).reshape(-1)]


def write_src(path, src):
    with open(path, 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)


j = FJ.volsky('fogbloom', 'post/fogbloom_view_ps.hlsl', ['WW_CELL_FOGLEAK'])
d = os.path.join(OUT, 'fogbloom')
os.makedirs(d, exist_ok=True)
write_src(os.path.join(d, 'prog.frag.glsl'), S.inject(S.expand(os.path.join(FEAT, 'post/cell_fogbloom.frag')), ['WW_CELL_FOGLEAK']))
U = dict(j['uniforms'])
P = np.asarray(U['projectionMatrix'], np.float64).reshape(4, 4)  # GL columns: P[col][row]
U['fbProj'] = [P[0, 0], P[1, 1]]
U['fbProjZ'] = [P[2, 2], P[3, 2]]
U['fbParams'] = [1.0, 0.5, 3.912 / 139986.0, 3.912 / 69993.0]  # the app's (cellfogbloom.cpp); the harness sweeps the window
U['fbTranslucent'] = 0
with open(os.path.join(d, 'uni.txt'), 'w', newline='\n') as f:
    for k, v in U.items():
        f.write('%s %s\n' % (k, ' '.join(repr(x) for x in flat(v))))
T = j['textures']
with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
    for k in ['volTex', 'volTile', 'volAirTerr', 'volAirRainMap']:
        t = T[k]
        fn = 'tex_%s.bin' % k
        t.data.tofile(os.path.join(d, fn))
        f.write('%s %s %s %d %d %d %s %s %d %s\n' % (k, t.kind, t.fmt, t.w, t.h, t.d, t.filt, t.wrap, t.mips, fn))
W, H = j['size']
open(os.path.join(d, 'job.txt'), 'w', newline='\n').write('%d %d\n' % (W, H))
print('fogbloom job: %dx%d, %d uniforms, 4 textures' % (W, H, len(U)))
