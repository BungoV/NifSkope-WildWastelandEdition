"""ROADS3 gate F1, part 0 -- the instrument is tested on KNOWN answers first.

Six checks, each with the other side of the floor so it is seen able to fail:

  1  a synthetic sheet built AS law (A) at a known constant opacity is fitted
     back to that opacity, and the residual is ~0;
  2  the same at a known RAMPED opacity (a skirt), recovered per texel;
  3  a sheet built NOT as law (A) -- a third, unrelated colour painted over the
     ground -- leaves a residual the size of the thing it is asked to explain,
     so the residual statistic is not cosmetic;
  4  the shuffled-ground floor really destroys a real fit;
  5  the mask finds exactly the texels that were painted, and nothing else;
  6  the signed distance and the second difference see a planted STEP and read
     ~0 on a planted smooth ramp.

    python r3_selftest.py   ->  logs/s0_selftest.txt
"""
import numpy as np

import r3lib as R3

rng = np.random.default_rng(7)
N = 128
ok, bad = 0, 0
lines = ['ROADS3 -- instrument self-test, known answers only', '']


def check(name, cond, got, want):
    global ok, bad
    global lines
    if cond:
        ok += 1
        lines.append('  PASS  %-46s  %s (want %s)' % (name, got, want))
    else:
        bad += 1
        lines.append('  FAIL  %-46s  %s (want %s)' % (name, got, want))


# a ground with real structure, and a flat road paint over part of it
G = np.zeros((N, N, 3))
base = rng.normal(90, 14, (N, N))
for k, tint in enumerate((1.02, 0.98, 0.86)):
    G[:, :, k] = np.clip(base * tint + rng.normal(0, 3, (N, N)), 0, 255)
mask = np.zeros((N, N), bool)
mask[:, 50:78] = True                      # a 28-texel wide road, N-S
Rp = np.zeros((N, N, 3))
Rp[:, :, 0], Rp[:, :, 1], Rp[:, :, 2] = 108.0, 110.0, 118.0   # flat blue-grey

# ---------------------------------------------------------------- 1 constant a
for a_true in (0.35, 0.6, 0.9):
    V = G.copy()
    V[mask] = (a_true * Rp + (1 - a_true) * G)[mask]
    f = R3.fit_alpha(V, Rp, G, mask)
    med = np.median(f['a'][f['read']])
    check('constant a = %.2f recovered' % a_true, abs(med - a_true) < 0.01,
          '%.4f' % med, '%.2f' % a_true)
    check('constant a = %.2f residual ~ 0' % a_true,
          f['resid'][f['read']].mean() < 0.01,
          '%.4f' % f['resid'][f['read']].mean(), '< 0.01')

# ------------------------------------------------------------------ 2 ramped a
yy, xx = np.mgrid[0:N, 0:N]
ramp = np.clip((np.minimum(xx - 50, 77 - xx) + 1) / 8.0, 0.0, 1.0) * 0.8
V = G.copy()
V[mask] = (ramp[:, :, None] * Rp + (1 - ramp[:, :, None]) * G)[mask]
f = R3.fit_alpha(V, Rp, G, mask)
err = np.abs(f['a'] - ramp)[f['read']]
check('ramped a recovered per texel', err.max() < 0.02,
      'max err %.4f' % err.max(), '< 0.02')
sd = R3.signed_dist(mask)
prof = {}
for d in range(1, 9):
    m = f['read'] & (sd == d)
    if m.sum():
        prof[d] = float(f['a'][m].mean())
check('edge profile of a rises with depth',
      prof and prof[1] < prof[8] and abs(prof[8] - 0.8) < 0.02,
      'a(1) %.3f a(8) %.3f' % (prof[1], prof[8]), 'a(1) < a(8) ~ 0.80')

# ------------------------------------------------ 3 a sheet that is NOT law (A)
V = G.copy()
third = np.zeros((N, N, 3))
third[:, :, 0], third[:, :, 1], third[:, :, 2] = 60.0, 130.0, 70.0   # green
V[mask] = third[mask]
f = R3.fit_alpha(V, Rp, G, mask)
fr = f['frac'][f['read']].mean()
check('off-law sheet leaves a big residual fraction', fr > 0.5,
      'resid/reach %.3f' % fr, '> 0.5')

# ------------------------------------------------------- 4 the shuffled floor
a_true = 0.5
V = G.copy()
V[mask] = (a_true * Rp + (1 - a_true) * G)[mask]
f_true = R3.fit_alpha(V, Rp, G, mask)
Gs = G.copy()
idx = np.flatnonzero(mask.ravel())
perm = rng.permutation(idx)
flat = Gs.reshape(-1, 3)
flat[idx] = flat[perm]
f_sh = R3.fit_alpha(V, Rp, Gs, mask)
check('shuffled ground destroys the fit',
      f_sh['frac'][f_sh['read']].mean() > 8 * max(f_true['frac'][f_true['read']].mean(), 1e-6),
      'frac true %.4f shuffled %.4f' % (f_true['frac'][f_true['read']].mean(),
                                        f_sh['frac'][f_sh['read']].mean()),
      'shuffled >> true')

# ---------------------------------------------------------------- 5 the mask
A = G.copy()
A[mask] = (0.5 * Rp + 0.5 * G)[mask]
m = R3.road_mask(A, G)
check('mask == the painted texels exactly',
      int((m != mask).sum()) == 0, '%d differ' % int((m != mask).sum()), '0')
check('mask is empty when nothing was painted',
      int(R3.road_mask(G, G).sum()) == 0, '%d' % int(R3.road_mask(G, G).sum()), '0')

# ------------------------------------------- 6 the step detector, both answers
Lsmooth = np.zeros((N, N))
Lstep = np.zeros((N, N))
for d in range(1, 15):
    mm = (sd == d)
    Lsmooth[mm] = 90 + 1.2 * d
    Lstep[mm] = 90 + 1.2 * d + (8.0 if d >= 5 else 0.0)
w_s, _ = R3.second_difference(R3.profile(Lsmooth, sd, lo=-1, hi=14))
w_t, at = R3.second_difference(R3.profile(Lstep, sd, lo=-1, hi=14))
check('second difference ~ 0 on a smooth ramp', w_s < 0.01, '%.4f' % w_s, '< 0.01')
check('second difference sees a planted 8-level step',
      w_t > 7.0, '%.4f at d=%s' % (w_t, at), '> 7')

lines += ['', 'SELF-TEST %d/%d  (%d failures)' % (ok, ok + bad, bad)]
R3.log(lines, 's0_selftest.txt')
raise SystemExit(0 if bad == 0 else 1)
