"""Crop the SAME texel region out of a terrain _msn sheet and blow it up with
nearest-neighbour, so the raw bytes are visible with no filtering and no
lighting at all.

  python msn_crop.py <in.DDS> <out.png> <mip> <size> <scale>

The crop is CENTRED on the mip, so the same call on two sheets of the same
tile takes the same texels of the same tile -- no claim is made here about
which cell a texel belongs to, because that mapping has not been calibrated
against the ESM in this lane.

FO4 terrain _msn is a MODEL-SPACE normal map and up is GREEN.  What is drawn
here is the stored RGB.  Note that the renderer's own path throws the stored
blue away and recomputes it from R and G
(res/shaders/fo4_default.frag: normal.b = sqrt(1 - dot(normal.rg, normal.rg))),
so two sheets that differ only in blue would render the same.
"""
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
from dds import DDS          # noqa: E402
from bcnp import decode_rgb  # noqa: E402


def main():
    src, out = sys.argv[1], sys.argv[2]
    mip = int(sys.argv[3])
    size = int(sys.argv[4])
    scale = int(sys.argv[5])

    d = DDS(src)
    a = decode_rgb(d, mip)
    h, w, _ = a.shape
    x0, y0 = (w - size) // 2, (h - size) // 2
    crop = a[y0:y0 + size, x0:x0 + size]
    img = Image.fromarray(crop, 'RGB').resize(
        (size * scale, size * scale), Image.NEAREST)
    img.save(out)
    f = crop.reshape(-1, 3).astype(float)
    print('%s  %s mip%d %dx%d  crop (%d,%d)+%d  x%d  meanRGB %.1f,%.1f,%.1f  '
          'SD %.2f,%.2f,%.2f  distinct %d'
          % (out, d.fmt, mip, w, h, x0, y0, size, scale,
             f[:, 0].mean(), f[:, 1].mean(), f[:, 2].mean(),
             f[:, 0].std(), f[:, 1].std(), f[:, 2].std(),
             len(np.unique(crop.reshape(-1, 3), axis=0))))


if __name__ == '__main__':
    main()
