"""Lane VKFAR3 (2026-11-15, plan row A1 step b): the GL program the app links for the screen GI with the ray-traced
cascades up (res/shaders/features/ao/cell_gihalf.frag), as the loader makes it: the #include expansion of
tests/spells/shader_link_check.py and the loader's defines (src/gl/glcontext.cpp Shader::load) that the source holds a
token of: WW_CELL_RTDDGI + WW_CELL_RTDDGI_CASCADES (the read, more than one cascade), WW_CELL_FOGLEAK (on unless 0),
WW_FRAME_UBO (on unless 0). The gate gihalfview.cpp compiles the file this writes.

    python -I tests/vkframe/gihalf_src.py <out file>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

src = S.expand(os.path.join(TREE, 'res', 'shaders', 'features', 'ao', 'cell_gihalf.frag'))
want = [('_CELL_RTDDGI', 'WW_CELL_RTDDGI'), ('_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES'), ('_CELL_FOGLEAK', 'WW_CELL_FOGLEAK'),
        ('WW_FRAME_UBO', 'WW_FRAME_UBO')]
defs = [d for tok, d in want if tok in src]
if 'WW_CELL_RTDDGI_CASCADES' not in defs:
    print('GIHALF-SRC FAIL (the source has no ray-traced read)')
    sys.exit(2)
out = S.inject(src, defs)
with open(sys.argv[1], 'wb') as f:
    f.write(out.encode('latin-1'))
print('GIHALF-SRC OK (%d bytes, defines %s)' % (len(out), ' '.join(defs)))
