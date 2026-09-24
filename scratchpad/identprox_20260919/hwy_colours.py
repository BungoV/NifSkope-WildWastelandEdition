#!/usr/bin/env python3
"""IDENTPROX (H2) -- name the colours bungo is pointing at in ident_east.png.

Re-runs HORIZON4's own `palette()` on the SAME pixel set the LEFT panel of
`images/ident_east.png` was painted from, so a group id here is the exact
colour on his screen.  Also writes a labelled crop of the top-left.
"""
import numpy as np
import sys

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
H4 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon4_20260919'
sys.path.insert(0, LANE)
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919')
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import h4core as H                                          # noqa: E402
import lodgen_native_decode as ND                           # noqa: E402

BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')
log = open(LANE + '/hwy_colours.log', 'w')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    log.write(s + '\n')
    log.flush()


def palette(ids, seed=12345):
    u, inv = np.unique(ids, return_inverse=True)
    r = np.random.RandomState(seed)
    h = r.permutation(len(u)) / max(1, len(u))
    s = 0.45 + 0.4 * r.rand(len(u))
    v = 0.60 + 0.35 * r.rand(len(u))
    i = np.floor(h * 6.0).astype(int) % 6
    f = h * 6.0 - np.floor(h * 6.0)
    p_, q_, t_ = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
    R = np.choose(i, [v, q_, p_, p_, t_, v])
    G = np.choose(i, [t_, v, v, q_, p_, p_])
    B = np.choose(i, [p_, p_, t_, v, v, q_])
    return np.stack([R, G, B], axis=1), u, h


def name_of(rgb):
    r, g, b = rgb
    mx, mn = max(rgb), min(rgb)
    if mx - mn < 0.10:
        return 'grey'
    hdeg = 0.0
    if mx == r:
        hdeg = 60 * (((g - b) / (mx - mn)) % 6)
    elif mx == g:
        hdeg = 60 * ((b - r) / (mx - mn) + 2)
    else:
        hdeg = 60 * ((r - g) / (mx - mn) + 4)
    for lo, hi, nm in ((0, 15, 'red'), (15, 40, 'orange'), (40, 70, 'yellow'),
                       (70, 95, 'yellow-green'), (95, 150, 'green'),
                       (150, 175, 'green-teal'), (175, 200, 'teal'),
                       (200, 250, 'blue'), (250, 290, 'violet'),
                       (290, 330, 'magenta/pink'), (330, 361, 'pink-red')):
        if lo <= hdeg < hi:
            return nm
    return '?'


def main():
    import cams as CAMS
    from scene import Terrain, Objects
    ter = Terrain()
    ob = Objects(verbose=False)
    cam = CAMS.build(ter, 1600, 900)['east']
    z = np.load(H4 + '/gb_east.npz')
    gb = H._GB(cam, z)
    T = ND.read_lodi(BAKE + '.lodi')
    L = ND.read_lodo(BAKE + '.lodo')
    n = T['header']['instanceCount']
    g = np.array(T['group'], dtype=np.int64)
    ch = np.zeros(n, dtype=np.int64)
    for ci, c in enumerate(T['chunks']):
        ch[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
    gid = ch * 100000 + g + 1
    mdl = np.array([L['string_at'](L['bases'][r['baseId']]['modelStringOffset'])
                    for r in T['instances']])

    v0 = ob.tri[gb.tri, 0]
    gpix = gid[ob.inst[v0]]
    obj = gb.objfirst
    cols, uids, _ = palette(gpix[obj])
    w, hh = cam.w, cam.h

    p('colours of the LEFT panel of horizon4/images/ident_east.png, for the')
    p('groups that paint the top-left (the elevated highway run):')
    p('')
    want = [100481, 100092, 100293, 100263, 100122, 100007, 100006, 100091,
            100090, 100109, 100480, 100494, 100495]
    yy, xx = np.divmod(np.arange(len(gb.kind)), w)
    for k in want:
        j = np.nonzero(uids == k)[0]
        if not len(j):
            p('   g%-8d not painted in this frame' % k)
            continue
        c = cols[j[0]]
        sel = obj & (gpix == k)
        npx = int(sel.sum())
        if not npx:
            continue
        sx, sy = xx[sel], yy[sel]
        names = {}
        for i in np.nonzero(gid == k)[0]:
            nm = mdl[i].split('\\')[-1]
            names[nm] = names.get(nm, 0) + 1
        p('   g%-8d  RGB (%.2f,%.2f,%.2f) = %-12s  %7s px  frame x %4d..%4d y %3d..%3d   %s'
          % (k, c[0], c[1], c[2], name_of(c), '{:,}'.format(npx),
             sx.min(), sx.max(), sy.min(), sy.max(),
             ', '.join('%s x%d' % (a, b) for a, b in names.items())))
    log.close()


if __name__ == '__main__':
    main()
