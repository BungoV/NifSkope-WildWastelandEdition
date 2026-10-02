#!/usr/bin/env python3
"""PRTP bake against a brute-force reference (2026-10-01).

  prtp_reference.py <soup.psp> <bakedir> [--probes K] [--rays M] [--red albedo|dir] [--ref <exe>]

The synth gate proves the bake re-traces itself; this proves what a probe's links
RECONSTRUCT matches what the probe actually sees, on a real cell. A test field (not
light): every surface gives albedo x (0.25 + 0.75 max(0, n.s)), sky gives 0 (FO4CS's
relight: sky never enters the fill). tests/prtp_reference.cpp traces K probes with
every triangle tested (no BVH, its own jittered directions); the bake's side is the
relight's own sum: link weight x the surfel's field, over effectiveCoverage
(coverage - unlinked).

Per probe:
  sky   -- worst octant |bake sky share - reference sky share| (full-sphere fraction)
  total -- |E_bake - E_ref| / E_ref, E = the sphere's summed field
  irr   -- band-2 SH irradiance over 26 normals, mean |bake - ref| / mean ref (gated)
  dir   -- sum over octants |bake - reference| / E_ref (reported, not gated)
Exterior cells only: an interior's misses are unlinked (unknown), not zero.
`--red albedo` shuffles the surfels' albedo, `--red dir` mirrors the link directions;
each must FAIL. One verdict line; exit 0 on PASS.

Lane BAKE4 (`.tbk` v4): a link names its side (the cell's back surfel) and the glass
tint on its way, a probe its sky tint per octant. The field a link carries is then
its surfel's, times the tint, per channel; the reference (rebuilt with its own glass,
tests/prtp_reference.cpp) traces the panes itself. With glass in the soup it also gates
  tsky -- worst octant |bake sky share x sky tint - reference sky share through glass|
`--red oneside` reads every link as the front side, `--red glass` ignores the tints.
"""
import math
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import files_in, floordiv, structure, surfel_of, unpack_dir  # noqa: E402

SUN = np.array([0.3, -0.5, 0.81])
SUN /= np.linalg.norm(SUN)
# Gates, as set before the first real run: sky 0.01 of the sphere (about 4 sigma of an
# 8192-ray octant share), total 10% median / 25% p95, dir 0.15 median / 0.35 p95.
SKY_MAX, TOT_MED, TOT_P95, DIR_MED, DIR_P95 = 0.01, 0.10, 0.25, 0.15, 0.35
# The relight stores band-2 SH and a surface reads it as cosine-convolved irradiance, so
# the directional gate is that irradiance over 26 normals: mean |bake - ref| / mean ref.
# Set 2026-10-01 before its first run: 0.10 median / 0.25 p95. The octant split (dir) is
# reported, not gated: a 70-unit surfel near a probe lands whole in one octant.
IRR_MED, IRR_P95 = 0.10, 0.25
A_BAND = np.array([np.pi] + [2 * np.pi / 3] * 3 + [np.pi / 4] * 5)
NORMALS = np.array([(x, y, z) for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)],
                   dtype=np.float64)
NORMALS /= np.linalg.norm(NORMALS, axis=1, keepdims=True)


def sh9(v):
    x, y, z = v[..., 0], v[..., 1], v[..., 2]
    return np.stack([np.full_like(x, 0.282095), 0.488603 * y, 0.488603 * z, 0.488603 * x, 1.092548 * x * y,
                     1.092548 * y * z, 0.315392 * (3 * z * z - 1), 1.092548 * x * z, 0.546274 * (x * x - y * y)], -1)


def irradiance(c):
    return sh9(NORMALS) @ (A_BAND * c)


def field(alb, nrm):
    return alb * (0.25 + 0.75 * max(0.0, float(np.dot(nrm, SUN))))


def surfel_alb(sf):
    # the surfel's albedo, per channel (v3 files: a grey field read as its mean was the same sum)
    return sf['alb'].astype(np.float64) / 255.0


def octant_of(v):
    return int(v[0] < 0) | (int(v[1] < 0) << 1) | (int(v[2] < 0) << 2)


