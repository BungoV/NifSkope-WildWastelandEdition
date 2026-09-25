"""Rule B (engine default) on bare chunks: predicted mean/SD vs vanilla's dim-4 tile on the same chunk, and vs ours."""
import pickle, sys, numpy as np, random
sys.path.insert(0, '.')
import law_predict as lp, vanilla_tiles as vt, lodgen_terrain_model as tm
w = pickle.load(open('w1_chunks.pkl', 'rb')); cls, stats = w['cls'], w['stats']
inb = sorted(k for k, c in cls.items() if c == 'bare' and -64 <= k[0] <= 31 and -48 <= k[1] <= 47)
random.seed(7); pick = random.sample(inb, 12)
print('chunk        ours(mean lum,SD)   ruleB(mean lum,SD)   vanilla(mean lum,SD)')
for k in pick:
    pts = [(k[0] * 4096 + (i + .5) * 512, k[1] * 4096 + (j + .5) * 512) for i in range(32) for j in range(32)]
    rb = []
    for wx, wy in pts:
        cx, cy = int(wx // 4096), int(wy // 4096)
        r = tm.composite(lp.e, lp.DATA, lp.cache, cx, cy, 4, lp.DEF, wx, wy, 32.0)
        if r: rb.append(lp.lum(r['colour']))
    rb = np.array(rb); t = vt.tile('Commonwealth', 4, *k)[..., :3].astype(np.float32)
    vl = t @ np.array([.2126, .7152, .0722], np.float32)
    om = stats[k][0] @ np.array([.2126, .7152, .0722])
    print('%-12s %6.1f %5.2f        %6.1f %5.2f          %6.1f %5.2f' % (k, om, stats[k][1], rb.mean(), rb.std(), vl.mean(), vl.std()))
