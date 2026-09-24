# IMPOSTORFIN1 -- how bright is the card against the mesh it replaces?
# For every bake-direction grab: mean luminance (Rec.709, 0..255) over the
# INTERSECTION of the mesh mask and the card mask (so coverage cannot pose as
# brightness), for the mesh grab and the card grab. Ratio card/mesh per set.
#   python tone.py GRABROOT [GRABROOT ...]
import sys, glob, os
import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], float)
W = np.array([0.2126, 0.7152, 0.0722])


def load(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(float)
    return a, np.abs(a - BG).sum(-1) > 12


for root in sys.argv[1:]:
    for t in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4'):
        ms = sorted(glob.glob('%s/%s/*_mesh.png' % (root, t)))
        lm, lc = [], []
        for m in ms:
            c = m.replace('_mesh.png', '_card.png')
            if not os.path.exists(c):
                continue
            am, mm = load(m); ac, mc = load(c)
            k = mm & mc
            if k.sum() < 50:
                continue
            lm.append((am[k] @ W).mean()); lc.append((ac[k] @ W).mean())
        if lm:
            print('%-44s %-9s views %2d  mesh %6.1f  card %6.1f  card/mesh %.3f' % (
                os.path.basename(root), t, len(lm), np.mean(lm), np.mean(lc), np.mean(lc) / np.mean(lm)))
