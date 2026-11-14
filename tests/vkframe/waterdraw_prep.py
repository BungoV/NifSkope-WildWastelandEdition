"""Lane VKWATER3 (row G5): the GL program of tests/vkframe/waterdraw.cpp, a water program as the app links it
(res/shaders/features/water/<prog>.prog: fo4_default.vert [+ fo4_waterdisp.tesc + .tese] + its .frag) with the loader's
#include expansion and the two defines the app injects and the shipped SPIR-V twins carry (WW_CELL_FOGLEAK, WW_FRAME_UBO).
The scene, the uniforms and the textures are made (seeded) by the harness.

    python -I tests/vkframe/waterdraw_prep.py <out dir> [program, default fo4_waterlook]

Writes prog.vert.glsl, prog.frag.glsl and, for a tessellated program, prog.tesc.glsl + prog.tese.glsl.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
SH = os.path.join(TREE, 'res', 'shaders')
WATER = os.path.join(SH, 'features', 'water')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
PROG = sys.argv[2] if len(sys.argv) > 2 else 'fo4_waterlook'
DEF = ['WW_CELL_FOGLEAK', 'WW_FRAME_UBO']
os.makedirs(OUT, exist_ok=True)
stages = []
for ln in open(os.path.join(WATER, PROG + '.prog'), encoding='latin-1'):
    if ln.startswith('shaders '):
        stages = ln.split()[1:]
if not stages:
    sys.exit('%s.prog: no shaders line' % PROG)
for f in ('prog.tesc.glsl', 'prog.tese.glsl'):  # a previous tessellated job's stages: moved aside, never this job's
    if os.path.exists(os.path.join(OUT, f)):
        os.replace(os.path.join(OUT, f), os.path.join(OUT, f + '.old'))
for st in stages:
    ext = st.rsplit('.', 1)[1]
    src = os.path.join(SH, st) if os.path.exists(os.path.join(SH, st)) else os.path.join(WATER, st)
    with open(os.path.join(OUT, 'prog.%s.glsl' % ext), 'w', newline='\n', encoding='latin-1') as f:
        f.write(S.inject(S.expand(src), DEF))
print('waterdraw program %s: %s, defines %s' % (PROG, ' + '.join(stages), ' '.join(DEF)))
