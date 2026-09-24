"""Bring the INDEPENDENT decoder (tests/spells/lodgen_native_decode.py) to v3.

Exact-once replacements, --check by default, CR count asserted (the file is
LF-only). The decoder shares no code with the C++ writers and this patch does
not change that: every new rule below is written from the contract's byte
tables, not from src/lodofile.cpp.
"""
import sys

SRC = "tests/spells/lodgen_native_decode.py"

EDITS = []


def edit(old, new):
    EDITS.append((old, new))


# ---- .lodo: version, flags, the ladder table ----
edit(
    """    if h['version'] == 1:
        raise Refusal('version 1: the v1 vertex blob is in source order and carries no loadOrderHash')
    if h['version'] != 2:
        raise Refusal('version %d' % h['version'])""",
    """    if h['version'] == 1:
        raise Refusal('version 1: the v1 vertex blob is in source order and carries no loadOrderHash')
    if h['version'] == 2:
        raise Refusal('version 2: a v2 library has no cluster ladder table (header 0xC0 was reserved), '
                      'so every cluster would read error 0 and be drawn at full detail at every distance')
    if h['version'] != 3:
        raise Refusal('version %d' % h['version'])""")

edit(
    """    if h['flags'] & ~7:
        raise Refusal('reserved flag bits set')
    (h['pluginCorpusHash'], h['objectCorpusHash'], h['modelCorpusHash'],""",
    """    if h['flags'] & ~15:
        raise Refusal('reserved flag bits set')
    (h['pluginCorpusHash'], h['objectCorpusHash'], h['modelCorpusHash'],""")

edit(
    """    h['loadOrderHash'] = le('Q', b, 0xB8)[0]
    if any(b[0xC0:0x100]):
        raise Refusal('reserved bytes 0xC0..0xFF not zero')""",
    """    h['loadOrderHash'] = le('Q', b, 0xB8)[0]
    # v3: the ladder's room, 0xC0..0xCD; the pad now starts at 0xCE
    h['offClusterLods'] = le('Q', b, 0xC0)[0]
    h['clusterLodStride'] = le('I', b, 0xC8)[0]
    h['levelMax'] = b[0xCC]
    h['ladderGroup'] = b[0xCD]
    if h['clusterLodStride'] != 48:
        raise Refusal('clusterLodStride %d; the v3 row is 48 bytes' % h['clusterLodStride'])
    if h['levelMax'] > 15:
        raise Refusal('levelMax %d past the format cap' % h['levelMax'])
    if bool(h['flags'] & 8) != bool(h['ladderGroup']):
        raise Refusal('the LADDER flag and ladderGroup disagree (%d / %d)' % (h['flags'] & 8, h['ladderGroup']))
    if not h['flags'] & 8 and h['levelMax']:
        raise Refusal('the LADDER flag is clear but levelMax is %d' % h['levelMax'])
    if any(b[0xCE:0x100]):
        raise Refusal('reserved bytes 0xCE..0xFF not zero')""")

edit(
    """            ('clusters', h['offClusters'], h['clusterCount'] * 16),
            ('materials', h['offMaterials'], h['materialCount'] * 16),""",
    """            ('clusters', h['offClusters'], h['clusterCount'] * 16),
            ('clusterLods', h['offClusterLods'], h['clusterCount'] * 48),
            ('materials', h['offMaterials'], h['materialCount'] * 16),""")

