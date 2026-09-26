import collections, pickle, sys
import numpy as np
D = pickle.load(open('out/flat_census.pkl', 'rb'))
M = [r for r in D['recs'] if r['status'] == 'measured' and not r['road'] and not r['disabled'] and r.get('gsq')]
pats = sys.argv[1].split(',')
for p in pats:
    rs = [r for r in M if p in r['modl'].lower() and np.median(r['glo']) <= 16]
    if not rs: print(p, 'none'); continue
    m50 = np.array([np.median(r['ghi']) for r in rs]); m90 = np.array([np.percentile(r['ghi'], 90) for r in rs])
    m75 = np.array([np.percentile(r['ghi'], 75) for r in rs])
    ra = np.array([r['steep'] / max(r['up'], 1) for r in rs])
    q = lambda a: '%5.1f/%5.1f/%5.1f' % tuple(np.percentile(a, [10, 50, 90]))
    print('%-22s n%4d  med %s  p75 %s  p90 %s  side/top %s' % (p, len(rs), q(m50), q(m75), q(m90), q(ra)))
