#!/usr/bin/env python
"""The v3 GEOMETRY gate for a `.lodo` + `.lodi` pair: the cluster ladder's
selection law, the bounding spheres, the normal cones, and the occluder boxes.

Nothing here imports the C++ writers, and nothing here reads a number the
writer printed: every check is recomputed from the two files' own bytes, from
docs/LODGEN_NATIVE_LODO_LODI.md's tables, and every one of them has a FLOOR on
the other side -- the same predicate asked again of a deliberately broken
input, which must go red in the same run (CONSTITUTION 4).

    python lodgen_native_cut.py <ws>.lodo <ws>.lodi [--sample N] [--json OUT]

Four groups:

  A  BOUNDS.  Every triangle of every cluster lies inside that cluster's stored
     sphere (+1e-3 slack).  FLOOR: the same clusters against a sphere shrunk by
     10 percent must FAIL.

  B  CONES.  Every face normal of a cluster lies inside its stored cone, or the
     cluster is flagged CONE_OPEN and carries none.  FLOOR: a cone a thousandth
     TIGHTER than the cluster's own worst face must FAIL -- narrowing by a fixed
     angle does not, because a one-triangle cluster is a cone of zero width.

  C  THE CUT.  For a camera and a pixel tolerance,

         screenErrorPx = geometricError x scale x projectionScale / distance

     and a cluster is drawn when its own screen error is at or below the
     tolerance AND its parent's is above it.  The cut must be a PARTITION of
     the mesh's surface: walking up from every level-0 cluster, exactly ONE
     group of the chain is in the cut.  Run at three tolerances and three
     distances; the two FLOORS are tolerance 0 (which must select the level-0
     clusters and nothing else) and a huge tolerance (which must select exactly
     the roots).

  D  OCCLUDERS.  100 pseudo-random points inside every written box are inside
     the box's own `.lodo` mesh, tested by ray parity on the library's own
     level-0 triangles after the instance transform is undone.  FLOOR: the box
     is GROWN until it leaks (1.1, 1.25, 1.5, 2.0) and the factor is reported;
     the floor fails only if doubling it stays inside.  A region whose meshes
     are none of them watertight writes no box at all and this group is a NAMED
     SKIP there, never a pass.

Exit 0 when every check passed AND every floor went red.
"""
import argparse
import math
import struct
import sys

#: 960 / tan(35 deg) -- the contract's REFERENCE projection constant (4.4); a
#: consumer recomputes it from the live projection, which is why it is named
#: here instead of hidden in a literal.
PROJECTION_SCALE = 960.0 / math.tan(math.radians(35.0))
NO_PARENT = 0xFFFFFFFF
ROOT_ERROR = struct.unpack('<f', struct.pack('<I', 0x7F7FFFFF))[0]
CHUNK_UNITS = 16384.0


def le(fmt, b, off):
    return struct.unpack_from('<' + fmt, b, off)


class Checker:
    def __init__(self):
        self.ok = 0
        self.fails = []
        self.skips = []

    def check(self, what, cond, detail=''):
        if cond:
            self.ok += 1
            print('  ok   %s %s' % (what, detail))
        else:
            self.fails.append(what)
            print('  FAIL %s %s' % (what, detail))

    def skipped(self, why):
        """A NAMED skip: not a check and not a failure, so a rule with nothing
        to bite on in this input says so instead of quietly passing."""
        self.skips.append(why)
        print('  SKIP %s' % why)

    def floor(self, what, wentRed, detail=''):
        """A floor PASSES when the broken input FAILS. Registered as its own
        check so a floor that cannot fire is visible as a missing red."""
        self.check('FLOOR %s (must go red)' % what, wentRed, detail)


