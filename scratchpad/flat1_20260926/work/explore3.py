import collections, os, pickle, sys
import numpy as np
D = pickle.load(open('out/flat_census.pkl', 'rb'))
M = [r for r in D['recs'] if r['status'] == 'measured' and not r['road'] and not r['disabled']]
pats = sys.argv[1].split(',')
agg = collections.defaultdict(list)
for r in M:
    if any(p in r['modl'].lower() for p in pats):
        agg[r['modl']].append(r)
for m, rs in sorted(agg.items(), key=lambda kv: -sum(x['up'] for x in kv[1]))[:int(sys.argv[2]) if len(sys.argv) > 2 else 40]:
    f = lambda k: np.median([x[k] for x in rs])
    print('n%4d top %5.1f bot %6.1f sq50 %5.1f sq90 %5.1f near %.2f sq %5d up %6.0f lod%d %-6s %s' % (len(rs), f('top'), f('bottom'), f('sq50'), f('sq90'), f('sqnear'), f('sq'), sum(x['up'] for x in rs)/256, rs[0]['hasLod'], rs[0]['kind'], m[-60:]))
