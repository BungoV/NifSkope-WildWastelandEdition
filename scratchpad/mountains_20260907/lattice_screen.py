"""Lane LATTICE -- the period of the pattern ON SCREEN, with a control.

The four swap frames share a camera, a mesh (within a pair) and a diffuse, so
the only thing that changes between A and B is the `_msn`.  Frame B is
therefore the CONTROL for frame A: whatever autocorrelation peak A has and B
does not is the lattice, and its lag is the on-screen period.

The patch is taken on the FRONT face where the ground is least foreshortened,
and the same patch is used on every frame.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, 'images')


def lum(path, box):
    a = np.asarray(Image.open(path).convert('RGB'), dtype=np.float64)
    x0, y0, x1, y1 = box
    a = a[y0:y1, x0:x1]
    return 0.2126 * a[:, :, 0] + 0.7152 * a[:, :, 1] + 0.0722 * a[:, :, 2]


def boxmean(a, k):
    r = k // 2
    p = np.pad(a, r, mode='edge')
    cs = np.cumsum(p, axis=0)
    cs = np.vstack([np.zeros((1, cs.shape[1])), cs])
    s = cs[k:] - cs[:-k]
    cs = np.cumsum(s, axis=1)
    cs = np.hstack([np.zeros((cs.shape[0], 1)), cs])
    return (cs[:, k:] - cs[:, :-k]) / float(k * k)


def acf(f, maxlag=40):
    """Normalised 2-D autocorrelation, rows and columns, of the high-passed
    patch.  Returned as two 1-D profiles."""
    g = f - boxmean(f, 15)
    g -= g.mean()
    n0, n1 = g.shape
    w = np.outer(np.hanning(n0), np.hanning(n1))
    g = g * w
    F = np.fft.fft2(g)
    c = np.fft.ifft2(np.abs(F) ** 2).real
    c /= c[0, 0]
    return c[0, :maxlag + 1], c[:maxlag + 1, 0]


def main(argv):
    box = (250, 560, 700, 810)          # the near, least foreshortened face
    if len(argv) >= 4:
        box = tuple(int(v) for v in argv[:4])
    frames = [('A ours mesh + OURS msn', 'swap_A_ourmesh_ourmsn.png'),
              ('B ours mesh + VANILLA msn', 'swap_B_ourmesh_vanmsn.png'),
              ('C van mesh + OURS msn', 'swap_C_vanmesh_ourmsn.png'),
              ('D van mesh + VANILLA msn', 'swap_D_vanmesh_vanmsn.png')]
    print('patch %s' % (box,))
    lags = list(range(2, 41))
    print('%-28s %s' % ('frame', ' '.join('%5d' % l for l in lags[:18])))
    for lab, f in frames:
        cx, cy = acf(lum(os.path.join(IMG, f), box))
        print('%-28s %s   (x)' % (lab, ' '.join('%5.2f' % cx[l] for l in lags[:18])))
    print()
    print('peaks (lag, value) of the x profile, lags 3..40:')
    for lab, f in frames:
        cx, cy = acf(lum(os.path.join(IMG, f), box))
        px = [(l, cx[l]) for l in range(3, 40)
              if cx[l] > cx[l - 1] and cx[l] > cx[l + 1]]
        py = [(l, cy[l]) for l in range(3, 40)
              if cy[l] > cy[l - 1] and cy[l] > cy[l + 1]]
        px.sort(key=lambda t: -t[1])
        py.sort(key=lambda t: -t[1])
        print('  %-28s x %s | y %s' % (
            lab,
            ' '.join('lag %2d %.3f' % t for t in px[:3]),
            ' '.join('lag %2d %.3f' % t for t in py[:3])))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
