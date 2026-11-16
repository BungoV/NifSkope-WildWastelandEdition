"""Lane VKFAR18 (route (a) step 5): the GL programs of tests/vkframe/reflrelightdraw.cpp, the reflection probe relight as
the app links it (features/reflections/cell_reflrelight.vert / .frag) with the loader's #include expansion and the two
define sets the shipped SPIR-V twins carry: base (WW_CELL_FOGLEAK, WW_FRAME_UBO: reflrelight_view_ps) and the RT GI key
(+ WW_CELL_RESTIR, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES: reflrelight_view_rt_ps, vkframe_spirv.sh fxr).

    python -I tests/vkframe/reflrelightdraw_prep.py <out dir>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
SH = os.path.join(TREE, 'res', 'shaders')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
KEYS = (('base', ['WW_CELL_FOGLEAK', 'WW_FRAME_UBO']),
        ('rt', ['WW_CELL_FOGLEAK', 'WW_FRAME_UBO', 'WW_CELL_RESTIR', 'WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES']))
os.makedirs(OUT, exist_ok=True)
for key, defs in KEYS:
    for stem, stage in (('cell_reflrelight.vert', 'vert'), ('cell_reflrelight.frag', 'frag')):
        src = os.path.join(SH, 'features', 'reflections', stem)
        text = S.inject(S.expand(src), defs)
        if '#include' in text:
            sys.exit('reflrelightdraw prep: an #include left unexpanded in %s' % src)
        with open(os.path.join(OUT, '%s.%s.glsl' % (key, stage)), 'w', newline='\n', encoding='latin-1') as f:
            f.write(text)
    print('reflrelightdraw program %s: cell_reflrelight.vert + .frag, defines %s' % (key, ' '.join(defs)))
