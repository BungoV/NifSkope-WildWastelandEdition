"""Lane SKYA: the physical sky's CPU math (src/gl/atmomath.h) against the numpy model and published numbers.
Runs no NifSkope. Builds tests/atmomath/atmomath_dump.cpp with the MSYS2 UCRT64 g++ into a scratch folder.

    python -I tests/spells/atmomath_check.py <scratch dir> [--red flat|irrflat|phase|noext|zenithw|skyzero|tintflat|hemi]

Gates:
  transmittance   cpuTrans (the key light's T, ATMOSUN1) vs atmo_model.transmittance (< 0.005 absolute)
  sun tint        mode Tint keeps the weather key's luminance at every sun height; at 45 deg it is the key's own
                  colour scaled by T45's chromaticity; a 3-degree sun is warmer (R/B up) -- Hillaire 2020 sec. 4
  sun full        mode Full at 45 deg = the weather's Day luminance exactly; a 3-degree sun is dimmer
  sky irradiance  skyIrradiance's colour change from 45 deg (R/B and G/B against 45 deg's, what the ambient tint
                  uses) vs the model's full sky (single + multiple scattering, cosine-weighted hemisphere) at 20,
                  10 and 3 deg: within 15 %
  ambient         45 deg: the weather's ambient exactly; 3 deg: same luminance, warmer
  moon phase      full = 1; quarter (90 deg) 0.07..0.12 of full (Krisciunas & Schaefer 1991 eq. 20; the quarter moon
                  about 1/10 of the full one, Allen's Astrophysical Quantities); new < 0.01; red "nophase" all 1
  stars           T(mu) / T(zenith) (starExtinction): 1 at the zenith; matches the model's T ratio (< 0.005) at
                  mu 0.5, 0.1, 0.03; at mu 0.03 the light is under half and redder (R/B up) -- Bruneton & Neyret 2008
  sky mean        skyViewMeanLum on a synthetic table of max(cos zenith, 0): the solid-angle mean is 1/2 (< 0.01);
                  red zenithw (bands weighted alike) gives 2/pi
  sky in view     (SKY2B EXPO2) skyMeanInView on the same table, a 50-degree camera at pitch 85 (all sky) and 0
                  (all sky; upper half sky) vs max(cos zenith, 0) averaged over the sky texels' own rays (< 0.01);
                  red hemi (EXPO1's hemisphere mean, 1/2) fails the pitch-85 camera
  exposure        the frame mean with the sky = cover x scene + (1 - cover) x sky; full cover = the scene alone
  weather tint    (ATMOCHECK1, the Sky Color row's Weather Tint) a synthetic dome of 5 rings (10 jittered vertices
                  merge to 5): the physical luminance kept exactly at every elevation; the chromaticity = the
                  Horizon's at 0 deg and below, the Sky-Upper's at the zenith, between at 20 deg; red "flat" (the
                  horizon color everywhere) fails the zenith
The sun/ambient gates read skyKeyLight, the function the build ships (atmosphere.cpp wwAtmoKeyLight).
Each red must make one gate fail: flat (WW_ATMO_SUN_RED=flat: T held at 45 deg, a low sun no warmer), phase (the
vanilla moon), skyzero; irrflat is the checker's own (no colour change), proving the irradiance gate can fail.
"""
import os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import atmo_model as A   # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))


