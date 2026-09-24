"""TILING3 hypothesis D, part 2 -- HOW MUCH of vanilla's fine colour detail is
derivable from vanilla's own `_msn`, and in what functional form?

d1_geology.py found the signal: the colour residual's ORIENTATION agrees with the
`_msn` residual's at +0.40 / +0.385 against floors of +0.012 / +0.070, and the
single strongest column of the linear fit is CURVATURE (r = -0.156 / -0.183),
where every Lambert column reads 0.00x.  So the term is not a light direction; it
is a cavity/curvature term -- and TILING2's azimuth sweep could never have found
it, because a sweep over light directions only ever spans nx, ny, nz.

This script does four things:

  K0  FIXES THE KNOWN-ANSWER CONTROL.  d1's K1 read 0.434 for an injected 0.50
      because the injected term was a CLIPPED Lambert, which is not in the span
      of nx,ny,nz.  Re-run with an UNCLIPPED Lambert (exactly in the span) to
      prove the fit machinery itself is exact, and keep the clipped number as the
      measured cost of that one nonlinearity.  A control that under-reads makes
      every verdict below a LOWER bound, which is the safe direction.

  D4  THE CEILING: a rich basis of everything a surface offers -- Lambert (3),
      slope, nz^2, and curvature / divergence / roughness / integrated height at
      four scales each -- fit to vanilla's colour residual.  R^2 is then "the most
      of vanilla's fine detail that ANY per-texel function of vanilla's own _msn
      could explain".  Floors: the whole basis rebuilt from the _msn's phase twin,
      the same from OUR _msn, and the same number of noise columns.

  D5  the same on 5 more of TILING2's 22 sheets (the brief's 5-sheet requirement).

  D6  WHERE THE REST IS.  The variance split: shading/curvature term vs what
      remains, and the remainder's spectrum and moments -- which is what
      hypotheses A/B/C are then tested against.

    python d2_deep.py   ->  logs/d2_deep.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
import d1_geology as D1                                       # noqa: E402

TILES = [('t2024', -20, 24), ('t2020', -20, 20)]
# 5 more of TILING2's 22, spread over the lattice (t0_pick.py's list)
MORE = [(-64, -60), (-4, -20), (-36, 16), (44, 32), (-4, 56)]


def hp(a, r=2):
    return T.hp_residual(np.asarray(a, np.float64), r=r)


def integrate_height(n):
    """Frankot-Chellappa: the height field whose gradient best matches the normal
    map, in the least-squares sense, solved in the frequency domain.

    p = -nx/nz, q = -ny/nz are the surface slopes; H(k) = (-i kx P - i ky Q) /
    (kx^2 + ky^2).  Periodic, which is wrong at the sheet border and right
    everywhere the fine detail lives."""
    nz = np.maximum(np.abs(n[:, :, 2]), 1e-3) * np.sign(np.where(n[:, :, 2] == 0, 1, n[:, :, 2]))
    p = -n[:, :, 0] / nz
    q = -n[:, :, 1] / nz
    h, w = p.shape
    ky = np.fft.fftfreq(h)[:, None] * 2 * np.pi
    kx = np.fft.fftfreq(w)[None, :] * 2 * np.pi
    den = kx ** 2 + ky ** 2
    den[0, 0] = 1.0
    P = np.fft.fft2(p)
    Q = np.fft.fft2(q)
    H = (-1j * kx * P - 1j * ky * Q) / den
    H[0, 0] = 0.0
    return np.real(np.fft.ifft2(H))


def lap(a, r):
    a = np.asarray(a, np.float64)
    return S._box(a, r) - a


def rich_basis(n):
    """Everything a per-texel function of THIS surface can offer."""
    nx, ny, nz = n[:, :, 0], n[:, :, 1], n[:, :, 2]
    slope = np.sqrt(nx * nx + ny * ny)
    cols = [nx, ny, nz, slope, nz * nz]
    names = ['nx', 'ny', 'nz', 'slope', 'nz^2']
    div = np.zeros_like(nx)
    div[1:-1, 1:-1] = ((nx[1:-1, 2:] - nx[1:-1, :-2])
                       + (ny[2:, 1:-1] - ny[:-2, 1:-1])) * 0.5
    # THE RAW curvature column MUST be here: the first run of this script left it
    # out (only its box-smoothed forms were in) and the "rich" basis then read a
    # LOWER R^2 than d1_geology.py's six columns -- impossible for a superset, and
    # the proof the basis was not one.  A ceiling basis has to contain every
    # column of every narrower basis it claims to dominate.
    cols.append(div); names.append('div@0')
    hgt = integrate_height(n)
    for r in (1, 2, 4, 8):
        cols.append(S._box(div, r)); names.append('div@%d' % r)
        cols.append(lap(nz, r)); names.append('lapNz@%d' % r)
        cols.append(S._box(slope, r) - slope); names.append('rough@%d' % r)
        cols.append(hgt - S._box(hgt, r)); names.append('hgtHP@%d' % r)
    return np.stack([hp(c).ravel() for c in cols], 1), names


def r2_and_fit(y, X):
    y = np.asarray(y, np.float64).ravel()
    y = y - y.mean()
    A = np.concatenate([X - X.mean(0, keepdims=True), np.ones((X.shape[0], 1))], 1)
    beta, *_ = np.linalg.lstsq(A, y, rcond=None)
    pred = A @ beta
    res = y - pred
    sy = float((y * y).sum())
    return (float(1.0 - (res * res).sum() / sy) if sy > 0 else 0.0), pred, res


def moments(a):
    x = np.asarray(a, np.float64).ravel()
    x = x - x.mean()
    s = x.std()
    if s <= 0:
        return 0.0, 0.0, 0.0
    z = x / s
    return float(s), float((z ** 3).mean()), float((z ** 4).mean())


def main():
    rng = np.random.default_rng(31)
    L = ['TILING3 hypothesis D part 2 -- the ceiling of what vanilla`s own _msn explains', '']
    out = {}

    # ------------------------------------------------------- K0, the fixed control
    L.append('=' * 78)
    L.append('K0  THE KNOWN-ANSWER CONTROL, FIXED')
    L.append('=' * 78)
    n = D1.van_normals(-20, 24)
    X6, _ = D1.basis(n)
    lo = np.array([0.6, 0.3, 0.7071]); lo /= np.linalg.norm(lo)
    lam_un = hp((n * lo).sum(2) * 255.0)                 # UNCLIPPED: in the span
    lam_cl = hp(np.clip((n * lo).sum(2), 0, 1) * 255.0)  # clipped: not in the span
    for lab, fld in (('UNCLIPPED Lambert (in the span)', lam_un),
                     ('clipped Lambert (not in the span)', lam_cl)):
        sh = fld / fld.std()
        for share in (0.50, 0.20, 0.05):
            y = np.sqrt(share) * sh + np.sqrt(1 - share) * rng.normal(0, 1, sh.shape)
            got, _, _ = r2_and_fit(y, X6)
            L.append('   %-36s share %.2f -> R^2 %.4f  (%+.1f%% of truth)'
                     % (lab, share, got, 100.0 * (got / share - 1.0)))
    L.append('   => the fit machinery is EXACT on a term inside its span; the shortfall')
    L.append('      on a clipped term is the clipping, and it means every R^2 below is a')
    L.append('      LOWER bound on what the surface explains.')
    # the rich basis's own overfit floor, measured not assumed
    nR, namesR = rich_basis(n)
    L.append('   rich basis: %d columns over %d samples; overfit floor (noise columns) ='
             % (nR.shape[1], nR.shape[0]))
    r_of, _, _ = r2_and_fit(rng.normal(0, 1, nR.shape[0]), rng.normal(0, 1, nR.shape))
    L.append('      R^2 = %.6f' % r_of)
    L.append('')

    # ------------------------------------------------------------- D4, the ceiling
    for name, cx, cy in TILES:
        van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        vhp = hp(van)
        nv = D1.van_normals(cx, cy)
        no = D1.our_normals(name, cx, cy)
        nt = np.stack([S.phase_twin(nv[:, :, k], seed=40 + k) for k in range(3)], 2)
        Xv, names = rich_basis(nv)
        Xo, _ = rich_basis(no)
        Xt, _ = rich_basis(nt)
        rv, pred, res = r2_and_fit(vhp, Xv)
        ro, _, _ = r2_and_fit(vhp, Xo)
        rt, _, _ = r2_and_fit(vhp, Xt)
        L.append('=' * 78)
        L.append('D4  chunk (%d,%d)   vanilla colour high-pass SD = %.3f' % (cx, cy, vhp.std()))
        L.append('=' * 78)
        L.append('   rich basis on VANILLA _msn   R^2 = %.5f   (%.1f%% of the residual`s variance)'
                 % (rv, 100 * rv))
        L.append('   the same on its PHASE TWIN   R^2 = %.5f   FLOOR' % rt)
        L.append('   the same on OUR  _msn        R^2 = %.5f   FLOOR' % ro)
        L.append('   %d noise columns             R^2 = %.5f   OVERFIT FLOOR' % (Xv.shape[1], r_of))
        # single-column ranking
        rs = sorted(((abs(T.corr(vhp, Xv[:, k])), names[k], T.corr(vhp, Xv[:, k]))
                     for k in range(len(names))), reverse=True)
        L.append('   strongest single columns: '
                 + '  '.join('%s %+.4f' % (nm, r) for _a, nm, r in rs[:6]))
        # D6, the split
        sd_v, sk_v, ku_v = moments(vhp)
        sd_p, sk_p, ku_p = moments(pred)
        sd_r, sk_r, ku_r = moments(res)
        L.append('   THE SPLIT  total SD %.3f = explained SD %.3f + remainder SD %.3f'
                 % (sd_v, sd_p, sd_r))
        L.append('   moments    vanilla skew %+.3f kurt %.3f | remainder skew %+.3f kurt %.3f'
                 ' | Gaussian 0.000 / 3.000' % (sk_v, ku_v, sk_r, ku_r))
        out[name] = dict(r2=rv, r2_twin=rt, r2_ours=ro, r2_overfit=r_of,
                         sd=sd_v, sd_expl=sd_p, sd_rem=sd_r,
                         skew=sk_v, kurt=ku_v, skew_rem=sk_r, kurt_rem=ku_r,
                         top=[(nm, r) for _a, nm, r in rs[:6]])
        L.append('')

    # ------------------------------------------------------- D5, five more sheets
    L.append('=' * 78)
    L.append('D5  five more of TILING2`s 22 shipped sheets (vanilla only -- no bake of ours)')
    L.append('=' * 78)
    L.append('   %-12s %8s %8s %8s %8s %8s' % ('chunk', 'D1 R^2', 'twin', 'D2 env', 'D3 dir', 'twin'))
    five = {}
    for (cx, cy) in MORE:
        try:
            van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
            nv = D1.van_normals(cx, cy)
        except Exception as e:
            L.append('   (%d,%d)  MISSING: %s' % (cx, cy, e))
            continue
        vhp = hp(van)
        nt = np.stack([S.phase_twin(nv[:, :, k], seed=50 + k) for k in range(3)], 2)
        Xv, _ = rich_basis(nv)
        Xt, _ = rich_basis(nt)
        rv, _, _ = r2_and_fit(vhp, Xv)
        rt, _, _ = r2_and_fit(vhp, Xt)
        mv = hp(nv[:, :, 2] * 255.0)
        mt = hp(nt[:, :, 2] * 255.0)
        e = D1.d2(vhp, mv)
        d = D1.d3(vhp, mv)
        dt = D1.d3(vhp, mt)
        L.append('   (%4d,%4d) %8.5f %8.5f %8.4f %8.4f %8.4f' % (cx, cy, rv, rt, e, d, dt))
        five['%d,%d' % (cx, cy)] = dict(r2=rv, r2_twin=rt, d2=e, d3=d, d3_twin=dt)
    out['five'] = five

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'd2_deep.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'd2_deep.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
