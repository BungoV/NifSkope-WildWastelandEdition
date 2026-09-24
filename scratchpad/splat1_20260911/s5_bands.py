"""SPLAT1 -- does the corrected tiling also put our sheet's SPECTRUM where
vanilla's is, or only its scalar variance? A scalar can be matched by accident;
six bands cannot."""
import os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import splatlib as S
import offline_bake as B

def bands(L):
    ctr, pw = S.radial_power(L)
    t = S.band_table(ctr, pw)
    tot = sum(x[1] for x in t)
    return [100.0 * x[1] / max(tot, 1e-12) for x in t], S.local_var(L).mean()

hdr = '  '.join('%7s' % n.split()[0] for n, _, _ in S.BANDS)
for (cx0, cy0) in ((-20, 24), (-20, 20)):
    print('=== chunk (%d,%d) ===   bands: %s' % (cx0, cy0, hdr))
    rows = [('VANILLA', S.lum(S.Dds(S.van_sheet(cx0, cy0)).level(0)))]
    rows.append(('OURS as shipped', S.lum(S.Dds(S.OURS[(cx0, cy0)]).level(0))))
    for t in (2048.0, 341.3333):
        sh = B.bake(cx0, cy0, 4, mip='code', tile=t)
        rows.append(('offline TILE=%.3f' % t,
                     S.lum(np.dstack([sh, np.full(sh.shape[:2], 255.0)]))))
    ref = None
    for name, L in rows:
        pc, lv = bands(L)
        if ref is None:
            ref = pc
        l1 = sum(abs(a - b) for a, b in zip(pc, ref))
        print('  %-22s lv %7.2f   %s   L1 vs vanilla %6.1f pts'
              % (name, lv, '  '.join('%6.1f%%' % v for v in pc), l1))
    print('')
