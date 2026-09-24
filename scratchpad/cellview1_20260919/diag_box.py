"""One-off: why does the checker's model-space box differ from the viewer's?"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))), 'tests', 'spells'))
import cell_open_check as ck

DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
dump = ck.read_dump(sys.argv[1])
cache = {}
# the placement with the SMALLEST rotation, where the box is almost axis-aligned
rows = sorted(dump.items(),
              key=lambda kv: sum(abs(r) for r in kv[1]['rot']))
for (ref, part), row in rows[:6]:
    b = ck.model_bounds(DATA, row['model'], cache)
    if not b:
        continue
    print('0x%08X %s' % (ref, row['model']))
    print('   rot %.4f %.4f %.4f  scale %.3f' % tuple(row['rot'] + [row['scale']]))
    print('   viewer box size  %8.2f %8.2f %8.2f'
          % tuple(row['bmax'][k] - row['bmin'][k] for k in range(3)))
    print('   checker box size %8.2f %8.2f %8.2f  (unrotated, x scale)'
          % tuple((b[1][k] - b[0][k]) * row['scale'] for k in range(3)))
    print('   viewer centre-pos %8.2f %8.2f %8.2f'
          % tuple((row['bmax'][k] + row['bmin'][k]) / 2 - row['pos'][k]
                  for k in range(3)))
    print('   checker centre    %8.2f %8.2f %8.2f  (model space, x scale)'
          % tuple((b[1][k] + b[0][k]) / 2 * row['scale'] for k in range(3)))
