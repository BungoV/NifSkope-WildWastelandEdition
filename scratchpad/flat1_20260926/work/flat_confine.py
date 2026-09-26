"""FLAT1 confinement gate: two bakes, one switch moved (flat objects on / off, or an override line). Every VT.2 colour
texel that changed must lie under a painted flat shape's footprint (flat_faith.py's `foot`: every fragment of every
painted shape at any coverage) or the footprint of a margin-cell placement (`mfoot`, which the bake measures and may
paint and the census box does not decide); counted strictly and allowing the 4x4 block that holds one.

  python flat_confine.py <bake A> <bake B>        (tags under flat_faith.BAKES; a path with a slash is taken as is)

Also: every file of the two mod folders, identical or not, and the non-colour sheets must not move."""
import filecmp
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flat_faith as ff  # noqa: E402


def root(tag):
    return tag if '/' in tag else ff.BAKES + tag


def files(tag):
    r = root(tag) + '/mod'
    out = {}
    for dp, _dn, fn in os.walk(r):
        for f in fn:
            p = os.path.join(dp, f)
            out[os.path.relpath(p, r).replace(os.sep, '/')] = p
    return out


def sheet(tag, role=1):
    old = ff.BAKES
    if '/' in tag:
        ff.BAKES = os.path.dirname(tag.rstrip('/')) + '/'
        tag = os.path.basename(tag.rstrip('/'))
    try:
        return ff.sheet(tag, role=role)
    finally:
        ff.BAKES = old


def noncolour(a, b):
    """per VT level: tiles whose non-colour sheets (every sheet but role 1, all mips) differ byte for byte"""
    import vtread
    out = []
    for lv in (2, 4):
        pa = root(a) + '/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.%d.lodt' % lv
        pb = root(b) + '/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.%d.lodt' % lv
        if not (os.path.exists(pa) and os.path.exists(pb)):
            continue
        va, vb = vtread.Vt(pa), vtread.Vt(pb)
        bad = checked = 0
        for i in range(va.tileCount):
            xa, xb = va.payload(i), vb.payload(i)
            if xa is None or xb is None:
                bad += (xa is None) != (xb is None)
                continue
            cov = bool(va.tFlags[i] & 2)
            for k in range(va.sheetCount):
                if va.sheets[k]['role'] == 1:
                    continue
                o0 = va.sheetOffset(cov, k, 0)
                o1 = va.sheetOffset(cov, k + 1, 0) if k + 1 < va.sheetCount else len(xa)
                checked += 1
                bad += xa[o0:o1] != xb[o0:o1]
        out.append('VT.%d: %d non-colour sheet slices compared, %d differ' % (lv, checked, bad))
    return '; '.join(out)


def blocks(m):
    H, W = m.shape
    b = m.reshape(H // 4, 4, W // 4, 4).any(axis=(1, 3))
    return np.repeat(np.repeat(b, 4, 0), 4, 1)


def main():
    a, b = sys.argv[1], sys.argv[2]
    fa, fb = files(a), files(b)
    same, diff = [], []
    for k in sorted(set(fa) | set(fb)):
        if k.endswith('.flat_objects_report.txt'):
            continue
        if k not in fa or k not in fb:
            diff.append(k + ' (only in one)')
        elif filecmp.cmp(fa[k], fb[k], shallow=False):
            same.append(k)
        else:
            diff.append(k)
    print('files identical %d, differ %d: %s' % (len(same), len(diff), ', '.join(diff)))
    sa, sb = sheet(a), sheet(b)
    print(noncolour(a, b))
    ch = np.abs(sa - sb).max(2) > 0
    D = np.load(os.path.join(HERE, 'out', 'flat_raster.npz'))
    foot = D['foot'] | D['mfoot']
    fb4 = blocks(foot)
    print('VT.2 colour: changed texels %d; outside the painted footprint: strict %d, outside its 4x4 blocks %d'
          ' (of the changed, under a margin placement only: %d)' % (
              ch.sum(), (ch & ~foot).sum(), (ch & ~fb4).sum(), (ch & D['mfoot'] & ~D['foot']).sum()))
    ma, mb = sheet(a, 5), sheet(b, 5)
    mch = np.abs(ma - mb).max(2) > 0
    print('VT.2 mask (ground-cover byte, suppressed under paint like the roads): changed texels %d; outside the'
          ' footprint: strict %d, outside its 4x4 blocks %d' % (mch.sum(), (mch & ~foot).sum(), (mch & ~fb4).sum()))
    out = ch & ~fb4
    if out.any():
        ys, xs = np.nonzero(out)
        cells = sorted(set(zip((ff.CX0 + xs // 256).tolist(), (ff.CY1 - ys // 256).tolist())))
        print('  exceptions at cells:', cells[:20], 'largest change %.0f levels' % np.abs(sa - sb).max(2)[out].max())
        np.save(os.path.join(HERE, 'out', 'confine_out.npy'), out)


if __name__ == '__main__':
    main()
