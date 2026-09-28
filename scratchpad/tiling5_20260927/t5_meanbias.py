"""TILING5 -- does the height blend shift the large-scale brightness?  Reads
existing sheets only; bakes nothing.

The height blend picks, per texel, the tap / the layer with the higher relief.
If relief correlates with brightness inside a texture (m1_height_source: the
coverage-weighted median is +0.196), picking by relief also picks brighter
texels, so each texture's AVERAGE on the sheet moves -- by a different amount
per texture, which is large-scale contrast, not grain.

Per sheet: mean luminance of arm and floor; the difference image D = arm - floor
low-passed by a box of 64 texels (29 m); its SD; the SD of the floor low-passed
the same way; and the correlation of low-passed D with the low-passed floor
(positive = the change amplifies the contrast that is already there).

    python t5_meanbias.py ARM [FLOOR=today]  ->  logs/meanbias_<ARM>.txt
"""
import os
import sys

import numpy as np

import t5_gates as TG
import splatlib as S                                          # noqa: E402


def box(a, r):
    """Box mean of radius r (window 2r+1), edge-clamped, separable."""
    k = 2 * r + 1
    p = np.pad(a, r, mode='edge')
    c = np.cumsum(np.cumsum(p, 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)


def main():
    arm = sys.argv[1]
    floor = sys.argv[2] if len(sys.argv) > 2 else 'today'
    L = ['Large-scale brightness: arm %s vs floor %s (luminance 0..255, box r=32)' % (arm, floor),
         '',
         '   %-9s %8s %8s %8s | %9s %9s %7s' % ('sheet', 'floor', 'arm', 'dMean',
                                                'SD(lpD)', 'SD(lpF)', 'r'),
         ]
    dm, rs = [], []
    for cx, cy in TG.SEL + TG.VAL:
        a = S.lum(S.Dds(TG.sheet(arm, cx, cy)).level(0)).astype(np.float64)
        f = S.lum(S.Dds(TG.sheet(floor, cx, cy)).level(0)).astype(np.float64)
        lpd = box(a - f, 32)
        lpf = box(f, 32)
        r = float(np.corrcoef(lpd.ravel(), lpf.ravel())[0, 1]) if lpd.std() > 0 else 0.0
        dm.append(float(a.mean() - f.mean()))
        rs.append(r)
        L.append('   %4d,%4d %8.3f %8.3f %+8.3f | %9.3f %9.3f %+7.3f'
                 % (cx, cy, f.mean(), a.mean(), a.mean() - f.mean(), lpd.std(), lpf.std(), r))
    L.append('')
    L.append('   dMean > 0 on %d of %d sheets, median %+.3f; r > 0 on %d of %d, median %+.3f'
             % (sum(1 for d in dm if d > 0), len(dm), sorted(dm)[len(dm) // 2],
                sum(1 for r in rs if r > 0), len(rs), sorted(rs)[len(rs) // 2]))
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(TG.HERE, 'logs', 'meanbias_%s.txt' % arm), 'w', newline='\n') as fo:
        fo.write(txt)


if __name__ == '__main__':
    main()
