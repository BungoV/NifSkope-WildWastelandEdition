"""lodi_occluder_building.py -- lane IDENT1 (2026-09-27): the ONE-BOX-A-BUILDING occluder gate, read from the
FILE's bytes (not the emitter's voxels).

usage: python lodi_occluder_building.py <lod base path without extension> [--dump dump.txt] [--json out.json]
                           [--inflate F] [--rays xXyYz] [--region x0,y0,x1,y1] [--grid 9] [--gate]
--gate: exit 1 unless every box pokes out by <= 1 percent AND the same boxes grown 1.25x put at least half of
        them over 1 percent (the red floor: a test that a grown box passes proves nothing). Needs --dump.
The dump is the emitter's measuring surface: bake with WW_LODI_GROUP_DUMP=<file>.

For every occluder box in the file (or in --region, in cells):
  * the BUILDING it belongs to = the placements of its carrier's group. With --dump, the group is the emitter's
    whole group (the dump's root, which is not cut at chunk lines), matched to the file by (ref, SCOL part).
    Without --dump, the building is the carrier alone (what the per-piece fit fitted, the "before" file).
  * grid^3 sample points on a regular lattice over the box, faces and corners included (its own frame).
  * a point is INSIDE the building when a ray along each named direction of the BOX's frame hits one of the
    building's placed level-0 triangles (exact ray/triangle, not voxels): x/X = +/- the box's first axis,
    y/Y the second, z/Z = up/down.
  * poke = the share of the box's points that are not inside. The gate: every box <= 1 percent.
--inflate F scales every half extent by F first: the refuter (a box grown past its walls MUST fail).
Also prints the count and the thickness (2 x the smallest half extent): median, min, max."""
import sys, os, json, math, collections
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_native_decode as ND
import lodl_channels_table as CT

NO_MESH = CT.NO_MESH


def deq(q, lo, ext):
    return lo + q / 65535.0 * ext


def placed_tris(L, inst, mesh):
    """Level-0 triangles of one placement in world space, (n,3,3) float64 -- the offline_objects.py transform."""
    m = inst['m']; s = inst['scaleF']
    pos = (inst['x'], inst['y'], inst['z'])
    me = L['meshes'][mesh]
    out = []
    for c in range(me['clusterFirst'], me['clusterFirst'] + me['clusterCount']):
        cll = L['clusterLods'][c]
        if not (cll['level'] == 0 or (cll['level'] < 0 and cll['parentCount'] == 0)):
            continue
        cl = L['clusters'][c]
        W = []
        for v in range(cl['vertexCount']):
            lv = L['vertices'][cl['vertexBase'] + v]
            lp = [deq(lv['px'], me['aabbMin'][0], me['aabbExtent'][0]) * s,
                  deq(lv['py'], me['aabbMin'][1], me['aabbExtent'][1]) * s,
                  deq(lv['pz'], me['aabbMin'][2], me['aabbExtent'][2]) * s]
            W.append([pos[r] + m[r * 3] * lp[0] + m[r * 3 + 1] * lp[1] + m[r * 3 + 2] * lp[2] for r in range(3)])
        li = c * 48
        for t in range(cl['triangleCount']):
            a, b, cc = L['localIndices'][li + 3 * t:li + 3 * t + 3]
            if max(a, b, cc) < len(W):
                out.append([W[a], W[b], W[cc]])
    return np.array(out, dtype=np.float64).reshape(-1, 3, 3)


def first_mesh(L, inst):
    if inst['baseId'] >= len(L['bases']):
        return NO_MESH
    base = L['bases'][inst['baseId']]
    for r in (base['rep0'], base['rep1'], base['rep2'], base['rep3']):
        if r != NO_MESH and r < len(L['meshes']):
            return r
    return NO_MESH


