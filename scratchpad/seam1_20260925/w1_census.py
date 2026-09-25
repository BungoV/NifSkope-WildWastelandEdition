"""W1: classify every dim-4 chunk of the Commonwealth by its LAND paint (Fallout4.esm; no plugin in his order moves
LAND here except 6 DLC cells elsewhere) and check each class against the baked VT.2 texels.
  painted  = some quadrant carries an ATXT layer
  baseonly = no layers, some BTXT          -> old law: the chunk's dominant base, FLAT (one texture averaged)
  bare     = no layers, no BTXT (or no LAND) -> old law: dominantBase 0 -> colour default 0xFF808080 (grey)"""
import pickle, sys, numpy as np
sys.path.insert(0, '.')
import vtread
d = pickle.load(open('fo4esm_cw.pkl', 'rb')); lands = d['lands']
cls = {}
for cy0 in range(-96, 96, 4):
    for cx0 in range(-96, 96, 4):
        lay = base = 0; nl = 0
        for y in range(4):
            for x in range(4):
                L = lands.get((cx0 + x, cy0 + y))
                if not L: continue
                nl += 1
                lay += sum(len(q) for q in L['layers']); base += sum(1 for b in L['base'] if b)
        cls[(cx0, cy0)] = 'noland' if nl == 0 else ('painted' if lay else ('baseonly' if base else 'bare'))
from collections import Counter
print('dim-4 chunks by paint class (whole 192x192):', Counter(cls.values()))
# playable box used by the overview: cells -64..31 x -48..47
inb = {k: v for k, v in cls.items() if -64 <= k[0] <= 31 and -48 <= k[1] <= 47}
print('inside the overview frame (-64,-48..31,47):', Counter(inb.values()))
# texel check on VT.2: per chunk, colour std over its 4 dim-2 tiles at mip 2, and the mean colour
v = vtread.Vt(r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt')
C = v.content >> 1; B = v.border >> 1
stats = {}
for (cx0, cy0), c in cls.items():
    px = []
    for dy in (0, 2):
        for dx in (0, 2):
            i = v.index(cx0 + dx, cy0 + dy)[0]
            p = v.payload(i)
            if p is None: continue
            o = v.sheetOffset(bool(v.tFlags[i] & 2), 0, 1)
            s = vtread.decode(p, o, v.stored >> 1, 71)[B:B + C, B:B + C, :3].reshape(-1, 3).astype(np.float32)
            px.append(s)
    if not px: stats[(cx0, cy0)] = None; continue
    a = np.concatenate(px); stats[(cx0, cy0)] = (a.mean(0), a.std(0).mean())
pickle.dump({'cls': cls, 'stats': stats}, open('w1_chunks.pkl', 'wb'))
for c in ('painted', 'baseonly', 'bare', 'noland'):
    tot = sum(1 for k in cls if cls[k] == c)
    s = [stats[k] for k in cls if cls[k] == c and stats[k] is not None]
    print('%-9s chunks %d, with VT.2 tiles %d' % (c, tot, len(s)))
    if not s: continue
    sd = np.array([x[1] for x in s]); mu = np.array([x[0] for x in s])
    print('%-9s n=%5d  within-chunk colour SD: median %.2f p90 %.2f max %.2f | share SD<2: %.3f | mean RGB %s' % (
        c, len(s), np.median(sd), np.percentile(sd, 90), sd.max(), (sd < 2).mean(), np.round(mu.mean(0), 1)))
    if c == 'bare':
        print('   bare chunks exactly 0x808080 (mean within 0.5 of 128 and SD<0.5):', int(((np.abs(mu - 128).max(1) < 0.5) & (sd < 0.5)).sum()))
