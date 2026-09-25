"""W1: painted chunks ranked by flatness (VT.2 mip-1 SD), with share of quadrants carrying a BTXT, layer count and the
old-law dominant base; and whether the old law's fallback covers most of the chunk."""
import pickle, sys, numpy as np
sys.path.insert(0, '.')
import law_predict as lp
w = pickle.load(open('w1_chunks.pkl', 'rb')); cls, stats = w['cls'], w['stats']
lands = lp.e.lands; ltex = lp.e.ltex
rows = []
for k, c in cls.items():
    if c != 'painted': continue
    nq = nb = nl = 0; cov = []
    for y in range(4):
        for x in range(4):
            L = lands[(k[0] + x, k[1] + y)]
            for q in range(4):
                nq += 1; nb += bool(L['base'][q]); nl += len(L['layers'][q])
    rows.append((stats[k][1], k, nb / nq, nl, stats[k][0]))
rows.sort()
def ed(f):
    t = ltex.get(f); return (t.get('edid') if isinstance(t, dict) else str(f)) if t else ('0' if not f else hex(f))
for sd, k, fb, nl, mu in rows[:25]:
    d = lp.dom(k[0], k[1])
    print('%-11s SD %5.2f  BTXT share %.2f  layers %4d  mean RGB %s  dominant %s' % (k, sd, fb, nl, np.round(mu).astype(int), ed(d)))
print('painted chunks with SD<4: %d of %d; SD<6: %d' % (sum(r[0] < 4 for r in rows), len(rows), sum(r[0] < 6 for r in rows)))
import collections
print('painted chunks by BTXT share bucket (<0.1,<0.5,>=0.5):', collections.Counter(0 if r[2] < .1 else (1 if r[2] < .5 else 2) for r in rows))
