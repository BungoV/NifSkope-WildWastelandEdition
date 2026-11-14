"""Lane VKFAR2 (row P8): the GL program of tests/vkframe/cloudfrontview.cpp, the tree's cloud_composite.frag with the
loader's #include expansion. The harness seeds the frame, the history, its depth and the scene depth itself.

    python -I tests/vkframe/cloudfrontview_prep.py <out dir>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
OUT = os.path.abspath(sys.argv[1])
import shader_link_check as S  # noqa: E402

d = os.path.join(OUT, 'cloudfront')
os.makedirs(d, exist_ok=True)
src = S.expand(os.path.join(FEAT, 'clouds', 'cloud_composite.frag'))
with open(os.path.join(d, 'prog.frag.glsl'), 'w', newline='\n', encoding='latin-1') as f:
    f.write(src)
print('cloudfront job: cloud_composite.frag expanded, %d lines' % src.count('\n'))
