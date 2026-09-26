"""ROADS1 confinement gate: two bakes of the same code, one variable moved (pavements on / off, or rung / new).
Every VT.2 colour texel that changed must lie under a road or pavement footprint (the independent raster's
`foot`, which counts every fragment of every stamped shape at any coverage).

  python confine.py <bake A> <bake B> [raster tag(s), comma-separated = union of footprints; default INGAME_NEWRULE]

1. every file of the two mod folders: identical or not (the sheets that are not colour must not move).
2. VT.2 colour: changed texels, and those outside the footprint -- strictly, and allowing the 4x4 compression
   block that holds a footprint texel (a block is encoded as one, so one changed texel moves its whole block)."""
import filecmp
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import faith_cmp as fc  # noqa: E402


def files(tag):
    root = fc.BAKES + tag + '/mod'
    out = {}
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            p = os.path.join(dp, f)
            out[os.path.relpath(p, root).replace(os.sep, '/')] = p
    return out


def main():
    a, b = sys.argv[1], sys.argv[2]
    rt = sys.argv[3] if len(sys.argv) > 3 else 'INGAME_NEWRULE'
    fa, fb = files(a), files(b)
    same, diff = [], []
    for k in sorted(set(fa) | set(fb)):
        if k not in fa or k not in fb:
            diff.append(k + ' (only in one)')
        elif filecmp.cmp(fa[k], fb[k], shallow=False):
            same.append(k)
        else:
            diff.append(k)
    print('files identical %d, differ %d: %s' % (len(same), len(diff), ', '.join(diff)))
    sa, sb = fc.sheet(a), fc.sheet(b)
    ch = np.abs(sa - sb).max(2) > 0
    foot = np.any([np.load(os.path.join(HERE, 'out', 'raster_%s.npz' % t))['foot'] for t in rt.split(',')], axis=0)
    H, W = foot.shape
    blk = foot.reshape(H // 4, 4, W // 4, 4).any(axis=(1, 3))
    blk = np.repeat(np.repeat(blk, 4, 0), 4, 1)
    print('VT.2 colour: changed texels %d; outside the footprint: strict %d, outside the footprint\'s 4x4 blocks %d' % (
        ch.sum(), (ch & ~foot).sum(), (ch & ~blk).sum()))
    out = ch & ~blk
    if out.any():
        ys, xs = np.nonzero(out)
        print('  exceptions at cells:', sorted(set(zip((fc.CX0 + xs // 256).tolist(), (fc.CY1 - ys // 256).tolist())))[:20])
        print('  largest change there: %.0f levels' % np.abs(sa - sb).max(2)[out].max())


if __name__ == '__main__':
    main()