edit(
    """    L['meshes'] = [dict(zip(('aabbMin', 'aabbExtent', 'uvMin', 'uvExtent', 'clusterFirst', 'clusterCount',
                             'flags', 'modelStringOffset', 'reserved'),
                            (lambda t: (t[0:3], t[3:6], t[6:8], t[8:10]) + t[10:])(
                                le('ffffffffffIHHII', b, h['offMeshes'] + i * 56)))) for i in range(h['meshCount'])]""",
    """    # v3: the mesh row's v2 reserved word became clusterCountL0 + levelCount + a byte
    L['meshes'] = [dict(zip(('aabbMin', 'aabbExtent', 'uvMin', 'uvExtent', 'clusterFirst', 'clusterCount',
                             'flags', 'modelStringOffset', 'clusterCountL0', 'levelCount', 'reserved'),
                            (lambda t: (t[0:3], t[3:6], t[6:8], t[8:10]) + t[10:])(
                                le('ffffffffffIHHIHBB', b, h['offMeshes'] + i * 56)))) for i in range(h['meshCount'])]""")

edit(
    """    L['materials'] = [dict(zip(('arrayClass',""",
    """    L['clusterLods'] = [dict(zip(('cx', 'cy', 'cz', 'radius', 'geometricError', 'parentError',
                                  'parentFirst', 'parentCount', 'level', 'reserved0',
                                  'coneAxis0', 'coneAxis1', 'coneCos', 'sourceTriangles', 'reserved1'),
                                 le('ffffffIHBBHHfII', b, h['offClusterLods'] + i * 48)))
                        for i in range(h['clusterCount'])]
    L['materials'] = [dict(zip(('arrayClass',""")

# mesh row rules: the two summary words are checked against the rows
edit(
    """        if m['clusterFirst'] + m['clusterCount'] > h['clusterCount']:
            raise Refusal('mesh %d cluster range past table' % i)""",
    """        if m['clusterFirst'] + m['clusterCount'] > h['clusterCount']:
            raise Refusal('mesh %d cluster range past table' % i)
        if m['levelCount'] == 0:
            raise Refusal('mesh %d levelCount is 0' % i)
        rows = L['clusterLods'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
        nL0 = sum(1 for r in rows if r['level'] == 0)
        lvMax = max([r['level'] for r in rows], default=0)
        if nL0 != m['clusterCountL0']:
            raise Refusal('mesh %d clusterCountL0 %d but %d rows are level 0' % (i, m['clusterCountL0'], nL0))
        if lvMax + 1 != m['levelCount']:
            raise Refusal('mesh %d levelCount %d but its deepest level is %d' % (i, m['levelCount'], lvMax))""")

edit(
    """        if c['flags'] & ~3:
            raise Refusal('cluster %d reserved flags' % i)""",
    """        if c['flags'] & ~7:
            raise Refusal('cluster %d reserved flags' % i)""")

