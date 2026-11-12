"""lane VKSKY1: the GL sky program's sources (atmo_sky.prog = cell_ao.vert + features/sky/atmo_sky.frag) with the
loader's #include expansion (tests/spells/shader_link_check.py), for tests/vkframe/skyview.cpp's GL reference.

    python -I tests/vkframe/skyview_expand.py <out dir>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

out = sys.argv[1]
for name, rel in (('cell_ao.vert.glsl', ('res', 'shaders', 'features', 'sky', 'cell_ao.vert')),
                  ('atmo_sky.frag.glsl', ('res', 'shaders', 'features', 'sky', 'atmo_sky.frag'))):
    p = os.path.join(TREE, *rel)
    if not os.path.exists(p):
        alt = os.path.join(TREE, 'res', 'shaders', rel[-1])
        p = alt if os.path.exists(alt) else p
    src = S.expand(p)
    with open(os.path.join(out, name), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)
    print('expanded %s from %s: %d lines' % (name, os.path.relpath(p, TREE), src.count('\n') + 1))
