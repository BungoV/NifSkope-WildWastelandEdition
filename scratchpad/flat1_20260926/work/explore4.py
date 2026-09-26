import collections, os, pickle, sys
import numpy as np
D = pickle.load(open('out/flat_census.pkl', 'rb'))
M = [r for r in D['recs'] if r['status'] == 'measured' and not r['road'] and not r['disabled'] and r.get('gsq')]
for r in M:
    r['g90'] = float(np.percentile(r['ghi'], 90)); r['gmax'] = float(r['ghi'].max())
    r['lo50'] = float(np.median(r['glo'])); r['lomin'] = float(r['glo'].min())
pats = sys.argv[1].split(',')
agg = collections.defaultdict(list)
for r in M:
    if any(p in r['modl'].lower() for p in pats):
        agg[r['modl']].append(r)
for m, rs in sorted(agg.items(), key=lambda kv: -sum(x['up'] for x in kv[1]))[:int(sys.argv[2]) if len(sys.argv) > 2 else 40]:
    f = lambda k: np.median([x[k] for x in rs])
    print('n%4d g90 %6.1f gmax %6.1f lo50 %6.1f  onroad %.2f  sq %5d up %6.0f %-6s %s' % (len(rs), f('g90'), f('gmax'), f('lo50'), f('onroad'), f('gsq'), sum(x['up'] for x in rs)/256, rs[0]['kind'], m[-58:]))
pickle.dump(M, open('out/M.pkl', 'wb'))
