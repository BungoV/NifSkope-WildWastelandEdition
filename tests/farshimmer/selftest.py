"""lane FARTOGGLE4: why a far object goes on and off on a moving camera with the TAA OFF, and why no footprint rule
fixes it without making the far field bolder -- as asserts. A later lane that finds such a rule must delete case 4.

  1. the gaps of the 4x pattern: the largest empty axis-aligned square s* (every footprint narrower than s* covers
     0 samples at some sub-pixel phase), and the 0.25 px column/row gap a thin strip falls through;
  2. the arithmetic: with no history a frame shows hits / 4 px^2, so a footprint kept ON at every phase shows
     >= 1/4 px^2; an object of true area a < 1/8 px^2 is either OFF at some phase (it blinks on a moving camera) or
     shown at >= 2x its weight. Nothing but a history (the temporal AA) or a BLEND (a splat of alpha a) can show it
     at its weight in every frame;
  3. the blink counter discriminates: an ideal area-weighted raster (weight = area every frame) blinks 0 times
     (green), today's 4x coverage rule on the same panning population blinks (the red: it must FAIL "no blink");
  4. the refusal: on that population, every min-footprint rule tried (0.25 .. 1.0 px, with and without an area cap)
     either still blinks or adds more than 25 % ink to the sub-pixel population (the bar: one sample level of 4).

python -I selftest.py   -> prints the numbers and "FARSHIMMER SELFTEST PASS" (rc 0) or FAIL (rc 1)
"""
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import farshimmer_raster as R   # noqa: E402

# the bar: at most one sample level of 4 more ink over the sub-pixel population. FARSHIMMER_INK_BAR=5 is the test's
# own red (a bar fitted to pass the 0.8 px rule): case 4 must then FAIL
INK_BAR = float(os.environ.get('FARSHIMMER_INK_BAR', '1.25'))
ok = True


def check(name, cond, detail):
    global ok
    print('%-4s %s: %s' % ('ok' if cond else 'FAIL', name, detail))
    ok = ok and bool(cond)


# 1. the gaps
ph = (np.arange(64) + 0.5) / 64.0
PX, PY = np.meshgrid(ph, ph)
PX, PY = PX.ravel(), PY.ravel()


def min_hits(s):
    return int(R.hits(PX, PX + s, PY, PY + s).min())


lo, hi = 0.0, 1.0
for _ in range(20):
    mid = (lo + hi) / 2
    if min_hits(mid) == 0:
        lo = mid
    else:
        hi = mid
s_star = hi
check('largest empty square of the 4x pattern', 0.70 < s_star <= 0.80, 's* = %.4f px (some phase covers 0 samples '
      'below it)' % s_star)
strip = lambda w: int(R.hits(PX, PX + w, PY, PY + 1.0).min())
check('a 1 px tall strip falls between the sample columns below 0.25 px', strip(0.24) == 0 and strip(0.26) >= 1,
      'min hits at width 0.24: %d, 0.26: %d' % (strip(0.24), strip(0.26)))

# 2. the arithmetic: kept on at every phase -> >= 1 sample -> >= 1/4 px^2
for a in (0.01, 0.05, 0.09, 0.125):
    s = max(np.sqrt(a), s_star)
    shown = min_hits(s) / 4.0
    check('area %.3f px^2 kept on at every phase' % a, shown / a >= 2.0 - 1e-9,
          'needs a %.3f px square, shows >= %.3f px^2 = %.1fx its weight' % (s, shown, shown / a))

# 3. + 4. a panning sub-pixel population (dots 0.1 .. 0.9 px, thin strips 0.03 .. 0.3 px x 2 .. 20 px)
rng = np.random.default_rng(5)
nd, nt, nf = 400, 400, 24
side = np.concatenate([rng.uniform(0.1, 0.9, nd), rng.uniform(0.03, 0.3, nt)])
other = np.concatenate([side[:nd] * rng.uniform(0.7, 1.3, nd), rng.uniform(2.0, 20.0, nt)])
vert = rng.random(nd + nt) < 0.5
bw = np.where(vert, side, other)
bh = np.where(vert, other, side)
x0, y0 = rng.uniform(0, 1000, nd + nt), rng.uniform(0, 1000, nd + nt)
# a slow pan with parallax: 0.3 .. 0.7 px a frame across, 0.08 .. 0.18 down (an exact 0.5 px step is degenerate: it
# maps the pattern's 0.25 px sample columns onto themselves, so a still strip never changes)
u = rng.uniform(0.6, 1.4, nd + nt)
frames = [dict(cx=x0 + 0.5 * u * k, cy=y0 + 0.13 * u * k, bw=bw, bh=bh, area=bw * bh) for k in range(nf)]
idx = np.arange(nd + nt)
# the ideal area-weighted raster: weight = the true area in every frame, through the same on/off counter
ideal_on = np.array([4.0 * f['area'] > 0 for f in frames])
ideal_blinks = int(((ideal_on[1:] != ideal_on[:-1]).sum(0) > 0).sum())
res = R.blink_census(frames, idx, [0.25, 0.3, 0.5, 0.7, 0.8, 1.0, (0.8, 1.25), (0.8, 1.5), (1.0, 1.5)])
for line in R.report(res, nf):
    print('     ' + line)
check('green: the ideal area-weighted raster blinks', ideal_blinks == 0, '0 rows')
today = int((res['blinks'][0] > 0).sum())
check('red: today\'s 4x coverage rule FAILS "no blink"', today > 0, '%d of %d rows blink at a 0.5 px pan' % (today, nd + nt))
area = res['area']
fits = []
for k, (s, gc) in enumerate(res['rules'][1:], 1):
    blinks = int((res['blinks'][k] > 0).sum())
    ink = float(res['weight'][k].sum() / area.sum())
    fits.append((s, gc, blinks, ink))
    if blinks == 0 and ink <= INK_BAR:
        check('refusal: min footprint %.2f cap %.2f' % (s, gc), False, 'blinks 0 AND ink %.2f <= %.2f: a fix exists, '
              'delete this case and ship it' % (ink, INK_BAR))
check('refusal: no min-footprint rule is both blink-free and within %.2fx ink' % INK_BAR,
      all(not (b == 0 and i <= INK_BAR) for _, _, b, i in fits),
      '; '.join('%.2f%s: blinks %d ink %.2f' % (s, '/%.2f' % gc if gc else '', b, i) for s, gc, b, i in fits))
print('FARSHIMMER SELFTEST %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
