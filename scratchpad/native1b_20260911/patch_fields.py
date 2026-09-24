"""v3 for tests/spells/lodgen_native_fields.py: the mesh report grew thirteen
columns before `model`, `mesh_indices` must read LEVEL 0 only (or every ACMR
number would be measuring the ladder), and section h. adds one subsection a
NEW field with its floor."""
import sys

SRC = "tests/spells/lodgen_native_fields.py"
EDITS = []


def edit(old, new):
    EDITS.append((old, new))


edit(
    """            t = line.split(None, 10)
            rows.append({'meshId': int(t[0]), 'tris': int(t[1]), 'srcVerts': int(t[2]),
                         'emitVerts': int(t[3]), 'acmrB': float(t[4]), 'acmrA': float(t[5]),
                         'atvrB': float(t[6]), 'atvrA': float(t[7]),
                         'bSrc': int(t[8]), 'bEmit': int(t[9]), 'model': t[10].strip()})""",
    """            # report VERSION 2 (v3 format): thirteen ladder columns before `model`,
            # which stays the line's remainder because it is the only token that
            # may hold a space
            t = line.split(None, 23)
            rows.append({'meshId': int(t[0]), 'tris': int(t[1]), 'srcVerts': int(t[2]),
                         'emitVerts': int(t[3]), 'acmrB': float(t[4]), 'acmrA': float(t[5]),
                         'atvrB': float(t[6]), 'atvrA': float(t[7]),
                         'bSrc': int(t[8]), 'bEmit': int(t[9]),
                         'levels': int(t[10]), 'clL0': int(t[11]), 'clLadder': int(t[12]),
                         'maxError': float(t[13]), 'groups': int(t[14]), 'refSmall': int(t[15]),
                         'refNoCut': int(t[16]), 'refFlat': int(t[17]), 'errExact': int(t[18]),
                         'errBounded': int(t[19]), 'welded': int(t[20]), 'uvConf': int(t[21]),
                         'bCoarse': int(t[22]), 'model': t[23].strip()})""")

edit(
    """    m = L['meshes'][meshId]
    out = []
    for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        cl = L['clusters'][c]""",
    """    m = L['meshes'][meshId]
    out = []
    for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        # v3: LEVEL 0 only -- the cache-order numbers describe the full-detail
        # geometry, and folding the ladder's copies in would measure the ladder
        if L['clusterLods'][c]['level'] != 0:
            continue
        cl = L['clusters'][c]""")

edit(
    """    print('%d checks, %d failures' % (ck.ok + len(ck.fails), len(ck.fails)))
    print('RESULT %s' % ('PASS' if not ck.fails else 'FAIL'))
    return 0 if not ck.fails else 1""",
    """    # ---- h. the v3 ladder, one subsection a field, each with its floor ----
    print('h. the cluster ladder (v3)')
    lods = L['clusterLods']
    lv = [r['level'] for r in lods]
    ck.check('h1 the LADDER flag is set and levelMax agrees with the table',
             bool(h['flags'] & 8) and h['levelMax'] == max(lv), '%d' % h['levelMax'])
    ck.check('h2 geometricError MOVES: more than one distinct value (%d)'
             % len(set(r['err'] for r in lods)), len(set(r['err'] for r in lods)) > 1)
    ck.check('h3 every level-0 row has error 0 and every deeper row has error > 0',
             all((r['err'] == 0.0) == (r['level'] == 0) for r in lods))
    ck.check('h4 the errors are monotone up every chain',
             all(r['err'] <= r['perr'] for r in lods))
    # the floor on the monotone rule: the same predicate against the errors
    # SWAPPED must go red, or the rule is passing on a table that could not
    # violate it
    finite = [r for r in lods if r['perr'] != ROOT]
    strict = sum(1 for r in finite if r['perr'] > r['err'])
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
                 all(r['levels'] > 1 or r['tris'] <= 16 or r['refNoCut'] + r['refFlat'] + r['refSmall'] > 0
                     for r in rows),
                 ', '.join(r['model'] for r in rows
                           if r['levels'] == 1 and r['tris'] > 16
                           and r['refNoCut'] + r['refFlat'] + r['refSmall'] == 0)[:200])
        grew = [r for r in rows if r['bCoarse'] > r['bSrc']]
        ck.check('h9 no coarse level opened a silhouette (the far-shadow rule)',
                 not grew, ', '.join(r['model'] for r in grew[:3]))
        ck.check('h10 the error columns MOVE: some meshes carry a non-zero maxError (%d)'
                 % sum(1 for r in rows if r['maxError'] > 0),
                 sum(1 for r in rows if r['maxError'] > 0) > 0)
    print('i. the occluder boxes (v3)')
    occ = T['occluders']
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

    print('%d checks, %d failures' % (ck.ok + len(ck.fails), len(ck.fails)))
    print('RESULT %s' % ('PASS' if not ck.fails else 'FAIL'))
    return 0 if not ck.fails else 1""")

edit(
    """from lodgen_native_decode import read_lodo, read_lodi, draw_key_ranks, Refusal, fnv1a64  # noqa: E402""",
    """from lodgen_native_decode import read_lodo, read_lodi, draw_key_ranks, Refusal, fnv1a64  # noqa: E402
from lodgen_native_decode import ROOT_ERROR as ROOT  # noqa: E402""")


def main():
    apply = "--apply" in sys.argv
    raw = open(SRC, "rb").read()
    cr = raw.count(b"\r")
    text = raw.decode("utf-8")
    ok = True
    for old, new in EDITS:
        n = text.count(old)
        print("x%d  %s" % (n, old.strip().split("\n")[0][:74]))
        if n != 1:
            ok = False
    if not ok:
        print("REFUSED: every anchor must match exactly once")
        return 1
    for old, new in EDITS:
        text = text.replace(old, new, 1)
    data = text.encode("utf-8")
    print("%d -> %d bytes, CR %d -> %d" % (len(raw), len(data), cr, data.count(b"\r")))
    if data.count(b"\r") != cr:
        print("REFUSED: CR count moved")
        return 1
    if not apply:
        print("--check only: nothing written")
        return 0
    open(SRC, "wb").write(data)
    print("applied")
    return 0


sys.exit(main())
