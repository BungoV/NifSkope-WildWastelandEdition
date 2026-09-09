"""Gate: bcnp.py must decode the _msn normal tiles exactly as dds.py does.

The diffuse gate (rec_decode_check.py) proved the vectorised path on the
DIFFUSE tiles.  The _msn tiles are a different corpus with different block
content, and one of the two decoders could still diverge on a code path the
diffuse never exercised (BC1 punch-through blocks, degenerate endpoints).  So
the gate is re-run here before any feature is believed.

It also reports the ALPHA range.  bcnp decodes RGB only; if the _msn packed a
normal component into alpha the way many BC3 normal maps do, the stated
channel convention (R=X east, G=up, B=Y north) would be wrong and every
feature below it worthless.  So alpha is checked, not assumed.
"""
import os
import sys

import numpy as np

from dds import DDS
from bcnp import decode_rgb

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
CASES = ['Commonwealth.4.0.0_msn.DDS', 'Commonwealth.4.-40.-40_msn.DDS',
         'Commonwealth.4.60.60_msn.DDS', 'Commonwealth.4.16.-24_msn.DDS']


def main():
    bad = 0
    seen = 0
    for name in CASES:
        p = os.path.join(VAN, name)
        if not os.path.exists(p):
            print('SKIP (absent) %s' % name)
            continue
        d = DDS(p)
        w, h, flat = d.decode(0)
        ref = np.frombuffer(bytes(flat), dtype=np.uint8).reshape(h, w, 4)
        got = decode_rgb(d, 0)
        diff = ref[:, :, :3].astype(int) - got.astype(int)
        n = int((diff != 0).sum())
        a = ref[:, :, 3]
        print('%-32s %s %dx%d mips=%d  mismatching channels %d  maxabs %d'
              % (name, d.fmt, w, h, d.mips, n, int(np.abs(diff).max())))
        print('    alpha min %d max %d mean %.2f   R mean %.1f  G mean %.1f  B mean %.1f'
              % (a.min(), a.max(), a.mean(),
                 got[:, :, 0].mean(), got[:, :, 1].mean(), got[:, :, 2].mean()))
        bad += n
        seen += 1
    print('GATE decode: %s (%d tiles)'
          % ('PASS' if bad == 0 and seen >= 2 else 'FAIL', seen))
    return 0 if bad == 0 and seen >= 2 else 1


if __name__ == '__main__':
    sys.exit(main())