edit(
    """        if i and (L['clusters'][i - 1]['meshId'], L['clusters'][i - 1]['materialId']) > (c['meshId'], c['materialId']):
            raise Refusal('cluster table not sorted at %d' % i)""",
    """        cl = L['clusterLods'][i]
        if i and (L['clusters'][i - 1]['meshId'], L['clusters'][i - 1]['materialId'],
                  L['clusterLods'][i - 1]['level']) > (c['meshId'], c['materialId'], cl['level']):
            raise Refusal('cluster table not sorted by (meshId, materialId, level) at %d' % i)
        # ---- v3: the ladder row parallel to this cluster ----
        if cl['reserved0'] or cl['reserved1']:
            raise Refusal('cluster %d ladder row reserved field not zero' % i)
        if not cl['radius'] > 0:
            raise Refusal('cluster %d sphere radius %r' % (i, cl['radius']))
        if cl['level'] > h['levelMax']:
            raise Refusal('cluster %d level %d past levelMax' % (i, cl['level']))
        if cl['level'] == 0 and cl['geometricError'] != 0.0:
            raise Refusal('cluster %d is level 0 with error %r' % (i, cl['geometricError']))
        if cl['level'] == 0 and cl['sourceTriangles'] != c['triangleCount']:
            raise Refusal('cluster %d is level 0 but sourceTriangles %d != triangleCount %d'
                          % (i, cl['sourceTriangles'], c['triangleCount']))
        if cl['level'] > 0 and not cl['geometricError'] > 0:
            raise Refusal('cluster %d is level %d with error %r' % (i, cl['level'], cl['geometricError']))
        isroot = cl['parentFirst'] == 0xFFFFFFFF
        if isroot != (cl['parentCount'] == 0) or isroot != (cl['parentError'] == ROOT_ERROR):
            raise Refusal('cluster %d disagrees with itself about being a root' % i)
        if not cl['geometricError'] <= cl['parentError']:
            raise Refusal('cluster %d: geometricError %r > parentError %r -- the ladder is not monotone'
                          % (i, cl['geometricError'], cl['parentError']))
        if not isroot:
            if cl['parentFirst'] + cl['parentCount'] > h['clusterCount']:
                raise Refusal('cluster %d parent range past the table' % i)
            pc = L['clusters'][cl['parentFirst']]
            pl = L['clusterLods'][cl['parentFirst']]
            if pc['meshId'] != c['meshId'] or pc['materialId'] != c['materialId']:
                raise Refusal('cluster %d parent crosses a mesh or a material' % i)
            if pl['level'] != cl['level'] + 1:
                raise Refusal('cluster %d parent is at level %d, not %d' % (i, pl['level'], cl['level'] + 1))
            if pl['geometricError'] != cl['parentError']:
                raise Refusal('cluster %d parentError does not equal its parent error' % i)
        if c['flags'] & 4:
            if cl['coneAxis0'] or cl['coneAxis1'] or cl['coneCos'] != -1.0:
                raise Refusal('cluster %d is CONE_OPEN but carries a cone' % i)
        elif not 0.0 < cl['coneCos'] <= 1.0:
            raise Refusal('cluster %d cone cosine %r outside (0, 1]' % (i, cl['coneCos']))""")

edit(
    """    if L['meshes'] and max(m['clusterCount'] for m in L['meshes']) != h['maxClustersPerMesh']:
        raise Refusal('maxClustersPerMesh wrong')""",
    """    if L['meshes'] and max(m['clusterCount'] for m in L['meshes']) != h['maxClustersPerMesh']:
        raise Refusal('maxClustersPerMesh wrong')
    if L['clusterLods'] and max(r['level'] for r in L['clusterLods']) != h['levelMax']:
        raise Refusal('levelMax %d but the ladder table reaches %d'
                      % (h['levelMax'], max(r['level'] for r in L['clusterLods'])))""")

# ---- .lodi: version, the occluder tables ----
edit(
    """    if h['version'] != 2:
        raise Refusal('version %d' % h['version'])
    if crc32(b[0x10:0x100]) != h['headerCrc32']:
        raise Refusal('headerCrc32 mismatch')
    if not h['flags'] & 1:
        raise Refusal('ROW_ORDER_NORTH_UP clear')""",
    """    if h['version'] == 2:
        raise Refusal('version 2: a v2 instance table has no occluder tables (header 0x98 and 0xA0 were '
                      'reserved), so every cell would read as occluding nothing')
    if h['version'] != 3:
        raise Refusal('version %d' % h['version'])
    if crc32(b[0x10:0x100]) != h['headerCrc32']:
        raise Refusal('headerCrc32 mismatch')
    if not h['flags'] & 1:
        raise Refusal('ROW_ORDER_NORTH_UP clear')""")

edit(
    """    h['loadOrderHash'] = le('Q', b, 0x90)[0]
    if any(b[0x98:0x100]):
        raise Refusal('reserved bytes 0x98..0xFF not zero')""",
    """    h['loadOrderHash'] = le('Q', b, 0x90)[0]
    # v3: the occluders' room, 0x98..0xAF; the pad now starts at 0xB0
    h['offOccluders'], h['offOccluderRanges'] = le('QQ', b, 0x98)
    h['occluderCount'] = le('I', b, 0xA8)[0]
    h['occluderStride'], h['maxOccludersPerCell'] = le('HH', b, 0xAC)
    if h['occluderStride'] != 40:
        raise Refusal('occluderStride %d; the v3 row is 40 bytes' % h['occluderStride'])
    if h['maxOccludersPerCell'] == 0:
        raise Refusal('maxOccludersPerCell is 0')
    if any(b[0xB0:0x100]):
        raise Refusal('reserved bytes 0xB0..0xFF not zero')""")

