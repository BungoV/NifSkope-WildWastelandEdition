"""IMPOSTOR16 job 4 -- IoU, holes, ghosts and popping over the 1-degree sweep.

    python metrics.py <sweep root> tag [tag ...]

<root>/<tag>/v_az%03d_el00_{mesh,card}.png from orb.sh (card = debug channel 2,
mesh = LOD channel 8, so neither silhouette can be the clear colour).
The N4 set frames the camera at ortho half-width 1.02 x 345.305 and the N16 sets
at 1.02 x 356.831 (aimCamera uses the set's own half extent), so each tag's pixel
counts are converted to the N16 scale by (hw16 / hw_tag)^2 before being printed as
pixels; shares and IoU are scale-free.
"""
import sys, os, numpy as np
from PIL import Image

root = sys.argv[1]
HW = {'n4_512': 345.305, 'n16_2k': 356.831, 'n16_1k': 356.831}


def mask(p):
    im = np.asarray(Image.open(p).convert('RGB'))
    return (im != im[0, 0]).any(-1)


for tag in sys.argv[2:]:
    d = os.path.join(root, tag)
    az = sorted(int(f[4:7]) for f in os.listdir(d) if f.endswith('_card.png'))
    C, M = [], []
    for a in az:
        C.append(mask(os.path.join(d, 'v_az%03d_el00_card.png' % a)))
        M.append(mask(os.path.join(d, 'v_az%03d_el00_mesh.png' % a)))
    k = (HW[tag] / HW['n16_2k']) ** 2          # px at this tag's scale -> px at N16 scale (N4 frames are zoomed in: smaller half-width)
    iou = np.array([(c & m).sum() / max(1, (c | m).sum()) for c, m in zip(C, M)])
    hole = np.array([(m & ~c).sum() / max(1, m.sum()) for c, m in zip(C, M)])
    ghost = np.array([(c & ~m).sum() / max(1, c.sum()) for c, m in zip(C, M)])
    n = len(az)
    cx = np.array([(C[i] ^ C[(i + 1) % n]).sum() for i in range(n)]) * k
    mx = np.array([(M[i] ^ M[(i + 1) % n]).sum() for i in range(n)]) * k
    marea = np.mean([m.sum() for m in M]) * k
    w = int(np.argmax(cx))
    print(f'{tag}: {n} views az {az[0]}..{az[-1]} el 0 (px at N16 scale, mesh area mean {marea:.0f} px)')
    print(f'  IoU mean {iou.mean():.4f} min {iou.min():.4f} (az {az[int(np.argmin(iou))]})')
    print(f'  hole share (mesh px the card misses) mean {hole.mean():.4f} max {hole.max():.4f} (az {az[int(np.argmax(hole))]})')
    print(f'  ghost share (card px outside mesh) mean {ghost.mean():.4f} max {ghost.max():.4f}')
    print(f'  popping: card per-step XOR worst {cx.max():.0f} px (az {az[w]}->{az[(w + 1) % n]}, {cx.max() / marea:.4f} of mesh area) mean {cx.mean():.0f} px ({cx.mean() / marea:.4f})')
    ex = cx - mx
    print(f'  spike: card worst/median step {cx.max() / np.median(cx):.2f} (mesh {mx.max() / np.median(mx):.2f}); card step above mesh step at same az: worst {ex.max():.0f} px ({ex.max() / marea:.4f})')
    print(f'  floor: mesh per-step XOR worst {mx.max():.0f} px mean {mx.mean():.0f} px; card/mesh worst {cx.max() / mx.max():.2f} mean {cx.mean() / mx.mean():.2f}')
