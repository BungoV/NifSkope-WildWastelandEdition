#!/usr/bin/env python3
"""What the SHEET actually holds, over every texel, and how isotropic it is."""
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
from sheet import HorizonSheet, BINS_PER_SHEET, SHIFT

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon1_20260918'
	 '/v8/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt')
hs = HorizonSheet(P)
v = hs.v
S = v.stored
p = v.payload(0)
cover = bool(v.table[0]['flags'] & 2)
bins = np.zeros((hs.azimuths, S, S), dtype=np.uint8)
for b in range(hs.azimuths):
	s = hs.hz[b // BINS_PER_SHEET]
	o = v.sheetOffset(cover, s, 0)
	px = np.frombuffer(p, dtype='<u4', count=S * S, offset=o).reshape(S, S)
	bins[b] = ((px >> SHIFT[b % BINS_PER_SHEET]) & 0xFF).astype(np.uint8)

deg = bins.astype(np.float32) * 90.0 / 255.0
print('tile 0: %d texels, %d bins' % (S * S, S * S * hs.azimuths))
print('byte  min %d max %d mean %.1f ; deg mean %.2f  (census horizonMeanElev 63.22)'
	  % (bins.min(), bins.max(), bins.mean(), deg.mean()))
print('zero bins %d of %d  (census horizonZeroBins 0)' % (int((bins == 0).sum()), bins.size))
for q in (0, 1, 5, 25, 50, 75, 95, 99, 100):
	print('  deg p%-3d %6.2f' % (q, np.percentile(deg, q)))
spread = deg.max(axis=0) - deg.min(axis=0)
print('per-texel bin spread (max-min) deg: mean %.2f  p50 %.2f  p95 %.2f  isotropic(<1deg) %.1f%%'
	  % (spread.mean(), np.percentile(spread, 50), np.percentile(spread, 95),
		 100.0 * (spread < 1.0).mean()))
print('texels whose MEAN bin is over 60 deg: %.1f%% ; over 80 deg: %.1f%%'
	  % (100.0 * (deg.mean(axis=0) > 60).mean(), 100.0 * (deg.mean(axis=0) > 80).mean()))
