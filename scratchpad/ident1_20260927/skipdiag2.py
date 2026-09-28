"""IDENT1: the occluder boxes' group members the poke gate cannot name a drawn mesh for, and why."""
import sys, collections
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-ident1/tests/spells')
import lodgen_native_decode as ND
import lodi_occluder_building as G
base, dump = sys.argv[1:3]
L = ND.read_lodo(base + '.lodo'); T = ND.read_lodi(base + '.lodi')
inst = T['instances']; cold = T['cold']
seen = {}; root = {}; by_root = collections.defaultdict(list)
for line in open(dump, encoding='utf-8', errors='replace'):
    if line.startswith('P '):
        f = line.split(' ', 17)
        k = (int(f[2], 16), int(f[3]))
        seen[k] = (int(f[6]), np.array([float(v) for v in f[10:13]]), np.array([float(v) for v in f[13:16]]), f[5], f[17].strip()[-60:])
        root[k] = int(f[16]); by_root[int(f[16])].append(k)
k2i = collections.defaultdict(list)
for ii, c in enumerate(cold):
    k2i[(c['refFormId'], c['scolPart'])].append(ii)
why = collections.Counter(); shown = 0
for bi, o in enumerate(T['occluders']):
    ci = o['instanceIndex']; k = (cold[ci]['refFormId'], cold[ci]['scolPart'])
    for kk in by_root.get(root.get(k), [k]):
        for ii in k2i.get(kk, []):
            sv = seen[kk]
            if G.drawn_mesh(L, inst[ii], sv[:3]) != G.NO_MESH: continue
            b = L['bases'][inst[ii]['baseId']]
            reps = sorted({b['rep%d' % j] for j in range(4) if b['rep%d' % j] != G.NO_MESH})
            cnt = [G.l0_tris(L, r) for r in reps]
            w = 'no reps' if not reps else ('tree' if sv[3] == '1' else ('count not among reps' if sv[0] not in cnt else 'tie on count and box'))
            why[w] += 1
            if shown < 12:
                shown += 1
                print('box', bi, 'carrier' if ii == ci else 'member', ii, w, 'dump tris', sv[0], 'reps', list(zip(reps, cnt)), sv[4])
print(why)
