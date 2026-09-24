"""IMPOSTOR16 -- the full-material numbers over the lit GIF sweep.
    python matmetrics.py <lit root> tag [tag ...]
brightness: card/mesh mean luma (Rec.709 on the 8-bit framebuffer), each over its own silhouette.
highlight: centroid of the brightest 2 % of each silhouette's pixels; card-vs-mesh distance
/ mesh silhouette height. Beside it the SIGNAL it must beat: the mesh highlight's own distance
from the mesh silhouette centroid (a card whose highlight sat at its silhouette centre would
score about that)."""
import sys, os, numpy as np
from PIL import Image
root = sys.argv[1]
for tag in sys.argv[2:]:
    d = os.path.join(root, tag)
    az = sorted(int(f[4:7]) for f in os.listdir(d) if f.endswith('_card.png'))
    R, HD, HS = [], [], []
    for a in az:
        out = {}
        for k in ('mesh', 'card'):
            im = np.asarray(Image.open(os.path.join(d, 'v_az%03d_el00_%s.png' % (a, k))).convert('RGB')).astype(float)
            m = (im != im[0, 0]).any(-1)
            L = im @ [.2126, .7152, .0722]
            ys, xs = np.nonzero(m); v = L[m]
            t = np.percentile(v, 98); h = v >= t
            out[k] = (v.mean(), np.array([xs[h].mean(), ys[h].mean()]), np.array([xs.mean(), ys.mean()]), ys.max() - ys.min() + 1)
        R.append(out['card'][0] / out['mesh'][0])
        H = out['mesh'][3]
        HD.append(np.linalg.norm(out['card'][1] - out['mesh'][1]) / H)
        HS.append(np.linalg.norm(out['mesh'][1] - out['mesh'][2]) / H)
    R = np.array(R); HD = np.array(HD); HS = np.array(HS)
    inb = ((R >= 0.9) & (R <= 1.1)).mean()
    print(f'{tag}: {len(az)} views. card/mesh luma mean {R.mean():.3f} min {R.min():.3f} (az {az[int(R.argmin())]}) max {R.max():.3f} (az {az[int(R.argmax())]}); views inside 0.9..1.1: {inb*100:.1f}%')
    print(f'   highlight centroid card-vs-mesh {HD.mean():.3f} of tree height (worst {HD.max():.3f}); mesh highlight off its own silhouette centre {HS.mean():.3f} (the signal)')
# ---- where the light sits: blurred-luma maps (sigma = 1/32 of the tree height),
# compared inside the pixels both silhouettes cover: Pearson r, and the "bright
# side" vector = luma-weighted offset of the brightest quarter from the silhouette
# centre, angle between card and mesh. Floor: the same numbers between the mesh
# at az and the mesh at az+60 (a picture lit from somewhere else).
from scipy.ndimage import gaussian_filter
def maps(p):
    im = np.asarray(Image.open(p).convert('RGB')).astype(float)
    m = (im != im[0, 0]).any(-1); L = im @ [.2126, .7152, .0722]
    ys, xs = np.nonzero(m); H = ys.max() - ys.min() + 1; s = H / 32
    Lb = gaussian_filter(L * m, s) / np.maximum(gaussian_filter(m.astype(float), s), 1e-3)
    t = np.percentile(Lb[m], 75); b = m & (Lb >= t)
    by, bx = np.nonzero(b); v = np.array([bx.mean() - xs.mean(), by.mean() - ys.mean()])
    return m, Lb, v
def cmp(a, b):
    both = a[0] & b[0]
    r = np.corrcoef(a[1][both], b[1][both])[0, 1]
    c = np.dot(a[2], b[2]) / (np.linalg.norm(a[2]) * np.linalg.norm(b[2]) + 1e-9)
    return r, np.degrees(np.arccos(np.clip(c, -1, 1)))
for tag in sys.argv[2:]:
    d = os.path.join(root, tag)
    az = sorted(int(f[4:7]) for f in os.listdir(d) if f.endswith('_card.png'))
    step = az[1] - az[0]; RS, AS, RF, AF = [], [], [], []
    for a in az[::3]:
        M = maps(os.path.join(d, 'v_az%03d_el00_mesh.png' % a)); C = maps(os.path.join(d, 'v_az%03d_el00_card.png' % a))
        M2 = maps(os.path.join(d, 'v_az%03d_el00_mesh.png' % ((a + 60) % 360)))
        r, ang = cmp(C, M); RS.append(r); AS.append(ang)
        r, ang = cmp(M2, M); RF.append(r); AF.append(ang)
    print(f'{tag} light placement over {len(RS)} views: card-vs-mesh blurred-luma r {np.mean(RS):.3f} (min {np.min(RS):.3f}), bright-side angle mean {np.mean(AS):.1f} deg (worst {np.max(AS):.1f}); FLOOR mesh-vs-mesh+60deg r {np.mean(RF):.3f}, angle {np.mean(AF):.1f} deg')
