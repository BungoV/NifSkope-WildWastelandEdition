"""W1: the 'bare' dim-4 chunks (LAND present, no BTXT on any quadrant, no ATXT layer) -- what the shipped VT paints
there, what vanilla's dim-4 LOD tile holds there, and what the engine-default law (rule B) predicts."""
import pickle, sys, numpy as np, math
sys.path.insert(0, '.')
import vanilla_tiles as vt
w = pickle.load(open('w1_chunks.pkl', 'rb')); cls, stats = w['cls'], w['stats']
bare = sorted(k for k, c in cls.items() if c == 'bare')
inb = [k for k in bare if -64 <= k[0] <= 31 and -48 <= k[1] <= 47]
mu = np.array([stats[k][0] for k in bare]); sd = np.array([stats[k][1] for k in bare])
print('bare chunks %d (in overview frame %d); ours VT.2 mip1: SD median %.2f, mean RGB range R %.0f..%.0f G %.0f..%.0f B %.0f..%.0f' % (
    len(bare), len(inb), np.median(sd), mu[:, 0].min(), mu[:, 0].max(), mu[:, 1].min(), mu[:, 1].max(), mu[:, 2].min(), mu[:, 2].max()))
# distinct flat colours (rounded to 4)
q = [tuple((stats[k][0] / 4).round().astype(int) * 4) for k in bare]
from collections import Counter
print('distinct flat colours (rgb/4):', Counter(q).most_common(6))
vh = vs = 0; vmu = []; vsd = []
for k in bare:
    t = vt.tile('Commonwealth', 4, k[0], k[1])
    if t is None: continue
    vh += 1; a = t[..., :3].reshape(-1, 3).astype(np.float32); vmu.append(a.mean(0)); vsd.append(a.std(0).mean())
vmu = np.array(vmu); vsd = np.array(vsd)
print('vanilla dim-4 tile present on %d of %d bare chunks; vanilla SD median %.2f p10 %.2f; mean RGB %s' % (
    vh, len(bare), np.median(vsd), np.percentile(vsd, 10), np.round(vmu.mean(0), 1)))
nv = [k for k in inb if vt.tile('Commonwealth', 4, k[0], k[1]) is None]
print('bare chunks IN the frame with no vanilla tile: %d' % len(nv), nv[:20])
