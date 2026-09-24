"""IMPOSTORAA1: per subject, a 3x EDGE CROP  mesh | card before (rung) | card after (2x bake).
Same bake view, same pixel window in all three grabs (the harness frames mesh and card alike).
The window is the 160x160 box holding the most MESH silhouette edge at the view with the most ink.
    python edgecrop.py GRAB_BEFORE GRAB_AFTER OUTDIR
Prints per subject: view, box, and the silhouette-edge IoU inside the box before/after
(edge = ink boundary dilated 1 px; a softer, truer edge scores higher)."""
import sys, os, glob
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage

GB, GA, OUT = sys.argv[1:4]
os.makedirs(OUT, exist_ok=True)
B, S = 160, 3

def font(sz):
    try: return ImageFont.truetype('C:/Windows/Fonts/consola.ttf', sz)
    except Exception: return ImageFont.load_default()
F = font(16)

def load(p):
    return np.asarray(Image.open(p).convert('RGB'))
def ink(a):
    return np.abs(a.astype(int) - a[2, 2].astype(int)).sum(-1) > 30
def edge(m):
    return ndimage.binary_dilation(m ^ ndimage.binary_erosion(m), iterations=1)

for t in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4'):
    ms = sorted(glob.glob(f'{GA}/{t}/v_*_mesh.png'))
    if not ms:
        print(t, 'no grabs'); continue
    best = max(ms, key=lambda p: ink(load(p)).sum())
    v = os.path.basename(best)[2:-9]
    M = load(best); CB = load(f'{GB}/{t}/v_{v}_card.png'); CA = load(f'{GA}/{t}/v_{v}_card.png')
    em = edge(ink(M)).astype(float)
    ii = ndimage.uniform_filter(em, B, mode='constant')
    y, x = np.unravel_index(np.argmax(ii), ii.shape)
    y0 = int(np.clip(y - B // 2, 0, M.shape[0] - B)); x0 = int(np.clip(x - B // 2, 0, M.shape[1] - B))
    sl = (slice(y0, y0 + B), slice(x0, x0 + B))
    def eiou(C):
        a = edge(ink(M))[sl]; b = edge(ink(C))[sl]
        return (a & b).sum() / max(1, (a | b).sum())
    def miou(C):
        a = ink(M)[sl]; b = ink(C)[sl]
        return (a & b).sum() / max(1, (a | b).sum())
    rb, ra = (eiou(CB), miou(CB)), (eiou(CA), miou(CA))
    print('%-9s view %s box x%d y%d  edge IoU before %.3f after %.3f | ink IoU before %.3f after %.3f'
          % (t, v, x0, y0, rb[0], ra[0], rb[1], ra[1]))
    tiles = [(M, 'mesh'), (CB, 'card BEFORE (rung, window grab)'), (CA, 'card AFTER (2x offscreen, 2x2 box)')]
    W = B * S; out = Image.new('RGB', (3 * W + 40, W + 70), (24, 24, 24)); dr = ImageDraw.Draw(out)
    dr.text((8, 6), '%s  bake view %s  3x crop %dx%d at (%d,%d)   edge IoU vs mesh: before %.3f  after %.3f'
            % (t, v, B, B, x0, y0, rb[0], ra[0]), fill=(255, 255, 255), font=F)
    for i, (a, lab) in enumerate(tiles):
        im = Image.fromarray(a[sl]).resize((W, W), Image.NEAREST)
        out.paste(im, (10 + i * (W + 10), 32)); dr.text((10 + i * (W + 10), W + 40), lab, fill=(230, 230, 230), font=F)
    out.save(f'{OUT}/{t}_edge3x.png')
