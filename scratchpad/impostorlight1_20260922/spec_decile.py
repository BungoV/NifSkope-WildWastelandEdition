# Which part of the mesh's top luma decile is SPECULAR (stage 5 - stage 4 on the
# mesh), and does the card's transfer rise against the mesh's spec-free luma?
import sys, glob, os
import numpy as np
from PIL import Image
BG = np.array([43, 45, 49], float); W = np.array([0.2126, 0.7152, 0.0722])
img = lambda p: np.asarray(Image.open(p).convert('RGB')).astype(float)
cov = lambda a: np.abs(a - BG).sum(-1) > 12
D = 'diag'
for t in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4'):
    L6, S, L4, C6, C4 = [], [], [], [], []
    for m1 in sorted(glob.glob('%s/s1_mv/%s/*_mesh.png' % (D, t))):
        v = os.path.basename(m1)[:-9]
        k = cov(img(m1)) & cov(img(m1.replace('_mesh', '_card')))
        g = lambda r, h: img('%s/%s/%s/%s_%s.png' % (D, r, t, v, h))[k] @ W
        L6.append(g('s6', 'mesh')); S.append(g('s5', 'mesh') - g('s4', 'mesh')); L4.append(g('s4', 'mesh'))
        C6.append(g('final/s6', 'card')); C4.append(g('fix_oren1/s4', 'card'))
    L6, S, L4, C6, C4 = map(np.concatenate, (L6, S, L4, C6, C4))
    def dec(ref, val):
        q = np.percentile(ref, np.linspace(0, 100, 11)); i = np.clip(np.searchsorted(q, ref, side='right') - 1, 0, 9)
        return [val[i == j].mean() for j in range(10)]
    print('%-9s spec (s5-s4 mesh) by s6 decile: %s' % (t, ' '.join('%5.1f' % x for x in dec(L6, S))))
    print('%-9s card s4 by MESH s4 decile     : %s' % (t, ' '.join('%5.1f' % x for x in dec(L4, C4))))
