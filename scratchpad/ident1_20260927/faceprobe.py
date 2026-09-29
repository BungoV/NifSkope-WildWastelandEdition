"""IDENT1: for the boxes the poke gate still fails, which lattice points are out, and how far the box face is
from the surface -- re-measured with the box shrunk by 0.5, 1, 2 u to show a quantisation-size overshoot.
usage: python faceprobe.py <base> <dump> <gate json>"""
import sys, json, collections
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-ident1/tests/spells')
import lodgen_native_decode as ND
import lodi_occluder_building as G
base, dump, js = sys.argv[1:4]
fails = {r['box'] for r in json.load(open(js))['rows'] if r['poke'] > 0.01}
L = ND.read_lodo(base + '.lodo'); T = ND.read_lodi(base + '.lodi')
inst = T['instances']; cold = T['cold']
seen = {}; root = {}; by_root = collections.defaultdict(list)
for line in open(dump, encoding='utf-8', errors='replace'):
    if line.startswith('P '):
        f = line.split(' ', 17); k = (int(f[2], 16), int(f[3]))
        seen[k] = (int(f[6]), np.array([float(v) for v in f[10:13]]), np.array([float(v) for v in f[13:16]]))
        root[k] = int(f[16]); by_root[int(f[16])].append(k)
k2i = collections.defaultdict(list)
for ii, c in enumerate(cold):
    k2i[(c['refFormId'], c['scolPart'])].append(ii)
for bi in sorted(fails):
    o = T['occluders'][bi]; ci = o['instanceIndex']; k = (cold[ci]['refFormId'], cold[ci]['scolPart'])
    tr = []
    for kk in by_root.get(root.get(k), [k]):
        for ii in k2i[kk]:
            me = o['meshId'] if ii == ci else G.drawn_mesh(L, inst[ii], seen[kk])
            if me != G.NO_MESH:
                tr.append(G.placed_tris(L, inst[ii], me))
    tr = np.concatenate(tr)
    R = np.array(ND.unpack_rotation(o['r0'], o['r1'], o['r2'])).reshape(3, 3)
    c = np.array([o['x'], o['y'], o['z']]); half = np.array([o['hx'], o['hy'], o['hz']])
    res = []
    for d in (0.0, 0.5, 1.0, 2.0, 4.0):
        res.append('%g u: %.4f' % (d, 1 - G.inside_share(tr, c, R, half - d, 'xXyYz', 9)))
    # which lattice points are out at 0: by box-frame coordinate
    g = np.linspace(-1, 1, 9)
    P = np.stack(np.meshgrid(g * half[0], g * half[1], g * half[2], indexing='ij'), -1).reshape(-1, 3)
    outs = []
    for p in P:
        if G.inside_share(tr, c + R @ p, R, np.zeros(3) + 1e-6, 'xXyYz', 1) < 0.5:
            outs.append(np.round(p / half, 2))
    outs = np.array(outs)
    faces = collections.Counter()
    for p in outs:
        for ax in range(3):
            if abs(p[ax]) == 1: faces['%s%s' % ('+' if p[ax] > 0 else '-', 'xyz'[ax])] += 1
    print('box %d: %d out; on faces %s; shrunk %s' % (bi, len(outs), dict(faces), '; '.join(res)))
