"""Item 3: the colour distance between each arm's render and vanilla's render.

The mask is the terrain, not the frame: the background is one constant colour
(read out of the frame's own top-left corner, never assumed), and a pixel counts
only if it is not that colour in EVERY arm -- so the silhouette is the same set
of pixels for every number below and an arm cannot win by covering less.

The statistic is the mean Euclidean distance in RGB levels between the arm and
the `van` render over that mask, plus the 95th percentile, plus the count of
pixels that differ at all. `van` against itself is 0 by construction; the FLOOR
for "did the switch change anything" is the BEFORE arm's own number.
"""
import json
import os
import numpy as np
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__))
IM = os.path.join(H, 'images')
ARMS = ['van', 'ourleg', 'ourvan', 'cache', 'mix']


def load(tag, arm):
    return np.asarray(Image.open(os.path.join(IM, 'r_%s_%s.png' % (tag, arm)))
                      .convert('RGB')).astype(np.float64)


out = {}
for tag in ('top',):
    imgs = {a: load(tag, a) for a in ARMS}
    bg = imgs['van'][0, 0]
    mask = np.ones(imgs['van'].shape[:2], bool)
    for a in ARMS:
        mask &= (np.abs(imgs[a] - bg).sum(axis=2) > 1e-9)
    ref = imgs['van']
    rec = {'frame': list(imgs['van'].shape[:2][::-1]),
           'background_rgb': [float(v) for v in bg],
           'terrain_pixels': int(mask.sum()),
           'terrain_fraction': float(mask.mean())}
    for a in ARMS:
        d = np.sqrt(((imgs[a] - ref) ** 2).sum(axis=2))[mask]
        rec[a] = {'mean_dist': float(d.mean()),
                  'p95_dist': float(np.percentile(d, 95)),
                  'max_dist': float(d.max()),
                  'pixels_differing': int((d > 0).sum()),
                  'rgb_mean': [float(imgs[a][:, :, k][mask].mean()) for k in range(3)]}
    # the pair the brief asks about: before (ourleg) -> after (ourvan)
    d = np.sqrt(((imgs['ourvan'] - imgs['ourleg']) ** 2).sum(axis=2))[mask]
    rec['ourleg_vs_ourvan'] = {'mean_dist': float(d.mean()),
                               'max_dist': float(d.max()),
                               'pixels_differing': int((d > 0).sum())}
    d = np.sqrt(((imgs['cache'] - imgs['ourleg']) ** 2).sum(axis=2))[mask]
    rec['ourleg_vs_cache'] = {'mean_dist': float(d.mean()),
                              'max_dist': float(d.max()),
                              'pixels_differing': int((d > 0).sum())}
    out[tag] = rec

print(json.dumps(out, indent=1))
json.dump(out, open(os.path.join(H, 'f3_render.json'), 'w'), indent=1)
