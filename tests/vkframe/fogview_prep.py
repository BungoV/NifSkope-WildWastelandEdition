"""Lane VKFAR2 (the froxel item): the GL program of tests/vkframe/fogview.cpp, the air pass's cell_volair as the app links
it (cell_volair.vert + cell_volair.frag, the loader's #include expansion) under the app's two switch sets
(glcontext.cpp): `rt` = WW_FRAME_UBO + WW_CELL_RTDDGI + WW_CELL_AIR_SKYVIEW (RT GI on, the shipped default), `plain` =
WW_FRAME_UBO alone (RT GI off: SKYVIEW needs RTDDGI). WW_CELL_FOGLEAK and WW_CELL_AIR_BOUNCE ship off.

    python -I tests/vkframe/fogview_prep.py <out dir>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
AIR = os.path.join(TREE, 'res', 'shaders', 'features', 'air')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
SETS = {'rt': ['WW_FRAME_UBO', 'WW_CELL_RTDDGI', 'WW_CELL_AIR_SKYVIEW'], 'plain': ['WW_FRAME_UBO']}
os.makedirs(OUT, exist_ok=True)
for name, defs in SETS.items():
    for src, dst in (('cell_volair.vert', 'vert'), ('cell_volair.frag', 'frag')):
        with open(os.path.join(OUT, 'volair_%s.%s.glsl' % (name, dst)), 'w', newline='\n', encoding='latin-1') as f:
            f.write(S.inject(S.expand(os.path.join(AIR, src)), defs))
    print('fogview program %s: cell_volair.vert + cell_volair.frag, defines %s' % (name, ' '.join(defs)))
