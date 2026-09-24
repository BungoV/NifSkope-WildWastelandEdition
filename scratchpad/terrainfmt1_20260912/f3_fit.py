"""Gate F3, the candidate built and fitted -- no quadrature assumption.

The candidate item 4 describes is: sample each land texture's own `_n` at the
FOOTPRINT mip with the same 341.333 tiling and the same layer weights, take its
departure from the repeat average (exactly what `--land-detail` already does on
the diffuse, `src/lodgen.cpp` ~9186), scale it by a strength and add it to the
height normal's east and north.

Here it is built directly on the two tiles, in the BEST CASE the generator could
ever hit: ONE land texture covering the whole chunk (no layer blend to average
the term down) and no weight less than 1. For each texture the strength is
FITTED -- the s that takes our sheet's 1-texel roughness to vanilla's -- and the
fit is reported beside what that s does to the normal itself.

If the best case needs a strength that distorts the normal, or cannot reach
vanilla's number at any strength, the candidate is refused with those numbers.
"""
import glob
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dds_np import Dds                                            # noqa: E402
from bc5_np import Bc5                                            # noqa: E402
from f3_common import rough, tiled, RES, TILE, CHUNKW, STEP       # noqa: E402

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
LAND = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Landscape'




files = []
for sub in ('Ground', 'Grass', 'Roads', 'Rocks'):
    files += sorted(glob.glob(os.path.join(LAND, sub, '*_n.DDS')))[:6]

TILES = [('Commonwealth.4.-20.24', 'ourleg'), ('Commonwealth.4.-16.24', 'ourleg2')]
out = {}
for ch, bake in TILES:
    ours = Dds(os.path.join(HERE, 'bake', bake, 'tex', ch + '_msn.DDS')).rgb(0)
    van = Dds(os.path.join(VAN, ch + '_msn.DDS')).rgb(0)
    tgt = 0.5 * (rough(van[:, :, 0]) + rough(van[:, :, 2]))
    base = 0.5 * (rough(ours[:, :, 0]) + rough(ours[:, :, 2]))
    rec = {'vanilla_rough1': tgt, 'ours_rough1': base, 'textures': {}}
    print('\n%s   vanilla %.3f   ours %.3f' % (ch, tgt, base))
    print('  %-40s %8s %8s %8s %10s' % ('land _n (one texture, whole chunk)',
                                        's=1', 'fit s', 'at fit s', 'unit |n| dev'))
    best = None
    for f in files:
        try:
            b = Bc5(f)
        except ValueError:
            continue
        if b.mips <= 6:
            continue
        xy = b.xy(6).astype(np.float64)
        dx = tiled(xy[:, :, 0])
        dy = tiled(xy[:, :, 1])
        dx -= dx.mean()
        dy -= dy.mean()
        e0 = ours[:, :, 0].astype(np.float64)
        n0 = ours[:, :, 2].astype(np.float64)

        def r_at(s):
            return 0.5 * (rough(e0 + s * dx) + rough(n0 + s * dy))

        r1 = r_at(1.0)
        lo, hi = 0.0, 1.0
        if r_at(hi) < tgt:
            while hi < 64.0 and r_at(hi) < tgt:
                hi *= 2.0
        for _ in range(40):
            mid = 0.5 * (lo + hi)
            if r_at(mid) < tgt:
                lo = mid
            else:
                hi = mid
        s = 0.5 * (lo + hi)
        # what that strength does to the normal: the tangential length of
        # (east, north) after the add, in units where 1.0 is the whole normal
        e = (e0 + s * dx) / 255.0 * 2 - 1
        n = (n0 + s * dy) / 255.0 * 2 - 1
        tang = np.sqrt(e * e + n * n)
        over = float((tang > 1.0).mean())
        rec['textures'][os.path.basename(f)] = {
            'rough1_at_s1': r1, 'fit_s': s, 'rough1_at_fit': r_at(s),
            'frac_tangential_over_1': over, 'max_tangential': float(tang.max())}
        print('  %-40s %8.3f %8.2f %8.3f %9.1f%% over 1'
              % (os.path.basename(f)[:40], r1, s, r_at(s), 100 * over))
        if best is None or r1 > best[1]:
            best = (os.path.basename(f), r1, s, over)
    rec['best_at_s1'] = {'texture': best[0], 'rough1': best[1], 'fit_s': best[2],
                         'frac_over_1': best[3]}
    s1 = [v['rough1_at_s1'] for v in rec['textures'].values()]
    fs = [v['fit_s'] for v in rec['textures'].values()]
    rec['mean_rough1_at_s1'] = float(np.mean(s1))
    rec['median_fit_s'] = float(np.median(fs))
    print('  MEAN over %d textures at strength 1: %.3f  (vanilla %.3f -> %.1f%%)'
          % (len(s1), np.mean(s1), tgt, 100 * np.mean(s1) / tgt))
    print('  MEDIAN strength needed to reach vanilla: %.2f' % np.median(fs))
    out[ch] = rec

json.dump(out, open(os.path.join(HERE, 'f3_fit.json'), 'w'), indent=1)
print('\nwrote f3_fit.json')
