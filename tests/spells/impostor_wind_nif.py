"""CARDFIX1 step 6 (IMPOSTORWIND1 job 3): the INDEPENDENT side of gate G1's reprojection.

Reads a tree NIF with lane IMPOSTORWIND1's own parser (scratchpad/impostorwind1_20260924/nifwind.py in
the main tree, read in place), composes every shape's world transform down the node tree, and
rasterises the model orthographically from a ring frame's camera with a z-buffer, interpolating each
vertex's RAW colour alpha across its triangles -- the weight W the tree vertex shader reads -- and 0 on a
shape without the Tree_Anim bit (the engine does not move it). Shares no line of code with NifSkope.

    eye(v) = (cos p, sin p, 0), right(v) = (-sin p, cos p, 0), up = (0, 0, 1), p = 2 pi v / V

usage (as a module, tests/spells/impostor_wind.py): raster(nif_bytes, phi_rad, size) -> (W float[size,size] 0..255, covered bool)
       python impostor_wind_nif.py <nif path | bns:<relpath>> -> prints the shapes, their flags and transforms
"""
import os, sys, struct
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorwind1_20260924')
import nifwind  # noqa: E402

BNS_BA2 = 'E:/Projects/Fallout 4 Mods/mods/Boston Natural Surroundings/BNS Trees - Main.ba2'


def load(spec):
    if spec.startswith('bns:'):
        import ba2lib
        return ba2lib.get(ba2lib.load(BNS_BA2), spec[4:])
    return open(spec, 'rb').read()


def xform(n, k):
    """The local transform of block k: (R 3x3, t 3, s)."""
    t, o, size = n.blocks[k]
    name, o = n.objnet(o)
    o += 4                                           # flags
    tr = np.array(struct.unpack_from('<3f', n.b, o)); o += 12
    R = np.array(struct.unpack_from('<9f', n.b, o)).reshape(3, 3); o += 36
    s, = struct.unpack_from('<f', n.b, o)
    return R, tr, s


def tris(n, k):
    """The triangles the card bake draws: a BSMeshLODTriShape's FIRST FILLED range (pics.py's rule)."""
    t, o, size = n.blocks[k]
    b = n.b
    name, o = n.avobject(o)
    o += 16 + 12
    desc = struct.unpack_from('<Q', b, o)[0]; o += 8
    ntri = struct.unpack_from('<I', b, o)[0]; o += 4
    nv = struct.unpack_from('<H', b, o)[0]; o += 2
    o += 4
    o += nv * (desc & 0xF) * 4
    tr = np.frombuffer(b, np.uint16, ntri * 3, o).reshape(ntri, 3).astype(np.int64)
    o += ntri * 6
    if t == 'BSMeshLODTriShape':
        l = struct.unpack_from('<3I', b, o)
        k2 = next((i for i in range(3) if l[i]), 0)
        tr = tr[sum(l[:k2]):sum(l[:k2 + 1])]
    return tr


def shapes(n):
    """[(name, tree flag, world positions (nv,3), raw alpha (nv,), triangles)]"""
    parent = {}
    for k, (t, o, sz) in enumerate(n.blocks):
        if t in nifwind.NODES:
            name, kids, extra = n.node_children(k)
            for c in kids:
                if c >= 0:
                    parent[c] = k

    def world(k):
        R, t, s = xform(n, k)
        M = np.eye(4); M[:3, :3] = R.T * s; M[:3, 3] = t   # NIF stores the rotation transposed
        if k in parent:
            return world(parent[k]) @ M
        return M
    out = []
    for k, (t, o, sz) in enumerate(n.blocks):
        if t not in nifwind.SHAPES:
            continue
        sh = n.shape(k)
        if sh['pos'] is None:
            continue
        f1, f2, mat = n.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(n.blocks) else (None, None, '')
        tree = bool(f2 is not None and f2 & nifwind.TREE_ANIM)
        M = world(k)
        P = sh['pos'] @ M[:3, :3].T + M[:3, 3]
        a = sh['cols'][:, 3].astype(np.float64) if sh['cols'] is not None else np.full(len(P), 255.0)
        out.append((sh['name'], tree, P, a, tris(n, k)))
    return out


def raster(data, phi, size, pad=0.0):
    """W (0..255, 0 on unflagged shapes) and coverage, the model filling the frame's bbox."""
    n = nifwind.Nif(data)
    S = shapes(n)
    e = np.array([np.cos(phi), np.sin(phi), 0.0])
    r = np.array([-np.sin(phi), np.cos(phi), 0.0])
    u = np.array([0.0, 0.0, 1.0])
    allP = np.concatenate([s[2] for s in S])
    X, Y = allP @ r, allP @ u
    x0, x1, y0, y1 = X.min(), X.max(), Y.min(), Y.max()
    sc = (size - 1) / max(x1 - x0, y1 - y0)
    cx, cy = 0.5 * (x0 + x1), 0.5 * (y0 + y1)
    Z = np.full((size, size), -1e30); Wm = np.zeros((size, size)); cov = np.zeros((size, size), bool)
    for name, tree, P, a, T in S:
        px = (P @ r - cx) * sc + size / 2.0
        py = size / 2.0 - (P @ u - cy) * sc
        pz = P @ e                                    # nearer the eye = larger
        wv = a if tree else np.zeros(len(P))
        for tri in T:
            xs, ys = px[tri], py[tri]
            ix0, ix1 = int(max(0, np.floor(xs.min()))), int(min(size - 1, np.ceil(xs.max())))
            iy0, iy1 = int(max(0, np.floor(ys.min()))), int(min(size - 1, np.ceil(ys.max())))
            if ix1 < ix0 or iy1 < iy0:
                continue
            gx, gy = np.meshgrid(np.arange(ix0, ix1 + 1) + 0.5, np.arange(iy0, iy1 + 1) + 0.5)
            (xa, xb, xc), (ya, yb, yc) = xs, ys
            d = (yb - yc) * (xa - xc) + (xc - xb) * (ya - yc)
            if abs(d) < 1e-12:
                continue
            l0 = ((yb - yc) * (gx - xc) + (xc - xb) * (gy - yc)) / d
            l1 = ((yc - ya) * (gx - xc) + (xa - xc) * (gy - yc)) / d
            l2 = 1 - l0 - l1
            ins = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not ins.any():
                continue
            z = l0 * pz[tri[0]] + l1 * pz[tri[1]] + l2 * pz[tri[2]]
            w = l0 * wv[tri[0]] + l1 * wv[tri[1]] + l2 * wv[tri[2]]
            sub = (slice(iy0, iy1 + 1), slice(ix0, ix1 + 1))
            win = ins & (z > Z[sub])
            Z[sub][win] = z[win]; Wm[sub][win] = w[win]; cov[sub][win] = True
    return Wm, cov


if __name__ == '__main__':
    n = nifwind.Nif(load(sys.argv[1]))
    for name, tree, P, a, T in shapes(n):
        print('%-40s tree %d  nv %5d  tris %5d  A mean %.1f  z %.1f..%.1f' % (
            name[:40], tree, len(P), len(T), a.mean(), P[:, 2].min(), P[:, 2].max()))
