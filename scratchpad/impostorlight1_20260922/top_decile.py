# What makes the mesh's TOP luma decile: albedo (stage 2) or N.L (the view-space
# normal's z under the headlight, stage 1)? Both halves, by the mesh's s6 decile.
import glob, os
import numpy as np
from PIL import Image
BG = np.array([43, 45, 49], float); W = np.array([0.2126, 0.7152, 0.0722])
img = lambda p: np.asarray(Image.open(p).convert('RGB')).astype(float)
cov = lambda a: np.abs(a - BG).sum(-1) > 12
D = 'diag'
for t in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4'):
    acc = {k: [] for k in ('L6', 'am', 'ac', 'zm', 'zc')}
    for m1 in sorted(glob.glob('%s/s1_mv/%s/*_mesh.png' % (D, t))):
        v = os.path.basename(m1)[:-9]; c1 = m1.replace('_mesh', '_card')
        k = cov(img(m1)) & cov(img(c1))
        g = lambda r, h: img('%s/%s/%s/%s_%s.png' % (D, r, t, v, h))[k]
        acc['L6'].append(g('s6', 'mesh') @ W)
        acc['am'].append(g('s2', 'mesh') @ W); acc['ac'].append(g('s2', 'card') @ W)
        acc['zm'].append(img(m1)[k][:, 2] / 127.5 - 1); acc['zc'].append(img(c1)[k][:, 2] / 127.5 - 1)
    a = {k: np.concatenate(v) for k, v in acc.items()}
    q = np.percentile(a['L6'], np.linspace(0, 100, 11)); i = np.clip(np.searchsorted(q, a['L6'], side='right') - 1, 0, 9)
    row = lambda k, f: ' '.join(f % a[k][i == j].mean() for j in range(10))
    print('%-9s albedo mesh %s' % (t, row('am', '%5.0f')))
    print('%-9s albedo card %s' % (t, row('ac', '%5.0f')))
    print('%-9s NdotL  mesh %s' % (t, row('zm', '%5.2f')))
    print('%-9s NdotL  card %s' % (t, row('zc', '%5.2f')))
