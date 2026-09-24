"""Lane GROUND1 Part B, the rest of gate F3.

  E1  NO BORDER SEAM. Two chunks that touch are baked as separate sheets from
      separate lattices. If the pass were lattice-local the boundary would show
      a step. The test compares the gradient step ACROSS the shared edge with
      the distribution of gradient steps INSIDE each sheet: a seam is a boundary
      step that sits in the far tail of the interior steps.
      Floor: the same statistic with the pass off. The bar is that the pass does
      not move the boundary step's percentile among the interior steps by more
      than the interior steps themselves move.

  E2  THE COLOUR MOVES TOWARD VANILLA. The fine band (finer than 4 texels) of
      the colour sheet's luminance, ours off, ours on, and vanilla's, over the
      same cells. The claim under test is only "closer than it was"; TILING3
      already put an R-squared ceiling of 0.018-0.023 on any per-texel law from
      the fine normal to vanilla's fine colour, so nothing here claims to
      reproduce vanilla's colour, only to stop being flat.
"""
import glob
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/splat1_20260911')
import splatlib as S                                            # noqa: E402
import b1_vanilla as B                                          # noqa: E402

NAME = re.compile(r'Commonwealth\.(\d+)\.(-?\d+)\.(-?\d+)(_msn|_data)?\.DDS$', re.I)


def cells(root, suffix):
    out = {}
    for p in sorted(glob.glob(os.path.join(root, 'tex', '*.DDS'))):
        m = NAME.search(os.path.basename(p))
        if not m:
            continue
        if (m.group(4) or '') != suffix:
            continue
        out[(int(m.group(2)), int(m.group(3)))] = p
    return out


def grad(path):
    gx, gy, _ = B.gradient_field(path)
    return gx, gy


def seam(root):
    """Boundary gradient step across every touching pair, against the interior."""
    sheets = cells(root, '_msn')
    rows = []
    for (cx, cy), p in sorted(sheets.items()):
        for dx, dy in ((4, 0), (0, 4)):   # a dim-4 chunk is 4 cells

            q = sheets.get((cx + dx, cy + dy))
            if q is None:
                continue
            ga = grad(p)
            gb = grad(q)
            if dx:
                a = np.stack([ga[0][:, -1], ga[1][:, -1]])
                b = np.stack([gb[0][:, 0], gb[1][:, 0]])
                ia = np.abs(np.diff(ga[0], axis=1)) + np.abs(np.diff(ga[1], axis=1))
            else:
                a = np.stack([ga[0][-1, :], ga[1][-1, :]])
                b = np.stack([gb[0][0, :], gb[1][0, :]])
                ia = np.abs(np.diff(ga[0], axis=0)) + np.abs(np.diff(ga[1], axis=0))
            edge = float(np.median(np.abs(a - b).sum(axis=0)))
            inner = float(np.median(ia))
            pct = float((ia < edge).mean() * 100.0)
            rows.append((cx, cy, dx and 'E' or 'N', edge, inner, pct))
    return rows


def fine_lum(path):
    d = S.Dds(path)
    a = d.level(0).astype(np.float64) / 255.0
    lum = 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]
    return float((lum - S._box(lum, 2)).std())


def main():
    print('E1  the border seam, gradient step across the shared edge')
    print('%-6s %-16s %10s %10s %10s' % ('bake', 'chunk/dir', 'edge', 'interior', 'pctile'))
    for m in ('off', 'on'):
        root = os.path.join(HERE, 'f4_' + m)
        rows = seam(root)
        if not rows:
            print('  %-4s no touching pair found' % m)
            continue
        for cx, cy, d, e, i, p in rows:
            print('%-6s %-16s %10.4f %10.4f %9.1f%%'
                  % (m, '%d,%d %s' % (cx, cy, d), e, i, p))
        med = np.median([r[5] for r in rows])
        print('  %-4s median percentile of the edge step among interior steps: %.1f%%'
              % (m, med))

    print()
    print('E2  the fine band of the colour sheet (SD of luminance minus its 5-texel mean)')
    off = cells(os.path.join(HERE, 'f4_off'), '')
    on = cells(os.path.join(HERE, 'f4_on'), '')
    print('%-12s %9s %9s %9s' % ('chunk', 'off', 'on', 'vanilla'))
    oo, nn, vv = [], [], []
    for key in sorted(off):
        if key not in on:
            continue
        vpath = S.van_sheet(key[0], key[1], '')
        v = fine_lum(vpath) if os.path.exists(vpath) else float('nan')
        a, b = fine_lum(off[key]), fine_lum(on[key])
        oo.append(a)
        nn.append(b)
        vv.append(v)
        print('%-12s %9.5f %9.5f %9.5f' % ('%d,%d' % key, a, b, v))
    if oo:
        mo, mn = float(np.median(oo)), float(np.median(nn))
        mv = float(np.nanmedian(vv))
        print('median      %9.5f %9.5f %9.5f' % (mo, mn, mv))
        print('  distance from vanilla: off %+.1f%%   on %+.1f%%'
              % (100.0 * (mo - mv) / mv, 100.0 * (mn - mv) / mv))
    return 0


if __name__ == '__main__':
    sys.exit(main())