edit(
    """            ('instances', h['offInstances'], h['instanceCount'] * 24),
            ('cold', h['offCold'], h['instanceCount'] * 8)]""",
    """            ('instances', h['offInstances'], h['instanceCount'] * 24),
            ('cold', h['offCold'], h['instanceCount'] * 8),
            ('occluders', h['offOccluders'], h['occluderCount'] * 40),
            ('occluderRanges', h['offOccluderRanges'], h['presentChunks'] * 16 * 8)]""")

edit(
    """    if crc32(b[h['offChunks']:h['offChunks'] + tabs[0][2]] + b[h['offCellRanges']:h['offCellRanges'] + tabs[1][2]]) != h['indexCrc32']:
        raise Refusal('indexCrc32 mismatch')""",
    """    # v3: indexCrc32 covers the chunk table, the cell ranges, the occluder table
    # and the occluder ranges, in that order
    if crc32(b[h['offChunks']:h['offChunks'] + tabs[0][2]]
             + b[h['offCellRanges']:h['offCellRanges'] + tabs[1][2]]
             + b[h['offOccluders']:h['offOccluders'] + tabs[4][2]]
             + b[h['offOccluderRanges']:h['offOccluderRanges'] + tabs[5][2]]) != h['indexCrc32']:
        raise Refusal('indexCrc32 mismatch')""")

edit(
    """    T['cold'] = [dict(zip(('refFormId', 'scolPart', 'identity'), le('IhH', b, h['offCold'] + i * 8)))
                 for i in range(h['instanceCount'])]""",
    """    T['cold'] = [dict(zip(('refFormId', 'scolPart', 'identity'), le('IhH', b, h['offCold'] + i * 8)))
                 for i in range(h['instanceCount'])]
    T['occluders'] = [dict(zip(('x', 'y', 'z', 'hx', 'hy', 'hz', 'r0', 'r1', 'r2', 'flags',
                                'instanceIndex', 'reserved'),
                               le('ffffffHHHHII', b, h['offOccluders'] + i * 40)))
                      for i in range(h['occluderCount'])]
    T['occluderRanges'] = [le('II', b, h['offOccluderRanges'] + i * 8) for i in range(h['presentChunks'] * 16)]
    occCursor = 0""")

edit(
    """        if total != c['instanceCount']:
            raise Refusal('chunk %d cell ranges do not partition it' % ci)""",
    """        if total != c['instanceCount']:
            raise Refusal('chunk %d cell ranges do not partition it' % ci)
        # v3: the occluder ranges walk the box table in the same cell order, and
        # every box names an instance OF ITS OWN CELL
        for k in range(16):
            of, ocnt = T['occluderRanges'][c['cellRangeOffset'] + k]
            cf, ccnt = T['cellRanges'][c['cellRangeOffset'] + k]
            if ocnt == 0:
                if of:
                    raise Refusal('chunk %d cell %d: empty occluder range not all-zero' % (ci, k))
                continue
            if ocnt > h['maxOccludersPerCell']:
                raise Refusal('chunk %d cell %d: %d occluders past the cap %d'
                              % (ci, k, ocnt, h['maxOccludersPerCell']))
            if of != occCursor:
                raise Refusal('chunk %d cell %d: occluders start at %d, expected %d' % (ci, k, of, occCursor))
            for bi in range(of, of + ocnt):
                o = T['occluders'][bi]
                if o['reserved']:
                    raise Refusal('occluder %d reserved not zero' % bi)
                if o['flags'] & ~1 or not o['flags'] & 1:
                    raise Refusal('occluder %d flags %d' % (bi, o['flags']))
                if min(o['hx'], o['hy'], o['hz']) <= 0:
                    raise Refusal('occluder %d has a non-positive half extent' % bi)
                if o['instanceIndex'] >= h['instanceCount']:
                    raise Refusal('occluder %d instanceIndex past the table' % bi)
                if not cf <= o['instanceIndex'] < cf + ccnt:
                    raise Refusal('occluder %d is listed in chunk %d cell %d but names instance %d, which is '
                                  'not one of that cell instances' % (bi, ci, k, o['instanceIndex']))
            occCursor += ocnt""")

