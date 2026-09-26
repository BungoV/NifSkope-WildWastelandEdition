"""FLAT1 override gate, the pair: a `nobake` line on a painted model and a `bake` line on a refused one. Every VT.2
colour texel that moved between the default bake and the override bake must lie under one of the two models'
placements (every placement in the box and its 2-cell margin, any decision), allowing the 4x4 block.

  python flat_pair.py <default tag> <override tag> <model> [<model> ...]      (models as in the report)"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flat_faith as ff  # noqa: E402
import flat_confine as fc  # noqa: E402

rg = ff.fg.rg


def main():
    a, b = sys.argv[1], sys.argv[2]
    want = {m.lower().replace('/', rg.BS) for m in sys.argv[3:]}
    R = rg.Reader()
    feet = {m: np.zeros((ff.S_H, ff.S_W), bool) for m in want}
    n = dict.fromkeys(want, 0)
    for d in rg.placements(R, ff.CX0 - 2, ff.CY0 - 2, ff.CX1 + 2, ff.CY1 + 2):
        bi = d['info']
        if not bi or not bi['modl'] or bi['modl'].lower().replace('/', rg.BS) not in want:
            continue
        m = bi['modl'].lower().replace('/', rg.BS)
        n[m] += 1
        for s, w in ff.fg.world_shapes(R, d) or []:
            px, py = ff.tris_px(w)
            for t in s['tris']:
                f = ff.frags(px[t], py[t])
                if f:
                    feet[m][f[0], f[1]] = True
    sa, sb = fc.sheet(a), fc.sheet(b)
    ch = np.abs(sa - sb).max(2) > 0
    foot = np.any(list(feet.values()), axis=0)
    for m in want:
        print('%s: %d placements, footprint %d texels, changed texels under it %d' % (m, n[m], feet[m].sum(), (ch & feet[m]).sum()))
    print('changed %d; outside the two models: strict %d, outside their 4x4 blocks %d' % (
        ch.sum(), (ch & ~foot).sum(), (ch & ~fc.blocks(foot)).sum()))


if __name__ == '__main__':
    main()
