"""ROADS1 section 1, part 2: does Bethesda's shipped sheet CARRY the road that
our top-down projection says is there, and does its `_msn` carry it too?

  python cmp_vanilla.py <masks.npz> <vanillaDir> <cx> <cy> <outdir>

Prints, for the colour sheet and for the `_msn` sheet, per family mask: the
mean colour, the mean luminance, the saturation, and -- the discriminator --
how far each family's texels sit from the sheet's own OFF-family background.
Controls: the same statistics on the road mask SHIFTED (a phase-randomised
twin of the same shape and area), and on the complement.
"""

import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                # noqa: E402

FAMS = ('road', 'trees', 'rocks', 'buildings', 'setdressing')


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    return np.array(px, dtype=np.float32).reshape(h, w, 4), t.fourcc.decode('latin-1')


def stats(name, img, m):
    if m.sum() == 0:
        print('%-16s (empty)' % name)
        return None
    s = img[m]
    lum = s[:, 0] * 0.2126 + s[:, 1] * 0.7152 + s[:, 2] * 0.0722
    sat = s[:, :3].max(1) - s[:, :3].min(1)
    print('%-16s n=%6d  rgb %5.1f %5.1f %5.1f  lum %5.1f  sat %5.1f'
          % (name, m.sum(), s[:, 0].mean() * 255, s[:, 1].mean() * 255,
             s[:, 2].mean() * 255, lum.mean() * 255, sat.mean() * 255))
    return dict(lum=lum.mean() * 255, sat=sat.mean() * 255,
                rgb=[float(s[:, k].mean() * 255) for k in range(3)])


def shift(m, dx, dy):
    return np.roll(np.roll(m, dy, axis=0), dx, axis=1)


def main(argv):
    npz, vanDir, cx, cy, outdir = argv[0], argv[1], int(argv[2]), int(argv[3]), argv[4]
    d = np.load(npz)
    col, fc = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx, cy)))
    msn, fcn = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy)))
    print('vanilla colour %s %s   msn %s %s' % (col.shape, fc, msn.shape, fcn))
    road = d['road']
    other = np.zeros_like(road)
    for f in FAMS[1:]:
        other |= d[f]
    bg = ~(road | other)

    print('--- vanilla COLOUR sheet ---')
    for f in FAMS:
        stats(f, col, d[f])
    stats('background', col, bg)
    stats('road shifted+64', col, shift(road, 64, 64))
    stats('road shifted-96', col, shift(road, -96, 48))

    print('--- vanilla _msn sheet (R,G = normal xy; B = whatever it holds) ---')
    for f in FAMS:
        stats(f, msn, d[f])
    stats('background', msn, bg)
    stats('road shifted+64', msn, shift(road, 64, 64))

    # normal deviation from straight up, per family: BC5U stores x and y
    print('--- _msn tilt (degrees from vertical), reconstructed z ---')
    nx = msn[:, :, 0] * 2.0 - 1.0
    ny = msn[:, :, 1] * 2.0 - 1.0
    nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 0.0, 1.0))
    tilt = np.degrees(np.arccos(np.clip(nz, -1, 1)))
    for f in FAMS:
        m = d[f]
        if m.sum():
            print('%-16s mean %5.2f  p95 %5.2f  sd %5.2f'
                  % (f, tilt[m].mean(), np.percentile(tilt[m], 95), tilt[m].std()))
    print('%-16s mean %5.2f  p95 %5.2f  sd %5.2f'
          % ('background', tilt[bg].mean(), np.percentile(tilt[bg], 95), tilt[bg].std()))
    sh = shift(road, 64, 64)
    print('%-16s mean %5.2f  p95 %5.2f  sd %5.2f'
          % ('road shifted', tilt[sh].mean(), np.percentile(tilt[sh], 95), tilt[sh].std()))

    os.makedirs(outdir, exist_ok=True)
    Image.fromarray((np.clip(col[:, :, :3], 0, 1) * 255).astype(np.uint8)).save(
        os.path.join(outdir, 'van_color.png'))
    Image.fromarray((np.clip(msn[:, :, :3], 0, 1) * 255).astype(np.uint8)).save(
        os.path.join(outdir, 'van_msn.png'))
    ov = (np.clip(col[:, :, :3], 0, 1) * 255).astype(np.uint8).copy()
    ov[road] = (ov[road] * 0.35 + np.array([255, 60, 60]) * 0.65).astype(np.uint8)
    Image.fromarray(ov).save(os.path.join(outdir, 'van_color_roadmask.png'))
    Image.fromarray((road * 255).astype(np.uint8)).save(
        os.path.join(outdir, 'roadmask.png'))
    print('wrote pictures to', outdir)


if __name__ == '__main__':
    main(sys.argv[1:])
