# IMPOSTORLIGHT1 -- the BRIGHTNESS LADDER (brief job 1b) and the TRANSFER.
#   python ladder.py MASKROOT STAGEROOT[=label] [STAGEROOT[=label] ...]
# MASKROOT = a stage-1 (normals) grab root: its images decide the covered
# pixels, because a normal colour is never the background colour, whereas a
# DARK lit pixel is -- the FIN1 tone.py mask (colour vs background, threshold
# 12) drops exactly the pixels this lane is about. Pixels = the INTERSECTION of
# the mesh's and the card's normal silhouettes, per view, so a coverage
# difference cannot pose as a brightness one.
# Per subject and stage: mean Rec.709 luma (0..255, of the 8-bit framebuffer)
# of mesh and card over those pixels, ratio card/mesh, and the share clipped
# at 255 (a pre-tonemap stage can clip; the ratio is then a floor, said so).
# TRANSFER (every root): mesh luma deciles -> the card's mean luma per decile,
# Spearman rho over the pixels, and the slope sign. A rising transfer has rho>0.
import sys, glob, os, json
import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], float)
SUBJ = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')
W = np.array([0.2126, 0.7152, 0.0722])


def img(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(float)


def cov(a):
    return np.abs(a - BG).sum(-1) > 12


def rank(x):
    r = np.empty(len(x)); r[np.argsort(x, kind='stable')] = np.arange(len(x)); return r


def run(maskroot, root, t):
    ms, cs, clip_m, clip_c = [], [], 0, 0
    for m1 in sorted(glob.glob('%s/%s/*_mesh.png' % (maskroot, t))):
        v = os.path.basename(m1)[:-len('_mesh.png')]
        c1 = m1.replace('_mesh.png', '_card.png')
        m = '%s/%s/%s_mesh.png' % (root, t, v); c = '%s/%s/%s_card.png' % (root, t, v)
        if not (os.path.exists(c1) and os.path.exists(m) and os.path.exists(c)):
            continue
        k = cov(img(m1)) & cov(img(c1))
        if k.sum() < 50:
            continue
        am = img(m)[k]; ac = img(c)[k]
        clip_m += int((am.max(-1) >= 254.5).sum()); clip_c += int((ac.max(-1) >= 254.5).sum())
        ms.append(am @ W); cs.append(ac @ W)
    if not ms:
        return None
    lm = np.concatenate(ms); lc = np.concatenate(cs)
    q = np.percentile(lm, np.linspace(0, 100, 11))
    idx = np.clip(np.searchsorted(q, lm, side='right') - 1, 0, 9)
    tr = [float(lc[idx == i].mean()) if (idx == i).any() else float('nan') for i in range(10)]
    trm = [float(lm[idx == i].mean()) if (idx == i).any() else float('nan') for i in range(10)]
    rho = float(np.corrcoef(rank(lm), rank(lc))[0, 1]) if lm.std() > 0 and lc.std() > 0 else float('nan')
    return dict(px=len(lm), mesh=float(lm.mean()), card=float(lc.mean()), ratio=float(lc.mean() / max(lm.mean(), 1e-6)),
                clip_mesh=clip_m / len(lm), clip_card=clip_c / len(lm), rho=rho, tr_mesh=trm, tr_card=tr)


if __name__ == '__main__':
    maskroot = sys.argv[1]
    out = {}
    for arg in sys.argv[2:]:
        root, _, lab = arg.partition('=')
        lab = lab or os.path.basename(root.rstrip('/'))
        for t in SUBJ:
            r = run(maskroot, root, t)
            if r is None:
                continue
            out['%s|%s' % (lab, t)] = r
            print('%-10s %-9s px %8d  mesh %6.1f card %6.1f  ratio %.3f  clip m %.3f c %.3f  rho %+.3f' % (
                lab, t, r['px'], r['mesh'], r['card'], r['ratio'], r['clip_mesh'], r['clip_card'], r['rho']))
            if os.environ.get('TRANSFER'):
                print('           transfer mesh ' + ' '.join('%5.0f' % x for x in r['tr_mesh']))
                print('           transfer card ' + ' '.join('%5.0f' % x for x in r['tr_card']))
    if os.environ.get('JSON'):
        json.dump(out, open(os.environ['JSON'], 'w'), indent=1)
