"""Gate: the whole-tile vectorised feature path must equal a naive per-quadrant one.

tile_features() reshapes a 512x512 tile into 8x8 quadrants and reduces all 35
per-texel maps at once.  A single transposed axis in that reshape would silently
attribute one cell's texture to its neighbour and every downstream conclusion
would be about the wrong cell.  So the means, stds and the roughness scalar are
recomputed here the slow, obvious way for a few quadrants and compared.

The neighbourhood operators (laplacian, box residuals, differences) are NOT
re-checked against a per-quadrant version because they deliberately differ: the
real path lets a quadrant's edge texels see the adjacent quadrant.  Only the
pointwise statistics can be compared exactly.
"""
import os
import sys

import numpy as np

from dds import DDS
from bcnp import decode_rgb
from msn_features import VAN, QP, tile_features, NAMES

CASES = ['Commonwealth.4.0.0_msn.DDS', 'Commonwealth.4.-40.-40_msn.DDS']


def main():
    worst = 0.0
    for name in CASES:
        p = os.path.join(VAN, name)
        if not os.path.exists(p):
            print('SKIP %s' % name)
            continue
        img = decode_rgb(DDS(p), 0)
        f = tile_features(img)
        n = (img.astype(np.float64) / 255.0 - 0.5) * 2.0
        for qr in (0, 3, 7):
            for qc in (0, 5, 7):
                sub = n[qr * QP:(qr + 1) * QP, qc * QP:(qc + 1) * QP]
                ref = list(sub.reshape(-1, 3).mean(0)) + list(sub.reshape(-1, 3).std(0))
                mn = sub.reshape(-1, 3).mean(0)
                mn = mn / np.linalg.norm(mn)
                u = sub / np.linalg.norm(sub, axis=2, keepdims=True)
                ref.append(np.arccos(np.clip((u * mn).sum(2), -1, 1)).mean())
                ref.append(np.hypot(sub[:, :, 0], sub[:, :, 2]).mean())
                got = [f[qr, qc, i] for i in (0, 1, 2, 3, 4, 5, 21, 24)]
                d = float(np.abs(np.array(ref) - np.array(got)).max())
                worst = max(worst, d)
        print('%-32s worst |naive - vectorised| over 9 quadrants = %.3e' % (name, worst))

    ok = worst < 2e-6
    print('GATE selftest: %s (tolerance 2e-6, float32 storage)'
          % ('PASS' if ok else 'FAIL'))
    print('feature count %d' % len(NAMES))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
