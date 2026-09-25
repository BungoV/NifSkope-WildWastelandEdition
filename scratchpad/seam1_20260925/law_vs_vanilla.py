"""Per-cell mean luminance of the §2.5 law under rule A (dominant base) and B (engine default), cells -24..-13 x 16..27,
correlated with Bethesda's shipped dim-4 LOD diffuse over the same cells (pattern agreement; tone is a separate matter)."""
import sys, numpy as np
sys.path.insert(0, '.')
import law_predict as lp, vanilla_tiles as vt
xs = range(-24, -12); ys = range(27, 15, -1)
m, n = vt.mosaic('Commonwealth', 4, -24, 16, -16, 24)
L = m[..., :3].astype(np.float32) @ np.array([0.2126, 0.7152, 0.0722], np.float32)
cpc = n // 4
V = L.reshape(12, cpc, 12, cpc).mean((1, 3))
res = {}
for rule in 'AB':
    g = np.zeros((12, 12))
    for j, cy in enumerate(ys):
        for i, cx in enumerate(xs):
            pts = [(cx * 4096 + (a + .5) * 512, cy * 4096 + (b + .5) * 512) for a in range(8) for b in range(8)]
            g[j, i] = np.nanmean([lp.sample(x, y, rule) for x, y in pts])
    res[rule] = g
    r = np.corrcoef(g.ravel(), V.ravel())[0, 1]
    # the block's own contrast: chunk (-20,20) cells vs its ring
    blk = g[4:8, 4:8].mean(); ring = np.concatenate([g[3, 3:9], g[8, 3:9], g[4:8, 3], g[4:8, 8]]).mean()
    print('rule %s: corr with vanilla per-cell %.3f   block mean %.1f ring mean %.1f (diff %.1f)' % (rule, r, blk, ring, blk - ring), flush=True)
blk = V[4:8, 4:8].mean(); ring = np.concatenate([V[3, 3:9], V[8, 3:9], V[4:8, 3], V[4:8, 8]]).mean()
print('vanilla: block mean %.1f ring mean %.1f (diff %.1f)' % (blk, ring, blk - ring))
np.save('law_cells.npy', np.stack([res['A'], res['B'], V]))
