"""Lane VKGROUND6 (the far ground page paint): the GL programs of tests/vkframe/fgpaintview.cpp as the app links them
(cell_fgpage.prog / cell_fgbc1.prog: cell_ao.vert + the .frag, the loader's #include expansion, no injected define).

    python -I tests/vkframe/fgpaintview_prep.py <out dir>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
SH = os.path.join(TREE, 'res', 'shaders')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

OUT = os.path.abspath(sys.argv[1])
PROGS = (('page', os.path.join(SH, 'cell_ao.vert'), os.path.join(SH, 'features', 'far-ground', 'cell_fgpage.frag')),
         ('bc1', os.path.join(SH, 'cell_ao.vert'), os.path.join(SH, 'features', 'far-ground', 'cell_fgbc1.frag')))
os.makedirs(OUT, exist_ok=True)
for key, vert, frag in PROGS:
    for src, stage in ((vert, 'vert'), (frag, 'frag')):
        text = S.inject(S.expand(src), [])
        if any(line.lstrip().startswith('#include') for line in text.split('\n')):
            sys.exit('fgpaintview prep: an #include left unexpanded in %s' % src)
        with open(os.path.join(OUT, '%s.%s.glsl' % (key, stage)), 'w', newline='\n', encoding='latin-1') as f:
            f.write(text)
    print('fgpaintview program %s: %s + %s, no define' % (key, os.path.basename(vert), os.path.basename(frag)))
