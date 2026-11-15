"""Lane VKFAR3 (row G3): the GL program of tests/vkframe/fargrounddraw.cpp, the far ring 0's live ground as the app links
it in the shader-decals option (fo4_default.vert + features/far-ground/fo4_cellcsmfgl.frag) with the two defines the app
injects and the shipped SPIR-V twins carry (WW_CELL_FOGLEAK, WW_FRAME_UBO).

BATCH1117: GL's side is the HLSL-built program (the readable HLSL is the reference), as the app loads it: the SPIR-V pair
tests/hlsllive/build_all.py writes for `fo4_cellcsmfgl.prog@WW_CELL_FOGLEAK+WW_FRAME_UBO` -> prog.vert.spv / prog.frag.spv.
The legacy GLSL (#include expanded, defines injected) is still written as prog.vert.glsl / prog.frag.glsl: the gate reads
its sampler types (shadow samplers) and runs it instead with FARGROUNDDRAW_LEGACY=1 (info only).

    python -I tests/vkframe/fargrounddraw_prep.py <out dir> [<SPIR-V set dir>]
The set dir (default <out>/spvset) is build_all's out dir; build_all runs into it (--only fo4_cellcsmfgl.prog, incremental).
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
DEF = ['WW_CELL_FOGLEAK', 'WW_FRAME_UBO']
KEYS = (('fo4_default.vert', 'prog.vert.spv'), ('fo4_cellcsmfgl.frag@' + '+'.join(DEF), 'prog.frag.spv'))
os.makedirs(OUT, exist_ok=True)
for src, dst in ((os.path.join(SH, 'fo4_default.vert'), 'prog.vert.glsl'),
                 (os.path.join(SH, 'features', 'far-ground', 'fo4_cellcsmfgl.frag'), 'prog.frag.glsl')):
    text = S.inject(S.expand(src), DEF)
    if '#include' in text:
        sys.exit('fargrounddraw prep: an #include left unexpanded in %s' % src)
    with open(os.path.join(OUT, dst), 'w', newline='\n', encoding='latin-1') as f:
        f.write(text)
r = subprocess.run([sys.executable, '-I', '-X', 'utf8', os.path.join(TREE, 'tests', 'hlsllive', 'build_all.py'), TREE, SET,
                    '--only', 'fo4_cellcsmfgl.prog'], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                   universal_newlines=True, encoding='utf-8', errors='replace')
built = [ln for ln in r.stdout.splitlines() if ln.startswith('A1 built')]
if r.returncode != 0:
    sys.exit('fargrounddraw prep: build_all rc %d: %s' % (r.returncode, r.stdout[-1500:]))
index = {}
with open(os.path.join(SET, 'spirv', 'index.txt'), encoding='utf-8') as f:
    for ln in f:
        k, _t, v = ln.rstrip('\n').partition('\t')
        if v:
            index[k] = v
for key, dst in KEYS:
    if key not in index:
        sys.exit('fargrounddraw prep: no SPIR-V stage %s in %s' % (key, SET))
    shutil.copyfile(os.path.join(SET, 'spirv', index[key]), os.path.join(OUT, dst))
    print('fargrounddraw SPIR-V %s: %s' % (key, index[key]))
print('fargrounddraw %s' % (built[-1] if built else 'build_all: no A1 line'))
print('fargrounddraw program: HLSL-built fo4_cellcsmfgl.prog@%s (legacy .frag written for its sampler types / info)'
      % '+'.join(DEF))
