"""ROADS1 gate R2: the ROAD-PRESENCE metric, with its floor and its ceiling.

  python road_metric.py <vanilla.DDS> <before.DDS> <after.DDS> <masks.npz> <out.json>

THE MASK. "Vanilla's road centreline, extracted from vanilla's own sheet by its
colour" -- so the mask is built from Bethesda's sheet alone, with no reference
to our output and none to the road geometry:

    a texel is road-coloured when its chroma (R - B) is below the midpoint of
    the two chroma means section 1 measured (road 15.0, background 20.1 -> 17.5)
    AND its luminance is above the midpoint of the two luminance means
    (92.6, 82.9 -> 87.7);
    the CENTRELINE is that mask eroded once with a 3x3 structuring element, so
    only cores survive and every half-covered edge texel is dropped.

THE EXTRACTOR'S OWN CONTROL, run first (ww-control-calibration): the mask must
agree with the INDEPENDENT geometric projection of the road meshes far better
than with the same projection displaced five ways. If it does not, the mask is
picking up something other than the road and the metric below is void.

THE METRIC. Over the centreline texels, the fraction at which our sheet is
within TOL of vanilla's, per channel, max over RGB.

    ceiling   vanilla against itself                = 1.000 by construction
    floor     our bake with --no-roads (the rung)
    after     our bake with roads on
    reference the same fraction on the NON-road background, which is what this
              pipeline achieves where it already agrees with vanilla

PRE-REGISTERED PASS (written before the numbers): TOL = 16 of 255, and the gate
is  after >= 2 x floor  AND  after >= 0.8 x reference.
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                    # noqa: E402

TOL = 16.0
CHROMA_T = 17.5
LUM_T = 87.7
SHIFTS = ((64, 64), (-96, 48), (128, -128), (0, 200), (200, 0))


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    a = np.array(px, dtype=np.float64).reshape(h, w, 4)[:, :, :3] * 255.0
    return a


def erode(m):
    out = m.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            out &= np.roll(np.roll(m, dy, axis=0), dx, axis=1)
    return out


def shift(m, dx, dy):
    return np.roll(np.roll(m, dy, axis=0), dx, axis=1)


def frac_within(a, b, m, tol):
    if m.sum() == 0:
        return float('nan')
    d = np.abs(a[m] - b[m]).max(1)
    return float((d <= tol).mean())


def main(argv):
    van, before, after, npz, out = argv[:5]
    V, B, A = sheet(van), sheet(before), sheet(after)
    assert V.shape == B.shape == A.shape, (V.shape, B.shape, A.shape)
    lum = V[:, :, 0] * .2126 + V[:, :, 1] * .7152 + V[:, :, 2] * .0722
    chroma = V[:, :, 0] - V[:, :, 2]
    roadish = (chroma <= CHROMA_T) & (lum >= LUM_T)
    centre = erode(roadish)
    geo = np.load(npz)['road']
    res = {}

    print('== the extractor\'s control ==')
    inter = float((centre & geo).sum()) / max(1, int(centre.sum()))
    ctrl = [float((centre & shift(geo, dx, dy)).sum()) / max(1, int(centre.sum()))
            for dx, dy in SHIFTS]
    print('   road-coloured texels %d, centreline (eroded) %d of %d'
          % (int(roadish.sum()), int(centre.sum()), centre.size))
    print('   centreline inside the geometric road footprint: %.3f' % inter)
    print('   the same against the DISPLACED footprint:       %.3f .. %.3f'
          % (min(ctrl), max(ctrl)))
    res['centrelineTexels'] = int(centre.sum())
    res['roadishTexels'] = int(roadish.sum())
    res['extractorInsideGeometry'] = inter
    res['extractorControl'] = ctrl

    bg = ~(roadish | geo)
    print('== the metric, TOL = %.0f of 255 ==' % TOL)
    rows = [
        ('ceiling  vanilla vs vanilla', frac_within(V, V, centre, TOL)),
        ('floor    ours --no-roads', frac_within(B, V, centre, TOL)),
        ('after    ours --roads', frac_within(A, V, centre, TOL)),
        ('reference background, --no-roads', frac_within(B, V, bg, TOL)),
        ('reference background, --roads', frac_within(A, V, bg, TOL)),
    ]
    keys = ['ceiling_vanilla', 'floor_ours', 'after_ours',
            'reference_background_noroads', 'reference_background_roads']
    for (n, v), k in zip(rows, keys):
        print('   %-34s %.4f' % (n, v))
        res[k] = v
    floor = rows[1][1]
    afterv = rows[2][1]
    ref = rows[4][1]
    ok1 = afterv >= 2.0 * floor
    ok2 = afterv >= 0.8 * ref
    print('   PASS after >= 2 x floor        : %.4f >= %.4f  %s'
          % (afterv, 2.0 * floor, 'ok' if ok1 else 'FAIL'))
    print('   PASS after >= 0.8 x reference  : %.4f >= %.4f  %s'
          % (afterv, 0.8 * ref, 'ok' if ok2 else 'FAIL'))
    res['pass'] = bool(ok1 and ok2)

    print('== whole-tile mean |colour error| vs vanilla (PARITY\'s own metric) ==')
    for n, X in (('--no-roads', B), ('--roads', A)):
        e = np.abs(X - V).max(2)
        print('   %-12s mean %.2f  p95 %.1f  max %.0f'
              % (n, e.mean(), np.percentile(e, 95), e.max()))
        res['wholeTile_' + n] = float(e.mean())
    for n, X in (('--no-roads', B), ('--roads', A)):
        e = np.abs(X[centre] - V[centre]).max(1)
        print('   on the centreline only, %-12s mean %.2f' % (n, e.mean()))
        res['centreline_' + n] = float(e.mean())
    json.dump(res, open(out, 'w'), indent=1)
    print('wrote', out)
    return 0 if res['pass'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
