"""WHITE1 addendum, second population: EVERY land sample outside the playable map (placement bounds -42..31 x -48..38,
EXTENT1 census), not only west of x -43. Same pale definition as localise.py, thresholds from this population."""
import numpy as np
h = np.load('h.npy'); on = np.load('on.npy').astype(np.float32); van = np.load('van.npy').astype(np.float32)
Wl = np.array([0.2126, 0.7152, 0.0722], np.float32)
land = h != -352.0
cxs = -96 + np.arange(6144) // 32; cys = 95 - np.arange(6144) // 32
play = ((cys[:, None] >= -48) & (cys[:, None] <= 38)) & ((cxs[None, :] >= -42) & (cxs[None, :] <= 31))
pop = land & ~play
gy, gx = np.gradient(h, 128.0); slope = np.degrees(np.arctan(np.hypot(gx, gy)))
for k, F in (('ON', on), ('VAN', van)):
    lm = (F @ Wl)[pop]; ch = (F.max(-1) - F.min(-1))[pop]; hb = h[pop]; sb = slope[pop]
    pale = (lm >= np.percentile(lm, 85)) & (ch <= np.median(ch)); p80 = np.percentile(hb, 80)
    print('%s: land outside the playable map, %d samples, pale %d (%.3f), height p80 %.0f' % (k, pop.sum(), pale.sum(), pale.mean(), p80))
    print('   above p80 height: pale %.3f non-pale %.3f | slope > 30 deg: pale %.3f non-pale %.3f (population %.3f) | corr(lum,h) %.3f corr(lum,slope) %.3f'
          % ((hb[pale] > p80).mean(), (hb[~pale] > p80).mean(), (sb[pale] > 30).mean(), (sb[~pale] > 30).mean(), (sb > 30).mean(),
             np.corrcoef(lm[::7], hb[::7])[0, 1], np.corrcoef(lm[::7], sb[::7])[0, 1]))
# confound check: is height still predictive once slope is held? pale share within slope bands, split at the p80 height
lm = (on @ Wl)[pop]; ch = (on.max(-1) - on.min(-1))[pop]; hb = h[pop]; sb = slope[pop]
pale = (lm >= np.percentile(lm, 85)) & (ch <= np.median(ch)); p80 = np.percentile(hb, 80)
for a, b in ((0, 30), (30, 40), (40, 50), (50, 90)):
    m = (sb >= a) & (sb < b)
    print('   ON slope %2d-%2d deg: pale share below p80 height %.3f, above p80 %.3f (n %d / %d)'
          % (a, b, pale[m & (hb <= p80)].mean(), pale[m & (hb > p80)].mean(), (m & (hb <= p80)).sum(), (m & (hb > p80)).sum()))
