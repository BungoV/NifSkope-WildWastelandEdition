"""Lane VKAIR1 (rows L5, L6): the jobs of tests/vkframe/fxview.cpp (volskyview.sh, cloudview.sh), written as files the
C++ harness reads: the GL program (the tree's .frag with the loader's #include expansion and defines), the uniforms
(one line a uniform: `name v0 v1 ...`), the textures (`tex.txt` + raw level-0 data) and, for the air, the global block's
336 bytes. The values are tests/vklight/frag_jobs.py's (the HLSL ports' own gate), no GL here.

    python -I tests/vkframe/fxview_prep.py volsky <out dir>
    python -I tests/vkframe/fxview_prep.py cloud <out dir> <cloud_inputs dir>
"""
import os, struct, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'vklight'))
mode, OUT = sys.argv[1], os.path.abspath(sys.argv[2])
if mode == 'cloud':
    os.environ['FRAG_CLOUDIN'] = os.path.abspath(sys.argv[3])
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


def write_prog(d, name, frag, defines):
    src = S.inject(S.expand(os.path.join(FEAT, frag)), defines)
    with open(os.path.join(d, name), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)


def write_uni(path, U):
    with open(path, 'w', newline='\n') as f:
        for k, v in U.items():
            f.write('%s %s\n' % (k, ' '.join(repr(x) for x in flat(v))))


def write_tex(d, T, order):
    with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
        for k in order:
            t = T[k]
            fn = 'tex_%s.bin' % k
            t.data.tofile(os.path.join(d, fn))
            f.write('%s %s %s %d %d %d %s %s %d %s\n' % (k, t.kind, t.fmt, t.w, t.h, t.d, t.filt, t.wrap, t.mips, fn))


def smooth3(n, seed):
    r = np.random.RandomState(seed)
    a = r.rand(n, n, n)
    for ax in range(3):
        a = (a + np.roll(a, 1, ax) + np.roll(a, -1, ax)) / 3.0
    a = (a - a.min()) / (a.max() - a.min())
    return (a * 255).astype(np.uint8)


if mode == 'volsky':
    leak = ('air/cell_volsky_ps__WW_CELL_FOGLEAK.hlsl', ['WW_CELL_FOGLEAK'])
    jobs = [(FJ.volsky('volsky_leak', *leak), 1),
            (FJ.volsky('volsky_leak_near', *leak, far_on=False, flags=2 | 8 | 32 | 64 | 65536, rule=1, night=False), 0)]
    for j, over in jobs:
        d = os.path.join(OUT, j['name'])
        os.makedirs(d, exist_ok=True)
        write_prog(d, 'prog.frag.glsl', 'air/cell_volsky.frag', ['WW_CELL_FOGLEAK'])
        U = dict(j['uniforms'])
        U['volAirDrift'] = [0.0, 0.0, 1.0, 0.5]
        write_uni(os.path.join(d, 'uni.txt'), U)
        T = dict(j['textures'])
        T['volAirNoise'] = Tex('3d', 'r8', smooth3(32, 5), 'linear', 'repeat')  # the drift lumps (R8, cubic, as vaNoiseLoad)
        write_tex(d, T, ['atApTex', 'volAirTerr', 'volAirRainMap', 'volTex', 'volTile', 'atSkyTransTex', 'volAirNoise'])
        g = bytearray(336)  # uniforms.glsl globalUniforms, std140
        vm = np.asarray(U['viewMatrix'], np.float32).reshape(3, 3)  # GL columns
        for c in range(3):
            struct.pack_into('<3f', g, 16 * c, *vm[c])
            struct.pack_into('<3f', g, 48 + 16 * c, *[1.0 if r == c else 0.0 for r in range(3)])
        struct.pack_into('<16f', g, 96, *np.asarray(U['projectionMatrix'], np.float32))
        struct.pack_into('<4i', g, 288, *[int(x) for x in U['viewportDimensions']])
        open(os.path.join(d, 'global.bin'), 'wb').write(bytes(g))
        W, H = j['size']
        open(os.path.join(d, 'job.txt'), 'w', newline='\n').write('%d %d %d\n' % (W, H, over))
        print('volsky job %s: %dx%d overSky %d, %d uniforms' % (j['name'], W, H, over, len(U)))
else:
    cj = {j['name']: j for j in FJ.cloud_jobs()}
    if not cj:
        sys.exit('no cloud inputs')
    for suffix, defs in (('', []), ('_a', ['WW_CLOUD_ACCUM'])):
        d = os.path.join(OUT, 'cloud' + suffix)
        os.makedirs(d, exist_ok=True)
        s4, prev, res = cj['cloud%s_march_s4' % suffix], cj['cloud%s_march_prev' % suffix], cj['cloud%s_resolve' % suffix]
        write_prog(d, 'march.frag.glsl', 'clouds/cloud_march.frag', defs)
        write_prog(d, 'resolve.frag.glsl', 'clouds/cloud_resolve.frag', defs)
        write_uni(os.path.join(d, 's4.uni'), s4['uniforms'])
        write_uni(os.path.join(d, 'prev.uni'), prev['uniforms'])
        write_uni(os.path.join(d, 'resolve.uni'), res['uniforms'])
        T = dict(s4['textures'])
        r = np.random.RandomState(17)  # a coverage map with structure (the port gate's 1x1 zero map draws its bands' own cover)
        m = r.rand(32, 32, 4).astype(np.float32)
        for ax in (0, 1):
            m = (m + np.roll(m, 1, ax) + np.roll(m, -1, ax)) / 3.0
        T['cvMap'] = Tex('2d', 'rgba32f', m, 'linear', 'clamp')
        T['cvShape'] = Tex('3d', 'r8', T['cvShape'].data, 'linear', 'repeat', mips=1)  # cloudvol.cpp: mipmapped, trilinear
        T['cvDetail'] = Tex('3d', 'r8', T['cvDetail'].data, 'linear', 'repeat', mips=1)
        order = ['cvMap', 'cvShape', 'cvDetail'] + (['cvBlue'] if suffix else []) + ['cvSunView', 'cvMoonView', 'cvScene', 'cvTrans']
        write_tex(d, T, order)
        (hw, hh), (mw, mh) = prev['size'], s4['size']
        open(os.path.join(d, 'job.txt'), 'w', newline='\n').write('%d %d %d %d %d\n' % (hw, hh, mw, mh, 1 if suffix else 0))
        print('cloud job cloud%s: history %dx%d, march %dx%d' % (suffix, hw, hh, mw, mh))
