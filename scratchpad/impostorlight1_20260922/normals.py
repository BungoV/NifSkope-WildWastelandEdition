# IMPOSTORLIGHT1 -- the NORMALS measurement (brief job 1a).
# Input: a stage-1 grab root (diag_run.sh 1 ...): per view, <v>_mesh.png = the
# mesh's GEOMETRIC view-space normal (what the bake photographs) and
# <v>_card.png = the card's reconstructed normal taken to view space, both
# packed n*0.5+0.5. Over the INTERSECTION of the two silhouettes:
#   sign agreement per axis = share of pixels where sign(card) == sign(mesh),
#     counted only where |mesh component| > DEAD (0.15) so a component that is
#     honestly ~0 cannot vote;
#   mean angle error in degrees.
# A flipped axis shows as a sign agreement near 0% on that axis.
#   python normals.py ROOT [ROOT ...]   (prints one row per subject + ALL)
import sys, glob, os, json
import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], float)
DEAD = 0.15
SUBJ = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4', 'cube_n4')


def load(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(float)
    return a, np.abs(a - BG).sum(-1) > 12


def unpack(a):
    n = a / 255.0 * 2.0 - 1.0
    return n / np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)


def measure(root, t):
    ag = [0, 0, 0]; cnt = [0, 0, 0]; angs = []; px = 0; views = 0
    for m in sorted(glob.glob('%s/%s/*_mesh.png' % (root, t))):
        c = m.replace('_mesh.png', '_card.png')
        if not os.path.exists(c):
            continue
        am, mm = load(m); ac, mc = load(c)
        k = mm & mc
        if k.sum() < 50:
            continue
        views += 1
        nm = unpack(am[k]); nc = unpack(ac[k]); px += len(nm)
        for i in range(3):
            v = np.abs(nm[:, i]) > DEAD
            ag[i] += int((np.sign(nm[v, i]) == np.sign(nc[v, i])).sum()); cnt[i] += int(v.sum())
        angs.append(np.degrees(np.arccos(np.clip((nm * nc).sum(-1), -1, 1))))
    if not views:
        return None
    a = np.concatenate(angs)
    return dict(views=views, px=px, sx=ag[0] / max(1, cnt[0]), sy=ag[1] / max(1, cnt[1]),
                sz=ag[2] / max(1, cnt[2]), nx=cnt[0], ny=cnt[1], nz=cnt[2],
                ang=float(a.mean()), ang_med=float(np.median(a)), ang_p90=float(np.percentile(a, 90)))


if __name__ == '__main__':
    out = {}
    for root in sys.argv[1:]:
        for t in SUBJ:
            r = measure(root, t)
            if r is None:
                continue
            out['%s|%s' % (os.path.basename(root.rstrip('/')), t)] = r
            print('%-14s %-9s views %2d px %8d  sign x %5.1f%% y %5.1f%% z %5.1f%%  angle mean %5.1f med %5.1f p90 %5.1f' % (
                os.path.basename(root.rstrip('/')), t, r['views'], r['px'], 100 * r['sx'], 100 * r['sy'], 100 * r['sz'],
                r['ang'], r['ang_med'], r['ang_p90']))
    if os.environ.get('JSON'):
        json.dump(out, open(os.environ['JSON'], 'w'), indent=1)
