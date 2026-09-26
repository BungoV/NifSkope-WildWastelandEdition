import collections, os, pickle, sys
import numpy as np
D = pickle.load(open('out/flat_census.pkl', 'rb'))
M = [r for r in D['recs'] if r['status'] == 'measured' and not r['road'] and not r['disabled']]
lim = float(sys.argv[1]) if len(sys.argv) > 1 else 96
agg = collections.defaultdict(list)
for r in M:
    if r['top'] <= lim:
        agg[r['modl']].append(r)
rows = []
for m, rs in agg.items():
    up = sum(x['up'] for x in rs) / 256
    rows.append((up, m, rs))
rows.sort(key=lambda t: -t[0])
for up, m, rs in rows[:int(sys.argv[2]) if len(sys.argv) > 2 else 70]:
    t = np.median([x['top'] for x in rs]); b = np.median([x['bottom'] for x in rs]); e = np.median([x['extent'] for x in rs])
    sr = np.median([x['steep'] / max(x['up'], 1) for x in rs])
    print('%8.0f n%4d top %5.1f bot %6.1f ext %6.1f st/up %5.2f %-6s d%d b%d t%d lod%d %s %s' % (up, len(rs), t, b, e, sr, rs[0]['kind'], rs[0]['decal'], rs[0]['blend'], rs[0]['test'], rs[0]['hasLod'], rs[0]['baseplugin'][:14], m[-64:]))