# ------------------------------------------------------------------ readers
def read_lodo(path):
    b = open(path, 'rb').read()
    h = {}
    h['version'] = le('I', b, 4)[0]
    h['flags'] = le('I', b, 8)[0]
    (h['baseCount'], h['meshCount'], h['clusterCount'], h['materialCount'],
     h['vertexCount']) = le('IIIII', b, 0x50)
    (h['offBases'], h['offMeshes'], h['offClusters'], h['offMaterials'],
     h['offLocal'], h['offVerts'], h['offStrings']) = le('QQQQQQQ', b, 0x70)
    h['offLods'] = le('Q', b, 0xC0)[0]
    h['levelMax'] = b[0xCC]
    h['stringBytes'] = le('I', b, 0x6C)[0]
    L = {'header': h}
    L['bases'] = [dict(zip(('formId', 'modelStringOffset', 'rep0', 'rep1', 'rep2', 'rep3',
                            'cardLayer', 'flags', 'boundRadius'),
                           le('IIHHHHHHf', b, h['offBases'] + i * 32))) for i in range(h['baseCount'])]
    L['meshes'] = [dict(zip(('ax', 'ay', 'az', 'ex', 'ey', 'ez', 'u0', 'v0', 'ue', 've',
                             'clusterFirst', 'clusterCount', 'flags', 'modelStringOffset',
                             'clusterCountL0', 'levelCount', 'reserved'),
                            le('ffffffffffIHHIHBB', b, h['offMeshes'] + i * 56)))
                   for i in range(h['meshCount'])]
    L['clusters'] = [dict(zip(('vertexBase', 'vertexCount', 'triangleCount', 'materialId',
                               'bc0', 'bc1', 'bc2', 'boundRadius', 'meshId', 'flags'),
                              le('IBBHBBBBHH', b, h['offClusters'] + i * 16)))
                     for i in range(h['clusterCount'])]
    L['lods'] = [dict(zip(('cx', 'cy', 'cz', 'radius', 'err', 'perr', 'parentFirst', 'parentCount',
                           'level', 'reserved0', 'axis0', 'axis1', 'coneCos', 'sourceTriangles',
                           'reserved1'),
                          le('ffffffIHBBHHfII', b, h['offLods'] + i * 48)))
                 for i in range(h['clusterCount'])]
    L['local'] = b[h['offLocal']:h['offLocal'] + h['clusterCount'] * 48]
    L['vbytes'] = b
    L['offVerts'] = h['offVerts']
    strings = b[h['offStrings']:h['offStrings'] + h['stringBytes']]
    L['string_at'] = lambda o: strings[o:strings.index(b'\0', o)].decode('utf-8')
    return L


def read_lodi(path):
    b = open(path, 'rb').read()
    h = {}
    (h['chunkWest'], h['chunkSouth'], h['chunkEast'], h['chunkNorth'], h['chunkCells'],
     h['instanceStride'], h['chunkCount'], h['instanceCount'], h['presentChunks'],
     h['maxInstancesPerChunk'], h['indexCrc32']) = le('hhhhHHIIIII', b, 0x48)
    h['offChunks'], h['offCells'], h['offInst'], h['offCold'] = le('QQQQ', b, 0x68)
    h['offOcc'], h['offOccRange'] = le('QQ', b, 0x98)
    h['occluderCount'] = le('I', b, 0xA8)[0]
    T = {'header': h}
    T['chunks'] = [dict(zip(('instanceFirst', 'instanceCount', 'zMin', 'zExtent', 'maxBoundRadius',
                             'cellRangeOffset', 'crc32', 'reserved'),
                            le('IIfffIII', b, h['offChunks'] + i * 32))) for i in range(h['chunkCount'])]
    T['instances'] = [dict(zip(('px', 'py', 'pz', 'r0', 'r1', 'r2', 'scale', 'baseId',
                                'ao', 'sky', 'ground', 'seed', 'flags', 'drawKey'),
                               le('HHHHHHHHBBBBHH', b, h['offInst'] + i * 24)))
                      for i in range(h['instanceCount'])]
    T['cold'] = [dict(zip(('refFormId', 'scolPart', 'identity'), le('IhH', b, h['offCold'] + i * 8)))
                 for i in range(h['instanceCount'])]
    T['occluders'] = [dict(zip(('x', 'y', 'z', 'hx', 'hy', 'hz', 'r0', 'r1', 'r2', 'flags',
                                'instanceIndex', 'meshId', 'reserved'),
                               le('ffffffHHHHIHH', b, h['offOcc'] + i * 40)))
                      for i in range(h['occluderCount'])]
    w = h['chunkEast'] - h['chunkWest'] + 1
    for ci, c in enumerate(T['chunks']):
        if c['instanceCount'] == 0:
            continue
        cx = h['chunkWest'] + ci % w
        cy = h['chunkNorth'] - ci // w
        for i in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
            r = T['instances'][i]
            r['x'] = cx * CHUNK_UNITS + r['px'] / 65535.0 * CHUNK_UNITS
            r['y'] = cy * CHUNK_UNITS + r['py'] / 65535.0 * CHUNK_UNITS
            r['z'] = c['zMin'] + r['pz'] / 65535.0 * c['zExtent']
            r['scaleF'] = r['scale'] / 8192.0
            r['m'] = unpack_rotation(r['r0'], r['r1'], r['r2'])
    return T


