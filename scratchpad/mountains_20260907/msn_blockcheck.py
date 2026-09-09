"""Is the _msn's fine detail content, or the seam between BC blocks?

The high-pass features are the whole point of this lane, and mean |3x3
laplacian| of nx measures 0.62 on a channel whose entire range is [-1, 1].
Either the normal map really does carry violent texel-scale material detail, or
the BC3 encoder is manufacturing it.  A block encoder quantises its two
endpoints per 4x4 block, so its error is discontinuous exactly at block
boundaries and smooth inside them.  Real content is not.

Test: the mean absolute first difference ACROSS a block boundary (columns 3->4,
7->8, ...) divided by the same quantity WITHIN blocks.  A ratio near 1 means the
detail is content.  A ratio well above 1 means a share of it is the encoder.

The DIFFUSE tiles are the control.  They are the same format, the same
compressor and the same bake, and they unarguably carry real material texture,
so their ratio is what "real content, block compressed" looks like on this
corpus.  Anything the _msn does beyond that is the encoder, not the gravel.
"""
import os
import sys

import numpy as np

from dds import DDS
from bcnp import decode_rgb
from msn_features import VAN

SAMPLE = ['0.0', '-40.-40', '60.60', '16.-24', '-88.28', '-20.40',
          '32.-52', '-64.-8', '8.64', '44.20']


def ratios(a):
    """(across-boundary, within-block) mean |first difference|, h and v."""
    dh = np.abs(a[:, 1:] - a[:, :-1])
    dv = np.abs(a[1:, :] - a[:-1, :])
    j = np.arange(dh.shape[1])
    i = np.arange(dv.shape[0])
    bh, wh = dh[:, j % 4 == 3], dh[:, j % 4 != 3]
    bv, wv = dv[i % 4 == 3], dv[i % 4 != 3]
    return bh.mean(), wh.mean(), bv.mean(), wv.mean()


def main():
    acc = {}
    nt = 0
    for s in SAMPLE:
        pm = os.path.join(VAN, 'Commonwealth.4.%s_msn.DDS' % s)
        pd = os.path.join(VAN, 'Commonwealth.4.%s.DDS' % s)
        if not (os.path.exists(pm) and os.path.exists(pd)):
            continue
        m = (decode_rgb(DDS(pm), 0).astype(np.float64) / 255.0 - 0.5) * 2.0
        d = decode_rgb(DDS(pd), 0).astype(np.float64) / 255.0
        lum = 0.2126 * d[:, :, 0] + 0.7152 * d[:, :, 1] + 0.0722 * d[:, :, 2]
        for nm, a in (('msn nx', m[:, :, 0]), ('msn ny', m[:, :, 1]),
                      ('msn nz', m[:, :, 2]), ('diffuse lum (control)', lum)):
            v = np.array(ratios(a))
            acc[nm] = acc.get(nm, np.zeros(4)) + v
        nt += 1
    print('mean |first difference| at mip 0, %d tiles' % nt)
    print('%-22s %10s %10s %8s | %10s %10s %8s'
          % ('channel', 'h across', 'h within', 'ratio', 'v across', 'v within', 'ratio'))
    for nm in ('msn nx', 'msn ny', 'msn nz', 'diffuse lum (control)'):
        bh, wh, bv, wv = acc[nm] / nt
        print('%-22s %10.5f %10.5f %8.2f | %10.5f %10.5f %8.2f'
              % (nm, bh, wh, bh / max(wh, 1e-12), bv, wv, bv / max(wv, 1e-12)))
    print()
    print('ratio ~1 = the fine detail is content; ratio >> the diffuse control = '
          'a share of it is the block encoder')


if __name__ == '__main__':
    sys.exit(main())
