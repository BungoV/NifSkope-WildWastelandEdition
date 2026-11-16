"""Lane VKCLOUD3: the jobs of tests/vkframe/cloudshadowview.cpp, written as files the C++ harness reads: the GL program
(the tree's cloud_shadowmap.frag with the loader's #include expansion), and per job its uniforms and textures
(tests/vklight/frag_jobs.py's cloudshadow jobs: the same inputs the readable HLSL twin was gated on, VKCLOUD1). The sheets
get mipmaps (the app's trilinear repeat, cloudshadow.cpp sheetsOf). The cloud inputs come from tests/vklight/cloud_inputs.py
(made under <out>/cloudin when missing, as frag.sh does).

    python -I tests/vkframe/cloudshadowview_prep.py <out dir>
"""
import os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
OUT = os.path.abspath(sys.argv[1])
CIN = os.path.join(OUT, 'cloudin')
if not os.path.exists(os.path.join(CIN, 'cloud_inputs.json')):
    subprocess.run([sys.executable, '-I', os.path.join(TREE, 'tests', 'vklight', 'cloud_inputs.py'), CIN], check=True,
                   stdin=subprocess.DEVNULL)
os.environ['FRAG_CLOUDIN'] = CIN
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'vklight'))
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


jobs = [j for j in FJ.cloudshadow_jobs() if not j['name'].startswith('probe_')]
if len(jobs) != 4:
    sys.exit('cloudshadow jobs: %d (want vol, band, beer, sheets)' % len(jobs))
src = S.expand(os.path.join(FEAT, 'clouds/cloud_shadowmap.frag'))
with open(os.path.join(OUT, 'prog.frag.glsl'), 'w', newline='\n', encoding='latin-1') as f:
    f.write(src)
names = []
for j in jobs:
    name = j['name'].replace('cloudshadow_', '')
    names.append(name)
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'uni.txt'), 'w', newline='\n') as f:
        for k, v in j['uniforms'].items():
            f.write('%s %s\n' % (k, ' '.join(repr(x) for x in flat(v))))
    T = j['textures']
    with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
        for k in ['cvShape', 'cvDetail', 'cvMap', 'csSheetTex', 'csSheetTex2']:
            ts = T[k] if isinstance(T[k], list) else [T[k]]
            for e, t in enumerate(ts):
                nm = k if not isinstance(T[k], list) else '%s[%d]' % (k, e)
                mips = 1 if k.startswith('csSheet') and t.w > 1 else t.mips
                fn = 'tex_%s_%d.bin' % (k, e)
                t.data.tofile(os.path.join(d, fn))
                f.write('%s %s %s %d %d %d %s %s %d %s\n' % (nm, t.kind, t.fmt, t.w, t.h, t.d, t.filt, t.wrap, mips, fn))
    W, H = j['size']
    open(os.path.join(d, 'job.txt'), 'w', newline='\n').write('%d %d\n' % (W, H))
open(os.path.join(OUT, 'jobs.txt'), 'w', newline='\n').write('\n'.join(names) + '\n')
print('cloudshadow jobs: %s (%dx%d)' % (' '.join(names), W, H))
