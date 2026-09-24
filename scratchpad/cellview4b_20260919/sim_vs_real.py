"""Lane CELLVIEW4B -- the real viewer's ground against CELLVIEW4's two
simulations, per quad and per quadrant.

HOW THE LIGHTING IS NORMALISED, because it has to be said before any number is
quoted.  The two targets are UNLIT: `splat_sim.py` composites raw diffuse texels
out of the shipped .dds files at the opacities the LAND record stores, with no
light, no vertex colour and no tone map.  The shot is a real render with all
three.  Nothing here tries to undo that physically.  What it does instead is the
weakest correction that still lets the two RULES be told apart:

  * reduce all three pictures to 32x32 per-quad block MEANS first, so a quad is
    one colour and the comparison is about which texture won a quad rather than
    about texels;
  * fit ONE gain and ONE offset per channel over the whole cell, mapping the
    shot onto the target.  A single global affine per channel can absorb an
    exposure and a black level.  It cannot absorb a per-quad difference, and a
    per-quad difference is exactly what separates "blended" from "mosaic".

So the absolute residual below is NOT a measure of how right the colour is --
it still carries all the lighting the fit could not remove.  The number that
carries meaning is the COMPARATIVE one: for the quads where the two rules
disagree, is the shot nearer the blended target or the mosaic one.  That is the
gate's own question and it is reported per quadrant here, because the director's
seam question is about one quadrant in particular.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SHOT = os.path.join(HERE, 'images', 'after_ground.png')
MOSAIC = os.path.join(HERE, '..', 'cellview4_20260919', 'images',
                      'sim_m20_7_mosaic.png')
BLEND = os.path.join(HERE, '..', 'cellview4_20260919', 'images',
                     'sim_m20_7_blended.png')
N = 32


def blocks(path, crop=None):
    im = Image.open(path).convert('RGB')
    a = np.asarray(im).astype(np.float64) / 255.0
    if crop:
        a = a[crop[1]:crop[3], crop[0]:crop[2]]
    h, w, _ = a.shape
    out = np.zeros((N, N, 3))
    for r in range(N):
        for c in range(N):
            y0, y1 = int(r * h / N), int((r + 1) * h / N)
            x0, x1 = int(c * w / N), int((c + 1) * w / N)
            out[r, c] = a[y0:y1, x0:x1].reshape(-1, 3).mean(axis=0)
    return out


def fit(shot, target):
    """One gain+offset per channel, least squares over the whole cell."""
    out = np.zeros_like(shot)
    gains = []
    for ch in range(3):
        x = shot[..., ch].ravel()
        y = target[..., ch].ravel()
        A = np.stack([x, np.ones_like(x)], axis=1)
        g, o = np.linalg.lstsq(A, y, rcond=None)[0]
        out[..., ch] = shot[..., ch] * g + o
        gains.append((g, o))
    return out, gains


def main():
    # The render is the whole window; the simulation is the cell alone. The
    # cell fills the ortho frame the camera was built for, so the shot is
    # cropped to its own square about the centre before it is blocked.
    im = Image.open(SHOT)
    w, h = im.size
    side = min(w, h)
    crop = ((w - side) // 2, (h - side) // 2,
            (w - side) // 2 + side, (h - side) // 2 + side)
    shot = blocks(SHOT, crop)
    mos = blocks(MOSAIC)
    bl = blocks(BLEND)

    fb, gains = fit(shot, bl)
    fm, _ = fit(shot, mos)

    print('sim-vs-real, Sanctuary -20,7, 32x32 per-quad block means')
    print('  shot  %s' % os.path.relpath(SHOT, HERE))
    print('  crop  %s (the cell square out of the %dx%d window)' % (crop, w, h))
    print('  per-channel gain,offset fitted shot->blended: %s'
          % ', '.join('%.3f,%+.3f' % g for g in gains))
    print()

    db = np.abs(fb - bl).mean(axis=2)
    dm = np.abs(fm - mos).mean(axis=2)
    dis = np.abs(bl - mos).mean(axis=2) > 0.02

    print('  %-26s %8s %8s' % ('', 'vs BLEND', 'vs MOSAIC'))
    print('  %-26s %8.4f %8.4f' % ('mean |residual|, all quads',
                                   db.mean(), dm.mean()))
    print('  %-26s %8.4f %8.4f' % ('mean |residual|, disagreeing',
                                   db[dis].mean(), dm[dis].mean()))
    print('  %-26s %8d' % ('disagreeing quads', int(dis.sum())))
    closer = db < dm
    print('  %-26s %8d  (%.1f%%)' % ('of those, nearer BLEND',
                                     int((closer & dis).sum()),
                                     100.0 * (closer & dis).sum() / dis.sum()))
    print()
    print('  per quadrant -- row<16 is the bottom half, col<16 the left half;')
    print('  quadrant 3 (TR) is the one with no BTXT and the one the director')
    print('  saw a seam against. NOTE the picture is rendered with +Y UP, so')
    print('  the TOP of the image is the HIGH row: block row 0 is the top.')
    names = {(0, 0): 'TL (2)', (0, 1): 'TR (3)',
             (1, 0): 'BL (0)', (1, 1): 'BR (1)'}
    print('  %-10s %8s %8s %10s %10s' % ('quadrant', 'vs BLEND', 'vs MOSAIC',
                                         'disagree', 'nearer BL'))
    for (rh, ch), nm in sorted(names.items()):
        r0, r1 = (0, 16) if rh == 0 else (16, 32)
        c0, c1 = (0, 16) if ch == 0 else (16, 32)
        sb = db[r0:r1, c0:c1]
        sm = dm[r0:r1, c0:c1]
        sd = dis[r0:r1, c0:c1]
        sc = (db < dm)[r0:r1, c0:c1]
        pct = (100.0 * (sc & sd).sum() / sd.sum()) if sd.sum() else float('nan')
        print('  %-10s %8.4f %8.4f %10d %9.1f%%'
              % (nm, sb.mean(), sm.mean(), int(sd.sum()), pct))

    # the centre-line rows and columns on their own: if the viewer invented a
    # seam, the quads that touch the centre lines are where it would show as a
    # residual spike against the target that does NOT have one
    print()
    print('  the two centre lines, on their own (rows/cols 15 and 16):')
    edge = np.zeros((N, N), bool)
    edge[15:17, :] = True
    edge[:, 15:17] = True
    print('    %-24s %8.4f  (whole cell %.4f)'
          % ('mean |residual| vs BLEND', db[edge].mean(), db.mean()))
    print('    %-24s %8.4f  (whole cell %.4f)'
          % ('ratio to the cell mean', db[edge].mean() / db.mean(), 1.0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
