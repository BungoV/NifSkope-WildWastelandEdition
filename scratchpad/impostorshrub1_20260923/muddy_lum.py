"""IMPOSTORSHRUB1 muddy: mesh grab vs card grab of one orbit set, measured on
the INTERSECTION of the two silhouettes (so a coverage difference cannot pass
for a colour one). Background = the corner pixel. Per view and mean:
luma (Rec.709 on the bytes), saturation ((max-min)/max of the mean colour) and
the card/mesh luma ratio.

  python muddy_lum.py SHOTDIR [SHOTDIR...]"""
import glob, re, sys
import numpy as np
from PIL import Image
L = np.array([0.2126, 0.7152, 0.0722])

def sat(m):
    return (m.max() - m.min()) / max(m.max(), 1e-6)

for d in sys.argv[1:]:
    rows = []
    for f in sorted(glob.glob(d + '/*_mesh.png')):
        a = np.asarray(Image.open(f).convert('RGB')).astype(float)
        b = np.asarray(Image.open(f.replace('_mesh', '_card')).convert('RGB')).astype(float)
        ma = (np.abs(a - a[0, 0]) > 3).any(-1)
        mb = (np.abs(b - b[0, 0]) > 3).any(-1)
        m = ma & mb
        if not m.any():
            continue
        ca, cb = a[m].mean(0), b[m].mean(0)
        v = re.search(r'az\d+_el\d+', f).group()
        rows.append((v, (ca * L).sum(), (cb * L).sum(), sat(ca), sat(cb), m.sum(), ma.sum(), mb.sum()))
    print(d)
    for v, la, lb, sa, sb, n, na, nb in rows:
        print('  %-12s mesh luma %6.1f sat %.3f | card luma %6.1f sat %.3f | ratio %.3f | px both %6d mesh %6d card %6d'
              % (v, la, sa, lb, sb, lb / la, n, na, nb))
    if rows:
        r = np.array([[x[1], x[2], x[3], x[4]] for x in rows])
        print('  MEAN         mesh luma %6.1f sat %.3f | card luma %6.1f sat %.3f | ratio %.3f'
              % (r[:, 0].mean(), r[:, 2].mean(), r[:, 1].mean(), r[:, 3].mean(), (r[:, 1] / r[:, 0]).mean()))
