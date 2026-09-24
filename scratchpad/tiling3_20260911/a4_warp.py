"""TILING3 -- the shippable form of hypothesis A, prototyped OFFLINE before the build.

A says vanilla's grain is the land textures' own grain with the phase broken.  The
question this script answers is HOW to break it, because the obvious way is worse
than the disease:

  per-tile random offset/rotation  ->  a hard seam every 10.67 texels, and TILING2
                                       already measured that a grid of hard edges
                                       is bungo's second complaint.

The form tested here is a CONTINUOUS one -- a smooth deterministic warp of the
world position before the texture lookup:

    wx' = wx + A * ox(wx,wy),   wy' = wy + A * oy(wx,wy)

with (ox,oy) a value-noise field on a lattice of `L` world units, smoothstep
interpolated, hashed from the integer lattice coordinate so it is a pure function
of world position: no seams anywhere, and byte-identical at 1 thread or 16 because
nothing depends on evaluation order or on a chunk's identity.

When A is comparable to one repeat, the phase of the repeat wanders by more than a
whole cycle over the sheet, so the FFT peak at 10.67 texels smears into the
broadband -- while the texture's own grain, its amplitude spectrum, its histogram
and its skew and kurtosis are carried through untouched, because a warp is a
resampling of the same texture, not a filter on it.

Swept here: the warp amplitude A, the lattice L, and the mip bias (A's other
half -- the code's mip is ~3 mips coarser than the texel, which is where TILING2's
factor-of-eight blur comes from).

Measured on every variant, on both tiles, BOTH GATES AT ONCE:
  * the repeat, against vanilla's law (0.264 absolute / 0.448 over the sheet's own
    null floor);
  * the grain: local variance and the band table against vanilla's, plus the
    moments -- because a variant that kills the repeat by blurring is the
    `average` bake bungo already rejected, and must read as a FAILURE here.

    python a4_warp.py  ->  logs/a4_warp.txt
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
import offline_bake as OB                                     # noqa: E402

TILE = 341.3333
TILES = [('t2024', -20, 24), ('t2020', -20, 20)]

# --------------------------------------------------------------------- the warp


def _hash01(i, j, k):
    """uint32 integer hash -> [0,1).  Written so the C++ can be the same lines."""
    h = (i.astype(np.uint32) * np.uint32(374761393)
         + j.astype(np.uint32) * np.uint32(668265263)
         + np.uint32(k) * np.uint32(2246822519))
    h ^= h >> np.uint32(13)
    h = h * np.uint32(1274126177)
    h ^= h >> np.uint32(16)
    return h.astype(np.float64) / 4294967296.0


def warp_offsets(wx, wy, lattice):
    """(ox,oy) in [-1,1], smooth, seamless, a pure function of world position."""
    gx = wx / lattice
    gy = wy / lattice
    i = np.floor(gx).astype(np.int64)
    j = np.floor(gy).astype(np.int64)
    fx = gx - i
    fy = gy - j
    sx = fx * fx * (3.0 - 2.0 * fx)
    sy = fy * fy * (3.0 - 2.0 * fy)
    out = []
    for k in (0, 1):
        a = _hash01(i, j, k)
        b = _hash01(i + 1, j, k)
        c = _hash01(i, j + 1, k)
        d = _hash01(i + 1, j + 1, k)
        v = ((a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy)
        out.append(v * 2.0 - 1.0)
    return out[0], out[1]


_orig_tap = OB._tap


def make_tap(amp, lattice, mipbias):
    def tap(dds, wx, wy, tile, upt, mip):
        if amp > 0.0:
            ox, oy = warp_offsets(wx, wy, lattice)
            wx = wx + amp * ox
            wy = wy + amp * oy
        m = mip
        if mipbias and not isinstance(mip, str):
            m = float(mip) + mipbias
        elif mipbias and mip == 'code':
            m = float(mipbias)
        return _orig_tap(dds, wx, wy, tile, upt, m)
    return tap


def bake_warped(cx, cy, amp, lattice, mipbias):
    OB._tap = make_tap(amp, lattice, mipbias)
    try:
        return OB.bake(cx, cy, dim=4, tile=TILE, mip='code')
    finally:
        OB._tap = _orig_tap


def moments(a):
    x = np.asarray(a, np.float64).ravel()
    x = x - x.mean()
    s = x.std()
    z = x / max(s, 1e-9)
    return float(s), float((z ** 3).mean()), float((z ** 4).mean())


def fine_share(L):
    ctr, pw = S.radial_power(np.asarray(L, np.float64))
    tab = S.band_table(ctr, pw)
    vals = [v for _n, v in tab]
    tot = sum(vals) or 1e-12
    return sum(vals[2:]) / tot          # share of variance finer than 4 texels


def main():
    L = ['TILING3 -- the warped (stochastic-phase) sample, prototyped offline', '']
    L.append('THE GATE IS BOTH AT ONCE: the repeat inside vanilla`s law (<= 0.264')
    L.append('absolute, <= 0.448 over the sheet`s own null floor) AND the grain within')
    L.append('20 % of vanilla`s (local variance, and the share of variance finer than 4')
    L.append('texels).  A row green on one and red on the other is a RED.')
    L.append('')
    out = {}
    for name, cx, cy in TILES:
        van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        vvis, vfl = T.tiling_visibility(van)
        vlv = S.local_var(van).mean()
        vfs = fine_share(van)
        vsd, vsk, vku = moments(T.hp_residual(van, r=2))
        L.append('=' * 92)
        L.append('chunk (%d,%d)' % (cx, cy))
        L.append('=' * 92)
        L.append('   %-34s %7s %7s %8s %8s %7s %6s %6s'
                 % ('variant', 'repeat', '/floor', 'locVar', 'fine%', 'hpSD', 'skew', 'kurt'))
        L.append('   ' + '-' * 88)
        L.append('   %-34s %7.3f %7.3f %8.2f %7.1f%% %7.3f %6.2f %6.2f   <- VANILLA'
                 % ('vanilla', vvis, vvis / max(vfl, 1e-9), vlv, 100 * vfs, vsd, vsk, vku))
        rows = {}
        variants = [('rung: code mip, no warp', 0.0, 1024.0, 0.0)]
        for mb in (0.0, -1.0, -2.0):
            for amp in (171.0, 341.0, 683.0, 1365.0):
                for lat in (1024.0, 2048.0, 4096.0):
                    if lat != 2048.0 and (mb != -1.0 or amp != 683.0):
                        continue           # sweep the lattice only at one point
                    variants.append(('warp A=%.0f L=%.0f mip%+.0f' % (amp, lat, mb),
                                     amp, lat, mb))
        for lab, amp, lat, mb in variants:
            sh = S.lum(bake_warped(cx, cy, amp, lat, mb))
            vis, fl = T.tiling_visibility(sh)
            lv = S.local_var(sh).mean()
            fs = fine_share(sh)
            sd, sk, ku = moments(T.hp_residual(sh, r=2))
            green_rep = vis <= 0.264 and (vis / max(fl, 1e-9)) <= 0.448
            green_gr = abs(lv / vlv - 1.0) <= 0.20 and abs(fs / vfs - 1.0) <= 0.20
            mark = ('BOTH GREEN' if (green_rep and green_gr)
                    else ('repeat OK' if green_rep else ('grain OK' if green_gr else '')))
            L.append('   %-34s %7.3f %7.3f %8.2f %7.1f%% %7.3f %6.2f %6.2f   %s'
                     % (lab, vis, vis / max(fl, 1e-9), lv, 100 * fs, sd, sk, ku, mark))
            rows[lab] = dict(vis=vis, ratio=vis / max(fl, 1e-9), locvar=lv,
                             fine=fs, sd=sd, skew=sk, kurt=ku,
                             rep_ok=bool(green_rep), grain_ok=bool(green_gr))
        L.append('   (vanilla locVar %.2f, fine share %.1f%%; the 20 %% window is locVar'
                 ' %.2f..%.2f and fine %.1f..%.1f%%)'
                 % (vlv, 100 * vfs, 0.8 * vlv, 1.2 * vlv, 80 * vfs, 120 * vfs))
        L.append('')
        out[name] = dict(van=dict(vis=vvis, floor=vfl, locvar=vlv, fine=vfs,
                                  sd=vsd, skew=vsk, kurt=vku), rows=rows)
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'a4_warp.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'a4_warp.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
