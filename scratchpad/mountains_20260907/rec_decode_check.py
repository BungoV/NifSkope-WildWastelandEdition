"""Gate: the vectorised decoder in bcnp.py must agree with dds.py exactly.

If this does not print PASS, nothing downstream of it is worth reading.
"""
import os
import numpy as np

from dds import DDS
from bcnp import decode_rgb

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
CASES = [('Commonwealth.4.0.0.DDS', 2), ('Commonwealth.4.0.0.DDS', 0),
         ('Commonwealth.4.-40.-40.DDS', 2), ('Commonwealth.4.60.60.DDS', 3),
         ('Commonwealth.4.-88.28.DDS', 2), ('Commonwealth.4.16.-24.DDS', 1)]

bad = 0
for name, mip in CASES:
    p = os.path.join(VAN, name)
    if not os.path.exists(p):
        print('SKIP (absent) %s' % name)
        continue
    d = DDS(p)
    w, h, flat = d.decode(mip)
    ref = np.frombuffer(bytes(flat), dtype=np.uint8).reshape(h, w, 4)[:, :, :3]
    got = decode_rgb(d, mip)
    diff = (ref.astype(int) - got.astype(int))
    n = int((diff != 0).sum())
    print('%-28s mip%d %4dx%-4d  mismatching channels %d  maxabs %d'
          % (name, mip, w, h, n, int(np.abs(diff).max())))
    bad += n
    a = np.frombuffer(bytes(flat), dtype=np.uint8).reshape(h, w, 4)[:, :, 3]
    print('    alpha: min %d max %d  (brief says constant 255)' % (a.min(), a.max()))

print('PASS' if bad == 0 else 'FAIL %d mismatching channels' % bad)
