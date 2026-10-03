"""lane SRGBTAG1: measure srgbtag_legacy.sh's shots. Background = the corner pixel; mesh pixels = any
channel more than 8 away from it. Prints one verdict line per measure."""
import sys, os
import numpy as np
from PIL import Image

out = sys.argv[1]

def load(tag):
    p = os.path.join(out, tag + '.png')
    if not os.path.exists(p):
        return None
    return np.asarray(Image.open(p).convert('RGB')).astype(np.int32)

def mask(a):
    bg = a[2, 2]
    return np.abs(a - bg).max(axis=2) > 8, bg

u, s, v, g = load('g1_u98'), load('g1_s99'), load('g1_van'), load('g2_suit')
g98 = load('g2_suit98')
if u is None or s is None:
    print('G1 NO PICTURE'); sys.exit(1)
mu, bg = mask(u); ms, _ = mask(s)
m = mu | ms
print(f'bg={tuple(int(x) for x in bg)} size={u.shape[1]}x{u.shape[0]} mesh_px_u98={int(mu.sum())} mesh_px_s99={int(ms.sum())}')
d = np.abs(u - s)[m]
print(f'G1 u98_vs_s99 mean|d|={d.mean():.3f} max|d|={int(d.max())} mean_u98={u[mu].mean():.2f} mean_s99={s[ms].mean():.2f} '
      f'ratio={s[ms].mean() / max(u[mu].mean(), 1e-6):.3f}')
if v is not None:
    mv, _ = mask(v)
    dv = np.abs(u - v)[m | mv]
    print(f'G1 fixture_used u98_vs_vanilla mean|d|={dv.mean():.3f}')
if g is not None:
    mg, bgg = mask(g)
    print(f'G2 suit mesh_px={int(mg.sum())} mean={g[mg].mean():.2f} rgb=({g[mg][:,0].mean():.1f},{g[mg][:,1].mean():.1f},{g[mg][:,2].mean():.1f})')
    if g98 is not None:
        m98, _ = mask(g98)
        dd = np.abs(g - g98)[mg | m98]
        print(f'G2 suit_vs_retag98 mean|d|={dd.mean():.3f} max|d|={int(dd.max())} mean_retag98={g98[m98].mean():.2f}')