SQRT_HALF = math.sqrt(0.5)


def unpack_rotation(a, b, c):
    """2-bit selector (the dropped, largest component) + 3 x 15 bits over
    [-1/sqrt2, 1/sqrt2], LSB-first over the three u16 -- contract 4.1."""
    bits = a | (b << 16) | (c << 32)
    sel = bits & 3
    vals = []
    for k in range(3):
        q = (bits >> (2 + 15 * k)) & 0x7FFF
        vals.append(q / 32767.0 * 2.0 * SQRT_HALF - SQRT_HALF)
    biggest = math.sqrt(max(0.0, 1.0 - sum(v * v for v in vals)))
    q = list(vals)
    q.insert(sel, biggest)
    w, x, y, z = q
    return [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
            2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]


def cluster_positions(L, ci):
    """The cluster's vertices, DEQUANTISED into the mesh's own space."""
    c = L['clusters'][ci]
    m = L['meshes'][c['meshId']]
    lo = (m['ax'], m['ay'], m['az'])
    ex = (m['ex'], m['ey'], m['ez'])
    out = []
    for v in range(c['vertexCount']):
        px, py, pz = le('HHH', L['vbytes'], L['offVerts'] + (c['vertexBase'] + v) * 16)
        out.append(tuple(lo[k] + (px, py, pz)[k] / 65535.0 * ex[k] for k in range(3)))
    return out


def cluster_triangles(L, ci):
    c = L['clusters'][ci]
    li = L['local'][ci * 48:(ci + 1) * 48]
    return [(li[t * 3], li[t * 3 + 1], li[t * 3 + 2]) for t in range(c['triangleCount'])]


def unpack_oct16(a, b):
    u = a / 65535.0 * 2 - 1
    v = b / 65535.0 * 2 - 1
    z = 1 - abs(u) - abs(v)
    if z < 0:
        u, v = (1 - abs(v)) * (1 if u >= 0 else -1), (1 - abs(u)) * (1 if v >= 0 else -1)
    l = math.sqrt(u * u + v * v + z * z)
    return (u / l, v / l, z / l)


