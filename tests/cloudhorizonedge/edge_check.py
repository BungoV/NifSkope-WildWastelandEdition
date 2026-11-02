"""Lane BUGS1102H (bungo 11-02): no straight hard edge across the sky where the cloud deck meets the horizon band.

bungo's Concord shot (Clear -> Overcast, Volume Clouds Off, main 4c9ea88e) showed the 2D cloud deck ending in a hard
straight line across the whole frame ~45 % down, with a flat pale band under it. The owner (lane WXBAND): the layers
drew the deck with no aerial perspective right down to HORIZONBAND1's fully hazed band. The fix there
(lookdev_clouds.frag ldApOn) fades the deck by the sky's transmittance to the deck; its red is
WW_LOOKDEV_RED=cloudnoap. tests/cloudlayerap gates the formula in numpy; this gate measures the PICTURE.

Measure (bars pre-registered before the first run on a live picture):
  per row y of the upper 70 % of the frame, d = L(y+3) - L(y-3) per column (Rec.709 luma, display levels);
  med(y) = median |d| over the columns; coh(y) = share of columns with the median's sign and |d| >= 6 levels.
  An EDGE row has coh >= 0.8 and med >= 6: a step of >= 6 levels in 6 rows that runs straight across >= 80 % of the
  width. A smooth sky gradient stays under 6 levels in 6 rows; hills, a skyline and trees are not straight (coh < 0.5
  on the shots, measured: Clear start 0.45, t13 0.42).
  PASS = no edge row. bungo's end shot: rows 482-490 med 28 coh 1.00 (FAIL); t40: med 8.8 coh 0.95 (FAIL).

    python -I edge_check.py --selftest            synthetic frames: a hard cut FAILS, a hazed fade and a skyline PASS
    python -I edge_check.py <png> [<png> ...]      PASS / FAIL per picture
    python -I edge_check.py --pair <green> <red>   green PASS and red FAIL, else FAIL (a red that passes = VACUOUS)
Exit 0 = all PASS.
"""
import sys
import numpy as np

K = 3
COH = 0.8
MED = 6.0
TOP = 0.70


def luma(img):
    a = np.asarray(img, dtype=np.float32)
    return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def edges(L):
    """rows (y, med, coh) that are an edge, and the worst row"""
    H = L.shape[0]
    ys = np.arange(K, int(H * TOP))
    d = L[ys + K] - L[ys - K]
    md = np.median(d, axis=1)
    med = np.median(np.abs(d), axis=1)
    coh = ((np.sign(d) == np.sign(md)[:, None]) & (np.abs(d) >= MED)).mean(axis=1)
    hit = (coh >= COH) & (med >= MED)
    w = int(np.argmax(med * coh))
    return [(int(ys[i]), float(med[i]), float(coh[i])) for i in np.nonzero(hit)[0]], (int(ys[w]), float(med[w]), float(coh[w]))


def judge(name, L):
    hit, w = edges(L)
    ok = not hit
    span = '' if ok else ' edge rows %d-%d (%d)' % (hit[0][0], hit[-1][0], len(hit))
    print('%s %s: worst row %d med %.2f coh %.3f%s' % ('PASS' if ok else 'FAIL', name, w[0], w[1], w[2], span))
    return ok


def load(p):
    from PIL import Image
    return luma(Image.open(p).convert('RGB'))


def synth(kind, W=1920, H=1080, seed=1):
    """sky rows 0..H*0.55 with a deck above a band; terrain below with a jagged skyline"""
    rng = np.random.default_rng(seed)
    y = np.arange(H, dtype=np.float32)[:, None] * np.ones((1, W), np.float32)
    x = np.arange(W, dtype=np.float32)[None, :] * np.ones((H, 1), np.float32)
    sky = 150.0 + 0.02 * y	# a smooth gradient (2 levels / 100 rows)
    band = 160.0
    cut = 486.0
    tex = 70.0 + 25.0 * np.sin(x / 37.0) * np.sin(y / 23.0) + rng.normal(0, 4, (H, W))
    if kind == 'hard':	# the deck at full share right down to the band, cut in 4 rows (bungo's end shot)
        share = np.clip((cut - y) / 4.0, 0.0, 1.0)
    elif kind == 'hazed':	# the deck's share falls to ~0.1 at the band top over ~3 deg (~120 rows), the band ramps in
        share = 0.1 + 0.9 * np.clip((cut - y) / 120.0, 0.0, 1.0) ** 1.5
        share *= np.clip((cut + 30.0 - y) / 30.0, 0.0, 1.0)
    else:
        share = np.zeros_like(y)
    bandw = np.clip((y - (cut - 26.0)) / 26.0, 0.0, 1.0)	# HORIZONBAND1's own ramp, 26 rows
    L = (1.0 - bandw) * (share * tex + (1.0 - share) * sky) + bandw * band
    sl = 540.0 + 25.0 * np.sin(x / 61.0) + 18.0 * np.sin(x / 13.0) + rng.normal(0, 3, (1, W))	# a skyline
    ground = 90.0 + rng.normal(0, 12, (H, W))
    return np.where(y > sl, ground, L)


def selftest():
    r = [not judge('selftest hard cut (red, must FAIL)', synth('hard')),
         judge('selftest hazed fade (green)', synth('hazed')),
         judge('selftest no deck, skyline only (control)', synth('none'))]
    return all(r)


def main(a):
    if not a or a[0] == '--selftest':
        ok = selftest()
    elif a[0] == '--pair':
        g = judge('green ' + a[1], load(a[1]))
        r = judge('red ' + a[2], load(a[2]))
        if r:
            print('VACUOUS: the red shows no edge, so the pair proves nothing')
        ok = g and not r
    else:
        ok = all([judge(p, load(p)) for p in a])
    print('CLOUDHORIZONEDGE %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
