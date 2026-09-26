"""FLAT1 scratch exploration of out/flat_census.pkl (prints only)."""
import collections
import os
import pickle
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
D = pickle.load(open(os.path.join(HERE, 'out', 'flat_census.pkl'), 'rb'))
recs = D['recs']
st = collections.Counter(r['status'] for r in recs)
print('status', dict(st))
M = [r for r in recs if r['status'] == 'measured' and not r['road'] and not r['disabled']]
print('measured, not road, not disabled:', len(M))
top = np.array([r['top'] for r in M])
bot = np.array([r['bottom'] for r in M])
edges = [-1e9, 0, 8, 16, 32, 48, 64, 70, 96, 128, 192, 256, 512, 1e9]
h, _ = np.histogram(top, edges)
area = np.array([r['up'] for r in M]) / 256.0
ha, _ = np.histogram(top, edges, weights=area)
for i in range(len(h)):
    print('top %6s..%-6s placements %6d  up-area texels %9.0f' % (edges[i], edges[i + 1], h[i], ha[i]))
print('bottom hist (placements with top<=128):')
sel = top <= 128
hb, eb = np.histogram(bot[sel], [-1e9, -256, -64, -32, -16, -8, 0, 8, 16, 32, 64, 1e9])
for i in range(len(hb)):
    print('  bottom %6s..%-6s %6d' % (eb[i], eb[i + 1], hb[i]))

pat = sys.argv[1:] or ['fence', 'car', 'curb', 'guardrail', 'wall', 'rail', 'barrier', 'jersey']
for p in pat:
    agg = collections.defaultdict(list)
    for r in M:
        if p in r['modl'].lower():
            agg[r['modl']].append(r)
    print('==', p, len(agg), 'models')
    for m, rs in sorted(agg.items(), key=lambda kv: -len(kv[1]))[:12]:
        t = np.array([x['top'] for x in rs]); b = np.array([x['bottom'] for x in rs])
        e = np.array([x['extent'] for x in rs]); u = np.array([x['up'] for x in rs]); s = np.array([x['steep'] for x in rs])
        print('  %-70s n%4d top med %6.1f min %6.1f  bot med %6.1f  ext %6.1f  up %8.0f steep %8.0f lod%d' % (
            m[-70:], len(rs), np.median(t), t.min(), np.median(b), np.median(e), np.median(u), np.median(s), rs[0]['hasLod']))
