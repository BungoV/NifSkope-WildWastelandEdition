"""Lane VKGROUND10: the GL programs of tests/vkframe/fargndrtdraw.cpp, the far ring 0's live ground (fo4_cellcsmfgl) under
the two RT GI keys the app links on the RT route: RT = FOGLEAK + RESTIR + RTDDGI + RTDDGI_CASCADES + FRAME_UBO, DG = the same
without RESTIR. GL's side is the HLSL-built SPIR-V (build_all.py --only fo4_cellcsmfgl.prog); the legacy GLSL under the same
key's defines is written for its sampler types and fill order (fargrounddraw's BATCH1118 rule).

    python -I tests/vkframe/fargndrtdraw_prep.py <out dir> [<SPIR-V set dir>]
Writes prog.vert.spv, prog_rt.frag.spv, prog_dg.frag.spv, prog.vert.glsl, prog_rt.frag.glsl, prog_dg.frag.glsl.
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
SH = os.path.join(TREE, 'res', 'shaders')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
SET = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(OUT, 'spvset')
BASE = ['WW_CELL_FOGLEAK', 'WW_FRAME_UBO']
KEYS = {
    'rt': sorted(BASE + ['WW_CELL_RESTIR', 'WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES']),
    'dg': sorted(BASE + ['WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES']),
}
os.makedirs(OUT, exist_ok=True)
vert = S.inject(S.expand(os.path.join(SH, 'fo4_default.vert')), BASE)
frag_src = os.path.join(SH, 'features', 'far-ground', 'fo4_cellcsmfgl.frag')
texts = {'prog.vert.glsl': vert}
for tag, defs in KEYS.items():
    texts['prog_%s.frag.glsl' % tag] = S.inject(S.expand(frag_src), defs)
for dst, text in texts.items():
    if '#include' in text:
        sys.exit('fargndrtdraw prep: an #include left unexpanded in %s' % dst)
    with open(os.path.join(OUT, dst), 'w', newline='\n', encoding='latin-1') as f:
        f.write(text)
r = subprocess.run([sys.executable, '-I', '-X', 'utf8', os.path.join(TREE, 'tests', 'hlsllive', 'build_all.py'), TREE, SET,
                    '--only', 'fo4_cellcsmfgl.prog'], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                   universal_newlines=True, encoding='utf-8', errors='replace')
built = [ln for ln in r.stdout.splitlines() if ln.startswith('A1 built')]
if r.returncode != 0:
    sys.exit('fargndrtdraw prep: build_all rc %d: %s' % (r.returncode, r.stdout[-1500:]))
index = {}
with open(os.path.join(SET, 'spirv', 'index.txt'), encoding='utf-8') as f:
    for ln in f:
        k, _t, v = ln.rstrip('\n').partition('\t')
        if v:
            index[k] = v
pairs = [('fo4_default.vert', 'prog.vert.spv')]
for tag, defs in KEYS.items():
    pairs.append(('fo4_cellcsmfgl.frag@' + '+'.join(defs), 'prog_%s.frag.spv' % tag))
for key, dst in pairs:
    if key not in index:
        sys.exit('fargndrtdraw prep: no SPIR-V stage %s in %s' % (key, SET))
    shutil.copyfile(os.path.join(SET, 'spirv', index[key]), os.path.join(OUT, dst))
    print('fargndrtdraw SPIR-V %s: %s' % (key, index[key]))
print('fargndrtdraw %s' % (built[-1] if built else 'build_all: no A1 line'))
