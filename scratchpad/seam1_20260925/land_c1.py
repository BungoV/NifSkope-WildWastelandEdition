"""C1 test: LAND winners per cell around the Sanctuary block, and the base/layer paint per quadrant."""
import sys, pickle, os
sys.path.insert(0, r'E:/Projects/NifskopeWWE-seam1/tests/spells')
from lodgen_cover_model import Esm, dominant_base
for p in ['X:/Programs/Steam/steamapps/common/Fallout 4/Data/DLCRobot.esm',
          'X:/Programs/Steam/steamapps/common/Fallout 4/Data/DLCworkshop03.esm']:
    e = Esm(p); e.walk(0x3C); print(os.path.basename(p), 'Commonwealth LAND cells', sorted(e.lands)[:10], len(e.lands))
e = Esm('X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'); e.walk(0x3C)
pickle.dump({'lands': e.lands, 'ltex': e.ltex, 'txst': e.txst}, open('fo4esm_cw.pkl', 'wb'))
print('Fallout4.esm Commonwealth LAND', len(e.lands), 'leftover', e.leftover)
def ed(f): return (e.ltex.get(f, {}).get('edid') or ('%08X' % f)) if f else '-'
print('\nper cell: quadrant bases (SW SE NW NE) | layers per quadrant | dim-4 chunk dominant base')
for cy in range(26, 16, -1):
    for cx in range(-24, -13):
        L = e.lands.get((cx, cy))
        if not L: print(cx, cy, 'NO LAND'); continue
        ch = (cx - ((cx + 96) % 4), cy - ((cy + 96) % 4))
        print('%4d %3d  base %-60s layers %s  chunkDom(%d,%d)=%s' % (cx, cy, ' '.join(ed(b) for b in L['base']),
              [len(l) for l in L['layers']], ch[0], ch[1], ed(dominant_base(e, ch[0], ch[1], 4))))
