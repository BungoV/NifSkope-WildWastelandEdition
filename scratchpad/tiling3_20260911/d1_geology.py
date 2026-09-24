"""TILING3 hypothesis D -- is vanilla's fine colour detail GEOLOGY, i.e. shading
or material selection driven by a surface finer than our 128-u LAND grid?

The brief's premise was that TILING2's slope/shading candidates "came from OUR
128-u grid".  THEY DID NOT: `t4_corr.py:normals()` reads `S.van_sheet(cx,cy,'_msn')`,
which is Bethesda's own shipped 512x512 `_msn` sheet -- the same resolution as the
colour sheet.  So the brief's D is not an untested idea; TILING2's best-of-8-azimuth
row (r = 0.0060 / 0.0045 against twin floors of -0.0017 / -0.0051) already tested
its LINEAR form.  This script tests D properly and in three forms, because a
"geology" term need not be a linear shading:

  D1  the BEST POSSIBLE linear shading -- a least-squares fit onto nx, ny, nz
      (which span every un-clipped Lambert dot(n,l) at every azimuth AND
      elevation, so this subsumes any azimuth sweep), plus slope magnitude,
      nz^2 and a curvature proxy.  Statistic R^2.
  D2  the ENVELOPE -- corr(blur(|colour hp|), blur(|msn hp|)).  Survives a sign
      flip and any monotone recolouring: "where the ground is rough, the colour
      is detailed".
  D3  the DIRECTION -- structure-tensor orientation agreement over 8x8 windows,
      coherence-weighted, mean(cos 2 dtheta).  Catches streaks/ridges/drainage
      lines that agree in direction without agreeing in value.

Every number has floors beside it:
  * the PHASE TWIN of the predictor (same power spectrum, alignment destroyed) --
    the one place TILING2 established the twin IS a valid floor (a structure
    statistic);
  * OUR OWN `_msn` for the same chunk, which must stay at the floor;
  * for D1 the overfit floor, and for D3 chance (0).

And the known-answer controls run FIRST, before any verdict is read:
  K1  a synthetic colour residual = a*shading(vanilla _msn) + noise at a known
      variance share must read R^2 within 10 % of that share (D1);
  K2  the same synthetic must read high on D2 and D3;
  K3  the colour residual against itself must read D3 = +1.000 and against its
      own 90-degree rotation -1.000.

    python d1_geology.py   ->  logs/d1_geology.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
sys.path.insert(0, HERE)
sys.path.insert(0, T2)
sys.path.insert(0, SP)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

TILES = [('t2024', -20, 24), ('t2020', -20, 20)]
RUNG = os.path.join(T2, 'out', 'rung')


def hp(a):
    return T.hp_residual(np.asarray(a, np.float64), r=2)


def _decode_msn(path):
    """THE TERRAIN `_msn` CHANNEL ORDER IS NOT (X,Y,Z) -> (R,G,B).

    `src/lodgen.cpp:5566` states it and says how it was measured, two
    independent ways, on 2026-09-07: **R = east, G = UP, B = north** -- up lives
    in green, in the 6-bit channel of RGB565, and predicting each cell's normal
    from VHGT scored a mean error of 0.0719 that way against 0.2401 for the next
    best orientation.  Vanilla's sheet agrees on sight: G means 242.5 with a
    minimum of 174 (never below 128), R and B straddle 128.

    This function returns the conventional (x=east, y=north, z=up) so everything
    downstream can be read normally.  The first run of this lane decoded R,G,B as
    x,y,z, which permuted the columns of the linear fit (harmless, R^2 is
    invariant) but built the CURVATURE column out of east and UP instead of east
    and north, and read the orientation off the north channel while calling it
    nz.  Re-run after the director's note."""
    d = S.Dds(path)
    c = d.level(0).astype(np.float64)[:, :, :3] / 255.0 * 2.0 - 1.0
    n = np.stack([c[:, :, 0], c[:, :, 2], c[:, :, 1]], 2)   # east, north, up
    ln = np.sqrt((n ** 2).sum(2))
    return n / np.maximum(ln, 1e-6)[:, :, None]


def van_normals(cx, cy):
    return _decode_msn(S.van_sheet(cx, cy, '_msn'))


