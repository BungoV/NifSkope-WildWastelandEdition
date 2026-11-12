"""lane VKSHARE1: the GL Modern TAA programs' sources (cell_ao.vert + features/post game_taa_mv / taa2 / taa2_rcas .frag)
with the loader's #include expansion (tests/spells/shader_link_check.py), for tests/vkframe/taaview.cpp's GL reference.

    python -I tests/vkframe/taaview_expand.py <out dir>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

out = sys.argv[1]
POST = ('res', 'shaders', 'features', 'post')
for name, rel in (('cell_ao.vert.glsl', ('res', 'shaders', 'cell_ao.vert')),
                  ('game_taa_mv.frag.glsl', POST + ('game_taa_mv.frag',)),
                  ('taa2.frag.glsl', POST + ('taa2.frag',)),
                  ('taa2_rcas.frag.glsl', POST + ('taa2_rcas.frag',))):
    src = S.expand(os.path.join(TREE, *rel))
    with open(os.path.join(out, name), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)
    print('expanded %s: %d lines' % (name, src.count('\n') + 1))
