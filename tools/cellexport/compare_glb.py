"""Compare a simplified .glb against the full-detail reference .glb (tools/cellexport, lane CELL1).

python compare_glb.py <reference.glb> <candidate.glb> [<candidate.glb> ...]

Per candidate, one block of plain numbers:
  triangles / vertices / primitives / materials / textures / has UVs
  bounds (min, max, size) and their difference from the reference, in metres
  two-way surface distance, in metres: area-weighted random points on each
    surface, nearest point in a dense sample of the other surface
    (candidate->reference = how far the new surface strays;
     reference->candidate = how much of the original is lost).
    The noise floor is the reference sample spacing, printed as 'floor'.
  open edges: edges used by one triangle after welding coincident positions
    (1 mm), with their total length; printed for the reference too, since
    kit pieces are open by design and only the change means anything.
"""
import json
import struct
import sys

import numpy as np
from scipy.spatial import cKDTree

CT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NC = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def load(path):
    d = open(path, 'rb').read()
    if d[:4] != b'glTF':
        raise SystemExit('%s is not a binary glTF' % path)
    o = 12
    js = bin_ = None
    while o < len(d):
        ln, kind = struct.unpack_from('<II', d, o)
        chunk = d[o + 8:o + 8 + ln]
        if kind == 0x4E4F534A:
            js = json.loads(chunk)
        elif kind == 0x004E4942:
            bin_ = chunk
        o += 8 + ln
    return js, bin_


def accessor(js, bin_, i):
    a = js['accessors'][i]
    v = js['bufferViews'][a['bufferView']]
    dt = np.dtype(CT[a['componentType']])
    n = NC[a['type']]
    off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = v.get('byteStride', 0) or dt.itemsize * n
    cnt = a['count']
    if stride == dt.itemsize * n:
        arr = np.frombuffer(bin_, dtype=dt, count=cnt * n, offset=off).reshape(cnt, n)
    else:
        raw = np.frombuffer(bin_, dtype=np.uint8, count=stride * (cnt - 1) + dt.itemsize * n, offset=off)
        arr = np.lib.stride_tricks.as_strided(raw, shape=(cnt, dt.itemsize * n), strides=(stride, 1))
        arr = np.ascontiguousarray(arr).view(dt).reshape(cnt, n)
    return arr


def node_matrix(n):
    if 'matrix' in n:
        return np.array(n['matrix'], dtype=np.float64).reshape(4, 4).T
    M = np.eye(4)
    t = n.get('translation', [0, 0, 0]); r = n.get('rotation', [0, 0, 0, 1]); s = n.get('scale', [1, 1, 1])
    x, y, z, w = r
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M[:3, :3] = R * np.array(s)
    M[:3, 3] = t
    return M


def mesh_of(path, skip_names=()):
    js, bin_ = load(path)
    P, T, info = [], [], dict(prims=0, uv=0, mats=set(), verts=0)
    base = 0

    def walk(ni, M):
        nonlocal base
        n = js['nodes'][ni]
        M = M @ node_matrix(n)
        if n.get('name', '') in skip_names:
            return
        if 'mesh' in n:
            for pr in js['meshes'][n['mesh']]['primitives']:
                if pr.get('mode', 4) != 4:
                    continue
                pos = accessor(js, bin_, pr['attributes']['POSITION']).astype(np.float64)
                pos = pos @ M[:3, :3].T + M[:3, 3]
                if 'indices' in pr:
                    idx = accessor(js, bin_, pr['indices']).reshape(-1, 3).astype(np.int64)
                else:
                    idx = np.arange(len(pos)).reshape(-1, 3)
                P.append(pos); T.append(idx + base); base += len(pos)
                info['prims'] += 1
                info['uv'] += 'TEXCOORD_0' in pr['attributes']
                if 'material' in pr:
                    info['mats'].add(pr['material'])
        for c in n.get('children', []):
            walk(c, M)

    scene = js['scenes'][js.get('scene', 0)]
    for r in scene['nodes']:
        walk(r, np.eye(4))
    pos = np.concatenate(P); tris = np.concatenate(T)
    info['verts'] = len(pos)
    info['textures'] = len(js.get('images', []))
    info['materials'] = len(info['mats'])
    return pos, tris, info


def sample(pos, tris, n, rng):
    a, b, c = pos[tris[:, 0]], pos[tris[:, 1]], pos[tris[:, 2]]
    area = 0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
    tot = area.sum()
    pick = rng.choice(len(tris), size=n, p=area / tot)
    u = rng.random(n); v = rng.random(n)
    flip = u + v > 1
    u[flip] = 1 - u[flip]; v[flip] = 1 - v[flip]
    pts = a[pick] + (b[pick] - a[pick]) * u[:, None] + (c[pick] - a[pick]) * v[:, None]
    return pts, tot


def open_edges(pos, tris):
    q = np.round(pos / 0.001).astype(np.int64)
    _, weld = np.unique(q, axis=0, return_inverse=True)
    t = weld.reshape(-1)[tris]
    e = np.concatenate([t[:, [0, 1]], t[:, [1, 2]], t[:, [2, 0]]])
    e.sort(axis=1)
    e = e[e[:, 0] != e[:, 1]]
    key = e[:, 0] * (int(weld.max()) + 1) + e[:, 1]
    k, cnt = np.unique(key, return_counts=True)
    once = k[cnt == 1]
    return len(once)


def main():
    rng = np.random.default_rng(1)
    ref = sys.argv[1]
    rp, rt, ri = mesh_of(ref, skip_names=('Ground',))
    rs, rarea = sample(rp, rt, 2_000_000, rng)
    rtree = cKDTree(rs)
    floor = np.sqrt(rarea / len(rs))
    rmin, rmax = rp.min(0), rp.max(0)
    print('REFERENCE %s: %d triangles, %d vertices, %d primitives, %d materials, %d textures, area %.0f m2, floor %.3f m, open edges %d'
          % (ref, len(rt), ri['verts'], ri['prims'], ri['materials'], ri['textures'], rarea, floor, open_edges(rp, rt)))
    print('  bounds min %s max %s' % (np.round(rmin, 2), np.round(rmax, 2)))
    for cand in sys.argv[2:]:
        cp, ct, ci = mesh_of(cand, skip_names=('Ground',))
        cs, carea = sample(cp, ct, 500_000, rng)
        d_c2r, _ = rtree.query(cs)
        ctree = cKDTree(sample(cp, ct, 2_000_000, rng)[0])
        d_r2c, _ = ctree.query(rs[::4])
        cmin, cmax = cp.min(0), cp.max(0)
        print('CANDIDATE %s' % cand)
        print('  %d triangles, %d vertices, %d primitives, %d materials, %d textures, UVs on %d/%d primitives'
              % (len(ct), ci['verts'], ci['prims'], ci['materials'], ci['textures'], ci['uv'], ci['prims']))
        print('  bounds min %s max %s; min shift %.2f m, max shift %.2f m'
              % (np.round(cmin, 2), np.round(cmax, 2), np.abs(cmin - rmin).max(), np.abs(cmax - rmax).max()))
        print('  area %.0f m2 (%.0f%% of reference)' % (carea, 100 * carea / rarea))
        for lab, dd in (('new->original', d_c2r), ('original->new', d_r2c)):
            print('  %s distance m: mean %.2f  median %.2f  p90 %.2f  p99 %.2f  max %.2f'
                  % (lab, dd.mean(), np.median(dd), np.percentile(dd, 90), np.percentile(dd, 99), dd.max()))
        print('  open edges %d' % open_edges(cp, ct))


if __name__ == '__main__':
    main()
