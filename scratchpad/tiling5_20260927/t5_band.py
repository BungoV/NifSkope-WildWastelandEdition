"""TILING5 -- the macro amplitude, set by measurement against vanilla, per band.

On a contiguous 3x3-sheet mosaic (Boston -8,-12..3,-1, or the rural camera) the
luminance (log) and the two opponent-colour axes are reduced to 32-texel blocks
(1024 units, ~15 m).  Two bands, the two the macro field lives in:

  band A  ~60 m .. ~234 m : SD of (box r=2 blocks) - (box r=8 blocks), inside the mosaic
  band B  ~234 m .. ~700 m: SD of the nine sheet means

for vanilla's sheets and for an arm's.  The LICENCE per band is what vanilla has
that the arm does not:  L = sqrt(max(0, van^2 - arm^2)).  The macro field's own
SD in the same band (the SAME reduction applied to lodgenMacroField evaluated on
the mosaic's block centres, averaged over 64 mosaic-sized windows across the map)
turns a licence into an amplitude:  A = L / SD_band(field).

Brightness is measured on log luminance (the gain is exp(Ab*F), so the field
enters log-lum linearly).  Hue and saturation act on chroma: a rotation by
theta moves an opponent vector of length C by C*theta, a scale k by C*(k-1); so
their licences are read on the opponent axes divided by the mosaic's mean
chroma magnitude.

    usage: python t5_band.py <arm> <mosaic: boston | x0,y0>   (x0,y0 = the SW chunk)
"""
import json
import os
import sys

import numpy as np

import t5_gates as T
import t5_hexcorr as H

BLK = 32


def mosaic(arm, x0, y0, boston):
    rows = []
    for j in (2, 1, 0):                       # north row first, as the sheets store it
        row = []
        for i in (0, 1, 2):
            cx, cy = x0 + 4 * i, y0 + 4 * j
            if arm == 'van':
                im = T.van_rgb(cx, cy)
            else:
                p = T.sheet(arm, cx, cy, boston)
                if not boston:
                    p = os.path.join(T.HERE, 'out', arm, 'r_%d_%d_%d_%d' % (x0, y0, x0 + 11, y0 + 11),
                                     'tex', 'Commonwealth.4.%d.%d.DDS' % (cx, cy))
                im = T.rgb(p)
            row.append(im)
        rows.append(np.concatenate(row, 1))
    return np.concatenate(rows, 0)


def blocks(img):
    h, w = img.shape[:2]
    return img.reshape(h // BLK, BLK, w // BLK, BLK, -1).mean(axis=(1, 3))


def box(a, r):
    # separable box, edge-clamped, on a 2-D grid
    p = np.pad(a, r, mode='edge')
    c = np.cumsum(np.cumsum(p, 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    n = 2 * r + 1
    return (c[n:, n:] - c[:-n, n:] - c[n:, :-n] + c[:-n, :-n]) / (n * n)


def bands(g):
    """g: blocks grid (48x48).  Returns (band A SD, band B SD)."""
    a = box(g, 2) - box(g, 8)
    m = 8
    A = float(a[m:-m, m:-m].std())
    s = g.shape[0] // 3
    means = [g[j * s:(j + 1) * s, i * s:(i + 1) * s].mean() for j in range(3) for i in range(3)]
    return A, float(np.std(means))


def channels(img):
    lum = 0.2126 * img[:, :, 0] + 0.7152 * img[:, :, 1] + 0.0722 * img[:, :, 2]
    o1 = img[:, :, 0] - img[:, :, 1]
    o2 = 0.5 * (img[:, :, 0] + img[:, :, 1]) - img[:, :, 2]
    b = blocks(np.stack([lum, o1, o2], 2))
    return np.log(np.maximum(b[:, :, 0], 1.0)), b[:, :, 1], b[:, :, 2]


def measure(img):
    ll, o1, o2 = channels(img)
    C = float(np.mean(np.hypot(o1, o2)))
    lA, lB = bands(ll)
    a1, b1 = bands(o1)
    a2, b2 = bands(o2)
    return dict(logL_A=lA, logL_B=lB, chroma_A=float(np.hypot(a1, a2)), chroma_B=float(np.hypot(b1, b2)),
                Cmean=C)


def field_bands():
    """The macro field's own band SDs, averaged over 64 mosaic windows."""
    rng = np.random.default_rng(7)
    acc = []
    n = 48
    span = 3 * 4 * 4096.0
    for _ in range(64):
        x0 = rng.uniform(-80, 60) * 4096.0
        y0 = rng.uniform(-80, 60) * 4096.0
        yy, xx = np.mgrid[0:n, 0:n]
        wx = x0 + (xx + 0.5) / n * span
        wy = y0 + (1.0 - (yy + 0.5) / n) * span
        acc.append([bands(H.field(wx, wy, ch)) for ch in range(3)])
    acc = np.array(acc)            # 64 x 3 x 2
    return np.sqrt((acc ** 2).mean(0))


def main():
    arm, where = sys.argv[1], sys.argv[2]
    if where == 'boston':
        x0, y0, boston = -8, -12, True
    else:
        x0, y0 = (int(v) for v in where.split(','))
        boston = False
    van = measure(mosaic('van', x0, y0, boston))
    got = measure(mosaic(arm, x0, y0, boston))
    fb = field_bands()
    out = dict(where=where, arm=arm, vanilla=van, arm_meas=got, field_band_sd=fb.tolist())
    print('mosaic %s  arm %s' % (where, arm))
    print('   %-10s %9s %9s %9s' % ('', 'vanilla', arm[:9], 'licence'))
    lic = {}
    for k in ('logL_A', 'logL_B', 'chroma_A', 'chroma_B'):
        L = float(np.sqrt(max(0.0, van[k] ** 2 - got[k] ** 2)))
        lic[k] = L
        print('   %-10s %9.4f %9.4f %9.4f' % (k, van[k], got[k], L))
    print('   %-10s %9.3f %9.3f' % ('Cmean', van['Cmean'], got['Cmean']))
    print('field band SD (A, B): bright %.3f %.3f  hue %.3f %.3f  sat %.3f %.3f'
          % tuple(fb.ravel()))
    Ab = min(lic['logL_A'] / fb[0][0], lic['logL_B'] / fb[0][1])
    Ah = min(lic['chroma_A'] / (got['Cmean'] * fb[1][0]), lic['chroma_B'] / (got['Cmean'] * fb[1][1]))
    # the saturation scale k = 1 + As*0.5*(1+F): its varying part is As*0.5*F
    As = min(lic['chroma_A'] / (got['Cmean'] * 0.5 * fb[2][0]), lic['chroma_B'] / (got['Cmean'] * 0.5 * fb[2][1]))
    print('AMPLITUDE CEILINGS (each alone): Ab %.4f  Ah %.4f rad  As %.4f' % (Ab, Ah, As))
    out.update(licence=lic, ceilings=dict(Ab=Ab, Ah=Ah, As=As))
    with open(os.path.join(T.HERE, 'logs', 'band_%s_%s.json' % (arm, where.replace(',', '_'))), 'w',
              newline='\n') as f:
        json.dump(out, f, indent=1)


if __name__ == '__main__':
    main()
