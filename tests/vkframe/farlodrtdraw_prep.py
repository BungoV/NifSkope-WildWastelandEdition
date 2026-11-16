"""Lane VKFAR28 (plan notes/vkfar26/next.md item 2): the GL programs of tests/vkframe/farlodrtdraw.cpp, the far LOD rings
under the RT GI keys as the loader builds them (res/shaders/hlsl/variants.txt, fo4_cellcsm.prog): fo4_default.vert +
fo4_cellcsm.frag with the #include expansion and the defines the app injects. prog_rt: the all-on key (RESTIR + RTDDGI +
RTDDGI_CASCADES), prog_dg: the Ray-traced GI key with no RT preset (RTDDGI + RTDDGI_CASCADES). The vertex stage keeps
farloddraw_prep.py's base defines.

    python -I tests/vkframe/farlodrtdraw_prep.py <out dir>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
SH = os.path.join(TREE, 'res', 'shaders')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
BASE = ['WW_CELL_FOGLEAK', 'WW_FRAME_UBO']
KEYS = (('prog_rt.frag.glsl', BASE + ['WW_CELL_RESTIR', 'WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES']),
        ('prog_dg.frag.glsl', BASE + ['WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES']))
os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, 'prog.vert.glsl'), 'w', newline='\n', encoding='latin-1') as f:
    f.write(S.inject(S.expand(os.path.join(SH, 'fo4_default.vert')), BASE))
frag = S.expand(os.path.join(SH, 'fo4_cellcsm.frag'))
for dst, defs in KEYS:
    with open(os.path.join(OUT, dst), 'w', newline='\n', encoding='latin-1') as f:
        f.write(S.inject(frag, defs))
    print('farlodrtdraw program %s: fo4_default.vert + fo4_cellcsm.frag, defines %s' % (dst, ' '.join(sorted(defs))))
