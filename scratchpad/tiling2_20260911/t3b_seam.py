"""TILING2 work item 1, part 2 -- two things t3_laws.py left unsaid.

1. THE RATIO. `vis` is an absolute amplitude, and our sheets carry more
   contrast than vanilla's, so the absolute number alone is not a fair
   comparison. The ratio vis/floor is "how far the repeat stands above this
   sheet's OWN broadband detail at that scale" and is dimensionless.

2. THE SEAM. `quadrant_hits` counts edge texels near a 64-texel line; that is a
   weak test at small N. The direct test of "a straight line on the quadrant
   grid" is the gradient across the line itself: for each of the seven interior
   vertical quadrant lines (x = 64,128..448) the mean |d/dx| ALONG that column,
   divided by the sheet's own mean |d/dx|, and the same for the seven rows.
   A sheet with no grid seam reads 1.0 on every one of the fourteen.

   python t3b_seam.py  -> logs/t3b_seam.txt
"""
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

OURS = {'t2024': (-20, 24), 't2020': (-20, 20)}


def binom_p(k, n, p):
    """P(X >= k) for X ~ Bin(n,p) -- exact, n is at most a few thousand."""
    if n == 0:
        return 1.0
    lg = math.lgamma
    terms = []
    for i in range(k, n + 1):
        terms.append(lg(n + 1) - lg(i + 1) - lg(n - i + 1)
                     + i * math.log(p) + (n - i) * math.log1p(-p))
    m = max(terms)
    return min(1.0, math.exp(m) * sum(math.exp(t - m) for t in terms))


def seam(L, quad=64):
    a = L.astype(np.float64)
    gx = np.abs(np.gradient(a, axis=1))
    gy = np.abs(np.gradient(a, axis=0))
    mx = gx[:, 2:-2].mean()
    my = gy[2:-2, :].mean()
    rx = [float(gx[:, k * quad].mean() / mx) for k in range(1, 512 // quad)]
    ry = [float(gy[k * quad, :].mean() / my) for k in range(1, 512 // quad)]
    r = rx + ry
    return max(r), float(np.mean(r)), r


def sheet_of(variant, tile):
    cx, cy = OURS[tile]
    return os.path.join(HERE, 'out', variant, tile, 'tex',
                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))


def main():
    J = json.load(open(os.path.join(HERE, 't3_laws.json')))
    L1 = ['TILING2 work item 1 part 2 -- the ratio, and the quadrant seam', '']
    hd = '%-26s %8s %8s %7s   %7s %7s   %s' % (
        'sheet', 'vis', 'floor', 'vis/fl', 'seamMax', 'seamAvg', 'hard on quad (p)')
    L1 += [hd, '-' * len(hd)]
    rat = []
    smx = []
    for r in J['vanilla']:
        L = S.lum(S.Dds(S.van_sheet(r['cx'], r['cy'])).level(0))
        mx, av, _all = seam(L)
        q = binom_p(r['hardOnQuad'], r['hardN'], r['quadChance']) if r['hardN'] else 1.0
        rat.append(r['vis'] / r['floor'])
        smx.append(mx)
        L1.append('%-26s %8.3f %8.3f %7.3f   %7.3f %7.3f   %4d/%-5d p=%.3f'
                  % (r['label'], r['vis'], r['floor'], r['vis'] / r['floor'],
                     mx, av, r['hardOnQuad'], r['hardN'], q))
    rat = np.array(rat)
    smx = np.array(smx)
    L1.append('-' * len(hd))
    L1.append('%-26s %8s %8s %7.3f   %7.3f' % ('VANILLA median of 22', '', '',
                                               float(np.median(rat)), float(np.median(smx))))
    L1.append('%-26s %8s %8s %7.3f   %7.3f   <-- THE CEILING'
              % ('VANILLA worst of 22', '', '', float(rat.max()), float(smx.max())))
    L1.append('')
    for r in J['ours']:
        L = S.lum(S.Dds(sheet_of(r['variant'], r['tile'])).level(0))
        mx, av, _all = seam(L)
        q = binom_p(r['hardOnQuad'], r['hardN'], r['quadChance']) if r['hardN'] else 1.0
        L1.append('%-26s %8.3f %8.3f %7.3f   %7.3f %7.3f   %4d/%-5d p=%.3g'
                  % (r['label'], r['vis'], r['floor'], r['vis'] / r['floor'],
                     mx, av, r['hardOnQuad'], r['hardN'], q))
    txt = '\n'.join(L1) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 't3b_seam.txt'), 'w', newline='\n') as f:
        f.write(txt)


if __name__ == '__main__':
    main()
