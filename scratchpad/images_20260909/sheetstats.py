"""Decode both sides' shipped/generated sheets and print what each MUST look
like before anything is rendered (nifskope-ww-vanilla-compare step 2)."""
import os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'mountains_20260907'))
from dds import DDS
from bcnp import decode_rgb

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
GEN = os.path.join(HERE, 'gen')
TILES = [(4, 'Commonwealth.4.28.24'), (8, 'Commonwealth.8.24.24'),
         (16, 'Commonwealth.16.16.16'), (32, 'Commonwealth.32.0.0')]

print('%-22s %-8s %-6s %-24s %-7s %-7s %-7s' %
      ('tile', 'side', 'sheet', 'mean RGB', 'lumSD', 'colour', 'relief'))
for dim, stem in TILES:
    for side, root in (('vanilla', VAN),
                       ('ours', os.path.join(GEN, 'ours%d' % dim, 'textures', 'terrain', 'Commonwealth'))):
        for kind, suf in (('diffuse', ''), ('_msn', '_msn')):
            p = os.path.join(root, stem + suf + '.DDS')
            if not os.path.exists(p):
                print('%-22s %-8s %-6s MISSING' % (stem, side, kind)); continue
            d = DDS(p)
            rgb = decode_rgb(d, 0).astype(np.float64)
            lum = rgb.mean(2)
            colour = float((rgb.max(2) - rgb.min(2)).mean())
            if suf == '_msn':
                n = rgb / 255.0 * 2.0 - 1.0
                up = np.clip(n[:, :, 1], -1, 1)
                relief = float(np.degrees(np.arccos(up)).mean())
            else:
                relief = float('nan')
            print('%-22s %-8s %-6s %-24s %-7.2f %-7.2f %-7.2f' %
                  (stem, side, kind,
                   '%.1f, %.1f, %.1f' % tuple(rgb.reshape(-1, 3).mean(0)),
                   lum.std(), colour, relief))
