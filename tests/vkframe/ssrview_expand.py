"""lane VKLIGHT2: the GL reflections programs' sources (cell_ssr.prog = cell_ssr.vert + cell_ssr.frag, cell_ssrwater.prog =
cell_ssr.vert + cell_ssrwater.frag) with the loader's #include expansion (tests/spells/shader_link_check.py), for
tests/vkframe/ssrview.cpp's GL reference.

    python -I tests/vkframe/ssrview_expand.py <out dir>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

out = sys.argv[1]
for name in ('cell_ssr.vert', 'cell_ssr.frag', 'cell_ssrwater.frag'):
    p = os.path.join(TREE, 'res', 'shaders', 'features', 'reflections', name)
    src = S.expand(p)
    with open(os.path.join(out, name + '.glsl'), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)
    print('expanded %s: %d lines' % (name, src.count('\n') + 1))
