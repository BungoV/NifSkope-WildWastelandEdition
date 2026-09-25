"""GREY1 candidate 2, render half. Same camera, same frame size, pixel for pixel:
  <F>_seed  : WW_LODL_CHANNEL=seed flat -- non-tree placements draw BLACK (seed 0); terrain/trees/background do not
  <F>_base  : WW_LOD_CHANNEL=12 -- raw base colour, unlit, no tone map (what the atlas texel + vertex colour give)
  <F>_lit   : the default Legacy look (headlight, white ambient, filmic tone map, GGX spec)
  <F>_lit_ao: the same with vertex colours forced on (AO in the vertex colour)
  <F>_lookdev: Lookdev, CommonwealthClear at 12:00 (weather sun + flat NAM0 ambient for legacy shapes)
Mask = building pixels: seed shot black (< 8 in every channel) AND the base shot not black.
Prints, over the mask: mean sRGB, saturation of the mean colour, mean per-pixel HSV saturation, mean luma; and the
per-pixel saturation ratio shot/base (median), which is the viewer's shading effect with the texel held fixed.
usage: measure_pics.py <F> [<F> ...]"""
import sys, os
import numpy as np
from PIL import Image
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pics')


def load(n):
    f = os.path.join(P, n + '.png')
    return np.asarray(Image.open(f).convert('RGB'), np.float32) / 255.0 if os.path.exists(f) else None


def sat(a):
    mx = a.max(-1)
    return np.where(mx > 1e-3, (mx - a.min(-1)) / np.maximum(mx, 1e-3), 0)


for F in sys.argv[1:]:
    seed, base = load(F + '_seed'), load(F + '_base')
    if seed is None or base is None:
        print(F, 'missing seed/base shot')
        continue
    m = (seed.max(-1) < 8 / 255) & (base.max(-1) > 8 / 255)
    print('== %s: %d building pixels of %d (%.1f%%)' % (F, m.sum(), m.size, 100 * m.mean()))
    sb = sat(base)[m]
    for sh in ('base', 'lit', 'lit_ao', 'lookdev'):
        im = load(F + '_' + sh)
        if im is None:
            print('  %-8s missing' % sh)
            continue
        px = im[m]
        mean = px.mean(0)
        s = sat(im)[m]
        ok = sb > 0.02
        ratio = np.median(s[ok] / sb[ok]) if ok.any() else float('nan')
        print('  %-8s mean sRGB %s  S(mean) %.3f  mean S %.3f  luma %.3f  | median S/S_base %.3f  | changed px %.1f%%' % (
            sh, np.round(mean, 3), (mean.max() - mean.min()) / max(mean.max(), 1e-6), s.mean(),
            float(np.dot(mean, (0.299, 0.587, 0.114))), ratio, 100 * (np.abs(im - base).max(-1)[m] > 2 / 255).mean()))
