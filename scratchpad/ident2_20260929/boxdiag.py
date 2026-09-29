"""IDENT2: why one occluder box pokes. usage: python boxdiag.py <base> <dump> <box index> [...]
Rebuilds the box's member triangles exactly as the gate does, prints the out lattice points (grid indices and the
ray that failed), and the poke when the box is moved 0.25 u / shrunk 0.5 u, per axis."""
import sys, os, collections
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
import lodi_occluder_building as G
import lodgen_native_decode as ND

base, dump = sys.argv[1], sys.argv[2]
L = ND.read_lodo(base + '.lodo')
T = ND.read_lodi(base + '.lodi')
inst, cold = T['instances'], T['cold']
key_to_ii = collections.defaultdict(list)
for ii, c in enumerate(cold):
    key_to_ii[(c['refFormId'], c['scolPart'])].append(ii)
root_of, seen_of, by_root = {}, {}, collections.defaultdict(list)
for line in open(dump, encoding='utf-8', errors='replace'):
    if not line.startswith('P '):
        continue
    f = line.split(' ', 17)
    k = (int(f[2], 16), int(f[3]))
    root_of[k] = int(f[16])
    seen_of[k] = (int(f[6]), np.array([float(v) for v in f[10:13]]), np.array([float(v) for v in f[13:16]]))
    by_root[int(f[16])].append(k)


def box_tris(o):
    ii = o['instanceIndex']
    k = (cold[ii]['refFormId'], cold[ii]['scolPart'])
    mem = [j for kk in by_root[root_of[k]] for j in key_to_ii.get(kk, [])] if k in root_of else [ii]
    out = []
    for m in mem:
        me = o['meshId'] if m == ii else G.drawn_mesh(L, inst[m], seen_of.get((cold[m]['refFormId'], cold[m]['scolPart'])))
        if me != G.NO_MESH and me < len(L['meshes']):
            t = G.placed_tris(L, inst[m], me)
            if len(t):
                out.append(t)
    return np.concatenate(out), len(mem)


for bi in [int(v) for v in sys.argv[3:]]:
    o = T['occluders'][bi]
    R = np.array(ND.unpack_rotation(o['r0'], o['r1'], o['r2'])).reshape(3, 3)
    half = np.array([o['hx'], o['hy'], o['hz']])
    c = np.array([o['x'], o['y'], o['z']])
    tr, nm = box_tris(o)
    print('box %d: centre %s half %s, %d members, %d tris, poke %.4f' % (
        bi, np.round(c, 2), np.round(half, 2), nm, len(tr), 1 - G.inside_share(tr, c, R, half, 'xXyYz', 9)))
    # which lattice points: per point, redo inside_share on single points via a 1-point "box"
    g = np.linspace(-1, 1, 9)
    outs = []
    for i in range(9):
        for j in range(9):
            for k in range(9):
                p = np.array([g[i] * half[0], g[j] * half[1], g[k] * half[2]])
                cc = c + R @ p
                if G.inside_share(tr, cc, R, np.zeros(3) + 1e-6, 'xXyYz', 1) < 1.0:
                    fail = [r for r in 'xXyYz' if G.inside_share(tr, cc, R, np.zeros(3) + 1e-6, r, 1) < 1.0]
                    outs.append(((i, j, k), ''.join(fail)))
    byplane = collections.Counter((('i', o_[0][0]) if len(set(q[0][0] for q in outs)) == 1 else ('j', o_[0][1]) if len(set(q[0][1] for q in outs)) == 1 else ('k', o_[0][2])) for o_ in outs)
    print('  out points %d; failing rays %s; planes %s' % (len(outs), collections.Counter(f for _, f in outs), byplane))
    print('  first out points', outs[:6])
    for ax in range(3):
        for dv in (-0.25, 0.25):
            e = np.zeros(3); e[ax] = dv
            print('  move axis %d by %+.2f -> poke %.4f' % (ax, dv, 1 - G.inside_share(tr, c + R @ e, R, half, 'xXyYz', 9)))
    for ax in range(3):
        h2 = half.copy(); h2[ax] -= 0.5
        print('  shrink axis %d by 0.5 -> poke %.4f' % (ax, 1 - G.inside_share(tr, c, R, h2, 'xXyYz', 9)))
