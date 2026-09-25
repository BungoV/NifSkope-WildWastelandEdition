"""Where to photograph: the Amphitheater's and one blasted maple's placements (dim-4 manifests of the installed
library, base -> slot-0 mesh from the .lodo), and the 8x8-cell window with the most hue placements."""
import sys, glob, collections
import numpy as np
exec(open('census.py').read().split('# ---- per mesh')[0])
D4 = glob.glob(D + '/Commonwealth.4.*.BTO.manifest.txt')
byFid = {B['formId']: B for B in L['bases']}
def slot0(B):
    ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
    return ms[0] if ms else None
hueMesh = set(l.split('\t')[1] for l in open('census_models.tsv').read().splitlines()[1:] if int(l.split('\t')[6]) > 0)
pts = collections.defaultdict(list); hue = collections.Counter()
for f in D4:
    for ln in open(f):
        p = ln.split()
        if ln.startswith('#') or len(p) != 11: continue
        B = byFid.get(int(p[1], 16))
        if not B: continue
        m = slot0(B)
        if m is None: continue
        n = names[m]; x, y, z = float(p[3]), float(p[4]), float(p[5])
        base = n.split(BS)[-1].lower()
        if base in ('amphitheater_lod_0.nif', 'treemapleblasted01_lod_1.nif', 'treeblasted02_lod_1.nif'):
            pts[base].append((x, y, z, p[9]))
        if n in hueMesh:
            hue[(int(np.floor(x / 4096)), int(np.floor(y / 4096)))] += 1
for k, v in pts.items():
    print(k, len(v), v[:3])
best = max(((sum(hue[(cx + i, cy + j)] for i in range(8) for j in range(8)), cx, cy) for cx in range(-64, 40) for cy in range(-60, 50)))
print('best 8x8 hue window: %d placements, cells %d,%d..%d,%d' % (best[0], best[1], best[2], best[1] + 7, best[2] + 7))
print('boston window -5,-10..2,-3 hue:', sum(hue[(cx, cy)] for cx in range(-5, 3) for cy in range(-10, -2)))
