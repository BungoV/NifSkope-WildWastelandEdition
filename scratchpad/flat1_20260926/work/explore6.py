import collections, pickle, sys
import numpy as np
D = pickle.load(open('out/flat_census.pkl', 'rb'))
M = [r for r in D['recs'] if r['status'] == 'measured' and not r['road'] and not r['disabled'] and r.get('gsq')]
k = int(sys.argv[2]) if len(sys.argv) > 2 else 16
for p in sys.argv[1].split(','):
    rs = [r for r in M if p in r['modl'].lower() and np.median(r['glo']) <= 16]
    if not rs: print(p, 'none'); continue
    m90 = np.array([np.percentile(r['ghi'], 90) for r in rs])
    ra = np.array([r['side'][k] / max(r['upvis'], 1) for r in rs])
    q = lambda a: '%6.2f/%6.2f/%6.2f/%6.2f' % tuple(np.percentile(a, [0, 10, 50, 90]))
    print('%-14s n%4d  p90 %s  side%d/top %s' % (p, len(rs), q(m90), k, q(ra)))
