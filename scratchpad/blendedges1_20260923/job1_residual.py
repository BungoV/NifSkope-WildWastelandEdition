import sys, numpy as np
sys.path.insert(0, '.')
from job1_steps import load, profiles, fold
for path, lab in ((sys.argv[1], 'ours'), (sys.argv[2], 'vanilla')):
    L = load(path); px, py = profiles(L)
    for ax, p in (('x', px), ('y', py)):
        b = np.arange(len(p)) + 1
        keep = (b % 64) != 0
        q = p.copy(); q[~keep] = np.nan
        s = []
        for P in (4, 8, 16, 32):
            ph = b % P
            m = np.array([np.nanmean(q[ph == k]) for k in range(P)])
            s.append('P%d peak %.3f@%d' % (P, m.max() / np.nanmean(m), int(np.nanargmax(m))))
        print(lab, ax, 'quadrant lines removed:', '  '.join(s))