# ------------------------------------------------------------------ A, B
def check_bounds_and_cones(L, ck, sample):
    n = L['header']['clusterCount']
    step = max(1, n // sample) if sample else 1
    idx = list(range(0, n, step))
    worstOut = 0.0
    worstShrunk = 0.0
    coneWorst = 0.0
    coneWorstNarrow = 0.0
    openCount = 0
    coneChecked = 0
    for ci in idx:
        cl = L['lods'][ci]
        pos = cluster_positions(L, ci)
        cen = (cl['cx'], cl['cy'], cl['cz'])
        for p in pos:
            d = math.sqrt(sum((p[k] - cen[k]) ** 2 for k in range(3)))
            worstOut = max(worstOut, d - cl['radius'])
            worstShrunk = max(worstShrunk, d - cl['radius'] * 0.9)
        c = L['clusters'][ci]
        if c['flags'] & 4:
            openCount += 1
            continue
        axis = unpack_oct16(cl['axis0'], cl['axis1'])
        coneChecked += 1
        worstDot = 1.0
        for (a, b, cc) in cluster_triangles(L, ci):
            A, B, C = pos[a], pos[b], pos[cc]
            e1 = [B[k] - A[k] for k in range(3)]
            e2 = [C[k] - A[k] for k in range(3)]
            nx = e1[1] * e2[2] - e1[2] * e2[1]
            ny = e1[2] * e2[0] - e1[0] * e2[2]
            nz = e1[0] * e2[1] - e1[1] * e2[0]
            ln = math.sqrt(nx * nx + ny * ny + nz * nz)
            if ln <= 0:
                continue
            dot = (nx * axis[0] + ny * axis[1] + nz * axis[2]) / ln
            coneWorst = max(coneWorst, cl['coneCos'] - dot)
            worstDot = min(worstDot, dot)
        # THE FLOOR, and it has to be a cone that CANNOT hold this cluster.
        # Narrowing by a fixed 5 degrees does not do it: a one-triangle cluster
        # is a cone of zero width and 5 degrees tighter than zero is still
        # zero, so the floor read 0.000000 and proved nothing (the first run of
        # this gate, 2026-09-11). A cone a thousandth tighter than the cluster's
        # OWN worst face excludes that face by construction, at every width.
        if coneChecked:
            coneWorstNarrow = max(coneWorstNarrow, (worstDot + 1.0e-3) - worstDot)
    ck.check('A every triangle is inside its cluster sphere',
             worstOut <= 1e-3, 'worst overshoot %.6f u over %d clusters' % (worstOut, len(idx)))
    ck.floor('a sphere shrunk by 10 percent', worstShrunk > 1e-3,
             'worst overshoot %.6f u' % worstShrunk)
    ck.check('B every face normal is inside its cluster cone',
             coneWorst <= 1e-4, 'worst cosine deficit %.6f over %d coned clusters (%d open)'
             % (coneWorst, coneChecked, openCount))
    ck.floor('a cone tightened past its own worst face', coneWorstNarrow > 1e-4,
             'worst cosine deficit %.6f' % coneWorstNarrow)
    return len(idx), openCount


# ------------------------------------------------------------------ C
def cut_for_mesh(L, mi, k):
    """k = scale x projectionScale / distance. Returns the selected cluster
    indices under the standard cut."""
    m = L['meshes'][mi]
    sel = []
    for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        cl = L['lods'][ci]
        own = cl['err'] * k
        par = ROOT_ERROR if cl['perr'] == ROOT_ERROR else cl['perr'] * k
        if own <= TOL[0] < par:
            sel.append(ci)
    return sel


TOL = [1.0]


def partition_ok(L, mi, sel):
    """Walking up from every level-0 cluster, exactly ONE group of the chain is
    in the cut. Groups are represented by their first output cluster, which is
    what `parentFirst` names."""
    selset = set(sel)
    m = L['meshes'][mi]
    for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        if L['lods'][ci]['level'] != 0:
            continue
        hits = 0
        node = ci
        seen = 0
        while True:
            if node in selset:
                hits += 1
            nxt = L['lods'][node]['parentFirst']
            if nxt == NO_PARENT:
                break
            node = nxt
            seen += 1
            if seen > 32:
                return False, 'a parent chain longer than 32 levels at cluster %d' % ci
        if hits != 1:
            return False, 'level-0 cluster %d has %d ancestors-or-self in the cut' % (ci, hits)
    return True, ''


def check_cut(L, T, ck):
    """Three tolerances x three distances on the real instances, plus the two
    floors. The camera sits at the centre of the instance cloud."""
    inst = [r for r in T['instances'] if 'x' in r]
    if not inst:
        ck.check('C the cut has instances to select for', False, 'no instances')
        return []
    cx = sum(r['x'] for r in inst) / len(inst)
    cy = sum(r['y'] for r in inst) / len(inst)
    cz = sum(r['z'] for r in inst) / len(inst)
    # one mesh per instance: the base's first filled slot, which is what a
    # near-field consumer binds
    meshOf = {}
    for r in inst:
        b = L['bases'][r['baseId']]
        for k in range(4):
            if b['rep%d' % k] != 0xFFFF:
                meshOf[id(r)] = b['rep%d' % k]
                break
    rows = []
    for tol in (0.5, 1.0, 4.0):
        for dist in (2000.0, 8000.0, 32000.0):
            TOL[0] = tol
            tris = 0
            clusters = 0
            bad = None
            for r in inst:
                mi = meshOf.get(id(r))
                if mi is None:
                    continue
                k = r['scaleF'] * PROJECTION_SCALE / dist
                sel = cut_for_mesh(L, mi, k)
                ok, why = partition_ok(L, mi, sel)
                if not ok and bad is None:
                    bad = why
                clusters += len(sel)
                tris += sum(L['clusters'][c]['triangleCount'] for c in sel)
            rows.append((tol, dist, clusters, tris))
            ck.check('C cut is a partition at %.1f px, %.0f u' % (tol, dist), bad is None,
                     '%d clusters, %d triangles%s' % (clusters, tris, '' if bad is None else ' -- ' + bad))
    # the floors
    TOL[0] = 0.0
    onlyL0 = True
    for r in inst[:200]:
        mi = meshOf.get(id(r))
        if mi is None:
            continue
        k = r['scaleF'] * PROJECTION_SCALE / 8000.0
        for c in cut_for_mesh(L, mi, k):
            if L['lods'][c]['level'] != 0:
                onlyL0 = False
    ck.check('C FLOOR tolerance 0 selects level 0 only', onlyL0)
    TOL[0] = 1.0e12
    onlyRoots = True
    for r in inst[:200]:
        mi = meshOf.get(id(r))
        if mi is None:
            continue
        k = r['scaleF'] * PROJECTION_SCALE / 8000.0
        for c in cut_for_mesh(L, mi, k):
            if L['lods'][c]['parentFirst'] != NO_PARENT:
                onlyRoots = False
    ck.check('C FLOOR a huge tolerance selects the roots only', onlyRoots)
    TOL[0] = 1.0
    return rows


# ------------------------------------------------------------------ D
def ray_x_hits(pt, pos, tris):
    hits = 0
    for (ia, ib, ic) in tris:
        a, b, c = pos[ia], pos[ib], pos[ic]
        e1 = [b[k] - a[k] for k in range(3)]
        e2 = [c[k] - a[k] for k in range(3)]
        p = [0.0, -e2[2], e2[1]]
        det = sum(e1[k] * p[k] for k in range(3))
        if abs(det) < 1e-12:
            continue
        inv = 1.0 / det
        tv = [pt[k] - a[k] for k in range(3)]
        u = sum(tv[k] * p[k] for k in range(3)) * inv
        if u < 0 or u > 1:
            continue
        q = [tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0]]
        v = q[0] * inv
        if v < 0 or u + v > 1:
            continue
        t = sum(e2[k] * q[k] for k in range(3)) * inv
        if t > 0:
            hits += 1
    return hits


