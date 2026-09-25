"""WHITE1 localisation: the pale band round the western land block, per pre-registered candidate, from the files
(no renderer, no build). Fields from cache.py (32 per cell, row 0 north, cells -96..95).
Pale texel (pre-registered before looking at the height/slope split): inside the western block, lum >= the block's
own p85 of lum AND chroma (max-min RGB) <= the block's own median chroma -- light AND grey, the look he named.
Western block = LAND samples (h != the -352 no-relief floor) with cell x < -43 (west of the playable map's -42).
usage: python localise.py"""
import os, sys, pickle, collections
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); os.chdir(HERE)
h = np.load('h.npy')
F = {k: np.load(k + '.npy').astype(np.float32) for k in ('on', 'off', 'pre', 'van')}
W = np.array([0.2126, 0.7152, 0.0722], np.float32)
L = {k: v @ W for k, v in F.items()}
C = {k: v.max(-1) - v.min(-1) for k, v in F.items()}
N = 6144; UPS = 128.0                                          # world units per sample
cx = lambda col: -96 + col / 32.0; cy = lambda row: 96 - row / 32.0
FLOOR = collections.Counter(h[::64, ::64].ravel().tolist()).most_common(1)[0][0]
land = h != FLOOR
rows = np.where(land.any(1))[0]; cols = np.where(land.any(0))[0]
print('floor height %.0f (the most common sample); land rows %d..%d = cell y %.2f..%.2f, cols %d..%d = cell x %.2f..%.2f'
      % (FLOOR, rows.min(), rows.max(), cy(rows.min()), cy(rows.max() + 1), cols.min(), cols.max(), cx(cols.min()), cx(cols.max() + 1)))

# slope (degrees) from central differences on the .lodl heights
gy, gx = np.gradient(h, UPS)
slope = np.degrees(np.arctan(np.hypot(gx, gy)))

# ---- 1. where the band is, and what each field reads across it ------------------------------------------------
def profile(axis, fixed, span, label):
    print('\n%s' % label)
    print('   %-9s %8s %6s  %6s %6s %6s %6s   %6s %6s' % ('cell', 'height', 'slope', 'ON', 'OFF', 'PRE', 'VAN', 'chrON', 'chrVAN'))
    for s in span:
        sl = (slice(fixed[0], fixed[1]), s) if axis == 'x' else (s, slice(fixed[0], fixed[1]))
        c = cx(s) if axis == 'x' else cy(s)
        print('   %9.2f %8.0f %6.1f  %6.1f %6.1f %6.1f %6.1f   %6.1f %6.1f' % (
            c, h[sl].mean(), slope[sl].mean(), L['on'][sl].mean(), L['off'][sl].mean(), L['pre'][sl].mean(),
            L['van'][sl].mean(), C['on'][sl].mean(), C['van'][sl].mean()))
W0 = cols.min(); N0 = rows.min()
profile('x', (3000, 3400), range(W0 - 16, W0 + 200, 8), 'WEST edge, rows 3000..3400 (cell y -11..1), column means')
profile('y', (1200, 1600), range(N0 - 16, N0 + 200, 8), 'NORTH edge, cols 1200..1600 (cell x -58.5..-46), row means')

# the band as a mask: ON lum > interior + 8 within 8 cells of the block's own edge
blk = land.copy(); blk[:, int((-43 + 96) * 32):] = False
edge_dist = np.full(h.shape, 9999, np.int32)
# distance (samples) to the nearest non-land sample along rows/cols (cheap, axis-aligned) -- enough for a rectangle's edge
for axis in (0, 1):
    a = land if axis == 1 else land.T
    d = np.zeros(a.shape, np.int32); run = np.zeros(a.shape[0], np.int32)
    for j in range(a.shape[1]):
        run = np.where(a[:, j], run + 1, 0); d[:, j] = run
    d2 = np.zeros(a.shape, np.int32); run = np.zeros(a.shape[0], np.int32)
    for j in range(a.shape[1] - 1, -1, -1):
        run = np.where(a[:, j], run + 1, 0); d2[:, j] = run
    dd = np.minimum(d, d2)
    edge_dist = np.minimum(edge_dist, dd if axis == 1 else dd.T)
ring = blk & (edge_dist <= 4 * 32)          # the outer 4 cells of the block
core = blk & (edge_dist > 8 * 32)           # more than 8 cells in
print('\nOuter 4 cells of the western block (%d samples) vs its core >8 cells in (%d samples):' % (ring.sum(), core.sum()))
for k in ('on', 'off', 'pre', 'van'):
    print('   %-3s lum ring %6.1f core %6.1f  step %+6.1f   chroma ring %5.1f core %5.1f' % (
        k.upper(), L[k][ring].mean(), L[k][core].mean(), L[k][ring].mean() - L[k][core].mean(), C[k][ring].mean(), C[k][core].mean()))
