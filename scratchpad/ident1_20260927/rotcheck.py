"""IDENT1: for every box the poke gate fails, re-measure with the box's rotation TRANSPOSED (the yaw's sign flipped)
and print both, plus the box yaw. A sign error in one of writer / codec / gate shows as the transposed poke ~0 on the
boxes that are not at a quarter turn.
usage: python rotcheck.py <base> <dump> <gate json>"""
import sys, os, json, math, collections
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-ident1/tests/spells')
import lodgen_native_decode as ND
import lodi_occluder_building as G

base, dump, js = sys.argv[1:4]
rows = [r for r in json.load(open(js))['rows'] if r['poke'] > 0.01]
L = ND.read_lodo(base + '.lodo')
T = ND.read_lodi(base + '.lodi')
inst = T['instances']; cold = T['cold']
key_to_ii = collections.defaultdict(list)
for ii, c in enumerate(cold):
    key_to_ii[(c['refFormId'], c['scolPart'])].append(ii)
root_of_key = {}; by_root = collections.defaultdict(list)
for line in open(dump, encoding='utf-8', errors='replace'):
    if line.startswith('P '):
        f = line.split(' ', 17)
        k = (int(f[2], 16), int(f[3])); root_of_key[k] = int(f[16]); by_root[int(f[16])].append(k)
for r in rows:
    o = T['occluders'][r['box']]
    R = np.array(ND.unpack_rotation(o['r0'], o['r1'], o['r2'])).reshape(3, 3)
    ii = o['instanceIndex']
    k = (cold[ii]['refFormId'], cold[ii]['scolPart'])
    mem = []
    for kk in by_root.get(root_of_key.get(k, -1), [k]):
        mem.extend(key_to_ii.get(kk, []))
    mem = mem or [ii]
    tr = np.concatenate([t for t in (G.placed_tris(L, inst[m], G.first_mesh(L, inst[m])) for m in mem if G.first_mesh(L, inst[m]) != G.NO_MESH) if len(t)] or [np.zeros((0, 3, 3))])
    half = np.array([o['hx'], o['hy'], o['hz']]); c = np.array([o['x'], o['y'], o['z']])
    a = 1 - G.inside_share(tr, c, R, half, 'xXyYz', 9)
    b = 1 - G.inside_share(tr, c, R.T, half, 'xXyYz', 9)
    yaw = math.degrees(math.atan2(R[1, 0], R[0, 0]))
    print('box %4d members %3d yaw %7.2f  poke R %.4f  poke R^T %.4f' % (r['box'], len(mem), yaw, a, b))
