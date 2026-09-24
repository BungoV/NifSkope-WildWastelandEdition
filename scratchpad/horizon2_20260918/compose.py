# Lane HORIZON2 step 5 -- HORIZON1s compose.py with LANE repointed at this
# lanes images and dump, and nothing else changed: the camera is measured the
# same way, from this lanes own calibration renders, and the check translation
# still stops the script if the map misses by more than 1.5 px.
# Lane HORIZON1 step 5 -- the two pictures that are made rather than rendered:
# the sixteen-bin grid, and the ray-cast comparison.
#
#   python compose.py bins     -> images/horizon_bins_close.png
#   python compose.py calib    -> prints the measured world->pixel map
#   python compose.py raycast  -> images/horizon_vs_raycast_*.png
#
# THE CAMERA IS MEASURED, NOT ASSUMED. WW_RENDER_VIEW=8 is ViewUser, an oblique
# orthographic view, and a red dot on the wrong pixel is worse than no picture:
# it would be a claim about WHERE the refuter disagrees, made up. So the map is
# read off the renderer's own output -- translate the camera by a known world
# vector, and every pixel of an orthographic image moves by exactly the
# projection of it -- and then CHECKED against a fourth translation the map did
# not see. A prediction that misses by more than a pixel stops this script.
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
IMG = LANE + '/images'
mode = sys.argv[1] if len(sys.argv) > 1 else 'bins'


def load(name):
    return Image.open('%s/%s.png' % (IMG, name)).convert('RGB')


def grey(name):
    return np.asarray(load(name).convert('L'), dtype=np.float64)


def shift_between(a, b):
    """Pixel translation from image a to image b, by phase correlation. Exact
    to the pixel for a rigid translation, which is what moving an orthographic
    camera produces."""
    fa = np.fft.rfft2(a - a.mean())
    fb = np.fft.rfft2(b - b.mean())
    cps = fa * np.conj(fb)
    mag = np.abs(cps)
    cps = np.where(mag > 1e-9, cps / np.maximum(mag, 1e-9), 0.0)
    corr = np.fft.irfft2(cps, s=a.shape)
    iy, ix = np.unravel_index(np.argmax(corr), corr.shape)
    h, w = a.shape
    dy = iy - h if iy > h // 2 else iy
    dx = ix - w if ix > w // 2 else ix
    return float(dx), float(dy), float(corr.max())