def inside_share(tris, centre, R, half, rays, grid):
    """Share of the box's lattice points that every named ray (in the box frame) sees a triangle beyond."""
    # the triangles in the box frame: local = R^T (p - c)
    loc = np.einsum('ij,tvi->tvj', R, tris - centre)          # R columns are the box axes
    # the lattice INCLUDES the faces and corners: a cell-centre lattice stays 1/grid inside every face and
    # cannot see a box grown by up to grid/(grid-1) (it passed a 1.1x inflation)
    g = np.linspace(-1.0, 1.0, grid)
    P = np.stack(np.meshgrid(g * half[0], g * half[1], g * half[2], indexing='ij'), -1).reshape(-1, 3)
    ok = np.ones(len(P), bool)
    for ax in range(3):
        want_pos = 'xyz'[ax] in rays
        want_neg = 'XYZ'[ax] in rays
        if not (want_pos or want_neg):
            continue
        o1, o2 = (ax + 1) % 3, (ax + 2) % 3
        A = loc[:, 0]; B = loc[:, 1]; C = loc[:, 2]
        # 2-D barycentrics in the (o1, o2) plane
        d = (B[:, o1] - A[:, o1]) * (C[:, o2] - A[:, o2]) - (C[:, o1] - A[:, o1]) * (B[:, o2] - A[:, o2])
        keep = np.abs(d) > 1e-9
        # only triangles whose 2-D bounds meet the box's cross-section
        lo1 = np.minimum(np.minimum(A[:, o1], B[:, o1]), C[:, o1]); hi1 = np.maximum(np.maximum(A[:, o1], B[:, o1]), C[:, o1])
        lo2 = np.minimum(np.minimum(A[:, o2], B[:, o2]), C[:, o2]); hi2 = np.maximum(np.maximum(A[:, o2], B[:, o2]), C[:, o2])
        keep &= (hi1 >= -half[o1]) & (lo1 <= half[o1]) & (hi2 >= -half[o2]) & (lo2 <= half[o2])
        idx = np.nonzero(keep)[0]
        hp = np.zeros(len(P), bool); hn = np.zeros(len(P), bool)
        for s0 in range(0, len(idx), 2048):
            k = idx[s0:s0 + 2048]
            a, b, c, dd = A[k], B[k], C[k], d[k]
            px = P[:, None, o1]; py = P[:, None, o2]
            w1 = ((b[None, :, o1] - px) * (c[None, :, o2] - py) - (c[None, :, o1] - px) * (b[None, :, o2] - py)) / dd
            w2 = ((c[None, :, o1] - px) * (a[None, :, o2] - py) - (a[None, :, o1] - px) * (c[None, :, o2] - py)) / dd
            w3 = 1.0 - w1 - w2
            inn = (w1 >= 0) & (w2 >= 0) & (w3 >= 0)
            t = w1 * a[None, :, ax] + w2 * b[None, :, ax] + w3 * c[None, :, ax]
            pa = P[:, None, ax]
            hp |= np.any(inn & (t > pa), axis=1)
            hn |= np.any(inn & (t < pa), axis=1)
        if want_pos:
            ok &= hp
        if want_neg:
            ok &= hn
    return float(ok.mean())


