"""Lane VKFAR1 (row G3): the job of tests/vkframe/farview.cpp, written as files the C++ harness reads: the GL program
(the tree's cell_fardots.vert / .frag with the loader's #include expansion and the WW_CELL_FOGLEAK define the shipped
SPIR-V twins), the fog's uniforms and its 5 textures (tests/vklight/frag_jobs.py's volsky job: the same air the HLSL
ports were gated on) and the global block's 336 bytes. The dots themselves are made by the harness (seeded).

    python -I tests/vkframe/farview_prep.py <out dir>
"""
import os, struct, sys
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


j = FJ.volsky('fardots', 'air/cell_volsky_ps__WW_CELL_FOGLEAK.hlsl', ['WW_CELL_FOGLEAK'])
d = os.path.join(OUT, 'fardots')
os.makedirs(d, exist_ok=True)
write_src(os.path.join(d, 'prog.vert.glsl'), S.inject(S.expand(os.path.join(FEAT, 'far-lod/cell_fardots.vert')), ['WW_CELL_FOGLEAK']))
write_src(os.path.join(d, 'prog.frag.glsl'), S.inject(S.expand(os.path.join(FEAT, 'far-lod/cell_fardots.frag')), ['WW_CELL_FOGLEAK']))
U = dict(j['uniforms'])
with open(os.path.join(d, 'uni.txt'), 'w', newline='\n') as f:
    for k, v in U.items():
        f.write('%s %s\n' % (k, ' '.join(repr(x) for x in flat(v))))
T = j['textures']
with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
    for k in ['atApTex', 'volAirTerr', 'volAirRainMap', 'volTex', 'volTile']:
        t = T[k]
        fn = 'tex_%s.bin' % k
        t.data.tofile(os.path.join(d, fn))
        f.write('%s %s %s %d %d %d %s %s %d %s\n' % (k, t.kind, t.fmt, t.w, t.h, t.d, t.filt, t.wrap, t.mips, fn))
g = bytearray(336)  # uniforms.glsl globalUniforms, std140 (cell_fardots.frag includes it; the pass reads none of it)
vm = np.asarray(U['viewMatrix'], np.float32).reshape(3, 3)
for c in range(3):
    struct.pack_into('<3f', g, 16 * c, *vm[c])
    struct.pack_into('<3f', g, 48 + 16 * c, *[1.0 if r == c else 0.0 for r in range(3)])
struct.pack_into('<16f', g, 96, *np.asarray(U['projectionMatrix'], np.float32))
struct.pack_into('<4i', g, 288, *[int(x) for x in U['viewportDimensions']])
open(os.path.join(d, 'global.bin'), 'wb').write(bytes(g))
W, H = j['size']
open(os.path.join(d, 'job.txt'), 'w', newline='\n').write('%d %d\n' % (W, H))
print('fardots job: %dx%d, %d uniforms, 5 textures' % (W, H, len(U)))
