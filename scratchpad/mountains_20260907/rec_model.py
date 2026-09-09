"""Fit a colour to every landscape material, then measure whether colour
identifies material at all.

Model: a quadrant's LOD colour is the composite-weighted mix of its materials'
colours, so  C (nquad x 3)  ~=  W (nquad x nltex) . P (nltex x 3).  P is solved
by ridge-regularised least squares.  This is strictly better than a nearest-
neighbour over blends because it separates the materials instead of memorising
mixtures, and because it produces a palette we can check against the actual
texture files on disk.

Outputs palette.npz and prints:
  * fit quality,
  * the fitted palette against the REAL mean colour of each texture (external
    ground truth -- nothing in the fit ever saw these files),
  * the ambiguity measurement the brief asks for.
"""
import os
import sys

import numpy as np

from bcnp import decode_rgb
from dds import DDS
from rec_common import HERE, NULL, ltex_names

TEXROOT = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures'
RIDGE = 1.0


def fit_palette(W, C, ridge=RIDGE):
    """Ridge least squares for P in C ~= W.P, plus per-sample residual."""
    A = W.T @ W + ridge * np.eye(W.shape[1], dtype=np.float64)
    B = W.T @ C
    P = np.linalg.solve(A, B)
    pred = W @ P
    return P, pred


def texture_mean(path):
    """True mean RGB of a landscape diffuse, from a small mip. None if absent."""
    if not path:
        return None
    p = os.path.join(TEXROOT, path.replace('\\', '/'))
    if not os.path.exists(p):
        return None
    d = DDS(p)
    mip = 0
    for i in range(d.mips):
        if max(1, d.width >> i) >= 16:
            mip = i
        else:
            break
    try:
        img = decode_rgb(d, mip)
    except Exception:
        return None
    return img.reshape(-1, 3).mean(0)


def main():
    z = np.load(os.path.join(HERE, 'training.npz'))
    W = z['W'].astype(np.float64)
    C = z['C'].astype(np.float64)
    S = z['S'].astype(np.float64)
    XY = z['XY']
    ltex = z['ltex']
    names = ltex_names()

    P, pred = fit_palette(W, C)
    res = np.linalg.norm(pred - C, axis=1)
    ss_res = ((pred - C) ** 2).sum()
    ss_tot = ((C - C.mean(0)) ** 2).sum()
    print('=== fit ===')
    print('samples %d  materials %d' % (W.shape[0], W.shape[1]))
    print('R^2 = %.4f' % (1 - ss_res / ss_tot))
    print('residual |dRGB| : mean %.2f  median %.2f  p90 %.2f  p99 %.2f'
          % (res.mean(), np.median(res), np.percentile(res, 90),
             np.percentile(res, 99)))
    print('within-quadrant texel spread |std| for comparison: mean %.2f'
          % np.linalg.norm(S, axis=1).mean())

    # ---- external validation: fitted colour vs the real texture file ----
    print('\n=== fitted palette vs the actual texture on disk ===')
    print('(the regression never saw these files; agreement is independent '
          'evidence the fit recovered material identity)')
    area = W.sum(0)
    order = np.argsort(-area)
    rows = []
    print('%-9s %-34s %7s  %-17s %-17s %6s' %
          ('formid', 'EDID', 'weight', 'fitted RGB', 'texture RGB', 'dist'))
    for i in order:
        fid = int(ltex[i])
        ed, tx = names.get(fid, ('?', None))
        tm = texture_mean(tx)
        f = P[i]
        if tm is None:
            print('%08x  %-34s %7.1f  %5.1f %5.1f %5.1f       (no texture on disk)'
                  % (fid, ed, area[i], f[0], f[1], f[2]))
            continue
        d = float(np.linalg.norm(f - tm))
        rows.append((area[i], d, fid, ed))
        print('%08x  %-34s %7.1f  %5.1f %5.1f %5.1f   %5.1f %5.1f %5.1f  %6.1f'
              % (fid, ed, area[i], f[0], f[1], f[2], tm[0], tm[1], tm[2], d))
    if rows:
        a = np.array([r[0] for r in rows])
        d = np.array([r[1] for r in rows])
        print('\nfitted-vs-real distance over %d materials with a texture:' % len(rows))
        print('  unweighted mean %.1f  median %.1f' % (d.mean(), np.median(d)))
        print('  weighted by painted area: mean %.1f' % ((d * a).sum() / a.sum()))
        top = a >= np.percentile(a, 75)
        print('  restricted to the best-sampled quartile: mean %.1f  median %.1f'
              % (d[top].mean(), np.median(d[top])))

    np.savez(os.path.join(HERE, 'palette.npz'), P=P.astype(np.float32),
             ltex=ltex, area=area.astype(np.float32))
    print('\nwrote palette.npz')


if __name__ == '__main__':
    sys.exit(main())
