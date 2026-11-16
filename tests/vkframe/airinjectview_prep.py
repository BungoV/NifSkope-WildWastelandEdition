"""Lane VKAIR4 (the air inject stage 0): the GL programs of tests/vkframe/airinjectview.cpp, the air pass's cell_volair as
the app links it (features/air/cell_volair.vert + .frag, the loader's #include expansion) under the two pixel-stage keys
the shipped twins carry (variants.txt: the default key and the all-on RT GI key).

    python -I tests/vkframe/airinjectview_prep.py <out dir>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
AIR = os.path.join(TREE, 'res', 'shaders', 'features', 'air')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
KEYS = (('def', ['WW_CELL_AIR_SKYVIEW', 'WW_CELL_FOGLEAK', 'WW_FRAME_UBO']),
        ('rt', ['WW_CELL_AIR_SKYVIEW', 'WW_CELL_FOGLEAK', 'WW_CELL_RESTIR', 'WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES',
                'WW_FRAME_UBO']))
os.makedirs(OUT, exist_ok=True)
for key, defs in KEYS:
    for stem, stage in (('cell_volair.vert', 'vert'), ('cell_volair.frag', 'frag')):
        src = os.path.join(AIR, stem)
        text = S.inject(S.expand(src), defs)
        if any(line.lstrip().startswith('#include') for line in text.split('\n')):
            sys.exit('airinjectview prep: an #include left unexpanded in %s' % src)
        with open(os.path.join(OUT, '%s.%s.glsl' % (key, stage)), 'w', newline='\n', encoding='latin-1') as f:
            f.write(text)
    print('airinjectview program %s: cell_volair.vert + .frag, defines %s' % (key, ' '.join(defs)))
