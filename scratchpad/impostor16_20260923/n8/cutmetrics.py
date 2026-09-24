"""IMPOSTOR16 N8 addendum -- cut variants S / A / F against the mesh and the nearest frame.
    python n8/cutmetrics.py
Dirs: n8/cuts/<sweep>/<variant>/<set>/v_az%03d_el%02d_{card,mesh}.png (card ch 2, mesh ch 8);
N16 S at el 0 = sweep/n16_2k (same run folder, same channels).
Per variant (NOISE = 3x3-majority speckle, added with the director ranking order, before any number): IoU mean/min; hole share mean/max; worst per-step card XOR / mean mesh area,
mesh's own worst step beside it; TORN share per view = |mesh & near & ~card| / |mesh & near|
(TEAR1 pop.py law), reported worst view, mean of the 5 worst views, mean over all views."""
import os, numpy as np
from scipy.ndimage import median_filter
from PIL import Image
L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostor16_20260923'
def mask(p):
    a = np.asarray(Image.open(p).convert('RGB')); return (a != a[0, 0]).any(-1)
def load(d, el, k):
    az = sorted(int(f[4:7]) for f in os.listdir(d) if f.endswith('_card.png'))
    return az, [mask(os.path.join(d, 'v_az%03d_el%02d_%s.png' % (a, el, k))) for a in az]
def where(sw, v, s):
    if sw == 'az' and v == 'S' and s == 'n16_2k':
        return os.path.join(L, 'sweep', 'n16_2k')
    return os.path.join(L, 'n8', 'cuts', sw, v, s)
for sw, el in (('az', 0), ('az20', 20)):
    for s in ('n8_2k', 'n16_2k'):
        nd = where(sw, 'near', s)
        if not os.path.isdir(nd): continue
        az, N = load(nd, el, 'card')
        for v in ('S', 'A', 'F'):
            d = where(sw, v, s)
            if not os.path.isdir(d): print(sw, s, v, 'MISSING'); continue
            a2, C = load(d, el, 'card'); _, M = load(d, el, 'mesh')
            assert a2 == az, (len(a2), len(az))
            n = len(az); area = np.mean([m.sum() for m in M])
            iou = np.array([(c & m).sum() / (c | m).sum() for c, m in zip(C, M)])
            hole = np.array([(m & ~c).sum() / m.sum() for c, m in zip(C, M)])
            cx = np.array([(C[i] ^ C[(i + 1) % n]).sum() for i in range(n)]) / area
            mx = np.array([(M[i] ^ M[(i + 1) % n]).sum() for i in range(n)]) / area
            torn = np.array([((m & q) & ~c).sum() / max(1, (m & q).sum()) for c, m, q in zip(C, M, N)])
            o = np.argsort(torn)[::-1]
            # NOISE: pixels whose covered state disagrees with their 3x3 majority, per covered px
            # (isolated speckle / pinholes); the mesh's own figure is the floor. Every 6th view.
            sp = np.mean([(median_filter(c.astype(np.uint8), 3) != c).sum() / c.sum() for c in C[::6]])
            spm = np.mean([(median_filter(m.astype(np.uint8), 3) != m).sum() / m.sum() for m in M[::6]])
            print(f'{s:7s} el{el:02d} {v}: IoU {iou.mean():.4f} (min {iou.min():.4f}) hole {hole.mean():.4f} (max {hole.max():.4f}) '
                  f'step worst {cx.max():.4f} mean {cx.mean():.4f} [mesh {mx.max():.4f}/{mx.mean():.4f}] '
                  f'TORN worst {torn[o[0]]*100:.1f}% az{az[o[0]]} top5 {torn[o[:5]].mean()*100:.1f}% mean {torn.mean()*100:.2f}% | speckle {sp:.4f} [mesh {spm:.4f}]')
