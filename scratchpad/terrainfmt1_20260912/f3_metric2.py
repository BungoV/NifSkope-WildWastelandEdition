"""Item 3, the render proof: the colour distance to VANILLA's own render.

The reference is vanilla's shipped BTR drawn with vanilla's shipped sheets --
Bethesda's chunk, not a reconstruction of it -- at a PINNED orthographic camera
(centre 8192,8192,8938, half-width 8600, 1358x1024, upp 12.665685 read back out
of release/ww_camera_pin.log), so every arm is framed by the same numbers rather
than by an auto-fit that could move with the mesh bounds.

The mask is the pixels covered in EVERY arm, so no arm can win by covering less.
The statistic is the mean Euclidean RGB distance to the reference in levels.

BEFORE = our chunk as the generator ships it today. It is the floor: an arm that
does not beat it changed nothing worth having.
"""
import json
import os
import numpy as np
from PIL import Image

H = os.path.dirname(os.path.abspath(__file__))
ARMS = [
    ('vanilla mesh + vanilla sheets (the reference)', 'p_vanmesh_van'),
    ('BEFORE: our mesh + vanilla sheets', 'p_ourmesh_van'),
    ('our mesh + our sheets, legacy container', 'p_ourmesh_ourleg'),
    ('our mesh + our sheets, --sheet-format vanilla', 'p_ourmesh_ourvan'),
    ('our mesh + our sheets, --msn-cache', 'p_ourmesh_cache'),
    ('AFTER: our mesh --no-terrain-identity + vanilla sheets', 'p_lit_notid'),
]
FLAT = [('vanilla mesh', 'p_flat_vanmesh'),
        ('our mesh, shipped default', 'p_flat_ourmesh'),
        ('our mesh, --no-terrain-identity', 'p_flat_notid')]


def L(t):
    return np.asarray(Image.open(os.path.join(H, 'images', t + '.png'))
                      .convert('RGB')).astype(np.float64)


imgs = {t: L(t) for _, t in ARMS}
bg = imgs['p_vanmesh_van'][0, 0]
mask = np.ones(imgs['p_vanmesh_van'].shape[:2], bool)
for t in imgs:
    mask &= (np.abs(imgs[t] - bg).sum(axis=2) > 1e-9)
ref = imgs['p_vanmesh_van']
out = {'frame': list(ref.shape[:2][::-1]), 'upp': 12.665685,
       'mask_pixels': int(mask.sum()), 'arms': {}}
print('mask %d px  (%.1f%% of the frame)' % (mask.sum(), 100 * mask.mean()))
print('%-56s %8s %8s %8s   %s' % ('arm', 'mean', 'p95', 'max', 'terrain RGB mean'))
for name, t in ARMS:
    d = np.sqrt(((imgs[t] - ref) ** 2).sum(axis=2))[mask]
    rgb = [float(imgs[t][:, :, k][mask].mean()) for k in range(3)]
    out['arms'][name] = {'mean_dist': float(d.mean()),
                         'p95_dist': float(np.percentile(d, 95)),
                         'max_dist': float(d.max()), 'rgb_mean': rgb}
    print('%-56s %8.2f %8.2f %8.2f   %s'
          % (name, d.mean(), np.percentile(d, 95), d.max(), np.round(rgb, 1)))
out['flat_vertex_colour'] = {}
print('\nWW_RENDER_FLAT (vertex colours, no texture, no lighting):')
for name, t in FLAT:
    a = L(t)
    rgb = [float(a[:, :, k][mask].mean()) for k in range(3)]
    sd = [float(a[:, :, k][mask].std()) for k in range(3)]
    out['flat_vertex_colour'][name] = {'rgb_mean': rgb, 'rgb_sd': sd}
    print('  %-36s RGB mean %s  SD %s' % (name, np.round(rgb, 1), np.round(sd, 1)))
json.dump(out, open(os.path.join(H, 'f3_render_pinned.json'), 'w'), indent=1)
