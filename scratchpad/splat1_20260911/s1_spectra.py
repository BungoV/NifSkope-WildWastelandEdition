"""SPLAT1 section 1 -- spectra and variances: ours vs vanilla vs twin vs
known-answer, on the two tiles TERRAIN-R and ROADS1 used.

Read-only. No generator, no exe.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import splatlib as S                                      # noqa: E402

TILES = [(-20, 24), (-20, 20)]


def sheet_lum(path):
    return S.lum(S.Dds(path).level(0)), S.Dds(path).fourcc.decode()


def row(name, L, extra=''):
    lv = S.local_var(L).mean()
    ctr, pw = S.radial_power(L)
    bands = S.band_table(ctr, pw)
    tot = sum(b[1] for b in bands)
    print('%-34s  lv %8.2f  (sd %5.2f)   var %8.2f   ' % (name, lv, np.sqrt(lv), L.var())
          + '  '.join('%5.1f%%' % (100.0 * b[1] / max(tot, 1e-12)) for b in bands)
          + ('   ' + extra if extra else ''))
    return lv, ctr, pw


print('BANDS (fraction of the sheet\'s spectral power, left to right):')
for b in S.BANDS:
    print('   %s' % b[0])
print('')

results = {}
for (cx, cy) in TILES:
    vpath = S.van_sheet(cx, cy)
    opath = S.OURS[(cx, cy)]
    V, vfc = sheet_lum(vpath)
    O, ofc = sheet_lum(opath)
    vimg = S.Dds(vpath).level(0)
    oimg = S.Dds(opath).level(0)
    err = np.abs(vimg[:, :, :3] - oimg[:, :, :3]).mean()
    print('=== chunk (%d,%d)  dim 4, 512 texels, 32 world units a texel ===' % (cx, cy))
    print('    vanilla %s %s' % (os.path.basename(vpath), vfc))
    print('    ours    %s %s' % (opath.replace(S.REPO + os.sep, ''), ofc))
    print('    whole-tile mean |RGB| difference ours vs vanilla: %.2f of 255' % err)
    print('')
    lvV, ctrV, pwV = row('VANILLA shipped sheet', V)
    lvO, ctrO, pwO = row('OURS    baked sheet', O)
    # controls
    tw = S.phase_twin(V, seed=101)
    row('  control: vanilla phase twin', tw, '(same spectrum, no structure)')
    sm = S.smooth_field(512, 512, cells=32, seed=9, amp=float(V.std()), mean=float(V.mean()))
    row('  known-answer: smooth field', sm)
    sp = sm + S.checker(512, 512, period=6, amp=12.0)
    row('  known-answer: + 6-texel checker', sp)
    print('')
    # the periods the candidates predict
    for period, what in ((64.0, 'texture tiling at TILE=2048 (64 tx)'),
                         (4.0, 'the 17x17 VTXT grid (128 u = 4 tx)'),
                         (S.checker_radius(4), '  same, as a 2-D lattice')):
        pv, mv, rv = S.peak_at(ctrV, pwV, period)
        po, mo, ro = S.peak_at(ctrO, pwO, period)
        print('    peak at period %6.2f tx  %-34s  vanilla %6.2fx   OURS %6.2fx'
              % (period, what, rv, ro))
    print('')
    print('    EXCESS local variance ours over vanilla: %.2f - %.2f = %.2f'
          % (lvO, lvV, lvO - lvV))
    print('    codec floor (a BC1 codec on smooth data, section 0) = 1.52')
    print('')
    results[(cx, cy)] = dict(lvV=lvV, lvO=lvO, err=float(err),
                             ctrV=ctrV, pwV=pwV, ctrO=ctrO, pwO=pwO)

np.savez(os.path.join(HERE, 's1.npz'),
         **{('%d_%d_%s' % (k[0], k[1], n)): np.asarray(v)
            for k, d in results.items() for n, v in d.items()})
print('wrote s1.npz')
