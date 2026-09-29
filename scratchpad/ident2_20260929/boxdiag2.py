import sys, collections, numpy as np
sys.argv = [sys.argv[0]] + sys.argv[1:3] + []
exec(open('boxdiag.py').read().split("for bi in")[0])
for bi in (438, 177):
    o = T['occluders'][bi]
    ii = o['instanceIndex']; k = (cold[ii]['refFormId'], cold[ii]['scolPart'])
    keys = by_root[root_of[k]]
    top = o['z'] + o['hz']
    bad = 0; rows = []
    for kk in keys:
        tcount, lo, hi = seen_of[kk]
        for m in key_to_ii.get(kk, []):
            me = o['meshId'] if m == ii else G.drawn_mesh(L, inst[m], seen_of.get(kk))
            t = G.placed_tris(L, inst[m], me) if me != G.NO_MESH and me < len(L['meshes']) else np.zeros((0,3,3))
            glo = t.reshape(-1,3).min(0) if len(t) else None; ghi = t.reshape(-1,3).max(0) if len(t) else None
            d = np.abs(ghi - hi).max() if len(t) else 1e9
            if len(t) != tcount or d > 1.0:
                bad += 1
                rows.append((kk, tcount, len(t), np.round(hi,1), None if ghi is None else np.round(ghi,1), me))
    print('box', bi, 'top z %.1f' % top, 'members', len(keys), 'members whose file tris differ from the dump (count or bounds > 1 u):', bad)
    for r in rows[:8]: print('  ', r)