edit(
    """    if present != h['presentChunks'] or covered != h['instanceCount'] or maxInst != h['maxInstancesPerChunk']:
        raise Refusal('presentChunks / instanceCount / maxInstancesPerChunk disagree with the table')
    return T""",
    """    if present != h['presentChunks'] or covered != h['instanceCount'] or maxInst != h['maxInstancesPerChunk']:
        raise Refusal('presentChunks / instanceCount / maxInstancesPerChunk disagree with the table')
    if occCursor != h['occluderCount']:
        raise Refusal('the cell ranges cover %d occluders but the header says %d'
                      % (occCursor, h['occluderCount']))
    return T""")

# ---- the known-answer keys ----
edit(
    """    ck.check('lodo.clusterCount', h['clusterCount'] == eval_int(exp['lodo.clusterCount']), h['clusterCount'])""",
    """    l0 = [i for i, r in enumerate(L['clusterLods']) if r['level'] == 0]
    ck.check('lodo.level0.clusterCount', len(l0) == eval_int(exp['lodo.level0.clusterCount']), len(l0))""")

edit(
    """    ck.check('lodo.vertexCount', h['vertexCount'] == eval_int(exp['lodo.vertexCount']), h['vertexCount'])
    tris = sum(c['triangleCount'] for c in L['clusters'])
    ck.check('lodo.triangles', tris == eval_int(exp['lodo.triangles']), tris)""",
    """    l0v = sum(L['clusters'][i]['vertexCount'] for i in l0)
    ck.check('lodo.level0.vertices', l0v == eval_int(exp['lodo.level0.vertices']), l0v)
    l0t = sum(L['clusters'][i]['triangleCount'] for i in l0)
    ck.check('lodo.level0.triangles', l0t == eval_int(exp['lodo.level0.triangles']), l0t)
    ck.check('lodo.levelMaxAtLeast', h['levelMax'] >= int(exp['lodo.levelMaxAtLeast']), h['levelMax'])
    roots = sum(r['sourceTriangles'] for r in L['clusterLods'] if r['parentFirst'] == 0xFFFFFFFF)
    ck.check('lodo.rootSourceTriangles', roots == eval_int(exp['lodo.rootSourceTriangles']), roots)""")

