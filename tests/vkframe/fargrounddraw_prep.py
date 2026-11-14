"""Lane VKFAR3 (row G3): the GL program of tests/vkframe/fargrounddraw.cpp, the far ring 0's live ground as the app links
it in the shader-decals option (fo4_default.vert + features/far-ground/fo4_cellcsmfgl.frag) with the loader's #include
expansion and the two defines the app injects and the shipped SPIR-V twins carry (WW_CELL_FOGLEAK, WW_FRAME_UBO).

    python -I tests/vkframe/fargrounddraw_prep.py <out dir>
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
                 (os.path.join(SH, 'features', 'far-ground', 'fo4_cellcsmfgl.frag'), 'prog.frag.glsl')):
    text = S.inject(S.expand(src), DEF)
    if '#include' in text:
        sys.exit('fargrounddraw prep: an #include left unexpanded in %s' % src)
    with open(os.path.join(OUT, dst), 'w', newline='\n', encoding='latin-1') as f:
        f.write(text)
print('fargrounddraw program: fo4_default.vert + far-ground/fo4_cellcsmfgl.frag, defines %s' % ' '.join(DEF))
