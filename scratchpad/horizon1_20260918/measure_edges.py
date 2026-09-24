#!/usr/bin/env python
"""HORIZON1 step 1a -- the .lodo long-edge distribution over the DRAWN slot-0
meshes of a region pair.

Why it exists: a horizon term evaluated per VERTEX and interpolated across a
triangle cannot resolve a shadow edge that falls inside that triangle.  The
size of that error is the triangle's edge length in world units, so the limit
of the whole design is a number this script measures rather than a number
anyone argues about.

It shares no code with the C++ writers; it reads through
tests/spells/lodgen_native_decode.py's byte-table reader, whose own numbers are
already pinned against the shipped code.

CONTROL (root MISTAKES 2026-09-18 08:0x -- a second reader is not believed
until one of its numbers reproduces a number the shipped code already prints):
this script prints `instances` and `slot0_instances`, which must equal the
`.lodi` header's instanceCount and slotInstances[0].  Both are read from the
header, so the control is the WALK agreeing with the header, not the header
with itself.

usage: measure_edges.py <ws>.lodo <ws>.lodi [--top N] [--json out.json]
"""
import argparse
import json
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tests', 'spells'))
import lodgen_native_decode as D  # noqa: E402

LOCAL_INDEX_BYTES = 48
LOCAL_INDEX_NONE = 0xFF


def dequant(v, mn, ext):
    return mn + (v / 65535.0) * ext


def mesh_edges(L, mid):
    """Every level-0 triangle edge of mesh `mid`, in MESH units at scale 1."""
    mesh = L['meshes'][mid]
    lo, hi = 0xFFFFFFFF, 0
    for c in range(mesh['clusterFirst'], mesh['clusterFirst'] + mesh['clusterCount']):
        cl = L['clusters'][c]
        lo = min(lo, cl['vertexBase'])
        hi = max(hi, cl['vertexBase'] + cl['vertexCount'])
    if hi <= lo:
        return []
    pos = []
    for v in range(lo, hi):
        lv = L['vertices'][v]
        pos.append((dequant(lv['px'], mesh['aabbMin'][0], mesh['aabbExtent'][0]),
                    dequant(lv['py'], mesh['aabbMin'][1], mesh['aabbExtent'][1]),
                    dequant(lv['pz'], mesh['aabbMin'][2], mesh['aabbExtent'][2])))
    out = []
    for c in range(mesh['clusterFirst'], mesh['clusterFirst'] + mesh['clusterCount']):
        if c < len(L['clusterLods']) and L['clusterLods'][c]['level'] != 0:
            continue
        cl = L['clusters'][c]
        li = L['localIndices'][c * LOCAL_INDEX_BYTES:(c + 1) * LOCAL_INDEX_BYTES]
        for t in range(cl['triangleCount']):
            a, b, cc = li[t * 3], li[t * 3 + 1], li[t * 3 + 2]
            if LOCAL_INDEX_NONE in (a, b, cc):
                continue
            p = [pos[cl['vertexBase'] + k - lo] for k in (a, b, cc)]
            for i in range(3):
                q, r = p[i], p[(i + 1) % 3]
                out.append(math.sqrt((q[0] - r[0]) ** 2 + (q[1] - r[1]) ** 2
                                     + (q[2] - r[2]) ** 2))
    return out