print('   height ring %.0f core %.0f; slope ring %.1f deg core %.1f deg; share of ring samples steeper than 30 deg %.3f (core %.3f)'
      % (h[ring].mean(), h[core].mean(), slope[ring].mean(), slope[core].mean(), (slope[ring] > 30).mean(), (slope[core] > 30).mean()))

# ---- C2: fill ON tracks vanilla ------------------------------------------------------------------------------
m = blk
cc_on_van = np.corrcoef(L['on'][m][::7], L['van'][m][::7])[0, 1]
cc_off_van = np.corrcoef(L['off'][m][::7], L['van'][m][::7])[0, 1]
print('\nC2 western block: corr(ON lum, VANILLA lum) %.3f; corr(OFF lum, VANILLA lum) %.3f; OFF lum sd %.2f (flat) ON sd %.2f VAN sd %.2f'
      % (cc_on_van, cc_off_van, L['off'][m].std(), L['on'][m].std(), L['van'][m].std()))

# ---- C3: painted cells in the band ---------------------------------------------------------------------------
lo = pickle.load(open(r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/land_lo.pkl', 'rb'))
band_cells = set()
rr, cc2 = np.where(ring[::32, ::32] | False)
for r_, c_ in zip(*np.where(ring)):
    pass
ringc = ring.reshape(192, 32, 192, 32).any((1, 3))
blkc = blk.reshape(192, 32, 192, 32).any((1, 3))
for r_, c_ in zip(*np.where(ringc)): band_cells.add((c_ - 96, 95 - r_))
blk_cells = set((c_ - 96, 95 - r_) for r_, c_ in zip(*np.where(blkc)))
print('C3 painted (BTXT!=0 or ATXT, his load order winner): ring cells %d, painted %d; block cells %d, painted %d; winners in ring: %s'
      % (len(band_cells), len(band_cells & lo['painted']), len(blk_cells), len(blk_cells & lo['painted']),
         collections.Counter(lo['winner'][c] for c in band_cells).most_common(3)))

# ---- C4: VT tile grid -- is the band at a tile edge? ------------------------------------------------------------
print('C4 VT.16 tile edges (cells) %s; block west edge x %.2f, north edge y %.2f; band = the outer %s cells, not a tile border'
      % (list(range(-96, 96, 16))[:4], cx(cols.min()), cy(rows.min()), '~3.5'))

# ---- ADDENDUM: pale = peaks? pale = steep faces? -----------------------------------------------------------------
for k in ('on', 'van'):
    lm, ch = L[k][blk], C[k][blk]
    pale = (lm >= np.percentile(lm, 85)) & (ch <= np.median(ch))
    hb, sb = h[blk], slope[blk]
    p80 = np.percentile(hb, 80)
    npale = ~pale
    print('\nADDENDUM (%s texels, western block %d samples, pale %d = %.3f): height p80 %.0f' % (k.upper(), blk.sum(), pale.sum(), pale.mean(), p80))
    print('   share above the block p80 height:   pale %.3f   non-pale %.3f   (population 0.200)' % ((hb[pale] > p80).mean(), (hb[npale] > p80).mean()))
    print('   share on slopes steeper than 30 deg: pale %.3f   non-pale %.3f   (population %.3f)' % ((sb[pale] > 30).mean(), (sb[npale] > 30).mean(), (sb > 30).mean()))
    print('   corr(lum, height) %.3f   corr(lum, slope) %.3f' % (np.corrcoef(lm[::7], hb[::7])[0, 1], np.corrcoef(lm[::7], sb[::7])[0, 1]))
    # same, excluding the outer 4-cell ring (the band) so the veins are measured on their own
    inner = blk & ~ring
    lm2, ch2, hb2, sb2 = L[k][inner], C[k][inner], h[inner], slope[inner]
    pale2 = (lm2 >= np.percentile(lm, 85)) & (ch2 <= np.median(ch))
    print('   veins only (ring excluded, same thresholds): pale %d; above p80 pale %.3f non-pale %.3f; >30 deg pale %.3f non-pale %.3f'
          % (pale2.sum(), (hb2[pale2] > p80).mean(), (hb2[~pale2] > p80).mean(), (sb2[pale2] > 30).mean(), (sb2[~pale2] > 30).mean()))
    # slope deciles -> pale share (the dose-response)
    q = np.percentile(sb, np.arange(0, 101, 10))
    print('   pale share by slope decile:', ' '.join('%.0f-%.0f:%.2f' % (q[i], q[i + 1], pale[(sb >= q[i]) & (sb <= q[i + 1])].mean()) for i in range(10)))
    q = np.percentile(hb, np.arange(0, 101, 10))
    print('   pale share by height decile:', ' '.join('%.0f:%.2f' % (q[i + 1], pale[(hb >= q[i]) & (hb <= q[i + 1])].mean()) for i in range(10)))
np.save('ring.npy', ring); np.save('blk.npy', blk)
