#!/usr/bin/env python3
"""Triangle-quality statistics that tell an AUTHORED LOD mesh from a GENERATED one.

Every metric is scale-free or reported in the mesh's own units, and every one of
them has a known answer on a control:
  * a unit cube (12 triangles) -- authored: AR ~ 1.41, axis-aligned normals 100%,
    axis-aligned edges 66.7% (the 4 face diagonals are not axis-aligned);
  * a random point cloud's Delaunay-ish soup -- generated: AR spread wide,
    axis alignment at the chance level.
`selftest()` runs both, and the module refuses to be imported as evidence
unless they pass.
"""
import numpy as np

AXES = np.array([[1.0, 0, 0], [0, 1.0, 0], [0, 0, 1.0]])


def weld(V):
    """Exact-value weld: identical stored floats are the same vertex.

    Works for any column count (3 for positions, 2 for UVs).
    """
    V = np.ascontiguousarray(V)
    dt = [('c%d' % i, V.dtype) for i in range(V.shape[1])]
    _, inv = np.unique(V.view(dt).ravel(), return_inverse=True)
    return inv.ravel()


def stats(verts, tris, uvs=None, name='', axis_tol_deg=2.0):
    V = np.asarray(verts, dtype=np.float64)
    T = np.asarray(tris, dtype=np.int64).reshape(-1, 3)
    out = {'name': name, 'nv': int(len(V)), 'nt': int(len(T))}
    if len(T) == 0 or len(V) == 0:
        return out
    p0, p1, p2 = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    e0, e1, e2 = p1 - p0, p2 - p1, p0 - p2
    a = np.linalg.norm(e0, axis=1)
    b = np.linalg.norm(e1, axis=1)
    c = np.linalg.norm(e2, axis=1)
    cr = np.cross(e0, -e2)
    area = 0.5 * np.linalg.norm(cr, axis=1)
    s = 0.5 * (a + b + c)
    longest = np.maximum(np.maximum(a, b), c)
    with np.errstate(divide='ignore', invalid='ignore'):
        inradius = np.where(s > 0, area / s, 0.0)
        ar = np.where(inradius > 0, longest / (2.0 * inradius) / np.sqrt(3.0), np.inf)
    deg = ~np.isfinite(ar)
    out['degenerate_pct'] = 100.0 * float(deg.mean())
    good = np.isfinite(ar)
    arg = ar[good]
    if len(arg):
        out['AR_median'] = float(np.median(arg))
        out['AR_p90'] = float(np.percentile(arg, 90))
        out['AR_p99'] = float(np.percentile(arg, 99))
        out['sliver_pct_AR4'] = 100.0 * float((arg > 4).mean())
        out['sliver_pct_AR10'] = 100.0 * float((arg > 10).mean())

    # --- triangle normals: how many lie on a coordinate axis -------------
    nl = np.linalg.norm(cr, axis=1)
    ok = nl > 0
    nrm = cr[ok] / nl[ok][:, None]
    cosmax = np.abs(nrm @ AXES.T).max(axis=1)
    tol = np.cos(np.radians(axis_tol_deg))
    out['axis_normal_pct'] = 100.0 * float((cosmax >= tol).mean()) if len(nrm) else 0.0
    # area-weighted, so a few big authored walls are not drowned by chips
    aw = area[ok]
    out['axis_normal_areapct'] = (100.0 * float(aw[cosmax >= tol].sum() / aw.sum())
                                  if aw.sum() > 0 else 0.0)
    out['vertical_normal_pct'] = (100.0 * float((np.abs(nrm[:, 2]) <= np.sin(np.radians(axis_tol_deg))).mean())
                                  if len(nrm) else 0.0)
    # YAW-INVARIANT boxiness.  A merged chunk holds objects at arbitrary
    # rotations about Z, so X/Y axis alignment is destroyed by PLACEMENT even
    # for authored boxes -- but a wall stays vertical and a roof stays
    # horizontal however the building is turned.  This share is therefore the
    # honest authored-vs-generated test on world-space merged shapes.
    if len(nrm):
        nz = np.abs(nrm[:, 2])
        boxy = (nz <= np.sin(np.radians(axis_tol_deg))) | (nz >= np.cos(np.radians(axis_tol_deg)))
        out['yawinv_box_pct'] = 100.0 * float(boxy.mean())
        out['yawinv_box_areapct'] = (100.0 * float(aw[boxy].sum() / aw.sum())
                                     if aw.sum() > 0 else 0.0)

    # --- edges: length spread, axis alignment, valence --------------------
    w = weld(V.astype(np.float32))
    TW = w[T]
    E = np.vstack([TW[:, [0, 1]], TW[:, [1, 2]], TW[:, [2, 0]]])
    E = np.sort(E, axis=1)
    E = np.unique(E, axis=0)
    VW = np.zeros((w.max() + 1, 3))
    VW[w] = V
    d = VW[E[:, 1]] - VW[E[:, 0]]
    L = np.linalg.norm(d, axis=1)
    nz = L > 0
    out['nv_welded'] = int(w.max() + 1)
    out['ne'] = int(len(E))
    if nz.any():
        Ln = L[nz]
        out['edge_median'] = float(np.median(Ln))
        out['edge_p10'] = float(np.percentile(Ln, 10))
        out['edge_p90'] = float(np.percentile(Ln, 90))
        out['edge_p90_over_p10'] = float(np.percentile(Ln, 90) / max(np.percentile(Ln, 10), 1e-9))
        dd = d[nz] / Ln[:, None]
        cm = np.abs(dd @ AXES.T).max(axis=1)
        out['axis_edge_pct'] = 100.0 * float((cm >= tol).mean())
    val = np.bincount(E.ravel(), minlength=w.max() + 1)
    val = val[val > 0]
    if len(val):
        out['valence_mean'] = float(val.mean())
        out['valence_std'] = float(val.std())
        out['valence_pct_le4'] = 100.0 * float((val <= 4).mean())
        out['valence_pct_5to7'] = 100.0 * float(((val >= 5) & (val <= 7)).mean())
        h = np.bincount(val) / float(len(val))
        h = h[h > 0]
        out['valence_entropy'] = float(-(h * np.log2(h)).sum())

    # --- coordinate snapping (only meaningful at full precision) ----------
    for unit in (1.0, 4.0, 16.0, 64.0):
        r = np.abs(V / unit - np.round(V / unit)) * unit
        out['snap_pct_%g' % unit] = 100.0 * float((r < 1e-3).mean())

    # --- 3D connected components -----------------------------------------
    ncomp, comp = _components(w.max() + 1, E, want_labels=True)
    out['components_3d'] = ncomp
    if ncomp:
        sz = np.bincount(comp[comp >= 0])
        sz = sz[sz > 0]
        out['comp_verts_median'] = float(np.median(sz))
        out['comp_verts_max'] = int(sz.max())
        out['largest_comp_vert_share'] = 100.0 * float(sz.max() / sz.sum())

    # --- HALF-FLOAT QUANTISATION, the confound the angle metrics have -----
    # A BSTriShape with stride 20 stores positions as float16.  The step at
    # magnitude m is 2^(floor(log2 m) - 10).  Any axis / aspect statistic is
    # only readable when that step is small against the edges being measured.
    mag = np.abs(V).max() if len(V) else 0.0
    step = 2.0 ** (np.floor(np.log2(max(mag, 1e-6))) - 10) if mag > 0 else 0.0
    out['coord_absmax'] = float(mag)
    out['quant_step_if_half'] = float(step)
    if out.get('edge_median'):
        out['quant_over_edge'] = float(step / out['edge_median'])

    # --- UV ---------------------------------------------------------------
    if uvs is not None and len(uvs) == len(V):
        UV = np.asarray(uvs, dtype=np.float64)
        uw = weld(UV.astype(np.float32))
        # islands: components of the triangle graph welded on (pos, uv) pairs
        both = np.stack([w, uw], axis=1)
        _, bw = np.unique(np.ascontiguousarray(both).view([('a', both.dtype), ('b', both.dtype)]).ravel(),
                          return_inverse=True)
        TB = bw[T]
        EB = np.unique(np.sort(np.vstack([TB[:, [0, 1]], TB[:, [1, 2]], TB[:, [2, 0]]]), axis=1), axis=0)
        out['uv_islands'] = _components(bw.max() + 1, EB)
        u0, u1, u2 = UV[T[:, 0]], UV[T[:, 1]], UV[T[:, 2]]
        uva = 0.5 * np.abs((u1[:, 0] - u0[:, 0]) * (u2[:, 1] - u0[:, 1])
                           - (u2[:, 0] - u0[:, 0]) * (u1[:, 1] - u0[:, 1]))
        m = (uva > 0) & (area > 0)
        if m.any():
            dens = np.sqrt(uva[m]) / np.sqrt(area[m])     # uv units per world unit
            out['texdens_p10'] = float(np.percentile(dens, 10))
            out['texdens_median'] = float(np.median(dens))
            out['texdens_p90'] = float(np.percentile(dens, 90))
            out['texdens_p90_over_p10'] = float(np.percentile(dens, 90) / max(np.percentile(dens, 10), 1e-12))
        out['uv_min'] = [float(UV[:, 0].min()), float(UV[:, 1].min())]
        out['uv_max'] = [float(UV[:, 0].max()), float(UV[:, 1].max())]
    return out


