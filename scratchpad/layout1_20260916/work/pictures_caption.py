"""Lane LAYOUT1 (2026-09-16): caption the three pictures.

Every path in a caption is read off disk at the moment the picture is made --
the file's own size and the frame's own size, never a number typed from the
brief.  make_pictures.py holds the drawing; this is the driver that names the
three files and says what each one has to show.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_pictures as mp                                   # noqa: E402

W = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/layout1_20260916/work/pics/'
OUT = mp.OUT
made = []

# --- 1. the object pair, opened from its new path ---------------------------
pair_src = W + 'pair.png'
pair_file = W + 'tree/FO4CSLOD/Commonwealth/Commonwealth.lodi'
if os.path.exists(pair_src) and os.path.exists(pair_file):
    mp.shot_caption(
        pair_src, OUT + '01_pair_from_its_new_path.png', pair_file,
        'the (-20,24) object pair, opened from FO4CSLOD/Commonwealth/',
        extra=('676 placements drawn from the library beside it, '
               '.lodo %d bytes' % os.path.getsize(
                   W + 'tree/FO4CSLOD/Commonwealth/Commonwealth.lodo'),))
    made.append('01')
else:
    print('MISSING', pair_src, 'or', pair_file)

# --- 2. a .lodt terrain level, decoded from its new path --------------------
lodt = None
for root, _dirs, files in os.walk(W + 'vt'):
    for f in sorted(files):
        if f.lower().endswith('.lodt'):
            p = os.path.join(root, f).replace(chr(92), '/')
            if lodt is None or os.path.getsize(p) > os.path.getsize(lodt):
                lodt = p
if lodt:
    print('lodt:', lodt)
    if mp.lodt_mosaic(lodt, OUT + '02_lodt_level_from_its_new_path.png'):
        made.append('02')
else:
    print('MISSING: no .lodt under', W + 'vt')

# --- 3. the panel, with the root label under the output field ---------------
panel = W + 'panel.png'
if os.path.exists(panel):
    mp.shot_caption(
        panel, OUT + '03_panel_root_label.png', panel,
        'the LOD Generation panel: the FO4CS target names its root',
        extra=('the label reads FO4CSLOD\\Commonwealth\\ and the stock engine '
               'hides it',))
    made.append('03')
else:
    print('MISSING', panel)

print('made:', ' '.join(made) or 'nothing')
for f in sorted(os.listdir(OUT)):
    print('  %s  %d bytes' % (f, os.path.getsize(OUT + f)))
