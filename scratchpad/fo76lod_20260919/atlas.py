#!/usr/bin/env python3
"""UV / atlas anatomy of a FO76 .bto shape.

Answers, for one shape:
  * how many connected components in 3D (= source objects, if the generator
    worked per object);
  * each component's UV bounding box, and whether those boxes OVERLAP (one
    object one cell) or interleave (a shared unwrap);
  * the cell lattice: the distinct box edges in u and v, the implied grid, the
    gaps between neighbouring cells (padding);
  * whether cell AREA tracks the component's 3D surface area or its projected
    (screen-ish) size -- correlation of log(cell area) against log(3D area),
    log(bbox diagonal^2) and log(footprint area);
  * UV islands inside one cell.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import meshstats
import nif76


def components(V, T):
    w = meshstats.weld(np.asarray(V, dtype=np.float32))
    TW = w[np.asarray(T, dtype=np.int64).reshape(-1, 3)]
    E = np.unique(np.sort(np.vstack([TW[:, [0, 1]], TW[:, [1, 2]], TW[:, [2, 0]]]), axis=1), axis=0)
    n, lab = meshstats._components(int(w.max()) + 1, E, want_labels=True)
    # map every ORIGINAL vertex to a component id
    comp = lab[w]
    uniq = {c: i for i, c in enumerate(sorted(set(comp.tolist())))}
    return np.array([uniq[c] for c in comp]), len(uniq)


def analyse(path, block, out_json=None):
    n = nif76.Nif76(path)
    sh = n.shapes[block]
    V = np.array(sh['verts'], dtype=np.float64)
    UV = np.array(sh['uvs'], dtype=np.float64)
    T = np.array(sh['tris'], dtype=np.int64).reshape(-1, 3)
    comp, ncomp = components(V, T)
    print('%s block %d %r' % (os.path.basename(path), block, sh['name']))
    print('  nv=%d nt=%d  components=%d' % (len(V), len(T), ncomp))
    print('  UV range u[%.4f %.4f] v[%.4f %.4f]'
          % (UV[:, 0].min(), UV[:, 0].max(), UV[:, 1].min(), UV[:, 1].max()))

    boxes, areas3d, diag, foot, nverts = [], [], [], [], []
    p0, p1, p2 = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    tri_area = 0.5 * np.linalg.norm(np.cross(p1 - p0, p2 - p0), axis=1)
    tri_comp = comp[T[:, 0]]
    for c in range(ncomp):
        m = comp == c
        u, v = UV[m, 0], UV[m, 1]
        boxes.append((u.min(), v.min(), u.max(), v.max()))
        areas3d.append(float(tri_area[tri_comp == c].sum()))
        P = V[m]
        d = P.max(axis=0) - P.min(axis=0)
        diag.append(float(np.linalg.norm(d)))
        foot.append(float(d[0] * d[1]))
        nverts.append(int(m.sum()))
    B = np.array(boxes)
    cell_area = (B[:, 2] - B[:, 0]) * (B[:, 3] - B[:, 1])

    # --- do the boxes overlap? -------------------------------------------
    order = np.argsort(-cell_area)
    ov = 0
    pairs = 0
    top = order[:min(400, len(order))]
    for i in range(len(top)):
        for j in range(i + 1, len(top)):
            a, b = B[top[i]], B[top[j]]
            pairs += 1
            if (a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]):
                ov += 1
    print('  UV bbox overlap among the %d largest components: %d of %d pairs (%.2f%%)'
          % (len(top), ov, pairs, 100.0 * ov / max(pairs, 1)))

    # --- the lattice ------------------------------------------------------
    def lattice(vals, tol=1.0 / 2048):
        vals = np.sort(np.asarray(vals))
        lines, cur = [], [vals[0]]
        for x in vals[1:]:
            if x - cur[-1] <= tol:
                cur.append(x)
            else:
                lines.append(float(np.mean(cur)))
                cur = [x]
        lines.append(float(np.mean(cur)))
        return lines
    ul = lattice(np.concatenate([B[:, 0], B[:, 2]]))
    vl = lattice(np.concatenate([B[:, 1], B[:, 3]]))
    print('  distinct u edges %d, distinct v edges %d  (a strict grid would be a small number)'
          % (len(ul), len(vl)))
    print('  u edges (first 24): %s' % ['%.4f' % x for x in ul[:24]])
    print('  v edges (first 24): %s' % ['%.4f' % x for x in vl[:24]])

    # --- padding: nearest gap between non-overlapping boxes in u ---------
    gaps = []
    for i in range(len(B)):
        for j in range(len(B)):
            if i == j:
                continue
            if B[j][0] >= B[i][2] and not (B[j][3] <= B[i][1] or B[j][1] >= B[i][3]):
                gaps.append(B[j][0] - B[i][2])
    gaps = np.array([g for g in gaps if g >= 0])
    if len(gaps):
        print('  horizontal gap to the next cell: min %.5f  p10 %.5f  median %.5f'
              % (gaps.min(), np.percentile(gaps, 10), np.median(gaps)))

    # --- does cell area track 3D area, size or footprint? -----------------
    ok = (cell_area > 0) & (np.array(areas3d) > 0) & (np.array(diag) > 0)
    def corr(x):
        a = np.log(cell_area[ok])
        b = np.log(np.asarray(x)[ok])
        return float(np.corrcoef(a, b)[0, 1])
    print('  log-log correlation of cell area with:  3D surface area %.3f   bbox diag^2 %.3f   footprint %.3f   vert count %.3f'
          % (corr(areas3d), corr(np.array(diag) ** 2), corr(np.maximum(foot, 1e-6)), corr(np.maximum(nverts, 1))))
    # texel density per component: uv area / 3d area
    dens = np.sqrt(cell_area[ok]) / np.sqrt(np.array(areas3d)[ok])
    print('  per-component texel density sqrt(uvArea)/sqrt(3dArea): p10 %.4g median %.4g p90 %.4g  (p90/p10 = %.2f)'
          % (np.percentile(dens, 10), np.median(dens), np.percentile(dens, 90),
             np.percentile(dens, 90) / np.percentile(dens, 10)))

    # --- islands inside the biggest cell ---------------------------------
    big = int(np.argmax(cell_area))
    m = comp == big
    idx = np.where(m)[0]
    remap = -np.ones(len(V), dtype=np.int64)
    remap[idx] = np.arange(len(idx))
    Tm = T[tri_comp == big]
    st = meshstats.stats(V[idx], remap[Tm], UV[idx], name='largest cell')
    print('  largest component: %d verts, %d tris, cell %.4f x %.4f, %d UV islands inside, texdens p90/p10 %.2f'
          % (st['nv'], st['nt'], B[big][2] - B[big][0], B[big][3] - B[big][1],
             st.get('uv_islands', -1), st.get('texdens_p90_over_p10', -1)))
    # a component with ~1897 verts, the one bungo clicked
    near = np.argsort(np.abs(np.array(nverts) - 1897))[:3]
    for k in near:
        print('  component with %d verts: cell u[%.4f %.4f] v[%.4f %.4f]  (%.4f x %.4f)'
              % (nverts[k], B[k][0], B[k][2], B[k][1], B[k][3],
                 B[k][2] - B[k][0], B[k][3] - B[k][1]))

    if out_json:
        json.dump(dict(file=os.path.basename(path), block=block, name=sh['name'],
                       ncomp=ncomp, boxes=B.tolist(), areas3d=areas3d,
                       nverts=nverts, u_edges=ul, v_edges=vl),
                  open(out_json, 'w'))
    return B, comp, ncomp


if __name__ == '__main__':
    analyse(sys.argv[1], int(sys.argv[2]),
            sys.argv[3] if len(sys.argv) > 3 else None)
