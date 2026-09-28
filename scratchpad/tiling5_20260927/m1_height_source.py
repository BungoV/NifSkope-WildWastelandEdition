"""TILING5 m1 -- which height source for the height-aware land blend.

FO4 landscape textures ship a diffuse and a normal map and no height map. The
three candidates the brief names are measured against the one thing that IS the
artist's relief: the height integrated back out of the texture's own normal map
(Frankot-Chellappa, periodic -- a landscape texture tiles, so the FFT's wrap is
the texture's own).

For every land texture the Commonwealth paints (weighted by how much ground it
covers: 1 per quadrant base, mean opacity per layer), at a mip of <= 256 texels:

  h_n    height integrated from the normal map; the green channel's sign is
         picked per texture by the lower integrability residual (the gradient of
         the integrated height against the map's own gradient), both printed
  lum    diffuse luminance            (candidate 1: free, the bake already has it)
  alpha  diffuse alpha                (candidate 2: free, if it carries anything)

and reports corr(candidate, h_n) per texture and the coverage-weighted median.
A second, independent reading: corr of each candidate with the normal map's
own "cavity" (1 - nz, i.e. slope), which needs no integration at all.

    python m1_height_source.py  ->  logs/m1_height_source.txt, m1_height_source.json
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = r'E:/Projects/NifskopeWildWastelandEdition'
SP = os.path.join(MAIN, 'scratchpad', 'splat1_20260911')
for p in (SP, os.path.join(MAIN, 'tests', 'spells')):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import offline_bake as OB                                     # noqa: E402
from lodgen_terrain_model import find_asset                   # noqa: E402

MAXSIDE = 256


def coverage():
    e = OB.esm()
    w = {}
    for key, land in e.lands.items():
        for q in range(4):
            b = land['base'][q]
            if b:
                w[b] = w.get(b, 0.0) + 1.0
            for lay in land['layers'][q]:
                f = lay['ltex']
                if not f:
                    continue
                op = np.asarray(lay['op'], np.float64)
                w[f] = w.get(f, 0.0) + float(np.clip(op, 0, 1).mean())
    return w


def normal_of(form):
    e = OB.esm()
    rec = e.ltex.get(form)
    if not rec:
        return None, ''
    ts = e.txst.get(rec['tnam'])
    if not ts or not ts['tx01']:
        return None, ''
    p = find_asset(OB.DATA, ts['tx01'])
    if not p:
        return None, ts['tx01']
    try:
        return S.Dds(p), ts['tx01']
    except Exception:
        return None, ts['tx01']


def pick_level(d):
    for m in range(d.maxMip + 1):
        if d.levels[m][1] <= MAXSIDE:
            return m
    return d.maxMip


def integrate(p, q):
    """Frankot-Chellappa on a periodic grid: the least-squares surface whose
    gradient is (p, q)."""
    h, w = p.shape
    wx = np.fft.fftfreq(w) * 2 * np.pi
    wy = np.fft.fftfreq(h) * 2 * np.pi
    WX, WY = np.meshgrid(wx, wy)
    P, Q = np.fft.fft2(p), np.fft.fft2(q)
    den = WX ** 2 + WY ** 2
    den[0, 0] = 1.0
    Z = (-1j * WX * P - 1j * WY * Q) / den
    Z[0, 0] = 0.0
    return np.real(np.fft.ifft2(Z))


def grad(z):
    gx = (np.roll(z, -1, 1) - np.roll(z, 1, 1)) * 0.5
    gy = (np.roll(z, -1, 0) - np.roll(z, 1, 0)) * 0.5
    return gx, gy


def resize_to(a, h, w):
    """Box-average a (H,W) field down to (h,w) (both powers of two)."""
    H, W = a.shape
    if (H, W) == (h, w):
        return a
    fy, fx = H // h, W // w
    return a[:h * fy, :w * fx].reshape(h, fy, w, fx).mean(axis=(1, 3))


def corr(a, b):
    a = a.ravel() - a.mean()
    b = b.ravel() - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d > 0 else 0.0


def wmedian(vals, wts):
    o = np.argsort(vals)
    v = np.asarray(vals)[o]
    c = np.cumsum(np.asarray(wts)[o])
    return float(v[np.searchsorted(c, c[-1] * 0.5)])


def main():
    cov = coverage()
    rows = []
    for form, wt in sorted(cov.items(), key=lambda t: -t[1]):
        dd, dname = OB.diffuse_of(form)
        nd, nname = normal_of(form)
        if dd is None or nd is None:
            rows.append(dict(form='%08X' % form, w=wt, skip='no diffuse' if dd is None else 'no normal'))
            continue
        nm = nd.level(pick_level(nd))
        H, W = nm.shape[:2]
        nx = nm[..., 0] / 127.5 - 1.0
        ny = nm[..., 1] / 127.5 - 1.0
        if nd.fourcc in (b'BC5U', b'ATI2'):
            nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 1e-4, 1.0))
        else:
            nz = np.clip(nm[..., 2] / 127.5 - 1.0, 0.05, 1.0)
        best = None
        for sgn in (1.0, -1.0):
            p = -nx / nz
            q = -(sgn * ny) / nz
            z = integrate(p, q)
            gx, gy = grad(z)
            res = float(np.sqrt(((gx - p) ** 2 + (gy - q) ** 2).mean())
                        / max(1e-9, np.sqrt((p * p + q * q).mean())))
            if best is None or res < best[0]:
                best = (res, sgn, z)
        res, sgn, hn = best
        cav = 1.0 - nz
        dimg = dd.level(pick_level(dd))
        lum = resize_to(S.lum(dimg), H, W) if dimg.shape[0] >= H else None
        alp = resize_to(dimg[..., 3], H, W) if dimg.shape[0] >= H else None
        if lum is None:
            rows.append(dict(form='%08X' % form, w=wt, skip='diffuse smaller than normal'))
            continue
        # the three-scale reading: raw, and low-passed (the LOD bake samples
        # ~3 mips coarser than mip 0, so the blend sees the LOW frequencies)
        lo = lambda a: resize_to(a, max(1, H // 8), max(1, W // 8))
        r = dict(form='%08X' % form, w=round(wt, 2), diffuse=dname, normal=nname,
                 side=W, ysign=sgn, integ_resid=round(res, 3),
                 lum_sd=round(float(lum.std()), 2), alpha_sd=round(float(alp.std()), 2),
                 c_lum=round(corr(lum, hn), 3),
                 c_alpha=round(corr(alp, hn), 3) if alp.std() > 0.5 else None,
                 c_lum_lo=round(corr(lo(lum), lo(hn)), 3),
                 c_alpha_lo=round(corr(lo(alp), lo(hn)), 3) if alp.std() > 0.5 else None,
                 c_lum_cav=round(corr(lum, cav), 3))
        rows.append(r)
        print('%(form)s w=%(w)7.1f side %(side)4d ysign %(ysign)+.0f resid %(integ_resid).3f  '
              'lumSD %(lum_sd)6.2f aSD %(alpha_sd)6.2f | corr(lum,h) %(c_lum)+.3f lo %(c_lum_lo)+.3f '
              '| corr(alpha,h) %(c_alpha)s lo %(c_alpha_lo)s | corr(lum,cavity) %(c_lum_cav)+.3f  %(diffuse)s' % r)
        sys.stdout.flush()
    ok = [r for r in rows if 'skip' not in r]
    W8 = [r['w'] for r in ok]
    summ = dict(
        textures=len(rows), measured=len(ok),
        skipped=[(r['form'], r['skip']) for r in rows if 'skip' in r],
        wmed_c_lum=wmedian([r['c_lum'] for r in ok], W8),
        wmed_c_lum_lo=wmedian([r['c_lum_lo'] for r in ok], W8),
        wmed_c_lum_cav=wmedian([r['c_lum_cav'] for r in ok], W8),
        frac_alpha_flat=sum(r['w'] for r in ok if r['c_alpha'] is None) / max(1e-9, sum(W8)),
        wmed_c_alpha=wmedian([r['c_alpha'] for r in ok if r['c_alpha'] is not None],
                             [r['w'] for r in ok if r['c_alpha'] is not None])
        if any(r['c_alpha'] is not None for r in ok) else None,
        frac_lum_pos=sum(r['w'] for r in ok if r['c_lum'] > 0.3) / max(1e-9, sum(W8)),
        frac_lum_neg=sum(r['w'] for r in ok if r['c_lum'] < -0.1) / max(1e-9, sum(W8)),
        wmed_resid=wmedian([r['integ_resid'] for r in ok], W8),
    )
    print(json.dumps(summ, indent=1))
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    with open(os.path.join(HERE, 'm1_height_source.json'), 'w', newline='\n') as f:
        json.dump(dict(summary=summ, rows=rows), f, indent=1)


if __name__ == '__main__':
    main()
