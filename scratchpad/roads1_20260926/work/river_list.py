"""ROADS1 work item 4: every placement crossing the three river-bank strips bungo circled.

The strips = the footprint of the placements the rung stamp refused as "raised-haslod" along the river
(RRoadCurveCustom01..11), rasterised at 16 units a texel. A ROAD / PAVEMENT placement crosses a strip when its own
footprint touches that mask; ANY OTHER placement is listed when its origin lies on the mask (it has no footprint in
the road stamp: "not a road model"). Writes out/river_strips.tsv, prints the summary.
  python river_list.py"""
import collections
import os
import pickle
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import roadgeo as rg  # noqa: E402

X0, Y0, X1, Y1 = -6, -6, 2, -2


def mask_of(R, plist):
    W, H = (X1 - X0 + 1) * 256, (Y1 - Y0 + 1) * 256
    im = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(im)
    for p in plist:
        model = R.model(p['modl']) or []
        rot = np.asarray(p['rot'])
        for s in model:
            wp = np.asarray(p['pos']) + (s['pos'] * p['scale']) @ rot.T
            px = (wp[:, 0] - X0 * 4096) / 16
            py = ((Y1 + 1) * 4096 - wp[:, 1]) / 16
            for t in s['tris']:
                d.polygon([(px[i], py[i]) for i in t], fill=255)
    return np.asarray(im) > 0


def main():
    R = rg.Reader()
    pl = pickle.load(open(os.path.join(HERE, 'out', 'road_placements.pkl'), 'rb'))
    river = [p for p in pl if rg.comps(p['modl'])[:3] == ['landscape', 'roads', 'river'] and p['decision'] != 'stamped']
    strip = mask_of(R, river)
    print('strip texels (16 units each):', int(strip.sum()), 'from', len(river), 'refused river pieces')
    rows = []
    road_refs = set()
    for p in pl:
        x, y = p['pos'][0], p['pos'][1]
        if not (X0 * 4096 - 4096 <= x < (X1 + 1) * 4096 + 4096 and Y0 * 4096 - 4096 <= y < (Y1 + 1) * 4096 + 4096):
            continue
        m = mask_of(R, [p])
        hit = int((m & strip).sum())
        if hit:
            rows.append((p['ref'], p['part'], p['modl'], p['base'], p['plugin'], p['decision'], hit))
            road_refs.add((p['ref'], p['part']))
    other = collections.Counter()
    for p in rg.placements(R, X0, Y0, X1, Y1):
        if (p['ref'], p['part']) in road_refs:
            continue
        px = int((p['pos'][0] - X0 * 4096) / 16)
        py = int(((Y1 + 1) * 4096 - p['pos'][1]) / 16)
        if not (0 <= py < strip.shape[0] and 0 <= px < strip.shape[1]) or not strip[py, px]:
            continue
        dec = rg.road_decision(p)
        if dec == 'stamped':
            dec = 'stamped (footprint misses the strip)'
        rows.append((p['ref'], p['part'], p['info']['modl'] if p['info'] else '?', p['base'],
                     p['plugin'], dec, 0))
        other[(p['info']['type'] if p['info'] else '?', dec)] += 1
    with open(os.path.join(HERE, 'out', 'river_strips.tsv'), 'w') as f:
        f.write('ref\tpart\tmodel\tbase\tplugin\tdecision\tstrip_texels\n')
        for r in rows:
            f.write('%08X\t%d\t%s\t%08X\t%s\t%s\t%d\n' % r)
    print('ROAD / PAVEMENT placements whose footprint crosses the strips:')
    for r in sorted([r for r in rows if r[6]], key=lambda r: (r[5], r[2])):
        print('  %08X %-46s base %08X %-13s %-22s %5d texels' % (r[0], r[2][:46], r[3], r[4], r[5], r[6]))
    print('other placements whose origin sits on the strips (no footprint in the road stamp):')
    for k, n in sorted(other.items(), key=lambda kv: -kv[1]):
        print('  %4d  %s %s' % (n, k[0], k[1]))


if __name__ == '__main__':
    main()
