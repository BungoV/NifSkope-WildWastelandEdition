"""IMPOSTOR16 N8 addendum -- TEAR as missing CHUNKS (added after the pre-registered torn
share turned out to carry frame-choice mismatch: at N8 az150 the blended card and the
nearest-frame card differ by 33-37 % of the mesh area in every variant, S included).
chunk share = |open(mesh & ~card, 7x7 square)| / |mesh|: model pixels the card misses in
patches at least 7 px across (pinholes and stipple removed, lumps kept). Floor = the
nearest single frame (WW_IMPOSTOR_BLEND=0), which has no blend and so no blend tear.
Every 3rd view of the 1-degree sweeps; worst view, mean of the 5 worst, mean."""
import os, numpy as np
from PIL import Image
from scipy.ndimage import binary_opening
L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostor16_20260923'
def m(p):
    a = np.asarray(Image.open(p).convert('RGB')); return (a != a[0, 0]).any(-1)
st = np.ones((7, 7), bool)
for sw, el in (('az', 0), ('az20', 20)):
    for s in ('n8_2k', 'n16_2k'):
        for v in ('near', 'S', 'A', 'F'):
            d = os.path.join(L, 'sweep', 'n16_2k') if (sw, v, s) == ('az', 'S', 'n16_2k') else os.path.join(L, 'n8', 'cuts', sw, v, s)
            ch = []
            az = list(range(0, 360, 3))
            for a in az:
                M = m(os.path.join(d, 'v_az%03d_el%02d_mesh.png' % (a, el))); C = m(os.path.join(d, 'v_az%03d_el%02d_card.png' % (a, el)))
                ch.append(binary_opening(M & ~C, st).sum() / M.sum())
            ch = np.array(ch); o = np.argsort(ch)[::-1]
            print(f'{s:7s} el{el:02d} {v:4s}: chunk share worst {ch[o[0]]*100:.2f}% az{az[o[0]]} top5 {ch[o[:5]].mean()*100:.2f}% mean {ch.mean()*100:.2f}%', flush=True)
