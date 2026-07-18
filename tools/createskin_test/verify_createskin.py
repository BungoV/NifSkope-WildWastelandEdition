"""Verify Create Skin (bind to node) + donor transfer outputs.

Stage A (unskinned target -> _skinned.nif):
  1. structure: Skin link -> BSSkin::Instance -> BSSkin::BoneData, 1 bone
  2. VertexDesc regained VA_SKINNING; every other attribute offset unchanged
  3. non-skin vertex bytes byte-identical to the unskinned input
  4. every vertex: weights (1,0,0,0) as float16, indices (0,0,0,0)
  5. triangles byte-identical
  6. must-not-move: max |(boneWorld*skinToBone)(v) - shapeWorld(v)| over all
     mesh vertices (the exact sum the skinning renderer computes, since all
     weight is on the one bone)

Stage B (_transferred.nif vs the skinned donor, Nearest Vertex on identical
geometry): per-vertex {bone: weight} must match the donor's.
"""
import struct, sys, io, contextlib, math
sys.path.insert(0, r'E:\Projects\ClaudeNifskope\tools\rigging_prototype')
import nifparse

SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape')
VA_SKINNING = 6

def parse(path):
    with contextlib.redirect_stdout(io.StringIO()):
        data, hdr, strings, blocks = nifparse.parse(path)
    return data, strings, blocks

def half(u):
    s = (u >> 15) & 1; e = (u >> 10) & 0x1F; f = u & 0x3FF
    if e == 0: v = (f / 1024.0) * 2.0 ** -14
    elif e == 31: v = math.inf
    else: v = (1.0 + f / 1024.0) * 2.0 ** (e - 15)
    return -v if s else v

def avobject(data, start):
    """Walk the NiAVObject prefix shared by nodes and shapes."""
    o = start
    name = struct.unpack_from('<I', data, o)[0]; o += 4
    ne = struct.unpack_from('<I', data, o)[0]; o += 4 + 4 * ne
    o += 8                       # controller, flags
    trans = struct.unpack_from('<3f', data, o); o += 12
    rot = [struct.unpack_from('<3f', data, o + 12 * r) for r in range(3)]; o += 36
    scale = struct.unpack_from('<f', data, o)[0]; o += 4
    o += 4                       # collision
    return name, trans, rot, scale, o

def shape_info(data, strings, blocks, want_unskinned=None):
    for i, tname, start, size in blocks:
        if tname not in SHAPES:
            continue
        nameidx, trans, rot, scale, o = avobject(data, start)
        o += 16                  # bounding sphere
        skin = struct.unpack_from('<i', data, o)[0]; o += 4
        o += 8                   # shader, alpha
        desc = struct.unpack_from('<Q', data, o)[0]; o += 8
        ntri = struct.unpack_from('<I', data, o)[0]; o += 4
        nv = struct.unpack_from('<H', data, o)[0]; o += 2
        o += 4                   # data size
        if want_unskinned is True and skin >= 0:
            continue
        if want_unskinned is False and skin < 0:
            continue
        stride = (desc & 0xF) * 4
        sk_off = (desc >> (4 * VA_SKINNING + 2)) & 0x3C
        vd = data[o:o + nv * stride]
        tris = data[o + nv * stride:o + nv * stride + ntri * 6]
        return dict(block=i, type=tname, name=strings[nameidx], start=start,
                    trans=trans, rot=rot, scale=scale, skin=skin, desc=desc,
                    stride=stride, sk_off=sk_off, nv=nv, ntri=ntri,
                    verts=vd, tris=tris)
    raise SystemExit('no matching shape found')

def node_tree(data, strings, blocks):
    nodes, children = {}, {}
    for i, tname, start, size in blocks:
        if 'NiNode' not in tname:
            continue
        nameidx, trans, rot, scale, o = avobject(data, start)
        nc = struct.unpack_from('<I', data, o)[0]; o += 4
        kids = [struct.unpack_from('<i', data, o + 4 * k)[0] for k in range(nc)]
        nodes[i] = (strings[nameidx], trans, rot, scale)
        children[i] = kids
    parent = {}
    for p, kids in children.items():
        for k in kids:
            if k >= 0:
                parent[k] = p
    return nodes, parent