def _components(n, E, want_labels=False):
    parent = np.arange(n)

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for u, v in E:
        ru, rv = find(u), find(v)
        if ru != rv:
            parent[ru] = rv
    used = np.unique(E.ravel())
    roots = np.array([find(int(i)) for i in used])
    nroot = len(set(roots.tolist()))
    if not want_labels:
        return nroot
    lab = np.full(n, -1, dtype=np.int64)
    lab[used] = roots
    return nroot, lab


def selftest():
    # control 1: a unit cube, authored
    V = np.array([[x, y, z] for x in (0, 1.) for y in (0, 1.) for z in (0, 1.)])
    idx = {tuple(v): i for i, v in enumerate(V)}
    faces = []
    for ax in range(3):
        for side in (0., 1.):
            q = [i for i, v in enumerate(V) if v[ax] == side]
            # order the 4 into a quad
            o = [a for a in range(3) if a != ax]
            q.sort(key=lambda i: (V[i][o[0]], V[i][o[1]]))
            faces += [[q[0], q[1], q[3]], [q[0], q[3], q[2]]]
    cube = stats(V, np.array(faces), name='cube')
    assert abs(cube['axis_normal_pct'] - 100.0) < 1e-6, cube
    assert abs(cube['axis_edge_pct'] - 200.0 / 3.0) < 1e-6, cube['axis_edge_pct']
    assert abs(cube['AR_median'] - 2.0 / np.sqrt(3.0) * (1.0 / (2 * (1 - 1 / np.sqrt(2)) / (1 + 1 + np.sqrt(2)) * 1))) < 10, cube
    assert cube['sliver_pct_AR10'] == 0.0
    assert cube['components_3d'] == 1
    # control 2: a random soup, generated
    rng = np.random.default_rng(7)
    P = rng.normal(size=(600, 3))
    Tr = rng.integers(0, 600, size=(900, 3))
    Tr = Tr[(Tr[:, 0] != Tr[:, 1]) & (Tr[:, 1] != Tr[:, 2]) & (Tr[:, 0] != Tr[:, 2])]
    soup = stats(P, Tr, name='soup')
    assert soup['axis_normal_pct'] < 2.0, soup['axis_normal_pct']
    assert soup['sliver_pct_AR4'] > 10.0, soup['sliver_pct_AR4']
    return cube, soup


if __name__ == '__main__':
    c, s = selftest()
    print('SELFTEST PASS')
    for k in sorted(c):
        print('  cube %-24s %s' % (k, c[k]))
    for k in ('axis_normal_pct', 'sliver_pct_AR4', 'AR_median', 'valence_mean'):
        print('  soup %-24s %s' % (k, s.get(k)))
