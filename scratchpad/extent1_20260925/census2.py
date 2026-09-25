import os, glob, struct, collections
import numpy as np
exec(open('census.py').read().split("BTR = ")[0])
mnx, mny, cw, ch, pres, g = load_dump('land_mo2.bin')
BTR = r'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth'
flat = collections.Counter(); sizes = collections.Counter()
for f in glob.glob(os.path.join(BTR, 'Commonwealth.4.*.BTR')):
    p = os.path.basename(f).split('.'); tx, ty = int(p[2]), int(p[3]); s = os.path.getsize(f)
    blk = g[ty-mny:ty-mny+4, tx-mnx:tx-mnx+4].astype(int)
    if blk.max() == blk.min() and s > 2629:
        flat[int(blk.max())*8] += 1; sizes[s//1000] += 1
print('relief-free tiles with BTR > 2629 B, by their single LAND height:', dict(flat))
print('their sizes (kB):', sorted(sizes.items())[:20])
big = set(); rel = set()
for f in glob.glob(os.path.join(BTR, 'Commonwealth.4.*.BTR')):
    p = os.path.basename(f).split('.'); tx, ty = int(p[2]), int(p[3]); s = os.path.getsize(f)
    blk = g[ty-mny:ty-mny+4, tx-mnx:tx-mnx+4].astype(int)
    if s >= 3000: big.add((tx, ty))
    if blk.max() > blk.min(): rel.add((tx, ty))
print('BTR >= 3000 B: %d tiles; LAND-relief tiles: %d; big-not-relief %d; relief-not-big %d' % (len(big), len(rel), len(big-rel), len(rel-big)))
xs=[k[0] for k in big]; ys=[k[1] for k in big]
print('BTR>=3000 tile origins x %d..%d y %d..%d -> cells x %d..%d y %d..%d' % (min(xs),max(xs),min(ys),max(ys),min(xs),max(xs)+3,min(ys),max(ys)+3))
