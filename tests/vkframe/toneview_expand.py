"""lane VKFRAME1 round 11: the GL tone program's sources (res/shaders/cell_hdr.vert + features/exposure/cell_hdr.frag) with
the loader's #include expansion (tests/spells/shader_link_check.py), for tests/vkframe/toneview.cpp's GL reference.

    python -I tests/vkframe/toneview_expand.py <out dir>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

out = sys.argv[1]
for name, rel in (('cell_hdr.vert.glsl', ('res', 'shaders', 'cell_hdr.vert')),
                  ('cell_hdr.frag.glsl', ('res', 'shaders', 'features', 'exposure', 'cell_hdr.frag'))):
    src = S.expand(os.path.join(TREE, *rel))
    with open(os.path.join(out, name), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)
    print('expanded %s: %d lines' % (name, src.count('\n') + 1))
