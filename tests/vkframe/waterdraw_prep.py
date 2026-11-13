"""Lane VKWATER3 (row G5): the GL program of tests/vkframe/waterdraw.cpp, the water as the app links it
(res/shaders/features/water/fo4_waterlook.prog: fo4_default.vert + fo4_waterlook.frag) with the loader's #include
expansion and the two defines the app injects and the shipped SPIR-V twins carry (WW_CELL_FOGLEAK, WW_FRAME_UBO).
The scene, the uniforms and the textures are made (seeded) by the harness.

    python -I tests/vkframe/waterdraw_prep.py <out dir>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
SH = os.path.join(TREE, 'res', 'shaders')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
DEF = ['WW_CELL_FOGLEAK', 'WW_FRAME_UBO']
os.makedirs(OUT, exist_ok=True)
for src, dst in ((os.path.join(SH, 'fo4_default.vert'), 'prog.vert.glsl'),
                 (os.path.join(SH, 'features', 'water', 'fo4_waterlook.frag'), 'prog.frag.glsl')):
    with open(os.path.join(OUT, dst), 'w', newline='\n', encoding='latin-1') as f:
        f.write(S.inject(S.expand(src), DEF))
print('waterdraw program: fo4_default.vert + fo4_waterlook.frag, defines %s' % ' '.join(DEF))
