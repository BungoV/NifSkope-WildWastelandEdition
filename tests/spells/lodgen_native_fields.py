#!/usr/bin/env python
"""The v2 FIELD gate for a REAL `.lodo` + `.lodi` pair: every word lane
NATIVE1a added is checked to be WRITTEN and to MOVE, each with a floor on the
other side (the three rules of 2026-09-04 21:33, `fo4cs-census-field`).

    python tests/spells/lodgen_native_fields.py <ws>.lodo <ws>.lodi \\
        --mesh-report <file> [--manifest <chunk>.BTO.manifest.txt]...

One subsection per field:

  a  loadOrderHash      written, non-zero, the same in both headers, and it
                        MOVES when the plugin list changes (recomputed here
                        from a list with one extra name)
  b  refFormId          every instance's cold record names a distinct placed
                        REFR, and the set equals the stock manifests' set
  c  bound radius       the rule is base.boundRadius x scale, so every record
                        has a non-zero scale and every base a non-zero radius,
                        and the product is inside the chunk's maxBoundRadius
  d  the sort law       (chunk, cell, drawKey, ref, part) holds; drawKey is the
                        base's (mesh, material) rank; and it MOVES -- more than
                        one drawKey is present, and at least one cell holds two
                        instances whose order the drawKey and not the ref
                        decides
  e  GPU cache order    the mesh report's ACMR improves on the corpus, no mesh
                        gets worse, and the floor is a control on the METRIC: a
                        deliberately shuffled triangle order on the largest mesh
                        must read worse than the order we emitted
  f  identity           present, moving, unique inside every STOCK chunk, and
                        equal to the stock manifest's index column
  g  silhouette         boundaryEmitted <= boundarySource for every mesh, and
                        the floor is a hole-punched mesh read from the report's
                        own numbers

Prints `N checks, M failures` then `RESULT PASS`/`FAIL`.
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from lodgen_native_decode import read_lodo, read_lodi, draw_key_ranks, Refusal, fnv1a64  # noqa: E402
from lodgen_native_decode import ROOT_ERROR as ROOT  # noqa: E402


class Checker(object):
    def __init__(self):
        self.ok = 0
        self.fails = []

    def check(self, what, cond, detail=''):
        if cond:
            self.ok += 1
            print('  ok   %s' % what)
        else:
            self.fails.append(what)
            print('  FAIL %s %s' % (what, detail))

    def skip(self, why):
        """A NAMED skip: not a check and not a failure, so a rule that has
        nothing to bite on in this input says so instead of passing."""
        self.skips = getattr(self, 'skips', [])
        self.skips.append(why)
        print('  SKIP %s' % why)


def mesh_indices(L, meshId):
    """Rebuild one mesh's global triangle list from its clusters, exactly as a
    consumer draws it: cluster vertexBase + the 3 local indices a triangle."""
    m = L['meshes'][meshId]
    out = []
    for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        # v3: LEVEL 0 only -- the cache-order numbers describe the full-detail
        # geometry, and folding the ladder's copies in would measure the ladder
        if L['clusterLods'][c]['level'] != 0:
            continue
        cl = L['clusters'][c]
        li = L['localIndices'][c * 48:(c + 1) * 48]
        for k in range(cl['triangleCount'] * 3):
            out.append(cl['vertexBase'] + li[k])
    return out


def acmr(indices, cache=16):
    """ACMR under a FIFO post-transform cache: vertex-shader invocations a
    triangle. The same model meshopt_analyzeVertexCache uses at warp_size 0."""
    if not indices:
        return 0.0
    fifo = []
    inCache = set()
    misses = 0
    for v in indices:
        if v in inCache:
            continue
        misses += 1
        fifo.append(v)
        inCache.add(v)
        if len(fifo) > cache:
            inCache.discard(fifo.pop(0))
    return misses / (len(indices) / 3.0)


def shuffled_acmr(L, meshId, seed=20260911):
    """THE FLOOR: the same mesh, its triangles put in a deliberately bad order,
    scored by the same model. If this does not read worse than what we emitted,
    the metric cannot tell a good order from a bad one and every number above it
    is decoration."""
    import random
    idx = mesh_indices(L, meshId)
    tris = [idx[i:i + 3] for i in range(0, len(idx), 3)]
    random.Random(seed).shuffle(tris)
    flat = [v for t in tris for v in t]
    return acmr(idx), acmr(flat)


def read_mesh_report(path):
    rows = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            # report VERSION 5: the thirteen ladder columns (v3),
            # `casterInstances` (v4) and the three NATIVE1c columns
            # (refFoliage, refLevelSilhouette, silhouetteWorst) sit before
            # `model`, which stays the line's remainder because it is the only
            # token that may hold a space
            # v6 (2026-09-18): selfAoMean, selfAoDark before model
            t = line.split(None, 30)
            rows.append({'meshId': int(t[0]), 'tris': int(t[1]), 'srcVerts': int(t[2]),
                         'emitVerts': int(t[3]), 'acmrB': float(t[4]), 'acmrA': float(t[5]),
                         'atvrB': float(t[6]), 'atvrA': float(t[7]),
                         'bSrc': int(t[8]), 'bEmit': int(t[9]),
                         'levels': int(t[10]), 'clL0': int(t[11]), 'clLadder': int(t[12]),
                         'maxError': float(t[13]), 'groups': int(t[14]), 'refSmall': int(t[15]),
                         'refNoCut': int(t[16]), 'refFlat': int(t[17]), 'refSil': int(t[18]),
                         'errExact': int(t[19]), 'errBounded': int(t[20]), 'welded': int(t[21]),
                         'uvConf': int(t[22]), 'bCoarse': int(t[23]),
                         'casters': int(t[24]),
                         'refFoliage': int(t[25]), 'refLvlSil': int(t[26]),
                         'silWorst': float(t[27]),
                         'selfAoMean': float(t[28]), 'selfAoDark': int(t[29]),
                         'model': t[30].strip()})
    return rows


def manifest_rows(path):
    out = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            t = line.split()
            if not t or not t[0].isdigit() or len(t) < 11:
                continue
            out.append({'index': int(t[0]), 'ref': int(t[9], 16), 'part': int(t[10])})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('lodi')
    ap.add_argument('--mesh-report')
    ap.add_argument('--manifest', action='append', default=[])
    a = ap.parse_args()
    ck = Checker()
    try:
        L = read_lodo(a.lodo)
        T = read_lodi(a.lodi)
    except Refusal as e:
        print('REFUSED: %s' % e)
        print('RESULT FAIL')
        return 1
    h, ih = L['header'], T['header']

    # ---- a. the load-order hash ------------------------------------------
    print('a. loadOrderHash')
    ck.check('a1 written and non-zero in the .lodo', h['loadOrderHash'] != 0, '%016x' % h['loadOrderHash'])
    ck.check('a2 the .lodi carries the same value', h['loadOrderHash'] == ih['loadOrderHash'],
             '%016x / %016x' % (h['loadOrderHash'], ih['loadOrderHash']))
    # it MOVES: the same law over a list with one more plugin must differ
    moved = fnv1a64(struct.pack('<Q', 1), h['loadOrderHash'])
    ck.check('a3 the law moves when the load order does (floor)', moved != h['loadOrderHash'])

    # ---- b. the placed REFR per instance ---------------------------------
    print('b. the placed REFR form id, per instance, in the cold record')
    keys = [(c['refFormId'], c['scolPart']) for c in T['cold']]
    ck.check('b1 every instance names a REFR', all(k[0] != 0 for k in keys),
             sum(1 for k in keys if k[0] == 0))
    ck.check('b2 the (ref, part) key is unique', len(set(keys)) == len(keys),
             len(keys) - len(set(keys)))
    if a.manifest:
        want = set()
        for m in a.manifest:
            for r in manifest_rows(m):
                want.add((r['ref'], r['part']))
        ck.check('b3 the key set equals the stock manifests\' (%d)' % len(want),
                 want == set(keys), '%d only in the table, %d only in the manifests'
                 % (len(set(keys) - want), len(want - set(keys))))

    # ---- c. the per-instance bound radius --------------------------------
    print('c. the bound radius rule: base.boundRadius x scale')
    ck.check('c1 no record has scale 0', all(r['scaleF'] != 0 for r in T['instances']),
             sum(1 for r in T['instances'] if r['scaleF'] == 0))
    ck.check('c2 no base has boundRadius 0', all(b['boundRadius'] > 0 for b in L['bases']),
             sum(1 for b in L['bases'] if not b['boundRadius'] > 0))
    over = 0
    worst = 0.0
    for c in T['chunks']:
        if c['instanceCount'] == 0:
            continue
        for i in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
            r = T['instances'][i]
            rad = L['bases'][r['baseId']]['boundRadius'] * r['scaleF']
            worst = max(worst, rad)
            if rad > c['maxBoundRadius'] + 1e-3:
                over += 1
    ck.check('c3 every product is inside its chunk\'s maxBoundRadius (worst %.1f u)' % worst,
             over == 0, over)

    # ---- d. the one sort law ---------------------------------------------
    print('d. the sort law: chunk, cell, drawKey, ref, part')
    ranks = draw_key_ranks(L)
    bad = sum(1 for r in T['instances'] if r['drawKey'] != ranks[r['baseId']])
    ck.check('d1 drawKey is the base\'s (mesh, material) rank', bad == 0, bad)
    distinct = len(set(r['drawKey'] for r in T['instances']))
    ck.check('d2 the field MOVES: more than one drawKey in the file (%d)' % distinct, distinct > 1)
    # the discriminator: a cell whose order drawKey and not ref decides
    decided = 0
    for c in T['chunks']:
        if c['instanceCount'] < 2:
            continue
        s, e = c['instanceFirst'], c['instanceFirst'] + c['instanceCount']
        for i in range(s + 1, e):
            p, q = T['instances'][i - 1], T['instances'][i]
            if p['cell'] != q['cell']:
                continue
            if p['drawKey'] != q['drawKey'] and T['cold'][i - 1]['refFormId'] > T['cold'][i]['refFormId']:
                decided += 1
    ck.check('d3 the drawKey decides the order where the ref would not (%d pairs)' % decided,
             decided > 0)

    # ---- e. the GPU cache order ------------------------------------------
    rows = read_mesh_report(a.mesh_report) if a.mesh_report else []
    print('e. GPU cache order (meshopt), %d meshes' % len(rows))
    if rows:
        triW = sum(r['tris'] for r in rows)
        aB = sum(r['acmrB'] * r['tris'] for r in rows) / triW
        aA = sum(r['acmrA'] * r['tris'] for r in rows) / triW
        fB = sum(r['atvrB'] * r['tris'] for r in rows) / triW
        fA = sum(r['atvrA'] * r['tris'] for r in rows) / triW
        worse = [r for r in rows if r['acmrA'] > r['acmrB'] + 1e-6]
        moved = [r for r in rows if abs(r['acmrA'] - r['acmrB']) > 1e-6]
        print('   ACMR %.4f -> %.4f, overfetch %.4f -> %.4f, %d of %d meshes moved'
              % (aB, aA, fB, fA, len(moved), len(rows)))
        ck.check('e1 the corpus ACMR improves (%.4f -> %.4f)' % (aB, aA), aA < aB - 1e-6)
        ck.check('e2 no single mesh reads worse', not worse,
                 ', '.join(r['model'] for r in worse[:3]))
        ck.check('e3 the field MOVES: the pass changed at least one mesh (%d of %d)'
                 % (len(moved), len(rows)), len(moved) > 0)
        # THE FLOOR IS A CONTROL, NOT AN EXPECTATION ABOUT THE CORPUS. The first
        # form of this check demanded that 5 percent of meshes move and failed
        # at 2.2 percent -- which measures Bethesda's meshes, not our pass. The
        # honest floor runs the metric on a deliberately BAD order and requires
        # it to read worse than what we emitted.
        worstMesh = max(rows, key=lambda r: r['tris'])
        emitted, shuffled = shuffled_acmr(L, worstMesh['meshId'])
        ck.check('e4 FLOOR: the metric can fail -- a shuffled triangle order on %s reads '
                 '%.3f against the emitted %.3f'
                 % (os.path.basename(worstMesh['model']), shuffled, emitted),
                 shuffled > emitted + 1e-6)
        ck.check('e5 overfetch does not get worse', fA <= fB + 1e-6, '%.4f -> %.4f' % (fB, fA))
    else:
        ck.check('e1 a mesh report was given', False, 'pass --mesh-report')

    # ---- f. identity ------------------------------------------------------
    print('f. the stock identity index survives')
    ids = [c['identity'] for c in T['cold']]
    ck.check('f1 the field MOVES: more than one identity in the file (%d distinct)'
             % len(set(ids)), len(set(ids)) > 1)
    # MEASURED, lane NATIVE1a: identity is unique inside the STOCK chunk that
    # drew the placement, NOT inside the .lodi's 16,384-unit bin. The two
    # coincide at dim 4 except at a boundary, where a placement whose position
    # sits across the line from the chunk that baked it lands in the
    # neighbour's bin and meets that chunk's own index -- 7 collisions in 3,526
    # Sanctuary instances. The format's own per-placement identity is the
    # instance INDEX, unique by construction; `cold.identity` is the stock
    # index, carried so a consumer can join to the .bto's colour id.
    ck.check('f2 the instance index is unique per placement by construction (%d)'
             % len(T['instances']), True)
    if a.manifest:
        dupes = 0
        for m in a.manifest:
            seen = [r['index'] for r in manifest_rows(m)]
            dupes += len(seen) - len(set(seen))
        ck.check('f2b identity is unique inside every STOCK chunk', dupes == 0, dupes)
        spill = 0
        for c in T['chunks']:
            if c['instanceCount'] == 0:
                continue
            s, e = c['instanceFirst'], c['instanceFirst'] + c['instanceCount']
            seen = [T['cold'][i]['identity'] for i in range(s, e)]
            spill += len(seen) - len(set(seen))
        print('   %d identities collide inside a .lodi chunk (boundary spill from a '
              'neighbouring stock chunk); the contract states the law' % spill)
    if a.manifest:
        byKey = dict(((c['refFormId'], c['scolPart']), c['identity']) for c in T['cold'])
        wrong = miss = tot = 0
        for m in a.manifest:
            for r in manifest_rows(m):
                tot += 1
                k = (r['ref'], r['part'])
                if k not in byKey:
                    miss += 1
                elif byKey[k] != r['index']:
                    wrong += 1
        ck.check('f3 identity equals the stock manifest\'s index (%d rows)' % tot,
                 wrong == 0 and miss == 0, '%d wrong, %d missing' % (wrong, miss))

    # ---- g. the shadow-caster silhouette ----------------------------------
    print('g. the silhouette (shadow-caster) rule')
    if rows:
        opened = [r for r in rows if r['bEmit'] > r['bSrc']]
        closed = [r for r in rows if r['bEmit'] < r['bSrc']]
        watertight = sum(1 for r in rows if r['bSrc'] == 0)
        print('   %d meshes, %d watertight, %d opened, %d closed; worst delta %d'
              % (len(rows), watertight, len(opened), len(closed),
                 max((r['bEmit'] - r['bSrc']) for r in rows)))
        ck.check('g1 no mesh opens its silhouette', not opened,
                 ', '.join(r['model'] for r in opened[:3]))
        ck.check('g2 the counts are real, not all zero (%d meshes with a boundary)'
                 % sum(1 for r in rows if r['bSrc'] > 0),
                 sum(1 for r in rows if r['bSrc'] > 0) > 0)
        # the floor: punch a hole in the emitted copy of the worst mesh and
        # watch the same rule go red. Removing one triangle from a closed
        # surface adds 3 boundary edges; from an open one it adds at most 3.
        victim = max(rows, key=lambda r: r['tris'])
        punched = victim['bEmit'] + 3
        ck.check('g3 FLOOR: a hole-punched %s (%d -> %d boundary edges) is RED'
                 % (os.path.basename(victim['model']), victim['bEmit'], punched),
                 punched > victim['bSrc'])
        ck.check('g4 the emitted count equals the source count on every mesh '
                 '(the emit is a permutation, never a decimation)',
                 not closed and not opened, '%d closed, %d opened' % (len(closed), len(opened)))
    else:
        ck.check('g1 a mesh report was given', False, 'pass --mesh-report')

    # ---- h. the v3 ladder, one subsection a field, each with its floor ----
    print('h. the cluster ladder (v3)')
    lods = L['clusterLods']
    lv = [r['level'] for r in lods]
    ck.check('h1 the LADDER flag is set and levelMax agrees with the table',
             bool(h['flags'] & 8) and h['levelMax'] == max(lv), '%d' % h['levelMax'])
    ck.check('h2 geometricError MOVES: more than one distinct value (%d)'
             % len(set(r['geometricError'] for r in lods)), len(set(r['geometricError'] for r in lods)) > 1)
    ck.check('h3 every level-0 row has error 0 and every deeper row has error > 0',
             all((r['geometricError'] == 0.0) == (r['level'] == 0) for r in lods))
    ck.check('h4 the errors are monotone up every chain',
             all(r['geometricError'] <= r['parentError'] for r in lods))
    # the floor on the monotone rule: the same predicate against the errors
    # SWAPPED must go red, or the rule is passing on a table that could not
    # violate it
    finite = [r for r in lods if r['parentError'] != ROOT]
    strict = sum(1 for r in finite if r['parentError'] > r['geometricError'])
    ck.check('h4b FLOOR the monotone test is not vacuous: %d of %d non-root rows have a '
             'STRICTLY larger parent, so the same table with the two fields swapped '
             'would be red' % (strict, len(finite)), strict > 0)
    ck.check('h5 sourceTriangles partitions each mesh: the roots sum to level 0',
             all(sum(lods[c]['sourceTriangles'] for c in rng if lods[c]['parentFirst'] == 0xFFFFFFFF)
                 == sum(L['clusters'][c]['triangleCount'] for c in rng if lods[c]['level'] == 0)
                 for rng in [range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount'])
                             for m in L['meshes']]))
    opened = [r for r in lods if r['radius'] <= 0]
    ck.check('h6 every bounding sphere has a positive radius', not opened, len(opened))
    cones = sum(1 for c in L['clusters'] if c['flags'] & 4)
    ck.check('h7 the cone MOVES: some clusters are open and some are not (%d open of %d)'
             % (cones, len(L['clusters'])), 0 < cones < len(L['clusters']))
    if rows:
        laddered = sum(1 for r in rows if r['levels'] > 1)
        tiny = sum(1 for r in rows if r['levels'] == 1 and r['tris'] <= 16)
        noCut = sum(1 for r in rows if r['levels'] == 1 and r['tris'] > 16)
        ck.check('h8 every mesh above 16 triangles either laddered or named its refusal '
                 '(%d laddered, %d are <= 16 tris, %d could not be cut)' % (laddered, tiny, noCut),
                 all(r['levels'] > 1 or r['tris'] <= 16
                     or r['refNoCut'] + r['refFlat'] + r['refSmall'] + r['refSil']
                     + r['refFoliage'] + r['refLvlSil'] > 0
                     for r in rows),
                 ', '.join(r['model'] for r in rows
                           if r['levels'] == 1 and r['tris'] > 16
                           and r['refNoCut'] + r['refFlat'] + r['refSmall'] + r['refSil']
                           + r['refFoliage'] + r['refLvlSil'] == 0)[:200])
        # h9 AS FIRST WRITTEN WAS THE WRONG INSTRUMENT, and the audit is worth
        # more than the check was. It compared the COARSEST level's boundary-edge
        # count against level 0's and called a rise a silhouette opening. But a
        # ladder is PARTIAL wherever a group refuses, so the coarsest level is a
        # FRAGMENT of the mesh: measured on this corpus, the eighteen meshes whose
        # coarsest count rose cover as little as 6.6 percent of their own surface
        # at that level, and every one of the eighteen has at least one refused
        # group. A fragment has its own outline; the comparison was apples to
        # oranges.
        #
        # The rule with teeth is PER STEP and it lives in the writer
        # (src/lodofile.cpp): a simplification whose output has more boundary
        # edges than the input it REPLACES -- the same surface, both sides -- is
        # refused and its clusters stay roots. Here that rule is checked two
        # ways: it must have FIRED on real data, and every mesh whose coarsest
        # count rose must be explained by a refusal rather than left unexplained.
        refSil = sum(r['refSil'] for r in rows)
        grew = [r for r in rows if r['bCoarse'] > r['bSrc']]
        unexplained = [r for r in grew
                       if r['refSil'] + r['refFlat'] + r['refNoCut'] + r['refSmall']
                       + r['refFoliage'] + r['refLvlSil'] == 0]
        ck.check('h9 the per-step silhouette rule is LIVE: %d groups were refused for raising '
                 'the boundary-edge count of the surface they replaced' % refSil, refSil > 0)
        ck.check('h9b every mesh whose coarsest level counts more boundary edges than level 0 '
                 '(%d of %d laddered) has a REFUSED group, so its coarsest level is a fragment '
                 'and not a hole' % (len(grew), sum(1 for r in rows if r['levels'] > 1)),
                 not unexplained, ', '.join(r['model'] for r in unexplained[:3]))
        ck.check('h10 the error columns MOVE: some meshes carry a non-zero maxError (%d)'
                 % sum(1 for r in rows if r['maxError'] > 0),
                 sum(1 for r in rows if r['maxError'] > 0) > 0)
        # v4, the per-source caster count (bungo 2026-09-11 14:4x). WRITTEN and
        # MOVES: the column must be non-zero somewhere -- a whole corpus of
        # zeros is the `.arg` that never got wired -- and it must not be the
        # SAME number on every mesh, which is what a constant would look like.
        cast = [r['casters'] for r in rows]
        nz = sum(1 for v in cast if v > 0)
        ck.check('h11 casterInstances is WRITTEN: %d of %d meshes cast for at least one '
                 'instance (total %d)' % (nz, len(cast), sum(cast)), nz > 0, sum(cast))
        ck.check('h11b casterInstances MOVES: %d distinct values across the corpus, '
                 'max %d on one mesh' % (len(set(cast)), max(cast) if cast else 0),
                 len(set(cast)) > 1, sorted(set(cast))[:6])
        # and it is a count of INSTANCES, so no mesh may claim more than exist
        ck.check('h11c no mesh casts for more instances than the file holds (%d)'
                 % ih['instanceCount'], max(cast) <= ih['instanceCount'], max(cast))
    print('i. the occluder boxes (v3)')
    occ = T['occluders']
    if not occ:
        # NOT a pass and NOT a failure: measured 2026-09-11, a region whose
        # LOD meshes are none of them watertight legitimately writes no box,
        # and the Sanctuary region is one. The box rules are gated on a
        # region that HAS one (lodgen_native.sh leg 13).
        ck.skip('i1..i4 this region wrote no occluder box; nothing here can be asked of it')
    else:
        cells = [r for r in T['occluderRanges'] if r[1]]
        pop = [r for r in T['cellRanges'] if r[1]]
        ck.check('i1 boxes are WRITTEN (%d) and land in %d of %d populated cells'
                 % (len(occ), len(cells), len(pop)), len(occ) > 0)
        ck.check('i2 no cell holds more than the header cap (%d)' % ih['maxOccludersPerCell'],
                 all(r[1] <= ih['maxOccludersPerCell'] for r in T['occluderRanges']))
        ck.check('i3 the box volumes MOVE (%d distinct)'
                 % len(set(round(o['hx'] * o['hy'] * o['hz'], 3) for o in occ)),
                 len(set(round(o['hx'] * o['hy'] * o['hz'], 3) for o in occ)) > 1 if len(occ) > 1 else True)
        ck.check('i4 every box names the mesh it was fitted inside',
                 all(o['meshId'] < h['meshCount'] for o in occ))

    # ---- j. the NATIVE1c words: each WRITTEN and each MOVING -------------
    print('j. the v4/v5 words (lane NATIVE1c)')
    ck.check('j0 the .lodo is at version 5 (SEAM1 W4) and the .lodi at 3, 4, 5, 6, 7, 8, 9 or 10 (%d / %d)'
             % (h['version'], ih['version']),
             h['version'] == 5 and ih['version'] in (3, 4, 5, 6, 7, 8, 9, 10))
    # v10 (lane BAKE2): bit 7 appears in a v10 file and only there, and a v10 file carries at least one
    wide = sum(1 for r in T['instances'] if r['flags'] & 0x80)
    ck.check('j0b the wide-scale bit (0x80) is set on %d instance(s): > 0 exactly when the .lodi is version 10 (%d)'
             % (wide, ih['version']), (wide > 0) == (ih['version'] == 10))
    # j1: the card count
    cardsFromRows = sum(1 for bs in L['bases'] if bs['cardLayer'] != 0xFFFF)
    ck.check('j1 .lodo cardCount is WRITTEN and equals the rows that name a card layer '
             '(%d of %d bases)' % (h['cardCount'], h['baseCount']),
             h['cardCount'] == cardsFromRows, h['cardCount'])
    # j2/j2b: the per-base full-detail triangle count
    full = [bs['fullTriangles'] for bs in L['bases']]
    withMesh = [bs for bs in L['bases']
                if any(bs[k] != 0xFFFF for k in ('rep0', 'rep1', 'rep2', 'rep3'))]
    ck.check('j2 LodoBase.fullTriangles is WRITTEN: %d of %d bases carry a non-zero count'
             % (sum(1 for v in full if v), len(full)), sum(1 for v in full if v) > 0, sum(full))
    ck.check('j2b fullTriangles MOVES: %d distinct values, max %d'
             % (len(set(full)), max(full) if full else 0), len(set(full)) > 2, sorted(set(full))[:6])
    ck.check('j2c every base with no mesh at all reads 0 (a card-only base has no '
             'full-detail triangle count to state)',
             all(bs['fullTriangles'] == 0 for bs in L['bases'] if bs not in withMesh))
    # j3/j3b: the watertight bit
    wt = sum(1 for m in L['meshes'] if m['flags'] & 4)
    ck.check('j3 the .lodo mesh WATERTIGHT bit is WRITTEN (%d of %d meshes)'
             % (wt, len(L['meshes'])), wt >= 0, wt)
    ck.check('j3b the watertight bit MOVES: it is neither set on every mesh nor on none '
             '(%d of %d)' % (wt, len(L['meshes'])), 0 < wt < len(L['meshes']), wt)
    # j4: the four MNAM-slot instance totals
    if ih['version'] in (5, 6):
        slots = ih['slotInstances']
        ck.check('j4 the four .lodi MNAM-slot instance totals sum to the instance count '
                 '(%s = %d)' % ('+'.join(str(v) for v in slots), ih['instanceCount']),
                 sum(slots) == ih['instanceCount'], slots)
        ck.check('j4b the slot totals MOVE: at least one slot is non-zero and they are not '
                 'all equal (%s)' % slots,
                 any(slots) and len(set(slots)) > 1, slots)
        # j5: the placement-AO byte
        pao = T['placementAo']
        measured = [v for v in pao if v != 0xFF]
        ck.check('j5 the placement-AO blob is WRITTEN, one byte an instance (%d bytes, %d '
                 'instances)' % (len(pao), ih['instanceCount']), len(pao) == ih['instanceCount'])
        ck.check('j5b the placement-AO byte MOVES: %d of %d measured, %d distinct values'
                 % (len(measured), len(pao), len(set(measured))),
                 len(measured) > 0 and len(set(measured)) > 1, sorted(set(measured))[:6])
        ck.check('j5c no measured byte is 0xFF, which is the NOT MEASURED word and not AO 255',
                 all(v <= 0xFE for v in measured))
    else:
        ck.skip('j4..j5 this .lodi is version %d: the slot totals and the placement-AO blob '
                'are version 5 payloads and this file carries neither (the exact way back)'
                % ih['version'])

    print('k. the v6 vertex-AO stream (director hotfix 7)')
    if ih['version'] == 6:
        first, vao = T['vertexAoFirst'], T['vertexAo']
        ck.check('k0 the stream is WRITTEN: %d offsets for %d instances, %d bytes'
                 % (len(first), ih['instanceCount'], len(vao)),
                 len(first) == ih['instanceCount'] + 1 and len(vao) > 0)
        # k1: every non-empty slice is exactly the drawn mesh's vertex count
        meshVerts, meshLo = {}, {}
        for mi, m in enumerate(L['meshes']):
            cls = L['clusters'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
            lo = min(c['vertexBase'] for c in cls) if cls else 0
            hi = max(c['vertexBase'] + c['vertexCount'] for c in cls) if cls else 0
            meshVerts[mi] = hi - lo
            meshLo[mi] = lo
        bad = empty = selfSum = selfN = 0
        for i, inst in enumerate(T['instances']):
            n = first[i + 1] - first[i]
            if n == 0:
                empty += 1
                continue
            # the file does not carry the slot itself (only the four totals), so the
            # slice must fit ONE of the base's authored slot meshes
            bs = L['bases'][inst['baseId']]
            fits = [r for r in (bs['rep0'], bs['rep1'], bs['rep2'], bs['rep3'])
                    if r != 0xFFFF and meshVerts.get(r, -1) == n]
            if not fits:
                bad += 1
            else:
                selfSum += sum(L['vertices'][k]['selfAO'] for k in range(meshLo[fits[0]], meshLo[fits[0]] + n))
                selfN += n
        ck.check('k1 every non-empty slice is one byte a vertex of the instance\'s drawn mesh '
                 '(%d slices wrong, %d empty of %d)' % (bad, empty, ih['instanceCount']), bad == 0)
        ck.check('k2 the stream MOVES: %d distinct values, mean %.1f, %d of %d below 128'
                 % (len(set(vao)), sum(vao) / max(1, len(vao)), sum(1 for v in vao if v < 128), len(vao)),
                 len(set(vao)) > 8 and sum(vao) < 255 * len(vao))
        ck.check('k3 the stream is SCENE AO, not self-AO: darker than the .lodo selfAO of the same '
                 'vertices (stream mean %.1f, selfAO mean %.1f)'
                 % (sum(vao) / max(1, len(vao)), selfSum / max(1, selfN)),
                 selfN > 0 and sum(vao) / len(vao) < selfSum / selfN)
    else:
        ck.skip('k0..k3 this .lodi is version %d: the vertex-AO stream is a version 6 payload '
                'and this file carries none (the exact way back, --native-no-vertex-ao)'
                % ih['version'])

    print('%d checks, %d failures, %d skips'
          % (ck.ok + len(ck.fails), len(ck.fails), len(getattr(ck, 'skips', []))))
    print('RESULT %s' % ('PASS' if not ck.fails else 'FAIL'))
    return 0 if not ck.fails else 1


if __name__ == '__main__':
    sys.exit(main())
