import sys, collections, numpy as np
exec(open('boxdiag.py').read().split("for bi in")[0])
for bi in map(int, sys.argv[3:]):
    o = T['occluders'][bi]; ii = o['instanceIndex']; k = (cold[ii]['refFormId'], cold[ii]['scolPart'])
    top = o['z'] + o['hz']
    rows = []
    for kk in by_root[root_of[k]]:
        tcount, lo, hi = seen_of[kk]
        for m in key_to_ii.get(kk, []):
            me = o['meshId'] if m == ii else G.drawn_mesh(L, inst[m], seen_of.get(kk))
            t = G.placed_tris(L, inst[m], me)
            if not len(t): continue
            v = t.reshape(-1, 3)
            rows.append((np.round(v.max(0) - hi, 2), np.round(v.min(0) - lo, 2), round(float(hi[2] - top), 2), inst[m]['scale'], me))
    d = np.array([np.abs(np.r_[r[0], r[1]]).max() for r in rows])
    print('box', bi, 'members', len(rows), 'max |file bound - emitter bound| per member: median %.3f max %.3f' % (np.median(d), d.max()))
    for r in sorted(rows, key=lambda r: -np.abs(np.r_[r[0], r[1]]).max())[:6]:
        print('   dHi', r[0], 'dLo', r[1], 'emitterTop-boxTop', r[2], 'scaleQ', r[3], 'mesh', r[4])