def lum(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def main():
    a = sys.argv[1:]
    out = a[0]
    red = a[a.index('--red') + 1] if '--red' in a else ''
    os.makedirs(out, exist_ok=True)
    exe = os.path.join(out, 'atmomath_dump.exe')
    src = os.path.join(ROOT, 'tests', 'atmomath', 'atmomath_dump.cpp')
    gxx = r'C:\msys64\ucrt64\bin\g++.exe'
    env = dict(os.environ, PATH=r'C:\msys64\ucrt64\bin;' + os.environ.get('PATH', ''))
    r = subprocess.run([gxx, '-O2', '-std=c++17', '-I', os.path.join(ROOT, 'src'), src, '-o', exe],
                       capture_output=True, text=True, env=env)
    if r.returncode:
        print(r.stderr)
        return 2
    lines = subprocess.run([exe], capture_output=True, text=True, env=env).stdout.split('\n')
    D = {}
    for l in lines:
        p = l.split()
        if p:
            D.setdefault(p[0], []).append(p[1:])
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        print(('PASS ' if ok else 'FAIL ') + name + ': ' + msg)
        fails += 0 if ok else 1

    haze = [tuple(float(x) for x in h[1:]) for h in D['haze']]
    err = 0.0
    for t in D['trans']:
        k, mu, rgb = int(t[0]), float(t[1]), np.array([float(x) for x in t[2:]])
        P = A.params(haze=haze[k])
        ref = A.transmittance(P, np.array([A.RB]), np.array([mu]), 4000)[0]
        err = max(err, np.abs(rgb - ref).max())
    gate('transmittance', err < 0.005, 'cpuTrans vs the model, worst |dT| = %.5f (< 0.005) at 12 rays' % err)

    key = np.array([float(x) for x in D['key'][0]])
    ambin = np.array([float(x) for x in D['ambin'][0]])
    rf = '1' if red == 'flat' else '0'
    sun = {(int(s[1]), s[2]): np.array([float(x) for x in s[3:6]]) for s in D['skykey'] if s[0] == rf}
    amb = {s[2]: np.array([float(x) for x in s[6:9]]) for s in D['skykey'] if s[0] == rf and s[1] == '1'}
    t1_45, t1_3 = sun[(1, '45')], sun[(1, '3')]
    dl = max(abs(lum(t1_45) - lum(key)), abs(lum(t1_3) - lum(key))) / lum(key)
    warm = (t1_3[0] / t1_3[2]) > (t1_45[0] / t1_45[2]) * 1.1
    gate('sun tint', dl < 1e-4 and warm, 'luminance kept to %.6f (< 1e-4), R/B at 3 deg %.3f vs 45 deg %.3f (warmer)'
         % (dl, t1_3[0] / t1_3[2], t1_45[0] / t1_45[2]))
    f45, f3 = sun[(2, '45')], sun[(2, '3')]
    dl = abs(lum(f45) - lum(key)) / lum(key)
    gate('sun full', dl < 1e-4 and lum(f3) < 0.7 * lum(f45),
         '45 deg luminance off by %.6f (< 1e-4), 3 deg / 45 deg = %.3f (< 0.7)' % (dl, lum(f3) / lum(f45)))
    d45 = np.abs(amb['45'] - ambin).max()
    dl3 = abs(lum(amb['3']) - lum(ambin)) / lum(ambin)
    gate('ambient', d45 < 1e-5 and dl3 < 1e-4 and amb['3'][0] / amb['3'][2] > ambin[0] / ambin[2],
         '45 deg off by %.7f, 3 deg luminance kept to %.6f, R/B %.3f vs %.3f' % (d45, dl3, amb['3'][0] / amb['3'][2],
                                                                              ambin[0] / ambin[2]))
    moon = {int(m[0]): (float(m[1]), float(m[2]), float(m[3])) for m in D['moon']}
    col = 2 if red == 'phase' else 1
    full, quarter, new = moon[0][col], moon[2][col], moon[4][col]
    sym = all(abs(moon[p][col] - moon[8 - p][col]) < 1e-7 for p in (1, 2, 3))
    gate('moon phase', abs(full - 1) < 1e-6 and 0.07 < quarter < 0.12 and new < 0.01 and sym
         and all(moon[p][2] == 1.0 for p in range(8)),
         'full %.4f, quarter %.4f (0.07..0.12), new %.6f (< 0.01), waxing = waning %s' % (full, quarter, new, sym))
    irr = {int(s[0]): np.array([float(x) for x in s[1:]]) for s in D['irr']}
    P = A.params(haze=haze[1])
    psi = A.MsTable(P, nu=16, nv=8, k=6, n=12)
    dirs, w = [], []
    for i in range(8):
        for j in range(16):
            el = (i + 0.5) / 8 * 90.0
            dirs.append(A.view_dir(el, (j + 0.5) / 16 * 360.0))
            w.append(np.sin(np.radians(el)) * np.cos(np.radians(el)))
    w = np.array(w)

    def model_e(deg):
        L = A.sky(P, 0.0, dirs, float(deg), psi, n=30)
        return (L * w[:, None]).sum(0) / w.sum()
    m45, c45 = model_e(45), irr[45]
    worst = 0.0
    for deg in (20, 10, 3):
        m, c = model_e(deg), irr[deg]
        for a, b in ((0, 2), (1, 2)):
            want = (m[a] / m[b]) / (m45[a] / m45[b])
            got = (c[a] / c[b]) / (c45[a] / c45[b])
            if red == 'irrflat':
                got = 1.0
            worst = max(worst, abs(got / want - 1))
    gate('sky irradiance', worst < 0.15, 'colour change vs 45 deg against the model, worst %.3f (< 0.15) at 20/10/3 deg'
         % worst)
    rx = '1' if red == 'noext' else '0'
    st = {float(s[1]): np.array([float(x) for x in s[2:5]]) for s in D['star'] if s[0] == rx}
    tz = A.transmittance(P, np.array([A.RB]), np.array([1.0]), 4000)[0]
    err = max(np.abs(st[mu] - A.transmittance(P, np.array([A.RB]), np.array([mu]), 4000)[0] / tz).max()
              for mu in (0.5, 0.1, 0.03))
    lo = st[0.03]
    gate('stars', np.abs(st[1.0] - 1).max() < 1e-6 and err < 0.005 and lum(lo) < 0.5 and lo[0] / lo[2] > 1.1,
         'zenith %s, worst vs the model %.5f (< 0.005), mu 0.03: luminance %.3f (< 0.5), R/B %.3f (> 1.1)'
         % (np.round(st[1.0], 6), err, lum(lo), lo[0] / lo[2]))
    sm = [float(x) for x in D['skymean'][0]]
    got = sm[1] if red == 'zenithw' else sm[0]
    gate('sky mean', abs(got - 0.5) < 0.01, 'hemisphere mean of cos zenith %.5f (want 0.5, < 0.01)' % got)
    # SKY2B (EXPO2): the sky in view = the mean of max(cos zenith, 0) over the sky texels' own rays
    werr, says = 0.0, []
    for v in D['viewmean']:
        c, m, so = int(v[0]), float(v[1]), float(v[2])
        f0, fx, fy = (np.array([float(x) for x in v[k:k + 3]]) for k in (3, 6, 9))
        qw, qh = 120, 68
        a = (np.arange(qw) + 0.5) / qw
        b = (np.arange(qh) + 0.5) / qh
        d = f0[None, None, :] + fx[None, None, :] * a[None, :, None] + fy[None, None, :] * b[:, None, None]
        cz = np.maximum(d[..., 2] / np.linalg.norm(d, axis=2), 0)
        if c == 1:
            cz = cz[qh // 2:]
        ref = float(cz.mean())
        got = 0.5 if red == 'hemi' else m
        werr = max(werr, abs(got - ref))
        says.append('%d: %.4f vs %.4f (share %.3f)' % (c, got, ref, so))
    gate('sky in view', werr < 0.01, 'skyMeanInView vs the texel rays, worst %.4f (< 0.01); %s' % (werr, '; '.join(says)))
    e = [float(x) for x in D['expo'][0]]
    want = 0.6 * 0.2 + 0.4 * 1.5
    got = e[1] if red == 'skyzero' else e[0]
    gate('exposure', abs(got - want) < 1e-6 and abs(e[2] - 0.2) < 1e-6,
         'cover 0.6: %.6f (want %.6f); full cover %.6f (want 0.2)' % (got, want, e[2]))
    tk = 1 if red == 'tintflat' else 0
    T = {float(x[2]): np.array([float(v) for v in x[3:6]]) for x in D['tint'] if int(x[0]) == tk}
    rings = int(D['tint'][0][1])
    phys = np.array([0.3, 0.5, 0.9])
    H, U = np.array([1.0, 0.5, 0.2]), np.array([0.2, 0.4, 1.0])

    def chroma(c):
        return c / c.sum()
    lerr = max(abs(lum(T[el]) - lum(phys)) for el in T)
    zen = np.abs(chroma(T[90.0]) - chroma(U)).max()
    hor = max(np.abs(chroma(T[0.0]) - chroma(H)).max(), np.abs(chroma(T[-5.0]) - chroma(H)).max())
    mid = chroma(T[20.0])
    between = bool(np.all(mid >= np.minimum(chroma(H), chroma(U)) - 1e-6) and np.all(mid <= np.maximum(chroma(H), chroma(U)) + 1e-6)
                   and np.abs(mid - chroma(H)).max() > 0.01)
    gate('weather tint', rings == 5 and lerr < 1e-5 and zen < 1e-4 and hor < 1e-4 and between,
         'rings %d (want 5), luminance worst %.2g, zenith chroma vs Sky-Upper %.2g, horizon vs Horizon %.2g, 20 deg between %s'
         % (rings, lerr, zen, hor, between))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