def compose(a, b):
    """a*b with (rot 3x3 row-major, trans, scale); v' = R*v*s + T."""
    (ra, ta, sa), (rb, tb, sb) = a, b
    r = [[sum(ra[i][k] * rb[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    t = [ta[i] + sa * sum(ra[i][k] * tb[k] for k in range(3)) for i in range(3)]
    return (r, t, sa * sb)

def apply_t(tf, v):
    r, t, s = tf
    return [t[i] + s * sum(r[i][k] * v[k] for k in range(3)) for i in range(3)]

def inverted(tf):
    r, t, s = tf
    ri = [[r[j][i] for j in range(3)] for i in range(3)]   # rotation transpose
    si = 1.0 / s
    ti = [-si * sum(ri[i][k] * t[k] for k in range(3)) for i in range(3)]
    return (ri, ti, si)

def world_of(block, local, parent, nodes):
    tf = local
    p = parent.get(block)
    while p is not None:
        _, tr, ro, sc = nodes[p]
        tf = compose((ro, tr, sc), tf)
        p = parent.get(p)
    return tf

def bsskin(data, blocks, inst_block):
    i, tname, start, size = blocks[inst_block]
    assert tname == 'BSSkin::Instance', tname
    root, dataref, nb = struct.unpack_from('<iiI', data, start)
    bones = [struct.unpack_from('<i', data, start + 12 + 4 * k)[0] for k in range(nb)]
    di, dt, dstart, dsize = blocks[dataref]
    assert dt == 'BSSkin::BoneData', dt
    nbd = struct.unpack_from('<I', data, dstart)[0]
    entries = []
    o = dstart + 4
    for k in range(nbd):
        sphere = struct.unpack_from('<4f', data, o); o += 16
        rot = [struct.unpack_from('<3f', data, o + 12 * r) for r in range(3)]; o += 36
        trans = struct.unpack_from('<3f', data, o); o += 12
        scale = struct.unpack_from('<f', data, o)[0]; o += 4
        entries.append((sphere, rot, trans, scale))
    return root, dataref, bones, entries

def positions(sh):
    """Half-precision positions from the vertex block (these files: not FULLPREC)."""
    out = []
    for v in range(sh['nv']):
        rec = sh['verts'][v * sh['stride']:(v + 1) * sh['stride']]
        out.append([half(struct.unpack_from('<H', rec, 2 * c)[0]) for c in range(3)])
    return out

def skin_of(sh, bone_names):
    """Per-vertex {bone_name: weight} from the packed skin fields."""
    out = []
    for v in range(sh['nv']):
        rec = sh['verts'][v * sh['stride']:(v + 1) * sh['stride']]
        w = [half(struct.unpack_from('<H', rec, sh['sk_off'] + 2 * s)[0]) for s in range(4)]
        idx = [rec[sh['sk_off'] + 8 + s] for s in range(4)]
        d = {}
        for s in range(4):
            if w[s] > 0:
                d[bone_names[idx[s]]] = d.get(bone_names[idx[s]], 0.0) + w[s]
        out.append(d)
    return out

def bone_names_of(data, strings, blocks, inst_block):
    root, dataref, bones, entries = bsskin(data, blocks, inst_block)
    names = []
    for b in bones:
        nameidx = struct.unpack_from('<I', data, blocks[b][2])[0]
        names.append(strings[nameidx])
    return names

def stage_a(orig_path, skinned_path):
    od, os_, ob = parse(orig_path)
    sd, ss, sb = parse(skinned_path)
    orig = shape_info(od, os_, ob, want_unskinned=True)
    new = shape_info(sd, ss, sb, want_unskinned=False)
    fails = []
    print(f"A: shape '{new['name']}' block {new['block']} nv={new['nv']}")

    # 1. structure
    root, dataref, bones, entries = bsskin(sd, sb, new['skin'])
    print(f"  skin link {new['skin']} -> Instance(root={root}, data={dataref}, bones={bones})")
    if len(bones) != 1 or len(entries) != 1:
        fails.append('expected exactly one bone')
    if root != 0:
        fails.append(f'skeleton root {root} != 0')

    # 2. desc: skinning flag on, other offsets unchanged
    if not (new['desc'] >> 44) & (1 << VA_SKINNING):
        fails.append('VA_SKINNING not set')
    for va in range(9):
        if va == VA_SKINNING:
            continue
        a = (orig['desc'] >> (4 * va + 2)) & 0x3C
        b = (new['desc'] >> (4 * va + 2)) & 0x3C
        if a != b:
            fails.append(f'attr {va} offset changed {a}->{b}')
    if new['stride'] != orig['stride'] + 12:
        fails.append(f"stride {orig['stride']}->{new['stride']}, expected +12")

    # 3. non-skin bytes preserved   4. weights/indices
    bad_attr = bad_skin = 0
    for v in range(new['nv']):
        old = orig['verts'][v * orig['stride']:(v + 1) * orig['stride']]
        rec = new['verts'][v * new['stride']:(v + 1) * new['stride']]
        keep = rec[:new['sk_off']] + rec[new['sk_off'] + 12:]
        if bytes(keep) != bytes(old):
            bad_attr += 1
        wgts = [struct.unpack_from('<H', rec, new['sk_off'] + 2 * s)[0] for s in range(4)]
        idxs = [rec[new['sk_off'] + 8 + s] for s in range(4)]
        if wgts != [0x3C00, 0, 0, 0] or idxs != [0, 0, 0, 0]:
            bad_skin += 1
    if bad_attr: fails.append(f'{bad_attr} verts lost non-skin attribute bytes')
    if bad_skin: fails.append(f'{bad_skin} verts without weight-1.0-slot-0 skin')
    print(f"  attribute preservation: {new['nv'] - bad_attr}/{new['nv']} identical; "
          f"skin fields exact: {new['nv'] - bad_skin}/{new['nv']}")

    # 5. triangles
    if bytes(new['tris']) != bytes(orig['tris']):
        fails.append('triangles changed')

    # 6. must-not-move
    nodes, parent = node_tree(sd, ss, sb)
    bind = bones[0]
    bname, btr, bro, bsc = nodes[bind]
    bone_world = world_of(bind, (bro, btr, bsc), parent, nodes)
    shape_world = world_of(new['block'], (new['rot'], new['trans'], new['scale']),
                           parent, nodes)
    sphere, rot, trans, scale = entries[0]
    s2b = (rot, trans, scale)
    comp = compose(bone_world, s2b)
    exp = compose(inverted(bone_world), shape_world)
    print(f"  bind node {bind} '{bname}'")
    print(f"  stored skin-to-bone: t=({trans[0]:.4f},{trans[1]:.4f},{trans[2]:.4f}) s={scale:.4f}")
    print(f"  expected inv(boneW)*shapeW: t=({exp[1][0]:.4f},{exp[1][1]:.4f},{exp[1][2]:.4f}) s={exp[2]:.4f}")
    worst = 0.0
    for p in positions(new):
        a = apply_t(comp, p)
        b = apply_t(shape_world, p)
        worst = max(worst, max(abs(a[i] - b[i]) for i in range(3)))
    print(f"  must-not-move: max |skinned - static| over {new['nv']} verts = {worst:.6f}")
    if worst > 1e-3:
        fails.append(f'mesh moves by up to {worst}')
    return fails

def stage_b(donor_path, transferred_path):
    dd, ds, db = parse(donor_path)
    td, ts, tb = parse(transferred_path)
    donor = shape_info(dd, ds, db, want_unskinned=False)
    got = shape_info(td, ts, tb, want_unskinned=False)
    fails = []
    dn = bone_names_of(dd, ds, db, donor['skin'])
    tn = bone_names_of(td, ts, tb, got['skin'])
    print(f"B: donor bones {len(dn)}, transferred bones {len(tn)}")
    print(f"  donor: {sorted(set(dn))}")
    print(f"  xfer:  {sorted(set(tn))}")
    dskin = skin_of(donor, dn)
    tskin = skin_of(got, tn)
    if donor['nv'] != got['nv']:
        return [f"vert count {got['nv']} != donor {donor['nv']}"]
    worst, mismatched = 0.0, 0
    for v in range(donor['nv']):
        allb = set(dskin[v]) | set(tskin[v])
        dev = max(abs(dskin[v].get(b, 0.0) - tskin[v].get(b, 0.0)) for b in allb)
        worst = max(worst, dev)
        if dev > 1.0 / 1024.0:
            mismatched += 1
    print(f"  weight round-trip: {donor['nv'] - mismatched}/{donor['nv']} verts match "
          f"(worst deviation {worst:.6f})")
    if mismatched > donor['nv'] // 100:
        fails.append(f'{mismatched} verts deviate beyond half-float tolerance')
    return fails

if __name__ == '__main__':
    mode = sys.argv[1]
    fails = stage_a(sys.argv[2], sys.argv[3]) if mode == 'A' \
        else stage_b(sys.argv[2], sys.argv[3])
    if fails:
        print('FAIL:')
        for f in fails:
            print(f'  - {f}')
        sys.exit(1)
    print('PASS')
