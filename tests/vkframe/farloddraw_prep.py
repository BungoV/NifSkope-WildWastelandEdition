"""Lane VKFAR2 (row G3): the GL program of tests/vkframe/farloddraw.cpp, the far LOD rings as the app links them
(fo4_default.vert + fo4_cellcsm.frag) with the loader's #include expansion and the two defines the app injects and the
shipped SPIR-V twins carry (WW_CELL_FOGLEAK, WW_FRAME_UBO). The scene, the uniforms and the textures are the harness's.

    python -I tests/vkframe/farloddraw_prep.py <out dir>
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
                 (os.path.join(SH, 'fo4_cellcsm.frag'), 'prog.frag.glsl')):
    with open(os.path.join(OUT, dst), 'w', newline='\n', encoding='latin-1') as f:
        f.write(S.inject(S.expand(src), DEF))
print('farloddraw program: fo4_default.vert + fo4_cellcsm.frag, defines %s' % ' '.join(DEF))