# ---------------------------------------------------------------- the bins
if mode == 'bins':
    # One tile a bin, in the SAME order and orientation the stream stores them:
    # bin 0 is NORTH (+Y) and the numbering runs CLOCKWISE toward EAST, so the
    # grid reads like a compass rose cut into four rows.
    tiles = ['bin_%02d' % b for b in range(16)]
    first = load(tiles[0])
    tw, th = first.width // 2, first.height // 2
    pad, top = 6, 26
    out = Image.new('RGB', (4 * tw + 5 * pad, 4 * (th + top) + 5 * pad), (24, 24, 28))
    d = ImageDraw.Draw(out)
    for i, t in enumerate(tiles):
        im = load(t).resize((tw, th), Image.LANCZOS)
        x = pad + (i % 4) * (tw + pad)
        y = pad + (i // 4) * (th + top + pad)
        out.paste(im, (x, y + top))
        d.text((x + 4, y + 6), 'bin %d  %.1f deg azimuth  %s'
               % (i, 360.0 * i / 16.0,
                  {0: '(NORTH, +Y)', 4: '(EAST)', 8: '(SOUTH)', 12: '(WEST)'}.get(i, '')),
               fill=(230, 230, 235))
    d.text((pad, out.height - 18),
           'Commonwealth 4.4.-12, close framing, WW_LODL_CHANNEL=horizonbin=<n>: '
           'grey = that bin\'s stored horizon elevation, 0 deg black .. 90 deg white. '
           'Bin 0 is +Y and the numbering runs clockwise.', fill=(190, 190, 195))
    out.save('%s/horizon_bins_close.png' % IMG)
    print('horizon_bins_close.png %dx%d' % (out.width, out.height))
    sys.exit(0)

# ------------------------------------------------------------- the camera
# EACH FRAMING IS MEASURED ON ITS OWN. It turns out the map does transfer -- the
# wide framing's columns are the close ones times 2600/8192 to within 0.5% -- but
# that is a MEASURED fact here and was not a safe assumption: the crude test that
# first asked the question (downscale the close render, correlate it into the
# wide one) answered "105 px out in Y", and it was the test that was wrong, not
# the camera. Five renders and a self-check settle it either way.
CAL = {
    'close': ([('calib_base', (24900.0, -41300.0, 450.0)),
               ('calib_dx', (25400.0, -41300.0, 450.0)),
               ('calib_dy', (24900.0, -40800.0, 450.0)),
               ('calib_dz', (24900.0, -41300.0, 950.0))],
              ('calib_chk', (25250.0, -40950.0, 800.0))),
    'full': ([('calibf_base', (24576.0, -40960.0, 0.0)),
              ('calibf_dx', (26576.0, -40960.0, 0.0)),
              ('calibf_dy', (24576.0, -38960.0, 0.0)),
              ('calibf_dz', (24576.0, -40960.0, 2000.0))],
             ('calibf_chk', (25976.0, -39560.0, 1400.0))),
}


def measure_camera(framing='close'):
    cal, _chk = CAL[framing]
    base = grey(cal[0][0])
    cols = []
    for name, c in cal[1:]:
        dx, dy, q = shift_between(base, grey(name))
        d = np.array(c) - np.array(cal[0][1])
        step = float(np.linalg.norm(d))
        # The image moves OPPOSITE the camera, so the projection of +1 world
        # unit along this axis is -(shift)/step.
        cols.append((-dx / step, -dy / step, q))
    M = np.array([[cols[0][0], cols[1][0], cols[2][0]],
                  [cols[0][1], cols[1][1], cols[2][1]]])
    return M, [c[2] for c in cols]


def check_camera(M, framing='close'):
    cal, chk = CAL[framing]
    dx, dy, q = shift_between(grey(cal[0][0]), grey(chk[0]))
    d = np.array(chk[1]) - np.array(cal[0][1])
    want = -M.dot(d)              # what the map predicts the image does
    got = np.array([dx, dy])
    return want, got, float(np.linalg.norm(want - got)), q


if mode == 'calib':
    bad = 0
    for framing in ('close', 'full'):
        M, q = measure_camera(framing)
        want, got, err, cq = check_camera(M, framing)
        print('%s framing, world -> pixel columns (px per world unit):' % framing)
        for i, ax in enumerate('xyz'):
            print('  %s: (%+.6f, %+.6f)  correlation peak %.4f' % (ax, M[0, i], M[1, i], q[i]))
        print('  CHECK, a translation the map never saw: predicted image shift '
              '(%+.2f,%+.2f), measured (%+.2f,%+.2f), %.2f px apart, peak %.4f'
              % (want[0], want[1], got[0], got[1], err, cq))
        bad += (err > 1.5)
    sys.exit(1 if bad else 0)

# ------------------------------------------------------- the ray-cast picture
if mode == 'raycast':
    CAM = {}
    for framing in ('close', 'full'):
        M, _ = measure_camera(framing)
        want, got, err, _ = check_camera(M, framing)
        if err > 1.5:
            sys.exit('%s camera check failed: predicted (%+.2f,%+.2f) measured (%+.2f,%+.2f), '
                     '%.2f px apart -- no picture is better than a wrong one'
                     % (framing, want[0], want[1], got[0], got[1], err))
        CAM[framing] = (M, err)
    # The refuter's own dump: one row a sampled receiver, one bit a sun position.
    rows = {}
    for pfx in ('vhorRefute', 'horizonRefute'):
        path = '%s/dump/%s.csv' % (LANE, pfx)
        if not os.path.exists(path):
            continue
        r = []
        for line in open(path):
            if line.startswith('#') or line.startswith('x,'):
                continue
            p = line.strip().split(',')
            if len(p) == 5:
                r.append((float(p[0]), float(p[1]), float(p[2]), int(p[3]), int(p[4])))
        rows[pfx] = r
    if not rows:
        sys.exit('no dump under %s/dump -- bake with WW_HORIZON_REFUTE_DUMP first' % LANE)

    AZ = [120.0, 240.0]
    EL = [5.0, 15.0, 30.0, 60.0]
    CENTRE = {'close': (24900.0, -41300.0, 450.0), 'full': (24576.0, -40960.0, 0.0)}
    ORTHO = {'close': 2600.0, 'full': 8192.0}
    for framing in ('close', 'full'):
        for ai, az in enumerate(AZ):
            for ei, el in enumerate(EL):
                if el == 60.0:
                    continue          # the twelve pictures are at 5, 15 and 30
                src = '%s/chunk_horizon_%s_e%02d_a%d' % (IMG, framing, int(el), int(az))
                if not os.path.exists(src + '.png'):
                    continue
                im = Image.open(src + '.png').convert('RGB')
                d = ImageDraw.Draw(im)
                cx, cy = im.width / 2.0, im.height / 2.0
                M, err = CAM[framing]
                c0 = np.array(CENTRE[framing])
                bit = 1 << (ai * len(EL) + ei)
                n_dis = n_tot = 0
                for pfx, rs in rows.items():
                    col = (255, 60, 60) if pfx == 'horizonRefute' else (255, 160, 40)
                    for (x, y, z, dis, _v) in rs:
                        n_tot += 1
                        if not (dis & bit):
                            continue
                        p = M.dot(np.array([x, y, z]) - c0)
                        px, py = cx + p[0], cy + p[1]
                        if -4 <= px < im.width + 4 and -4 <= py < im.height + 4:
                            d.ellipse([px - 2, py - 2, px + 2, py + 2], fill=col)
                            n_dis += 1
                d.rectangle([0, im.height - 40, im.width, im.height], fill=(16, 16, 18))
                d.text((8, im.height - 34),
                       'sun az %d el %d, %s framing. RED = terrain texel, ORANGE = LOD vertex where the '
                       'stored bins and the full-resolution reference cast' % (int(az), int(el), framing),
                       fill=(240, 240, 245))
                d.text((8, im.height - 20),
                       'disagree about this sun (%d of %d sampled receivers in frame). '
                       'Camera measured, not assumed: check translation off by %.2f px.'
                       % (n_dis, n_tot, err), fill=(240, 240, 245))
                out = '%s/horizon_vs_raycast_%s_e%02d_a%d.png' % (IMG, framing, int(el), int(az))
                im.save(out)
                print('%s  %d marks' % (os.path.basename(out), n_dis))
    sys.exit(0)

sys.exit('unknown mode %r' % mode)