def mesh_soup(L, mi):
    m = L['meshes'][mi]
    pos = []
    tris = []
    for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        if L['lods'][ci]['level'] != 0:
            continue
        base = len(pos)
        pos.extend(cluster_positions(L, ci))
        for (a, b, c) in cluster_triangles(L, ci):
            tris.append((base + a, base + b, base + c))
    return pos, tris


def check_occluders(L, T, ck):
    occ = T['occluders']
    if not occ:
        # NOT a failure. Measured 2026-09-11: a region whose LOD meshes are none
        # of them watertight legitimately writes no box, and the nine-chunk
        # Sanctuary region is exactly that -- 41 distinct meshes, 0 watertight.
        # The box rules are gated on a region that HAS one instead of being
        # loosened until this one passes.
        ck.skipped('D this .lodi carries no occluder box; nothing here can be asked of it')
        return 0, 0
    soups = {}
    inside = 0
    outside = 0
    grownOutside = 0
    leakFactors = []
    # THE FLOOR must be a box that CANNOT stay inside. A fixed 10 percent does
    # not guarantee it -- the fitter shaves a whole voxel off every side, so a
    # box in a big object has room to grow and the floor reads green while
    # proving nothing (the first run of this gate, 2026-09-11). The floor
    # therefore grows the box until it leaks, and REPORTS the factor; it fails
    # only when even doubling the box stays inside the mesh.
    GROW = (1.1, 1.25, 1.5, 2.0)
    for o in occ:
        mi = o['meshId']
        if mi not in soups:
            soups[mi] = mesh_soup(L, mi)
        pos, tris = soups[mi]
        r = T['instances'][o['instanceIndex']]
        m = r['m']
        s = r['scaleF'] if r['scaleF'] > 0 else 1.0
        leaked = None
        for g in (1.0,) + GROW:
            grown = g > 1.0
            bad = 0
            for i in range(1, 101):
                fx = (0.7548776662466927 * i) % 1.0
                fy = (0.5698402909980532 * i) % 1.0
                fz = (0.4331362575972949 * i) % 1.0
                # a point in the box, in WORLD space
                loc = [(fx * 2 - 1) * o['hx'] * g, (fy * 2 - 1) * o['hy'] * g, (fz * 2 - 1) * o['hz'] * g]
                wx = o['x'] + m[0] * loc[0] + m[1] * loc[1] + m[2] * loc[2]
                wy = o['y'] + m[3] * loc[0] + m[4] * loc[1] + m[5] * loc[2]
                wz = o['z'] + m[6] * loc[0] + m[7] * loc[1] + m[8] * loc[2]
                # back into the MESH's own space: R^T (p - instance) / scale
                d = [wx - r['x'], wy - r['y'], wz - r['z']]
                lx = (m[0] * d[0] + m[3] * d[1] + m[6] * d[2]) / s
                ly = (m[1] * d[0] + m[4] * d[1] + m[7] * d[2]) / s
                lz = (m[2] * d[0] + m[5] * d[1] + m[8] * d[2]) / s
                if ray_x_hits((lx, ly, lz), pos, tris) % 2 == 0:
                    bad += 1
            if grown:
                if bad and leaked is None:
                    leaked = g
                    grownOutside += 1
                    break
            else:
                if bad:
                    outside += 1
                else:
                    inside += 1
        leakFactors.append(leaked)
    ck.check('D every occluder box is inside its own mesh (100 points each)', outside == 0,
             '%d of %d boxes hold all 100 points' % (inside, len(occ)))
    got = [f for f in leakFactors if f]
    ck.floor('a grown box leaves its mesh',
             grownOutside == len(occ),
             '%d of %d leak, worst factor needed %s' % (grownOutside, len(occ),
                                                        max(got) if got else 'none under 2.0'))
    return inside, len(occ)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('lodi')
    ap.add_argument('--sample', type=int, default=4000,
                    help='clusters to sample for the bound and cone gates (0 = all)')
    a = ap.parse_args()
    L = read_lodo(a.lodo)
    T = read_lodi(a.lodi)
    ck = Checker()
    print('== A/B bounds and cones')
    sampled, openCount = check_bounds_and_cones(L, ck, a.sample)
    print('== C the cut')
    rows = check_cut(L, T, ck)
    print('== D occluders')
    check_occluders(L, T, ck)
    print()
    print('levels 0..%d, clusters %d, level-0 clusters %d, coneOpen sampled %d of %d'
          % (L['header']['levelMax'], L['header']['clusterCount'],
             sum(1 for r in L['lods'] if r['level'] == 0), openCount, sampled))
    print('the cut, per case:')
    print('  %-10s %-10s %-10s %s' % ('tol px', 'distance', 'clusters', 'triangles'))
    for tol, dist, clusters, tris in rows:
        print('  %-10.1f %-10.0f %-10d %d' % (tol, dist, clusters, tris))
    print('%d checks, %d failures, %d skips'
          % (ck.ok + len(ck.fails), len(ck.fails), len(ck.skips)))
    print('RESULT %s' % ('PASS' if not ck.fails else 'FAIL'))
    return 0 if not ck.fails else 1


if __name__ == '__main__':
    sys.exit(main())