def our_normals(name, cx, cy):
    return _decode_msn(os.path.join(RUNG, name, 'tex',
                                    'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy)))


# ------------------------------------------------------------------ D1, the fit

def basis(n):
    """Every predictor a surface can offer a LINEAR shading model, high-passed.

    nx, ny, nz span dot(n,l) for every light direction, so fitting them fits the
    best Lambert at every azimuth and elevation at once (and any sum of lights,
    and any sky term that is linear in n).  The three extra columns are the
    obvious NON-linear ones a material-selection rule would use."""
    nx, ny, nz = n[:, :, 0], n[:, :, 1], n[:, :, 2]
    slope = np.sqrt(nx * nx + ny * ny)
    curv = np.zeros_like(nx)
    curv[1:-1, 1:-1] = ((nx[1:-1, 2:] - nx[1:-1, :-2])
                        + (ny[2:, 1:-1] - ny[:-2, 1:-1])) * 0.5
    cols = [nx, ny, nz, slope, nz * nz, curv]
    names = ['nx', 'ny', 'nz', 'slope', 'nz^2', 'curv']
    return np.stack([hp(c).ravel() for c in cols], 1), names


def r2_fit(y, X):
    """R^2 of the least-squares fit of y onto X (an intercept is added)."""
    y = np.asarray(y, np.float64).ravel()
    y = y - y.mean()
    A = np.concatenate([X - X.mean(0, keepdims=True),
                        np.ones((X.shape[0], 1))], 1)
    beta, *_ = np.linalg.lstsq(A, y, rcond=None)
    res = y - A @ beta
    sy = float((y * y).sum())
    return float(1.0 - (res * res).sum() / sy) if sy > 0 else 0.0


# --------------------------------------------------------------- D2, the envelope

def envelope(a, r=4):
    return S._box(np.abs(np.asarray(a, np.float64)), r)


def d2(vhp, mhp, r=4):
    return T.corr(envelope(vhp, r), envelope(mhp, r))


# -------------------------------------------------------------- D3, the direction

def orient(a, win=8):
    """Structure-tensor orientation per `win` x `win` window, and its coherence.

    Returns (c2, s2, w): cos 2theta, sin 2theta and the coherence weight, one per
    window.  Doubling the angle is what makes an orientation (mod 180) into a
    vector that can be averaged."""
    a = np.asarray(a, np.float64)
    gy, gx = np.gradient(a)
    jxx, jyy, jxy = gx * gx, gy * gy, gx * gy
    h, w = a.shape
    H, W = h // win, w // win

    def blk(m):
        return m[:H * win, :W * win].reshape(H, win, W, win).sum((1, 3))
    Jxx, Jyy, Jxy = blk(jxx), blk(jyy), blk(jxy)
    num = np.sqrt((Jxx - Jyy) ** 2 + 4.0 * Jxy ** 2)
    den = Jxx + Jyy
    coh = num / np.maximum(den, 1e-12)
    c2 = (Jxx - Jyy) / np.maximum(num, 1e-12)
    s2 = (2.0 * Jxy) / np.maximum(num, 1e-12)
    return c2, s2, coh * den          # weight = coherent energy


def d3(a, b, win=8):
    ca, sa, wa = orient(a, win)
    cb, sb, wb = orient(b, win)
    w = np.sqrt(np.maximum(wa, 0) * np.maximum(wb, 0))
    cosd = ca * cb + sa * sb          # cos(2 theta_a - 2 theta_b)
    sw = w.sum()
    return float((cosd * w).sum() / sw) if sw > 0 else 0.0


# ------------------------------------------------------------------------- main

def main():
    rng = np.random.default_rng(11)
    L = ['TILING3 hypothesis D -- is vanilla`s fine colour detail geology?', '']
    L.append('THE BRIEF`S PREMISE IS WRONG AND IS RECORDED AS SUCH: TILING2`s slope and')
    L.append('shading candidates did NOT come from our 128-u grid. t4_corr.py:normals()')
    L.append('reads S.van_sheet(cx,cy,"_msn") = Bethesda`s shipped 512x512 _msn sheet,')
    L.append('the same resolution as the colour sheet. This script re-tests D anyway,')
    L.append('in three forms, because TILING2 only tested its LINEAR best-of-8 form.')
    L.append('')
    out = {}

    # ---------------------------------------------------------------- K, first
    L.append('=' * 78)
    L.append('KNOWN-ANSWER CONTROLS (run before any verdict)')
    L.append('=' * 78)
    n = van_normals(-20, 24)
    X, names = basis(n)
    lo = np.array([0.6, 0.3, 0.7071])
    lo /= np.linalg.norm(lo)
    shade = hp(np.clip((n * lo).sum(2), 0, 1) * 255.0)
    for share in (0.50, 0.20, 0.05):
        sh = shade / shade.std()
        noise = rng.normal(0, 1, sh.shape)
        y = np.sqrt(share) * sh + np.sqrt(1.0 - share) * noise
        got = r2_fit(y, X)
        ok = abs(got - share) <= 0.10 * share + 0.002
        L.append('K1 D1  injected shading share %.2f -> R^2 %.4f   %s'
                 % (share, got, 'OK' if ok else '**FAIL**'))
    sh = shade / shade.std()
    y50 = np.sqrt(0.5) * sh + np.sqrt(0.5) * rng.normal(0, 1, sh.shape)
    mhp = hp(n[:, :, 2] * 255.0)
    L.append('K2 D2  synthetic (50%% shading) vs the _msn envelope      %+.4f' % d2(y50, mhp))
    L.append('K2 D3  synthetic (50%% shading) vs the _msn orientation   %+.4f' % d3(y50, mhp))
    van24 = S.lum(S.Dds(S.van_sheet(-20, 24)).level(0))
    v24 = hp(van24)
    L.append('K3 D3  vanilla residual vs ITSELF                        %+.4f' % d3(v24, v24))
    L.append('K3 D3  vanilla residual vs its own 90-degree rotation    %+.4f'
             % d3(v24, np.rot90(v24)))
    L.append('K3 D3  vanilla residual vs white noise (chance = 0)      %+.4f'
             % d3(v24, rng.normal(0, 1, v24.shape)))
    L.append('K3 D2  vanilla residual vs white noise (chance = 0)      %+.4f'
             % d2(v24, rng.normal(0, 1, v24.shape)))
    L.append('')

    # ------------------------------------------------------------- the verdicts
    for name, cx, cy in TILES:
        van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        vhp = hp(van)
        nv = van_normals(cx, cy)
        no = our_normals(name, cx, cy)
        nt = np.stack([S.phase_twin(nv[:, :, k], seed=20 + k) for k in range(3)], 2)

        Xv, _ = basis(nv)
        Xo, _ = basis(no)
        Xt, _ = basis(nt)

        L.append('=' * 78)
        L.append('chunk (%d,%d)   vanilla colour high-pass SD = %.3f' % (cx, cy, vhp.std()))
        L.append('=' * 78)
        L.append('D1  best possible linear shading, R^2 of the 6-column fit')
        r_v = r2_fit(vhp, Xv)
        r_t = r2_fit(vhp, Xt)
        r_o = r2_fit(vhp, Xo)
        r_rand = r2_fit(vhp, rng.normal(0, 1, Xv.shape))
        L.append('      vanilla _msn (the candidate)        R^2 = %.5f' % r_v)
        L.append('      its phase twin        (FLOOR)       R^2 = %.5f' % r_t)
        L.append('      OUR  _msn             (FLOOR)       R^2 = %.5f' % r_o)
        L.append('      6 columns of noise    (OVERFIT)     R^2 = %.5f' % r_rand)
        # per-column simple correlations, for the record
        cols = []
        for k, nm in enumerate(names):
            cols.append('%s %+.4f' % (nm, T.corr(vhp, Xv[:, k])))
        L.append('      per column r: ' + '  '.join(cols))

        mv = hp(nv[:, :, 2] * 255.0)
        mo = hp(no[:, :, 2] * 255.0)
        mt = hp(nt[:, :, 2] * 255.0)
        L.append('D2  envelope agreement, corr(blur|colour hp|, blur|msn hp|)')
        L.append('      vanilla _msn (the candidate)        %+.4f' % d2(vhp, mv))
        L.append('      its phase twin        (FLOOR)       %+.4f' % d2(vhp, mt))
        L.append('      OUR  _msn             (FLOOR)       %+.4f' % d2(vhp, mo))
        L.append('D3  orientation agreement, mean cos 2(dtheta), 8x8 windows')
        L.append('      vanilla _msn (the candidate)        %+.4f' % d3(vhp, mv))
        L.append('      its phase twin        (FLOOR)       %+.4f' % d3(vhp, mt))
        L.append('      OUR  _msn             (FLOOR)       %+.4f' % d3(vhp, mo))
        # the slope-magnitude form as well, since a material rule would use it
        sv = hp(np.sqrt(nv[:, :, 0] ** 2 + nv[:, :, 1] ** 2) * 255.0)
        st = hp(np.sqrt(nt[:, :, 0] ** 2 + nt[:, :, 1] ** 2) * 255.0)
        L.append('      (slope magnitude instead of nz)     D2 %+.4f  floor %+.4f'
                 % (d2(vhp, sv), d2(vhp, st)))
        L.append('      (slope magnitude instead of nz)     D3 %+.4f  floor %+.4f'
                 % (d3(vhp, sv), d3(vhp, st)))
        L.append('')
        out[name] = dict(d1=r_v, d1_twin=r_t, d1_ours=r_o, d1_noise=r_rand,
                         d2=d2(vhp, mv), d2_twin=d2(vhp, mt), d2_ours=d2(vhp, mo),
                         d3=d3(vhp, mv), d3_twin=d3(vhp, mt), d3_ours=d3(vhp, mo),
                         vhp_sd=float(vhp.std()))

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'd1_geology.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'd1_geology.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
