"""BLENDEDGES1 job 1 -- where do the hard colour lines sit?

Input: a north-up colour crop, x = 0 on the chunk's WEST edge, y = 0 on its
NORTH edge, 32 units a texel, so the 2,048-unit quadrant lines fall BETWEEN
texels 64k-1 and 64k on both axes, and the 128-unit opacity grid (17 x 17 a
quadrant) has spacing 4 texels.

For every boundary between neighbouring columns (and rows) the mean |step| in
luminance across it, over the whole crop, divided by the crop's own mean step.
A crop with no line on a boundary reads ~1.0 there.  Then the profile is
FOLDED at periods 2..128: the fraction of the step energy per phase, against
the flat 1/P it would be with no periodic structure.  The same numbers on
vanilla's shipped sheet of the same chunk are the control (vanilla has no
quadrant composite, so any period-64 peak there would say the instrument, not
the bake, makes it).

    python job1_steps.py <png or dds> [label] ...
"""
import sys
import numpy as np
from PIL import Image


def load(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(np.float64)
    return a @ np.array([0.299, 0.587, 0.114])


def profiles(L):
    dx = np.abs(np.diff(L, axis=1))          # boundary b between column b and b+1
    dy = np.abs(np.diff(L, axis=0))
    px = dx.mean(0) / dx.mean()
    py = dy.mean(1) / dy.mean()
    return px, py


def fold(p, P):
    """phase = (b+1) mod P, so phase 0 = the boundary AT a multiple of P."""
    ph = (np.arange(len(p)) + 1) % P
    m = np.array([p[ph == k].mean() for k in range(P)])
    return m


def report(path, label, upt_scale=1):
    L = load(path)
    n = L.shape[0]
    q = 64 * upt_scale
    g = 4 * upt_scale
    px, py = profiles(L)
    out = ['== %s  (%s, %dx%d)' % (label, path, L.shape[1], L.shape[0])]
    for ax, p in (('x (vertical lines)', px), ('y (horizontal lines)', py)):
        quad = [float(p[k * q - 1]) for k in range(1, n // q)]
        out.append('  %s: quadrant boundaries %s  max %.3f mean %.3f' % (
            ax, ' '.join('%.2f' % v for v in quad), max(quad), np.mean(quad)))
        for P in (2, 4, 8, 16, 32, 64, 128):
            P2 = P * upt_scale
            if P2 > n // 2:
                continue
            m = fold(p, P2)
            k = int(np.argmax(m))
            out.append('    fold P=%-3d  peak phase %-3d  peak/mean %.3f  (min/mean %.3f)' % (
                P2, k, m[k] / m.mean(), m.min() / m.mean()))
        top = np.argsort(p)[::-1][:12]
        out.append('    12 strongest boundaries (b+1, ratio, mod64, mod4): ' + ', '.join(
            '%d:%.2f(%d,%d)' % (b + 1, p[b], (b + 1) % q, (b + 1) % g) for b in top))
    # joint: fraction of the strongest 2% steps (per texel, not per column)
    dx = np.abs(np.diff(L, axis=1)); dy = np.abs(np.diff(L, axis=0))
    for ax, d in (('x', dx), ('y', dy)):
        thr = np.quantile(d, 0.98)
        idx = np.nonzero(d >= thr)
        b = (idx[1] if ax == 'x' else idx[0]) + 1
        on_q = float(np.mean(b % q == 0)); on_g = float(np.mean(b % g == 0))
        ph4 = np.bincount(b % g, minlength=g) / len(b)
        out.append('  top-2%% %s steps: %d; on quadrant line %.3f (chance %.3f); on 4-grid phase0 %.3f; 4-phase split %s' % (
            ax, len(b), on_q, 1.0 / q, on_g, ' '.join('%.3f' % v for v in ph4)))
    return '\n'.join(out)


if __name__ == '__main__':
    args = sys.argv[1:]
    for i in range(0, len(args), 2):
        print(report(args[i], args[i + 1] if i + 1 < len(args) else args[i]))
