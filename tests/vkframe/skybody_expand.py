"""lane VKFAR2 G2: the GL programs tests/vkframe/skybody.cpp draws as its reference, with the loader's #include expansion
(tests/spells/shader_link_check.py): the sky (atmo_sky.prog = cell_ao.vert + atmo_sky.frag), the stars (lookdev_clouds.prog)
and the moon globe (sky_moon.prog = lookdev_bill.vert + sky_moon.frag).

    python -I tests/vkframe/skybody_expand.py <out dir>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

SH = ('res', 'shaders')
out = sys.argv[1]
for name, rel in (('cell_ao.vert.glsl', SH + ('features', 'sky', 'cell_ao.vert')),
                  ('atmo_sky.frag.glsl', SH + ('features', 'sky', 'atmo_sky.frag')),
                  ('lookdev_clouds.vert.glsl', SH + ('features', 'clouds', 'lookdev_clouds.vert')),
                  ('lookdev_clouds.frag.glsl', SH + ('features', 'clouds', 'lookdev_clouds.frag')),
                  ('lookdev_bill.vert.glsl', SH + ('lookdev_bill.vert',)),
                  ('sky_moon.frag.glsl', SH + ('features', 'sky', 'sky_moon.frag'))):
    p = os.path.join(TREE, *rel)
    if not os.path.exists(p):
        alt = os.path.join(TREE, *(SH + (rel[-1],)))
        p = alt if os.path.exists(alt) else p
    src = S.expand(p)
    with open(os.path.join(out, name), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)
    print('expanded %s from %s: %d lines' % (name, os.path.relpath(p, TREE), src.count('\n') + 1))
