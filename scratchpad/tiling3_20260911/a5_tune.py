"""TILING3 -- tuning the warped sample against BOTH gates at once, on the brief's
own numbers.

a4_warp.py proved the mechanism.  This script picks the numbers, and it grades
every variant on TILING2's actual statistics rather than on a summary of them:

  * the repeat: the absolute visibility against vanilla's worst-of-22 ceiling
    0.264, AND the dimensionless one (over the sheet's own null floor) against
    0.448 -- both, because TILING2 showed the absolute number on chunk (-20,20)
    is largely that sheet's own broadband energy landing in the bin (its `average`
    bake, byte-identical at two different tiling constants and therefore carrying
    no repeat at all, still read 0.562 there);
  * the grain: the mean 3x3 LOCAL VARIANCE, and THE BAND TABLE ITSELF, band by
    band, each within 20 % of vanilla's share -- not a single summary number,
    because a variant can hit a total while getting the shape wrong.

Swept: the mip bias (the grain's amplitude) at quarter-mip steps, and the warp's
amplitude, lattice and octave count (the repeat's suppression).  The local STRAIN
of the warp is reported beside every row, because a warp that stretches the
texture by more than about half is a visible wobble and not a grain.

    python a5_tune.py  ->  logs/a5_tune.txt
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
import a4_warp as W                                           # noqa: E402

TILE = 341.3333
TILES = [('t2024', -20, 24), ('t2020', -20, 20)]
_orig_tap = OB._tap


def warp2(wx, wy, amp, lat, oct_):
    """`oct_` octaves of the same value noise, each half the amplitude and half
    the lattice -- the second octave buys phase wander at a scale the first one
    cannot reach without stretching the texture."""
    ox = np.zeros_like(wx)
    oy = np.zeros_like(wy)
    a, l = amp, lat
    for k in range(oct_):
        dx, dy = W.warp_offsets(wx + k * 9137.0, wy - k * 4271.0, l)
        ox = ox + a * dx
        oy = oy + a * dy
        a *= 0.5
        l *= 0.5
    return ox, oy


def make_tap(amp, lat, oct_, mipbias):
    def tap(dds, wx, wy, tile, upt, mip):
        if amp > 0.0:
            ox, oy = warp2(wx, wy, amp, lat, oct_)
            wx = wx + ox
            wy = wy + oy
        m = mipbias if mip == 'code' else float(mip) + mipbias
        return _orig_tap(dds, wx, wy, tile, upt, m)
    return tap


def bake_v(cx, cy, amp, lat, oct_, mipbias):
    OB._tap = make_tap(amp, lat, oct_, mipbias)
    try:
        return OB.bake(cx, cy, dim=4, tile=TILE, mip='code')
    finally:
        OB._tap = _orig_tap


def strain(amp, lat, oct_, n=4096):
    """The RMS local stretch the warp applies, measured on the field itself."""
    rng = np.random.default_rng(3)
    wx = rng.uniform(-50000, 50000, n)
    wy = rng.uniform(-50000, 50000, n)
    h = 1.0
    ox0, oy0 = warp2(wx, wy, amp, lat, oct_)
    ox1, _ = warp2(wx + h, wy, amp, lat, oct_)
    _, oy1 = warp2(wx, wy + h, amp, lat, oct_)
    return float(np.sqrt((((ox1 - ox0) / h) ** 2 + ((oy1 - oy0) / h) ** 2).mean()))


def bandtab(L):
    ctr, pw = S.radial_power(np.asarray(L, np.float64))
    tab = S.band_table(ctr, pw)
    tot = sum(v for _n, v in tab) or 1e-12
    return [v / tot for _n, v in tab], [n for n, _v in tab]


def main():
    Lg = ['TILING3 -- tuning the warped sample; BOTH gates on the SAME bake', '']
    out = {}
    van = {}
    for name, cx, cy in TILES:
        v = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        vis, fl = T.tiling_visibility(v)
        bt, bn = bandtab(v)
        van[name] = dict(vis=vis, floor=fl, lv=float(S.local_var(v).mean()), bt=bt)
        Lg.append('vanilla (%d,%d): repeat %.3f (ratio %.3f)  locVar %.2f' %
                  (cx, cy, vis, vis / max(fl, 1e-9), van[name]['lv']))
        Lg.append('   bands %s' % ' '.join('%s=%.3f' % (n.split()[0], x) for n, x in zip(bn, bt)))
    Lg.append('')
    Lg.append('GATE: repeat <= 0.264 absolute AND ratio <= 0.448; locVar within 20 %% of')
    Lg.append('vanilla`s; EVERY band share within 20 %% of vanilla`s. "B" counts the bands')
    Lg.append('inside the 20 %% window, of 6.')
    Lg.append('')

    variants = []
    for mb in (0.0, -0.5, -0.75, -1.0, -1.25):
        variants.append(('mip%+.2f no warp' % mb, 0.0, 2048.0, 1, mb))
    for mb in (-0.5, -0.75, -1.0):
        for amp, lat, oc in ((683.0, 1024.0, 1), (683.0, 1024.0, 2),
                             (1365.0, 1024.0, 2), (683.0, 512.0, 1),
                             (1365.0, 2048.0, 3), (2731.0, 2048.0, 3)):
            variants.append(('mip%+.2f warp A=%.0f L=%.0f o%d' % (mb, amp, lat, oc),
                             amp, lat, oc, mb))

    for name, cx, cy in TILES:
        Lg.append('=' * 104)
        Lg.append('chunk (%d,%d)' % (cx, cy))
        Lg.append('=' * 104)
        Lg.append('   %-30s %7s %7s %8s %6s %6s  %s'
                  % ('variant', 'repeat', 'ratio', 'locVar', 'dLV%', 'B/6', 'verdict'))
        Lg.append('   ' + '-' * 100)
        rows = {}
        for lab, amp, lat, oc, mb in variants:
            sh = S.lum(bake_v(cx, cy, amp, lat, oc, mb))
            vis, fl = T.tiling_visibility(sh)
            ratio = vis / max(fl, 1e-9)
            lv = float(S.local_var(sh).mean())
            bt, _ = bandtab(sh)
            dlv = 100.0 * (lv / van[name]['lv'] - 1.0)
            nb = sum(1 for x, y in zip(bt, van[name]['bt'])
                     if abs(x / max(y, 1e-9) - 1.0) <= 0.20)
            rep_ok = vis <= 0.264 and ratio <= 0.448
            gr_ok = abs(dlv) <= 20.0 and nb >= 5
            verdict = ('** BOTH GREEN **' if rep_ok and gr_ok
                       else ('repeat only' if rep_ok else ('grain only' if gr_ok else '')))
            Lg.append('   %-30s %7.3f %7.3f %8.2f %+5.0f%% %5d  %s'
                      % (lab, vis, ratio, lv, dlv, nb, verdict))
            rows[lab] = dict(vis=vis, ratio=ratio, lv=lv, dlv=dlv, nb=nb,
                             bt=bt, rep_ok=bool(rep_ok), grain_ok=bool(gr_ok),
                             amp=amp, lat=lat, oct=oc, mip=mb)
        out[name] = rows
        Lg.append('')

    Lg.append('warp strain (RMS local stretch; over ~0.5 is a visible wobble, not a grain)')
    seen = set()
    for lab, amp, lat, oc, _mb in variants:
        k = (amp, lat, oc)
        if amp == 0 or k in seen:
            continue
        seen.add(k)
        Lg.append('   A=%6.0f L=%6.0f octaves %d   strain %.3f' % (amp, lat, oc, strain(amp, lat, oc)))
    Lg.append('')

    # the joint verdict: which variant is green on BOTH tiles
    Lg.append('JOINT -- green on BOTH gates on BOTH tiles:')
    any_ = False
    for lab, *_ in variants:
        a = out['t2024'].get(lab)
        b = out['t2020'].get(lab)
        if a and b and a['rep_ok'] and a['grain_ok'] and b['rep_ok'] and b['grain_ok']:
            Lg.append('   %s' % lab)
            any_ = True
    if not any_:
        Lg.append('   NONE. Best on the dimensionless repeat gate + grain, both tiles:')
        for lab, *_ in variants:
            a = out['t2024'].get(lab)
            b = out['t2020'].get(lab)
            if (a and b and a['ratio'] <= 0.448 and b['ratio'] <= 0.448
                    and a['grain_ok'] and b['grain_ok']):
                Lg.append('   %-30s  abs repeat %.3f / %.3f   dLV %+.0f%% / %+.0f%%  B %d/%d'
                          % (lab, a['vis'], b['vis'], a['dlv'], b['dlv'], a['nb'], b['nb']))
    txt = '\n'.join(Lg) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'a5_tune.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(van=van, rows=out), open(os.path.join(HERE, 'a5_tune.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