def main(a):
    soup, bake = a[0], a[1]
    k = int(a[a.index('--probes') + 1]) if '--probes' in a else 32
    rays = int(a[a.index('--rays') + 1]) if '--rays' in a else 8192
    red = a[a.index('--red') + 1] if '--red' in a else ''
    here = os.path.dirname(os.path.abspath(__file__))
    ref = a[a.index('--ref') + 1] if '--ref' in a else os.path.join(here, '..', '..', 'release', 'prtp_reference.exe')
    fails = []
    tbks = structure(files_in(bake), fails)
    if fails or not tbks:
        print('reference FAIL: the bake does not read (%s)' % '; '.join(fails[:3] or ['no .tbk']))
        return 1
    rows = [(t, i) for _, t in tbks for i in range(len(t['probes']))]
    pick = [rows[int(j)] for j in np.linspace(0, len(rows) - 1, min(k, len(rows)))]
    rng = np.random.default_rng(7)
    if red == 'albedo':
        for _, t in tbks:
            s = t['surfels'].copy()
            s['alb'] = s['alb'][rng.permutation(len(s))]
            t['surfels'] = s

    work = os.path.join(bake, '_reference')
    os.makedirs(work, exist_ok=True)
    pf = os.path.join(work, 'probes.txt')
    with open(pf, 'w') as f:
        for t, i in pick:
            f.write('%.4f %.4f %.4f\n' % tuple(float(v) for v in t['probes'][i]['pos']))
    out = os.path.join(work, 'reference.tsv')
    r = subprocess.run([ref, soup, pf, str(rays), *('%g' % v for v in SUN), out], capture_output=True, text=True)
    if r.returncode != 0:
        print('reference FAIL: the tracer said rc %d %s' % (r.returncode, r.stderr.strip()[:200]))
        return 1
    R = np.loadtxt(out, comments='#', ndmin=2)

    sky_err, tot_err, dir_err, irr_err, tsky_err = [], [], [], [], []
    glassy = R.shape[1] >= 74
    for (t, i), row in zip(pick, R):
        pr = t['probes'][i]
        ref_sky = row[1:41:5]
        ref_L = np.stack([row[3:41:5], row[4:41:5], row[5:41:5]], 1).mean(1)   # per octant, grey
        ref_sh = row[41:50]
        e_ref = float(ref_L.sum())
        pk = tuple(floordiv(pr['pos'][c], t['cell']) for c in range(3))
        T = np.zeros(8)
        C = np.zeros(9)
        a, b = int(pr['off']), int(pr['off'] + pr['cnt'])
        for l, x in zip(t['links'][a:b], t['lext'][a:b]):
            sf = surfel_of(t, pk, l, x, ignore_side=(red == 'oneside'))
            if sf is None:
                continue
            n = sf['nrm'].astype(np.float64) / 32767.0
            d = unpack_dir(l['dir'])
            if red == 'dir':
                d = -d
            tint = np.ones(3) if red == 'glass' else x['tint'].astype(np.float64) / 255.0
            v = float(l['w']) * float(pr['scale']) * field(float(np.mean(surfel_alb(sf) * tint)), n)
            T[octant_of(d)] += v
            C += sh9(d) * v * 4 * np.pi
        if glassy:
            st = t['pext'][i]['skytint'].astype(np.float64).mean(1) / 255.0
            if red == 'glass':
                st = np.ones(8)
            ref_tsky = row[50:74].reshape(8, 3).mean(1)
            tsky_err.append(float(np.max(np.abs(pr['sky'].astype(np.float64) / 8.0 * st - ref_tsky))))
        eff = max(float(pr['cov']) - float(pr['unl']), 1e-3)
        T /= eff
        C /= eff
        e_bake = float(T.sum())
        sky_err.append(float(np.max(np.abs(pr['sky'].astype(np.float64) / 8.0 - ref_sky))))
        if e_ref > 1e-4:
            tot_err.append(abs(e_bake - e_ref) / e_ref)
            dir_err.append(float(np.abs(T - ref_L).sum()) / e_ref)
            ir = irradiance(ref_sh)
            irr_err.append(float(np.abs(irradiance(C) - ir).mean() / max(ir.mean(), 1e-6)))
    sky_err, tot_err, dir_err, irr_err, tsky_err = map(np.array, (sky_err, tot_err, dir_err, irr_err, tsky_err))
    if sky_err.max() > SKY_MAX:
        fails.append('sky worst %.4f > %.2f' % (sky_err.max(), SKY_MAX))
    tsky = ''
    if glassy and len(tsky_err):
        tsky = '; sky through glass worst %.4f' % tsky_err.max()
        if tsky_err.max() > SKY_MAX:
            fails.append('sky through glass worst %.4f > %.2f' % (tsky_err.max(), SKY_MAX))
    tm, tp = np.median(tot_err), np.percentile(tot_err, 95)
    dm, dp = np.median(dir_err), np.percentile(dir_err, 95)
    if tm > TOT_MED or tp > TOT_P95:
        fails.append('total median %.3f / p95 %.3f over %.2f / %.2f' % (tm, tp, TOT_MED, TOT_P95))
    im, ip = np.median(irr_err), np.percentile(irr_err, 95)
    if im > IRR_MED or ip > IRR_P95:
        fails.append('irradiance median %.3f / p95 %.3f over %.2f / %.2f' % (im, ip, IRR_MED, IRR_P95))
    worst = int(np.argmax(tot_err))
    print('reference %s%s: %d probes x %d rays (brute force, every triangle); sky worst %.4f%s; total median %.3f '
          'p95 %.3f worst %.3f (probe %d); irradiance median %.3f p95 %.3f; octant split median %.3f p95 %.3f; %s'
          % ('PASS' if not fails else 'FAIL', ' [red ' + red + ']' if red else '', len(pick), rays,
             sky_err.max(), tsky, tm, tp, tot_err.max(), worst, im, ip, dm, dp, '; '.join(fails) if fails else 'sound'))
    with open(os.path.join(work, 'per_probe%s.tsv' % ('_' + red if red else '')), 'w') as f:
        f.write('n\tx\ty\tz\tsky\ttotal\tdir\tirr\n')
        for n, ((t, i), s, te, de, ie) in enumerate(zip(pick, sky_err, tot_err, dir_err, irr_err)):
            p = t['probes'][i]['pos']
            f.write('%d\t%.0f\t%.0f\t%.0f\t%.4f\t%.4f\t%.4f\t%.4f\n' % (n, p[0], p[1], p[2], s, te, de, ie))
    return 0 if not fails else 1


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