edit(
    """    ck.check('lodo.cluster1.triangleCount', L['clusters'][1]['triangleCount'] == int(exp['lodo.cluster1.triangleCount']))
    ck.check('lodo.cluster2.triangleCount', L['clusters'][2]['triangleCount'] == int(exp['lodo.cluster2.triangleCount']))
    ck.check('lodo.cluster2.sizeClass', L['clusters'][2]['flags'] & 3 == int(exp['lodo.cluster2.sizeClass']))""",
    """    def l0_of(mesh):
        m = L['meshes'][mesh]
        return [i for i in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount'])
                if L['clusterLods'][i]['level'] == 0]
    m1 = l0_of(1)
    ck.check('lodo.mesh1.l0cluster0.triangleCount',
             L['clusters'][m1[0]]['triangleCount'] == int(exp['lodo.mesh1.l0cluster0.triangleCount']),
             L['clusters'][m1[0]]['triangleCount'])
    ck.check('lodo.mesh1.l0cluster1.triangleCount',
             L['clusters'][m1[1]]['triangleCount'] == int(exp['lodo.mesh1.l0cluster1.triangleCount']),
             L['clusters'][m1[1]]['triangleCount'])
    ck.check('lodo.mesh1.l0cluster1.sizeClass',
             L['clusters'][m1[1]]['flags'] & 3 == int(exp['lodo.mesh1.l0cluster1.sizeClass']))
    m0 = l0_of(0)
    ck.check('lodo.mesh0.l0cluster0.coneOpen',
             bool(L['clusters'][m0[0]]['flags'] & 4) == bool(int(exp['lodo.mesh0.l0cluster0.coneOpen'])),
             L['clusters'][m0[0]]['flags'])
    ck.check('lodo.mesh1.l0cluster0.coneOpen',
             bool(L['clusters'][m1[0]]['flags'] & 4) == bool(int(exp['lodo.mesh1.l0cluster0.coneOpen'])),
             L['clusters'][m1[0]]['flags'])
    for key, ci in (('lodo.mesh0.l0cluster0.sphere', m0[0]), ('lodo.mesh1.l0cluster0.sphere', m1[0])):
        want = [float(x) for x in exp[key].split(',')]
        r = L['clusterLods'][ci]
        got = [r['cx'], r['cy'], r['cz'], r['radius']]
        # the stored radius is widened by the writer's own epsilon, never shrunk
        ck.check(key, all(abs(a - b) < 0.01 for a, b in zip(want[:3], got[:3]))
                 and want[3] <= got[3] <= want[3] * 1.001 + 0.01, got)""")

edit(
    """    ck.check('lodi.chunk4.instanceCount',
             T['chunks'][4]['instanceCount'] == int(exp['lodi.chunk4.instanceCount']),
             T['chunks'][4]['instanceCount'])""",
    """    ck.check('lodi.chunk4.instanceCount',
             T['chunks'][4]['instanceCount'] == int(exp['lodi.chunk4.instanceCount']),
             T['chunks'][4]['instanceCount'])
    # v3
    ck.check('lodi.occluderCount', ih['occluderCount'] == int(exp['lodi.occluderCount']), ih['occluderCount'])
    ck.check('lodi.maxOccludersPerCell',
             ih['maxOccludersPerCell'] == int(exp['lodi.maxOccludersPerCell']), ih['maxOccludersPerCell'])
    if T['occluders']:
        o = T['occluders'][0]
        want = [float(x) for x in exp['lodi.occluder0.centre'].split(',')]
        got = [o['x'], o['y'], o['z']]
        ck.check('lodi.occluder0.centre', all(abs(a - b) < 0.01 for a, b in zip(want, got)), got)
        want = [float(x) for x in exp['lodi.occluder0.half'].split(',')]
        got = [o['hx'], o['hy'], o['hz']]
        ck.check('lodi.occluder0.half', all(abs(a - b) < 0.01 for a, b in zip(want, got)), got)
        ck.check('lodi.occluder0.instanceIndex',
                 o['instanceIndex'] == int(exp['lodi.occluder0.instanceIndex']), o['instanceIndex'])
        ck.check('lodi.occluder0.cell',
                 T['instances'][o['instanceIndex']]['cell'] == int(exp['lodi.occluder0.cell']),
                 T['instances'][o['instanceIndex']]['cell'])""")

edit(
    """class Refusal(Exception):
    pass""",
    """class Refusal(Exception):
    pass


#: `LodoClusterLod::parentError` at a root -- FLT_MAX, so no tolerance is above it
ROOT_ERROR = struct.unpack('<f', struct.pack('<I', 0x7F7FFFFF))[0]""")


def main():
    apply = "--apply" in sys.argv
    raw = open(SRC, "rb").read()
    cr = raw.count(b"\r")
    text = raw.decode("utf-8")
    ok = True
    for old, new in EDITS:
        n = text.count(old)
        print("x%d  %s" % (n, old.strip().split("\n")[0][:76]))
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