def main():
    argv = list(sys.argv[1:])
    opt = {}
    for key in ('--dump', '--json', '--inflate', '--rays', '--region', '--grid'):
        if key in argv:
            k = argv.index(key); opt[key] = argv[k + 1]; del argv[k:k + 2]
    gate = '--gate' in argv
    argv = [a for a in argv if a != '--gate']
    base = argv[0]
    inflate = float(opt.get('--inflate', 1.0))
    rays = opt.get('--rays', 'xXyYz')
    grid = int(opt.get("--grid", 9))
    region = [int(v) for v in opt['--region'].split(',')] if '--region' in opt else None
    L = ND.read_lodo(base + '.lodo')
    T = ND.read_lodi(base + '.lodi')
    inst = T['instances']; cold = T['cold']
    key_to_ii = collections.defaultdict(list)
    for ii, c in enumerate(cold):
        key_to_ii[(c['refFormId'], c['scolPart'])].append(ii)
    members_of = None
    if '--dump' in opt:
        root_of_key = {}
        by_root = collections.defaultdict(list)
        for line in open(opt['--dump'], encoding='utf-8', errors='replace'):
            if not line.startswith('P '):
                continue
            f = line.split(' ', 17)
            k = (int(f[2], 16), int(f[3]))
            root_of_key[k] = int(f[16])
            by_root[int(f[16])].append(k)

        def members_of(ii):
            k = (cold[ii]['refFormId'], cold[ii]['scolPart'])
            r = root_of_key.get(k)
            if r is None:
                return [ii]
            out = []
            for kk in by_root[r]:
                out.extend(key_to_ii.get(kk, []))
            return out or [ii]
    tri_cache = {}

    def tris_of(ii):
        if ii not in tri_cache:
            me = first_mesh(L, inst[ii])
            tri_cache[ii] = placed_tris(L, inst[ii], me) if me != NO_MESH else np.zeros((0, 3, 3))
        return tri_cache[ii]
    def measure(infl):
        rows = []
        for bi, o in enumerate(T['occluders']):
            if region and not (region[0] * 4096 <= o['x'] < (region[2] + 1) * 4096
                               and region[1] * 4096 <= o['y'] < (region[3] + 1) * 4096):
                continue
            R = np.array(ND.unpack_rotation(o['r0'], o['r1'], o['r2'])).reshape(3, 3)
            half = np.array([o['hx'], o['hy'], o['hz']]) * infl
            centre = np.array([o['x'], o['y'], o['z']])
            ii = o['instanceIndex']
            mem = members_of(ii) if members_of else [ii]
            tr = [tris_of(m) for m in mem]
            tr = np.concatenate([t for t in tr if len(t)] or [np.zeros((0, 3, 3))])
            share = inside_share(tr, centre, R, half, rays, grid) if len(tr) else 0.0
            rows.append({'box': bi, 'carrier': ii, 'members': len(mem), 'tris': int(len(tr)),
                         'poke': round(1.0 - share, 4), 'thick': round(2 * float(min(half)), 1),
                         'vol': float(8 * half[0] * half[1] * half[2]),
                         'centre': [round(float(v)) for v in centre], 'half': [round(float(v), 1) for v in half]})
        return rows
    rows = measure(inflate)
    pk = np.array([r['poke'] for r in rows]) if rows else np.zeros(0)
    th = np.array([r['thick'] for r in rows]) if rows else np.zeros(0)
    vol = np.array([r['vol'] for r in rows]) if rows else np.zeros(0)
    out = {'file': base, 'dump': opt.get('--dump'), 'inflate': inflate, 'rays': rays, 'grid': grid,
           'boxes': len(rows), 'boxesOver1pct': int((pk > 0.01).sum()),
           'pokeMax': float(pk.max()) if len(pk) else None,
           'pokeVolumeWeighted': float((pk * vol).sum() / vol.sum()) if len(pk) else None,
           'thickMedian': float(np.median(th)) if len(th) else None,
           'thickMin': float(th.min()) if len(th) else None, 'thickMax': float(th.max()) if len(th) else None,
           'halfMedian': [float(np.median([r['half'][k] for r in rows])) for k in range(3)] if rows else None,
           'worst': sorted(rows, key=lambda r: -r['poke'])[:8]}
    rc = 0
    if gate:
        grown = measure(1.25)
        leak = sum(1 for r in grown if r['poke'] > 0.01)
        out['floorGrown1.25Over1pct'] = leak
        ok_in = len(rows) > 0 and out['boxesOver1pct'] == 0
        ok_floor = len(grown) > 0 and leak * 2 >= len(grown)
        print('%s   every box pokes out of its building by <= 1 percent: %d boxes, %d over, worst %.4f'
              % ('ok' if ok_in else 'FAIL', len(rows), out['boxesOver1pct'], out['pokeMax'] or 0))
        print('%s   FLOOR the same boxes grown 1.25x leave their buildings: %d of %d over 1 percent'
              % ('ok' if ok_floor else 'FAIL', leak, len(grown)))
        rc = 0 if (ok_in and ok_floor) else 1
    js = json.dumps(out, indent=1)
    if '--json' in opt:
        open(opt['--json'], 'w').write(json.dumps(dict(out, rows=rows), indent=1))
    print(js)
    sys.exit(rc)


if __name__ == '__main__':
    main()
