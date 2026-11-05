"""Lane WXBAND (WEATHER-HORIZON-BAND, 2026-11-02): the 2D cloud layers take the sky's aerial perspective to the deck.

The seam: on Concord under Overcast the layers (Volume Clouds Off) drew the deck un-hazed right down to the horizon
band (HORIZONBAND1, atmo_skyband.glsl), which is fully hazed: a dark deck on a pale band with a hard top edge.
lookdev_clouds.frag now draws a layer texel with alpha a x tm and colour c x tair / tm, tair = T(cam, mu) / T(deck, mu1)
along the view ray to the Low band's base (as cloud_march.frag's cvApOn), tm = max channel of tair. Over the sky
backdrop that is a x tair x c + (1 - a x tm) x sky: the deck fades into the sky's own colour toward the horizon,
and the sky meets the band with no seam (HORIZONBAND1's own gate).

Gates (bars pre-registered before the first run; numbers from atmo_model.transmittance, the LUT's math):
  S1 seam:    the deck's share tm at the band top (the c3 fixture's tSky = kSkyBandT row) <= 0.15   red noap: 1.0
  S2 control: tm at 20 deg >= 0.5 (the deck is not erased overhead)
  S3 smooth:  max |d tm| per 0.1 deg from the band top to 15 deg <= 0.05 (no new hard edge)
Inputs: the shot's haze (live_c3_g2.notes c3_wb5_tend: haze 0.821/km, scale 0.143 km), camera 9400 units (134 m),
deck base 1.5 km (WwCloudLightning::kDefBaseKm: Cloud Map off, as the harness runs).

    python -I tests/cloudlayerap/cloudlayerap_check.py [--red noap]
"""
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'spells'))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'features', 'sky'))  # sky helpers
import atmo_model as A

RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
HAZE = (0.821, 0.143, 0.0)
CAM_KM = 9400 * A.UNIT_M / 1000.0
DECK_KM = 1.5
BAND_T = 0.03
FIX = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'features', 'sky', 'horizonband1', 'c3_column.txt')


def band_top():
    rows = [ln.split() for ln in open(FIX) if ln.split() and ln.split()[0] == '100']
    rows = [(float(p[2]), float(p[3])) for p in rows]
    return min(e for e, t in rows if t >= BAND_T)


def share(el_deg):
    """tm = max channel of tair = T(cam, mu) / T(deck, mu1), the shader's formula (lookdev_clouds.frag)"""
    if RED == 'noap':
        return np.ones_like(np.asarray(el_deg, np.float64))
    P = A.params(HAZE)
    mu = np.sin(np.radians(np.asarray(el_deg, np.float64)))
    r0, R1 = A.RB + CAM_KM, A.RB + DECK_KM
    s = -r0 * mu + np.sqrt(np.maximum(0.0, r0 * r0 * (mu * mu - 1.0) + R1 * R1))
    r1 = np.sqrt(r0 * r0 + s * s + 2.0 * r0 * mu * s)
    mu1 = (r0 * mu + s) / r1
    ta = A.transmittance(P, np.full(mu.shape, r0), mu, n=256)
    tb = A.transmittance(P, r1, mu1, n=256)
    tair = np.clip(ta / np.maximum(tb, 1e-6), 0.0, 1.0)
    return tair.max(axis=-1)


def main():
    e0 = band_top()
    fails = 0
    s1 = float(share(np.array([e0]))[0])
    ok = s1 <= 0.15
    fails += not ok
    print('S1 seam: deck share at the band top (%.3f deg) %.4f (bar <= 0.15) %s' % (e0, s1, 'PASS' if ok else 'FAIL'))
    s2 = float(share(np.array([20.0]))[0])
    ok = s2 >= 0.5
    fails += not ok
    print('S2 control: deck share at 20 deg %.4f (bar >= 0.5) %s' % (s2, 'PASS' if ok else 'FAIL'))
    el = np.arange(e0, 15.0 + 1e-9, 0.1)
    sh = share(el)
    st = float(np.abs(np.diff(sh)).max())
    ok = st <= 0.05
    fails += not ok
    print('S3 smooth: max step per 0.1 deg %.4f (bar <= 0.05) %s' % (st, 'PASS' if ok else 'FAIL'))
    for e in (1.0, 2.0, 3.0, 5.0, 8.0, 12.0):
        print('INFO share el %4.1f %.4f' % (e, float(share(np.array([e]))[0])))
    print('CLOUDLAYERAP %s %d%s' % ('PASS' if fails == 0 else 'FAIL', 3 - fails if fails == 0 else fails,
          '' if fails == 0 else ' failed') + (' red=' + RED if RED else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