def pct(sorted_vals, p):
    if not sorted_vals:
        return 0.0
    k = (len(sorted_vals) - 1) * p / 100.0
    f = int(math.floor(k))
    c = min(f + 1, len(sorted_vals) - 1)
    return sorted_vals[f] + (sorted_vals[c] - sorted_vals[f]) * (k - f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('lodi')
    ap.add_argument('--top', type=int, default=20)
    ap.add_argument('--json')
    a = ap.parse_args()

    L = D.read_lodo(a.lodo)
    T = D.read_lodi(a.lodi)
    hi = T['header']
    inst = T['instances']

    # slot 0 is the drawn rung for the placements in slotInstances[0]; the
    # record itself carries no slot, so the slot is derived exactly as the
    # writer's `slotInstances` partition is: instances are SORTED, and the
    # reader takes the slot from the .lodi's own per-instance mnamSlot when it
    # has one, else from the base's rep0 when rep0..rep3 are equal (the common
    # case: 2,970 of the bases in this region).  We therefore measure the mesh
    # REFERENCED BY rep0, per drawn placement, and say so.
    per_mesh = {}
    scales = []
    n_slot0 = 0
    for r in inst:
        b = L['bases'][r['baseId']] if r['baseId'] < len(L['bases']) else None
        if not b:
            continue
        mid = b['rep0']
        if mid == 0xFFFF or mid >= len(L['meshes']):
            continue
        n_slot0 += 1
        scales.append(r['scale'] / 8192.0)
        per_mesh.setdefault(mid, 0)
        per_mesh[mid] += 1

    cache = {}
    all_edges = []          # one entry per drawn placement-edge, scale applied
    by_model = {}
    for mid, count in per_mesh.items():
        if mid not in cache:
            cache[mid] = mesh_edges(L, mid)
        e = cache[mid]
        if not e:
            continue
        name = L['string_at'](L['meshes'][mid]['modelStringOffset'])
        by_model.setdefault(name, {'placements': 0, 'edges': 0, 'max': 0.0,
                                   'over512': 0, 'over1024': 0, 'over2048': 0,
                                   'tris': 0})
        m = by_model[name]
        m['placements'] += count

    # the drawn population: every placement's own edges at its own scale
    over = {512: 0, 1024: 0, 2048: 0}
    tri_over = {512: 0, 1024: 0, 2048: 0}
    total_tris = 0
    for r in inst:
        b = L['bases'][r['baseId']] if r['baseId'] < len(L['bases']) else None
        if not b:
            continue
        mid = b['rep0']
        if mid == 0xFFFF or mid >= len(L['meshes']):
            continue
        e = cache.get(mid) or []
        if not e:
            continue
        s = r['scale'] / 8192.0   # u16 -> float, NATIVE 4.1 (`scale = v / 8192`)
        name = L['string_at'](L['meshes'][mid]['modelStringOffset'])
        m = by_model[name]
        for i in range(0, len(e), 3):
            tri = [e[i] * s, e[i + 1] * s, e[i + 2] * s]
            total_tris += 1
            m['tris'] += 1
            mx = max(tri)
            if mx > m['max']:
                m['max'] = mx
            for bar in (512, 1024, 2048):
                if mx > bar:
                    tri_over[bar] += 1
                    m['over%d' % bar] += 1
            for x in tri:
                all_edges.append(x)
                m['edges'] += 1
                for bar in (512, 1024, 2048):
                    if x > bar:
                        over[bar] += 1

    all_edges.sort()
    res = {
        'lodo': os.path.basename(a.lodo),
        'lodi': os.path.basename(a.lodi),
        'lodi_version': hi.get('version'),
        'instances_header': hi.get('instanceCount'),
        'instances_walked': len(inst),
        'slot0_walked': n_slot0,
        'slotInstances': [hi.get('slotInstances%d' % k, None) for k in range(4)]
                         if 'slotInstances0' in hi else hi.get('slotInstances'),
        'drawn_triangles': total_tris,
        'drawn_edges': len(all_edges),
        'edge_p50': pct(all_edges, 50),
        'edge_p90': pct(all_edges, 90),
        'edge_p99': pct(all_edges, 99),
        'edge_max': all_edges[-1] if all_edges else 0.0,
        'edges_over_512': over[512],
        'edges_over_1024': over[1024],
        'edges_over_2048': over[2048],
        'tris_over_512': tri_over[512],
        'tris_over_1024': tri_over[1024],
        'tris_over_2048': tri_over[2048],
        'scale_min': min(scales) if scales else 0,
        'scale_max': max(scales) if scales else 0,
    }
    top = sorted(by_model.items(), key=lambda kv: -kv[1]['over512'])[:a.top]
    res['top'] = [{'model': k, **v} for k, v in top]

    print('instances_header %s instances_walked %d slot0_walked %d'
          % (res['instances_header'], res['instances_walked'], n_slot0))
    print('drawn_triangles %d drawn_edges %d' % (total_tris, len(all_edges)))
    print('edge p50 %.1f p90 %.1f p99 %.1f max %.1f'
          % (res['edge_p50'], res['edge_p90'], res['edge_p99'], res['edge_max']))
    print('edges over 512 %d / 1024 %d / 2048 %d'
          % (over[512], over[1024], over[2048]))
    print('triangles with an edge over 512 %d / 1024 %d / 2048 %d  (of %d)'
          % (tri_over[512], tri_over[1024], tri_over[2048], total_tris))
    print('scale range %.3f .. %.3f' % (res['scale_min'], res['scale_max']))
    print('--- top %d by triangles with an edge over 512 ---' % a.top)
    for row in res['top']:
        print('%-58s pl %5d tri %7d over512 %6d over1024 %5d over2048 %4d max %.0f'
              % (row['model'][:58], row['placements'], row['tris'],
                 row['over512'], row['over1024'], row['over2048'], row['max']))
    if a.json:
        with open(a.json, 'w') as f:
            json.dump(res, f, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
